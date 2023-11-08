#define __USE_GNU
#define _GNU_SOURCE
#include "ptedit_header.h"
#include <memory.h>
#include <sched.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/mman.h>

#define R0E_VECTOR 200


#ifndef var_to_string
#define __var_to_string(x) #x
#define _var_to_string(x) __var_to_string(x)
#define var_to_string(x) _var_to_string(x)
#endif

/**
 * Struct for the IDT
 */
typedef struct {
  uint16_t limit;
  size_t base __attribute__((packed));
} r0e_idt_t;

/**
 * Struct defining an IDT entry
 */
typedef struct {
  uint16_t offset_1; // offset bits 0..15
  uint16_t selector; // a code segment selector in GDT or LDT
  uint8_t ist;       // bits 0..2 holds Interrupt Stack Table offset, rest of bits zero.
  uint8_t gate : 4;
  uint8_t z : 1;
  uint8_t dpl : 2;
  uint8_t p : 1;
  uint16_t offset_2; // offset bits 16..31
  uint32_t offset_3; // offset bits 32..63
  uint32_t zero;     // reserved
} r0e_idt_entry_t;

/**
 * Function pointer signature for the callback function
 */
typedef size_t (*r0e_callback_t)(void);

/** Original return address for the IRQ handler */
static size_t __attribute__((aligned(4096))) r0e_ret_addr = 0;
/** Address of callback function */
static size_t __attribute__((aligned(4096))) r0e_entry_point = 0;
/** Virtual address of the IDT */
static r0e_idt_entry_t *r0e_sys_idt;
/** Original IDT entry for the used interrupt vector */
static r0e_idt_entry_t r0e_original_entry;

/**
 * User-space interrupt handler for calling the callback function.
 * Disables SMAP and SMEP to make all virtual memory usable for the callback. 
 */
static void __attribute__((naked, aligned(4096))) r0e_irq_handler() {
  asm volatile("mov %cr4, %r11\n"
               "andq $~0x300000, %r11\n"
               "mov %r11, %cr4\n"               // disable SMAP and SMEP
               "mov (%rsp), %r11\n"
               "mov %r11, r0e_ret_addr(%rip)\n" // save original return address
               "mov r0e_entry_point(%rip), %r11\n"
               "callq *%r11\n"                  // call callback
               "mov r0e_ret_addr(%rip), %r11\n"
               "mov %r11, (%rsp)\n"             // restore return address
               "iretq\n"                        // return to ring 3
               ".align 4096\n"
               "nop\n");                        // ensure irq handler is at least one page
}

/**
 * Pretty-print an IDT entry
 *
 * @param[in] entry The IDT entry
 *
 */
static void r0e_dump_idt_entry(r0e_idt_entry_t entry) {
  printf(
      "Selector(%d), IST(%d), P(%d), DPL(%d), Z(%d), gate(%d), offset(%zx)\n",
      entry.selector, entry.ist, entry.p, entry.dpl, entry.z, entry.gate,
      entry.offset_1 | (((size_t)entry.offset_2) << 16) |
          (((size_t)entry.offset_3) << 32));
}

/**
 * Map the IDT to the virtual-address space
 *
 * @return A virtual address that can be used to access the IDT
 */
static void *r0e_map_idt() {
  r0e_idt_t idt;
  asm volatile("sidt %0" : "=m"(idt) : : "memory");
  // umip prevents us from getting the real IDT base address, make an educated guess for Linux
  if(idt.base >= 0xffffffffffff0000ull) {
       idt.base = 0xfffffe0000000000ull;
  }
  size_t pfn = ptedit_pte_get_pfn((void *)(idt.base), 0);
  pfn &= 0x7ffffffff;
  if(!pfn) {
    return NULL;
  }
  return ptedit_pmap(pfn * 4096, 4096);
}

int r0e_lock_user_page(void* addr) {
  *(volatile char *)addr; // force mapping
  // prevent swapping
  return mlock(addr, 4096);
}

/**
 * Initialize r0e
 *
 * @return 0 on sucess, 1 otherwise
 */
int r0e_init() {
  if (ptedit_init()) {
    fprintf(stderr, "[r0e] Could not init PTEditor\n");
    return 1;
  }

#if 0
  // freeze to core
  int cpu = 0;
  getcpu(&cpu, NULL);
  cpu_set_t mask;
  mask.__bits[0] = 1 << cpu;
  if (sched_setaffinity(getpid(), sizeof(cpu_set_t), &mask)) {
    fprintf(stderr, "[r0e] Could not lock application to current core\n");
    return 1;
  }
#endif

  if(r0e_lock_user_page(r0e_irq_handler)) {
    fprintf(stderr, "[r0e] Could not lock IRQ handler in memory\n");
    return 1;
  }
  if(r0e_lock_user_page(&r0e_ret_addr) || r0e_lock_user_page(&r0e_entry_point)) {
    fprintf(stderr, "[r0e] Could not lock global data in memory\n");
    return 1;
  }
  // bring irq handler into the kernel
  ptedit_pte_clear_bit(r0e_irq_handler, 0, PTEDIT_PAGE_BIT_USER); 

  // map IDT to add IRQ handler
  r0e_sys_idt = (r0e_idt_entry_t *)r0e_map_idt();
  if(!r0e_sys_idt) {
    fprintf(stderr, "[r0e] Could not find IDT\n");
    return 1;
  }

#define LOW_PTR(x) (((size_t)(x)) & 0xffff)
#define MID_PTR(x) ((((size_t)(x)) >> 16) & 0xffff)
#define HIGH_PTR(x) ((((size_t)(x)) >> 32) & 0xffffffff)
#define GATE_INTERRUPT 14
#define GATE_TRAP 15
#define USER_DPL 3
#define KERNEL_DPL 0

  r0e_idt_entry_t patched_idt_entry;
  patched_idt_entry.dpl = USER_DPL;
  patched_idt_entry.p = 1;
  patched_idt_entry.z = 0;
  patched_idt_entry.ist = 0;
  patched_idt_entry.gate = GATE_INTERRUPT;
  patched_idt_entry.selector = 16;
  patched_idt_entry.offset_1 = LOW_PTR(r0e_irq_handler);
  patched_idt_entry.offset_2 = MID_PTR(r0e_irq_handler);
  patched_idt_entry.offset_3 = HIGH_PTR(r0e_irq_handler);

  r0e_original_entry = r0e_sys_idt[R0E_VECTOR];
  r0e_sys_idt[R0E_VECTOR] = patched_idt_entry;
  return 0;
}

/**
 * Cleanup r0e when no other functionality of r0e is required anymore
 */
void r0e_cleanup() {
  r0e_sys_idt[R0E_VECTOR] = r0e_original_entry;
  munmap(r0e_sys_idt, 4096);
  munlock(r0e_irq_handler, 4096);
  ptedit_cleanup();
}

/**
 * Execute a callback function in ring 0
 *
 * @param[in] fnc The callback function that should be executed in ring 0
 *
 * @return The return value of the callback function
 */
size_t __attribute__((naked)) r0e_unsafe_call(r0e_callback_t fnc) {
  asm volatile("push %%rbx\n"
               "push %%rcx\n"
               "push %%rdx\n"
               "push %%rsi\n"
               "push %%rdi\n"
               "push %%r8\n"
               "push %%r9\n"
               "push %%r10\n"
               "push %%r11\n" // save registers
               "mov %%rdi, %0\n" // store entry point in r0e_entry_point
               "int $" var_to_string(R0E_VECTOR) "\n" // go to ring0 using interrupt
               "pop %%r11\n"
               "pop %%r10\n"
               "pop %%r9\n"
               "pop %%r8\n"
               "pop %%rdi\n"
               "pop %%rsi\n"
               "pop %%rdx\n"
               "pop %%rcx\n"
               "pop %%rbx\n" // restore registers
               "retq" : "=m"(r0e_entry_point) : : "memory");
}

/**
 * Execute a callback function in ring 0, ensuring that the executed code is in memory
 *
 * @param[in] fnc The callback function that should be executed in ring 0
 *
 * @return The return value of the callback function
 */
size_t r0e_call(r0e_callback_t fnc) {
  if(r0e_lock_user_page(fnc)) {
      return -1;
  }
  return r0e_unsafe_call(fnc);
}
