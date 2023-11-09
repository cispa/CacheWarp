#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <asm/mtrr.h>
#include <sys/ioctl.h>
#include <sched.h>
#include <sys/ioctl.h>
#include <asm/unistd.h>
#include <assert.h>

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

uint64_t add(uint64_t* data) {
  printf("phys: %p\n", get_physical_addr((uint64_t)data));
  uint64_t ret;

  for (int i = 0; i < 10000000; i++)
  asm volatile (
        ".rept 4000\n"
        "movq (%0), %%r11\n"
        "addq $1, %%r11\n"
        "movq %%r11, (%0)\n"
        ".endr\n"

        :: "r"(data) : "memory");

  ret = *data;
  return ret;
}

int main(int argc, char *argv[]) {

    init_pagemap();
    char* data = mmap(NULL, 4096*256, PROT_READ|PROT_WRITE, MAP_ANON|MAP_SHARED|MAP_HUGETLB, 0, 0);
    if (data == MAP_FAILED) {
          printf("mmap failed\n");
          return -1;
    }

    memset(data, 0, 4096*256);
    printf("phys: %p\n", get_physical_addr((uint64_t)data));
    printf("result: %ld\n", add((uint64_t*)(data+225*64)));

    munmap(data, 4096*256);

    return 0;
}