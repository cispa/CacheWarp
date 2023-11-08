#define _GNU_SOURCE  
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <pthread.h>
#include "cacheutils.h"

#define ASSIST_CORE1 6
#define ASSIST_CORE2 14
#define ASSIST_CORE3 15


#define BUFFER_SIZE 4096 * 4096 * 4
#define TARGET_SIZE 4096 * 16
#define TARGET_CL_NUM   (TARGET_SIZE / 64)
#define TARGET_PAGE_NUM (TARGET_SIZE / 4096)

#define ITERATION 10000

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

// L2 1024 sets (6 + 10 -> 16)
void __attribute__((aligned(0x1000))) prime_l2_set(void* addr, int C, int D, int L, int S){
    for (int s = 0; s < S-D ; s+=L) {
        for(int c = 0; c < C; c++) {
            for(int d = 0; d < D; d++) {
                maccess(addr+((s+d)<<16));
            }
        }
    }
    mfence();
}

// L3 16384 sets (6 + 14 -> 20) 
void __attribute__((aligned(0x1000))) prime_l2_l3_set(void* addr, int C, int D, int L, int S){
    for (int s = 0; s < S-D ; s+=L) {
        for(int c = 0; c < C; c++) {
            for(int d = 0; d < D; d++) {
                maccess(addr+((s+d)<<20));
            }
        }
    }
    mfence();
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


int main(int argc, char *argv[]) {
    
    if (argc < 2) {
	    printf("sudo ./hist <CORE>\n");
        exit(-1);
    }
    
    int core = atoi(argv[1]);
    pin_to_core(core);

    init_pagemap();
    char* data = mmap(NULL, BUFFER_SIZE, PROT_READ|PROT_WRITE, MAP_ANON|MAP_SHARED|MAP_HUGETLB, 0, 0);
    char* test = mmap(NULL, TARGET_SIZE, PROT_READ|PROT_WRITE, MAP_ANON|MAP_SHARED, 0, 0);
    if (data == MAP_FAILED || test == MAP_FAILED) {
        printf("mmap failed\n");
        return -1;
    }

    uint64_t t_arr[ITERATION] = {0};
    memset(data, 1, BUFFER_SIZE);
    memset(test, 1, TARGET_SIZE);
    uint64_t phys_addr = get_physical_addr( (uint64_t)data);
    printf("evict_buffer phys: %p\n", (void*)phys_addr);

    uint64_t target_phys_addr = get_physical_addr( (uint64_t)test);
    printf("target phys: %p\n", (void*)target_phys_addr);

    int set_idx = (target_phys_addr & 0xfffff) >> 6;
    printf("target l3_index: %x\n", set_idx);

    for (int i = 0; i < ITERATION; i++) {
        prime_l2_set(data+(set_idx<<6), 1, 1, 1, 14); mfence();
        t_arr[i] = reload_t(test);
    }

    int l2_miss = majority(t_arr, ITERATION);
    printf("After L2 Prime: %d\n", l2_miss);

    for (int i = 0; i < ITERATION; i++) {
        flush(data); mfence();
        t_arr[i] = reload_t(data);
    }
    int l3_miss = majority(t_arr, ITERATION);
    printf("After Flush: %d\n", l3_miss);

    int threshold = (2 * l2_miss + 3 * l3_miss) / 5; 

    // access all sets once
    for (int i = 0; i < TARGET_CL_NUM; i++) {
        maccess(test+(i<<6)); 
    }

    // brute force
    
    int fail = 0;
    for (int C = 1; C < 4; C++)
    {
        for (int D = 1; D < 3; D++)
        {
            for (int L = 1; L < 2; L++)
            {
                for (int S = 25; S < 40; S++) 
                {
                    int l = 100000;
                    uint64_t evc_time = 0;
                    while(l--) {
                        uint64_t begin = rdtsc();
                        prime_l2_l3_set(data+(set_idx<<6), C, D, L, S);
                        evc_time += (rdtsc() - begin);
                        int delta = reload_t(test);
                        if (delta < threshold){
                            fail++;
                        }
                    }
                    printf("Fail Times: %5d | Evict Duration: %ld | C:%d D:%d L:%d S:%d\n", fail, evc_time/100000, C, D, L, S);
                    if (fail < 2) 
                    {
                        munmap(data, BUFFER_SIZE);
                        munmap(test, TARGET_SIZE);
                        exit(0);
                    }
                    fail = 0;
                }
            }
        }
    }
    
    munmap(data, BUFFER_SIZE);
    munmap(test, TARGET_SIZE);

    return 0;
}
