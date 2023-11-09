Before you try any experiments that need to drop smth, make sure you setup the environment correctly, e.g., MSR bit, offline cores.

## Documentation for params

All the parameters are written to a shared page for interaction/guidance.

**num_non_zeros**: Number of non-zero steps we want to perform.

**interval**: APIC interval writing to TMICT at the end of NPF handler.

**DEC**: Flag of whether need to decrypt VMSA (Only in debug mode, e.g., boot VM with -allow-debug).

**UC_VMSA**: Mark VMSA into uncacheable (Note that we currently hardcode using MTRR 0x208/0x209, please check which pair is available on your system).

**INVD**: Invalidate cache after VM breaks (when we find the target).

**NO_STEP**: Do not care the stepping is zero step or not (i.e., do not observe VMSA).

**0x77**: Hook Switch.

**PAGE_VERBOSE**: Print guest physical addr of each NPF (Instruction Fetch).

**INSTR_VERBOSE**: Register change of each non-zero-step.

**VEC_SEQ**: 
`1` stands for a special sequence of register changes is provided to locate the target.
`0` stands for blindly drop

**count**: How many instructions (single-step) needs to be executed after the sequence is matched.

**OFFSET 2050**: The correct register change of the last 2nd instruction before the target instruction.

**OFFSET 2054**: The correct register change of the last instruction before the target instruction.

**OFFSET 2052**: Drop when the pattern is matched `i`th.

**WBNOINVD**: Execute `WBNOINVD` before `VMRUN`

**WBINVD**: Execute `WBINVD` before `VMRUN`

**RUNTIME**: Want to know the timing of each stepping

**ZS_TLB_FLUSH**: Clear the present bit of the current guest page (Instruction Fetch) even after a zero-step.
