We implement the interrupt framework based on Linux 6.1.0 (commit 6f1f5caed5bfadd1cc8bdb0563eb8874dc3573ca).

It allows the attacker to interrupt the VM at any attacker-chosen points to exploit the VM.
The framework privides three abilities to perform the attack, namely reliable single-stepping, when to drop (location), and what to drop (written back unrelated dirty cachelines).

On SEV-SNP, the register changes is not available anymore. 
Please see the paper for more information.

`kvm_unpre_all()`, `kvm_unpre_gfn`, `kvm_unpre_all_except_gfn` contain the code to clear the present bit of guest pages in the TDP. 

In `svm_vcpu_create()`, the malicious hypervisor can easily use huge pages as eviction buffers. 

`intr_interception()` contains the code to interact with the controller in user-space.
On SEV-ES, the attacker can track the changes of guest register state (by reading the ciphertext of VMSA) for each instruction within each page (by reading the error_info in NPF_handler).
The attacker can debug locally via e.g., GDB to observe a special sequence to locate the target to drop. (Why is dynamically debug needed? E.g., `xor esi, esi` does not modify esi if esi was `0`.)
On SEV-SNP, the register changes is not available anymore. 
Please see the paper for more information.

`npf_interception` contains the code to set APIC timer for the next timer interrupt.

`svm_vcpu_enter_exit()` contains the code for L2-L3 cache eviction and invalidate the cache (L2 and the entire L3 shared with the CCX).

To maintain memory consistency of other cores, the attacker can evict other dirty data into the main memory. Note that the eviction here is for a L3 with 16384 sets which is shared among 4 cores.

`__svm_sev_es_vcpu_run()` contains the code that we used to reset the APIC Timer before `VMRUN`.