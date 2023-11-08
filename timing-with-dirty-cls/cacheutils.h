#ifndef _CACHEUTILS_H_
#define _CACHEUTILS_H_

#include <assert.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <linux/perf_event.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <stdint.h>
#include <signal.h>
#include <setjmp.h>

#define RDPRU ".byte 0x0f, 0x01, 0xfd"
#define RDPRU_ECX_MPERF	0
#define RDPRU_ECX_APERF	1

#define ARM_PERF            1
#define ARM_CLOCK_MONOTONIC 2
#define ARM_TIMER           3

/* ============================================================
 *                    User configuration
 * ============================================================ */
size_t CACHE_MISS = 0;

#define USE_RDTSC_BEGIN_END     0

#define USE_RDTSCP              1

#define ARM_CLOCK_SOURCE        ARM_CLOCK_MONOTONIC

/* ============================================================
 *                  User configuration End
 * ============================================================ */

// ---------------------------------------------------------------------------
static size_t perf_fd;
void perf_init() {
  static struct perf_event_attr attr;
  attr.type = PERF_TYPE_HARDWARE;
  attr.config = PERF_COUNT_HW_CPU_CYCLES;
  attr.size = sizeof(attr);
  attr.exclude_kernel = 1;
  attr.exclude_hv = 1;
  attr.exclude_callchain_kernel = 1;

  perf_fd = syscall(__NR_perf_event_open, &attr, 0, -1, -1, 0);
  assert(perf_fd >= 0);

  // ioctl(perf_fd, PERF_EVENT_IOC_RESET, 0);
}

#if defined(__i386__) || defined(__x86_64__)
// ---------------------------------------------------------------------------
uint64_t rdtsc() {
  uint64_t a, d;
  asm volatile("mfence");
#if USE_RDTSCP
  asm volatile("rdtscp" : "=a"(a), "=d"(d) :: "rcx");
#else
  asm volatile("rdtsc" : "=a"(a), "=d"(d));
#endif
  a = (d << 32) | a;
  asm volatile("mfence");
  return a;
}

// ---------------------------------------------------------------------------
uint64_t __rdtsc_begin() {
  uint64_t a, d;
  asm volatile ("mfence\n\t"
    "CPUID\n\t"
    "RDTSCP\n\t"
    "mov %%rdx, %0\n\t"
    "mov %%rax, %1\n\t"
    "mfence\n\t"
    : "=r" (d), "=r" (a)
    :
    : "%rax", "%rbx", "%rcx", "%rdx");
  a = (d<<32) | a;
  return a;
}

// ---------------------------------------------------------------------------
uint64_t __rdtsc_end() {
  uint64_t a, d;
  asm volatile("mfence\n\t"
    "RDTSCP\n\t"
    "mov %%rdx, %0\n\t"
    "mov %%rax, %1\n\t"
    "CPUID\n\t"
    "mfence\n\t"
    : "=r" (d), "=r" (a)
    :
    : "%rax", "%rbx", "%rcx", "%rdx");
  a = (d<<32) | a;
  return a;
}

// ---------------------------------------------------------------------------
void flush(void *p) { asm volatile("clflush 0(%0)\n" : : "c"(p) : "rax"); }

// ---------------------------------------------------------------------------
void maccess(void *p) { asm volatile("movq (%0), %%rax\n" : : "c"(p) : "rax"); }

void maccess_wr(void *p, int val) { asm volatile("movq %%rax, (%1)\n" : : "a"(val), "c"(p) : ); }

// ---------------------------------------------------------------------------
void mfence() { asm volatile("mfence"); }

// ---------------------------------------------------------------------------
void nospec() { asm volatile("lfence"); }

#define speculation_start(label) asm goto ("call %l0" : : : : label##_retp); 
#define speculation_end(label) asm goto("jmp %l0" : : : : label); label##_retp: asm goto("lea %l0(%%rip), %%rax\nmovq %%rax, (%%rsp)\nret\n" : : : "rax" : label); label: asm volatile("nop");

// example usage: asm volatile(INTELASM("clflush [rax]\n\t"));
#define INTELASM(code) ".intel_syntax noprefix\n\t" code "\n\t.att_syntax prefix\n"


#include <cpuid.h>
// ---------------------------------------------------------------------------
unsigned int xbegin() {
  unsigned status;
  asm volatile(".byte 0xc7,0xf8,0x00,0x00,0x00,0x00" : "=a"(status) : "a"(-1UL) : "memory");
  return status;
}

// ---------------------------------------------------------------------------
void xend() {
  asm volatile(".byte 0x0f; .byte 0x01; .byte 0xd5" ::: "memory");
}

// ---------------------------------------------------------------------------
int has_tsx() {
  if (__get_cpuid_max(0, NULL) >= 7) {
    unsigned a, b, c, d;
    __cpuid_count(7, 0, a, b, c, d);
    return (b & (1 << 11)) ? 1 : 0;
  } else {
    return 0;
  }
}

// ---------------------------------------------------------------------------
void maccess_tsx(void* ptr) {
    if (xbegin() == (~0u)) {
        maccess(ptr);
        xend();
    }
}

#elif defined(__aarch64__)
#if ARM_CLOCK_SOURCE == ARM_CLOCK_MONOTONIC
#include <time.h>
#endif

// ---------------------------------------------------------------------------
uint64_t rdtsc() {
#if ARM_CLOCK_SOURCE == ARM_PERF
  long long result = 0;

  asm volatile("DSB SY");
  asm volatile("ISB");

  if (read(perf_fd, &result, sizeof(result)) < (ssize_t) sizeof(result)) {
    return 0;
  }

  asm volatile("ISB");
  asm volatile("DSB SY");

  return result;
#elif ARM_CLOCK_SOURCE == ARM_CLOCK_MONOTONIC
  asm volatile("DSB SY");
  asm volatile("ISB");
  struct timespec t1;
  clock_gettime(CLOCK_MONOTONIC, &t1);
  uint64_t res = t1.tv_sec * 1000 * 1000 * 1000ULL + t1.tv_nsec;
  asm volatile("ISB");
  asm volatile("DSB SY");
  return res;
#elif ARM_CLOCK_SOURCE == ARM_TIMER
  uint64_t result = 0;

  asm volatile("DSB SY");
  asm volatile("ISB");
  asm volatile("MRS %0, cntvct_el0" : "=r"(result));
  asm volatile("DSB SY");
  asm volatile("ISB");

  return result;
#else
#error Clock source not supported
#endif
}
// ---------------------------------------------------------------------------
uint64_t __rdtsc_begin() {
#if ARM_CLOCK_SOURCE == ARM_PERF
  long long result = 0;

  asm volatile("DSB SY");
  asm volatile("ISB");

  if (read(perf_fd, &result, sizeof(result)) < (ssize_t) sizeof(result)) {
    return 0;
  }

  asm volatile("DSB SY");

  return result;
#elif ARM_CLOCK_SOURCE == ARM_CLOCK_MONOTONIC
  asm volatile("DSB SY");
  asm volatile("ISB");
  struct timespec t1;
  clock_gettime(CLOCK_MONOTONIC, &t1);
  uint64_t res = t1.tv_sec * 1000 * 1000 * 1000ULL + t1.tv_nsec;
  asm volatile("DSB SY");
  return res;
#elif ARM_CLOCK_SOURCE == ARM_TIMER
  uint64_t result = 0;

  asm volatile("DSB SY");
  asm volatile("ISB");
  asm volatile("MRS %0, PMCCNTR_EL0" : "=r"(result));
  asm volatile("ISB");

  return result;
#else
#error Clock source not supported
#endif
}


// ---------------------------------------------------------------------------
uint64_t __rdtsc_end() {
#if ARM_CLOCK_SOURCE == ARM_PERF
  long long result = 0;

  asm volatile("DSB SY");

  if (read(perf_fd, &result, sizeof(result)) < (ssize_t) sizeof(result)) {
    return 0;
  }

  asm volatile("ISB");
  asm volatile("DSB SY");

  return result;
#elif ARM_CLOCK_SOURCE == ARM_CLOCK_MONOTONIC
  asm volatile("DSB SY");
  struct timespec t1;
  clock_gettime(CLOCK_MONOTONIC, &t1);
  uint64_t res = t1.tv_sec * 1000 * 1000 * 1000ULL + t1.tv_nsec;
  asm volatile("ISB");
  asm volatile("DSB SY");
  return res;
#elif ARM_CLOCK_SOURCE == ARM_TIMER
  uint64_t result = 0;

  asm volatile("DSB SY");
  asm volatile("MRS %0, PMCCNTR_EL0" : "=r"(result));
  asm volatile("DSB SY");
  asm volatile("ISB");

  return result;
#else
#error Clock source not supported
#endif
}

// ---------------------------------------------------------------------------
void flush(void *p) {
  asm volatile("DC CIVAC, %0" ::"r"(p));
  asm volatile("DSB ISH");
  asm volatile("ISB");
}

// ---------------------------------------------------------------------------
void maccess(void *p) {
  volatile uint32_t value;
  asm volatile("LDR %0, [%1]\n\t" : "=r"(value) : "r"(p));
  asm volatile("DSB ISH");
  asm volatile("ISB");
}

// ---------------------------------------------------------------------------
void mfence() { asm volatile("DSB ISH"); }

// ---------------------------------------------------------------------------
void nospec() { asm volatile("DSB SY\nISB"); }


#elif defined(__riscv)

static inline uint64_t rdtsc() {
  uint64_t val;
  asm volatile("fence rw,rw" : : : "memory");
  asm volatile ("rdcycle %0" : "=r"(val));
  asm volatile("fence rw,rw" : : : "memory");
  return val;
}

//Implementation just for legacy 
// ---------------------------------------------------------------------------
static inline uint64_t __rdtsc_begin() {
  return rdtsc();
}

//Implementation just for legacy 
// ---------------------------------------------------------------------------
static inline uint64_t __rdtsc_end() {
  return rdtsc(); 
}

//dcache flush according to: https://wiki.attacking.systems/en/compiler/unknown-instructions
// ---------------------------------------------------------------------------
static inline void flush(void* addr) {
    asm volatile("xor a7, a7, a7\n"
                 "add a7, a7, %0\n"
                 ".long 0x278800b" // DCACHE.CIVA a7
                 : : "r"(addr) : "a7","memory"); 
}

//dereference memory address at adddr and load double word 
// ---------------------------------------------------------------------------
static inline void maccess(void* addr) {
    asm volatile("ld a7, (%0)" : : "r"(addr) : "a7","memory");
}

//all loads and writes finish before continue
// ---------------------------------------------------------------------------
static inline void mfence() {asm volatile("fence rw,rw" : : : "memory");} 

//all loads finish before continue
// ---------------------------------------------------------------------------
static inline void nospec() {asm volatile("fence r,rw" : : : "memory");} 

#endif

// ---------------------------------------------------------------------------
int flush_reload(void *ptr) {
  uint64_t start = 0, end = 0;

#if USE_RDTSC_BEGIN_END
  start = __rdtsc_begin();
#else
  start = rdtsc();
#endif
  maccess(ptr);
#if USE_RDTSC_BEGIN_END
  end = __rdtsc_end();
#else
  end = rdtsc();
#endif

  mfence();

  flush(ptr);

  if (end - start < CACHE_MISS) {
    return 1;
  }
  return 0;
}

// ---------------------------------------------------------------------------
int flush_reload_t(void *ptr) {
  uint64_t start = 0, end = 0;

#if USE_RDTSC_BEGIN_END
  start = __rdtsc_begin();
#else
  start = rdtsc();
#endif
  maccess(ptr);
#if USE_RDTSC_BEGIN_END
  end = __rdtsc_end();
#else
  end = rdtsc();
#endif

  mfence();

  flush(ptr);

  return (int)(end - start);
}

// ---------------------------------------------------------------------------
int reload_t(void *ptr) {
  uint64_t start = 0, end = 0;

#if USE_RDTSC_BEGIN_END
  start = __rdtsc_begin();
#else
  start = rdtsc();
#endif
  maccess(ptr);
#if USE_RDTSC_BEGIN_END
  end = __rdtsc_end();
#else
  end = rdtsc();
#endif

  mfence();

  return (int)(end - start);
}


// ---------------------------------------------------------------------------
size_t detect_flush_reload_threshold() {
  size_t reload_time = 0, flush_reload_time = 0, i, count = 1000000;
  size_t dummy[16];
  size_t *ptr = dummy + 8;

  maccess(ptr);
  for (i = 0; i < count; i++) {
    reload_time += reload_t(ptr);
  }
  for (i = 0; i < count; i++) {
    flush_reload_time += flush_reload_t(ptr);
  }
  reload_time /= count;
  flush_reload_time /= count;

  return (flush_reload_time + reload_time * 2) / 3;
}

// ---------------------------------------------------------------------------
void maccess_speculative(void* ptr) {
    int i;
    size_t dummy = 0;
    void* addr;

    for(i = 0; i < 50; i++) {
        size_t c = ((i * 167) + 13) & 1;
        addr = (void*)(((size_t)&dummy) * c + ((size_t)ptr) * (1 - c));
        flush(&c);
        mfence();
        if(c / 0.5 > 1.1) maccess(addr);
    }
}


// ---------------------------------------------------------------------------
static jmp_buf trycatch_buf;

// ---------------------------------------------------------------------------
void unblock_signal(int signum __attribute__((__unused__))) {
  sigset_t sigs;
  sigemptyset(&sigs);
  sigaddset(&sigs, signum);
  sigprocmask(SIG_UNBLOCK, &sigs, NULL);
}

// ---------------------------------------------------------------------------
void trycatch_segfault_handler(int signum) {
  (void)signum;
  unblock_signal(SIGSEGV);
  unblock_signal(SIGFPE);
  longjmp(trycatch_buf, 1);
}

// ---------------------------------------------------------------------------
int try_start() {
#if defined(__i386__) || defined(__x86_64__)
    if(has_tsx()) {
        unsigned status;
        // tsx begin
        asm volatile(".byte 0xc7,0xf8,0x00,0x00,0x00,0x00"
                 : "=a"(status)
                 : "a"(-1UL)
                 : "memory");
        return status == (~0u);
    } else 
#endif
    {
        signal(SIGSEGV, trycatch_segfault_handler); 
        signal(SIGFPE, trycatch_segfault_handler); 
        return !setjmp(trycatch_buf);
    }
}

// ---------------------------------------------------------------------------
void try_end() {
#if defined(__i386__) || defined(__x86_64__)
    if(!has_tsx()) 
#endif
    {
        signal(SIGSEGV, SIG_DFL);
        signal(SIGFPE, SIG_DFL);
    }
}

// ---------------------------------------------------------------------------
void try_abort() {
#if defined(__i386__) || defined(__x86_64__)
    if(has_tsx()) {
        asm volatile(".byte 0x0f; .byte 0x01; .byte 0xd5" ::: "memory");
    } else 
#endif
    {
        maccess(0);
    }
}


#endif
