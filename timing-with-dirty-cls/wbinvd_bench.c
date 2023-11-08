#include "r0e.h"
#include "cacheutlis.h"
#include <stdio.h>

#define PAGE_SIZE 4096
#define REPS 10000
#define MEMSIZE PAGE_SIZE * 256
#define N_LINES 512

uint64_t rdpru_a() {
    uint64_t a, d;
    asm volatile("mfence");
    asm volatile(".byte 0x0f,0x01,0xfd" : "=a"(a), "=d"(d) : "c"(1) : );
    a = (d << 32) | a;
    asm volatile("mfence");
    return a;
}

size_t wbinvd() {
    asm volatile("wbinvd");
    return 0;
}

size_t bench() {
    uint64_t start, end;
    start = rdpru_a();
    flush(&start);
    mfence();
    asm volatile("invd");
    end = rdpru_a();
    return end-start;
}

int main() {

  char* dirtset = mmap(NULL, PAGE_SIZE * 256, PROT_READ|PROT_WRITE, MAP_ANON|MAP_PRIVATE|MAP_HUGETLB, 0, 0);
  if(dirtset == MAP_FAILED) {
    printf("No huge pages enabled\n");
    return -1;
  }

  if (r0e_init()) {
    printf("Could not initialize r0e\n");
    return 1;
  }

  printf("Running up to %d dirty cache lines, for %d reps\n",N_LINES, REPS);

  uint64_t avg_measure[N_LINES] = {0};
  for (size_t r = 0; r < REPS; r++)
  {
    for (size_t i = 0; i < N_LINES; i++)
    {
	r0e_call(wbinvd);
        // Make lines dirty
        for (size_t j = 0; j < i; j++)
        {
            dirtset[j*64] = 0xce;
        }
        // Time wbinvd instruction
        avg_measure[i] += r0e_call(bench);
    }
  }
  printf("dirty line cnt, time\n");
  for (size_t i = 0; i < N_LINES; i++ )
  {
    printf("%ld,%ld\n", i, avg_measure[i]/REPS);
  }
  
  

  r0e_cleanup();
  printf("[r0e] Done!\n");
  return 0;
}
