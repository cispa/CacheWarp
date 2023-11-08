#define _GNU_SOURCE  
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include "cacheutils.h"

#include "../module/leaky.h"

#define ISOLATE_CORE 1
#define BUFFER_SIZE 4096 * 4096 * 4
#define ITERATION 10000
#define HIST_LEN 2000

#define LOGFILE "invd-scope-hist.csv"

int g_pagemap_fd = -1;

// Extract the physical page number from a Linux /proc/PID/pagemap entry.
uint64_t frame_number_from_pagemap(uint64_t value) {
  return value & ((1ULL << 54) - 1);
}

void init_pagemap() {
  g_pagemap_fd = open("/proc/self/pagemap", O_RDONLY);
  assert(g_pagemap_fd >= 0);
}

uint64_t get_physical_addr(uint64_t virtual_addr) {
  uint64_t value;
  off_t offset = (virtual_addr / 4096) * sizeof(value);
  int got = pread(g_pagemap_fd, &value, sizeof(value), offset);
  assert(got == 8);

  // Check the "page present" flag.
  assert(value & (1ULL << 63));

  uint64_t frame_num = frame_number_from_pagemap(value);
  return (frame_num * 4096) | (virtual_addr & (4095));
}

void __attribute__((aligned(0x1000))) prime_l3_set(void* addr, int C, int D, int L, int S){
  for (int s = 0; s < S-D ; s+=L ){
    for(int c = 0; c < C; c++) {
      for(int d = 0; d < D; d++) {
	      maccess(addr+((s+d) << 20));
      }
    }
  }
}

// L2 1024 sets
void __attribute__((aligned(0x1000))) prime_l2_set(void* addr, int C, int D, int L, int S){
  for (int s = 0; s < S-D ; s+=L ){
    for(int c = 0; c < C; c++) {
      for(int d = 0; d < D; d++) {
	      maccess(addr+((s+d) << 16));
      }
    }
  }
}

// L1 64 sets
void __attribute__((aligned(0x1000))) prime_l1_set(void* addr, int C, int D, int L, int S){
  for (int s = 0; s < S-D ; s+=L ){
    for(int c = 0; c < C; c++) {
      for(int d = 0; d < D; d++) {
	      maccess(addr+((s+d) << 12));
      }
    }
  }
}

uint64_t majority(uint64_t* arr, int n) {
  uint64_t res = 0;
  for (int i = 0; i < 16; ++i) {
    int ones = 0, zeros = 0;

    for (int j = 0; j < n - 1; j++) {
      if ((arr[j] & (1 << i)) != 0) ++ones;
      else ++zeros;
    }
    if (ones > zeros) res |= (1 << i);
  }
  return res;
}

void pin_to_core(int core) {
    cpu_set_t cpuset;
    pthread_t thread;

    thread = pthread_self();

    CPU_ZERO(&cpuset);
    CPU_SET(core, &cpuset);

    pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
}

size_t l1_histogram[HIST_LEN], l2_histogram[HIST_LEN], l3_histogram[HIST_LEN], dram_histogram[HIST_LEN], invd_histogram[HIST_LEN*4], invd_ht_histogram[HIST_LEN*4];
char __attribute__((aligned(4096))) addr[4096];

#define MEASURE_CORE 7
#define WBINVD_CORE 6
#define WBINVD_HT_CORE 14

volatile int t_sync = 0;

void* wbinvd_thread(void* dummy) {
    pin_to_core(WBINVD_CORE);
    int leaky_fd = open(LEAKY_DEVICE_PATH, O_RDONLY);
    if (leaky_fd < 0) {
        fprintf(stderr, "Error: Could not open userkernel device: %s\n", LEAKY_DEVICE_PATH);
        exit(-1);
    }
    
    while (1) {
      while (!t_sync) {sched_yield();}
      ioctl(leaky_fd, LEAKY_IOCTL_CMD_WBINVD, (size_t)0);
      t_sync = 0;
    }
  
    exit(EXIT_SUCCESS);
    
}

int main(int argc, char *argv[]) {
    FILE* logfile = fopen(LOGFILE, "w+");
    if (logfile == NULL) {
      fprintf(stderr, "Error: Could not open logfile: %s\n", LOGFILE);
      return -1;
    }

    fprintf(logfile, "Time,l1,l2,l3,dram,invd_not_ht,invd_ht\n");

    pin_to_core(MEASURE_CORE);

    pthread_t p;
    pthread_create(&p, NULL, wbinvd_thread, NULL);
    sched_yield();


    init_pagemap();
    char* data = mmap(NULL, BUFFER_SIZE, PROT_READ|PROT_WRITE, MAP_ANON|MAP_SHARED|MAP_HUGETLB, 0, 0);
    if (data == MAP_FAILED) {
          printf("mmap failed\n");
          return -1;
    }

    uint64_t t_arr[ITERATION] = {0};
    memset(addr, 1, 4096);
    memset(data, 1, BUFFER_SIZE);

    uint64_t target_phys_addr = get_physical_addr( (uint64_t)addr);
    // printf("target phys: %p\n", (void*)target_phys_addr);

    int l1_set_idx = (target_phys_addr & 0xfff) >> 6;
    int l2_set_idx = (target_phys_addr & 0xffff) >> 6;
    int l3_set_idx = (target_phys_addr & 0xfffff) >> 6;

    for (int i = 0; i < ITERATION; i++) {
      maccess(addr);
      int delta = reload_t(addr);
      if (delta < HIST_LEN)
        l1_histogram[delta]++;
      t_arr[i] = delta;
    }

    uint64_t l1_hit = majority(t_arr, ITERATION);
    printf("L1 Hits:%ld\n", l1_hit);

    for (int i = 0; i < ITERATION; i++) {
      prime_l1_set((void*)(data+(l1_set_idx<<6)), 1, 1, 1, 10); mfence();
      int delta = reload_t(addr);
      if (delta < HIST_LEN)
        l2_histogram[delta]++;
      t_arr[i] = delta;
    }
    int l1_miss = majority(t_arr, ITERATION);
    printf("L2 Hits: %d\n", l1_miss);
    
    for (int i = 0; i < ITERATION; i++) {
      int idx = i % 64;
      prime_l2_set((void*)(data+((l2_set_idx+idx)<<6)), 1, 1, 1, 18); mfence();
      int delta = reload_t(addr+(idx<<6));
      if (delta < HIST_LEN)
        l3_histogram[delta]++;
      maccess(addr);
    }

    for (int i = 0; i < ITERATION; i++) {
      flush(addr);
      int delta = reload_t(addr);
      if (delta < HIST_LEN)
    	  dram_histogram[delta]++;
    }

    maccess(addr);
    for (int i = 0; i < ITERATION * 4; i++) {
      switch (i&3) {
        // L2
        case 0:
          prime_l1_set((void*)(data+(l1_set_idx<<6)), 1, 1, 1, 10); mfence();
          break;
        // L3
        case 1:
          prime_l2_set((void*)(data+(l2_set_idx<<6)), 1, 1, 1, 18); mfence();
          break;
        // DRAM
        case 2:
          prime_l3_set((void*)(data+(l3_set_idx<<6)), 1, 1, 1, 38); mfence();
          break;
        // L1
        case 3:
          maccess(addr); mfence();
          break;
        default:
          break;
      }

      t_sync = 1;
      while (t_sync) {asm volatile("nop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\n"); sched_yield();}
      mfence();

      int delta = reload_t(addr);
      if (delta < HIST_LEN)
        invd_histogram[delta]++;
      maccess(addr);
    }

    pin_to_core(WBINVD_HT_CORE); 
    flush_reload(addr);

    for (int i = 0; i < ITERATION * 4; i++) {
      switch (i&3) {
        // L2
        case 0:
          prime_l1_set((void*)(data+(l1_set_idx<<6)), 1, 1, 1, 12); mfence();
          break;
        // L3
        case 1:
          prime_l2_set((void*)(data+(l2_set_idx<<6)), 1, 1, 1, 20); mfence();
          break;
        // DRAM
        case 2:
          prime_l3_set((void*)(data+(l3_set_idx<<6)), 1, 1, 1, 38); mfence();
          break;
        // L1
        case 3:
          maccess(addr); mfence();
          break;
        default:
          break;
      }

      t_sync = 1;
      while (t_sync) {asm volatile("nop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\n"); sched_yield();}
      mfence();
      
      int delta = reload_t(addr);
      if (delta < HIST_LEN)
        invd_ht_histogram[delta]++;
      maccess(addr);
    }

    for (size_t i = 0; i < HIST_LEN; i++) {
      size_t l1 = 0, l2 = 0, l3 = 0, dram = 0, invd = 0, invd_ht = 0;
      l1 += l1_histogram[i];
      l2 += l2_histogram[i];
      l3 += l3_histogram[i];
      dram += dram_histogram[i];
      invd += invd_histogram[i];
      invd_ht += invd_ht_histogram[i];
      fprintf(logfile, "%4zu,%5zu,%5zu,%5zu,%5zu,%5zu,%5zu\n", i, l1, l2, l3, dram, invd, invd_ht);
    }

    munmap(data, BUFFER_SIZE);
    munmap(addr, 4096);

    return 0;
}
