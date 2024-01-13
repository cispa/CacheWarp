# CacheWarp
This repository contains the experiments of evaluation and case studies discussed in the paper 
* "CacheWarp: Software-based Fault Injection using Selective State Reset" (USENIX Security'24). 

You can find the paper at the [USENIX website](https://www.usenix.org/conference/usenixsecurity24/presentation/zhang-ruiyi). For the demos, please check our [website](https://cachewarpattack.com/).

## Overview

We introduce a new software-based fault injection attack on AMD SEV Vms, exploiting the `INVD` instruction. 
It allows the hypervisor to revert data modifications of the VM on a single-store granularity, leading to an old (stale) view of memory for the VM.

## Platform

We tested the attack on AMD EPYC 7252 for SEV-ES, AMD EPYC 7313P and 7443 CPUs for SEV-SNP. 

As suggested by AMD, on AMD EPYC 7252, the host OS, QEMU, and OVMF are built with the master branch (Linux kernel 6.1.0, QEMU v7.2.0-2-g5204b499a6, OVMF commit cda98df, firmware 0.24.15). 

For AMD SEV-SNP, we use the snp-latest branch (commit ad91624, firmware 1.54.01). 
The victim VMs are configured with a single virtual CPU and 4 GB of main memory. 

The victim system is running Ubuntu 20.04 LTS (Linux kernel 5.15.0).

## Mitigation

AMD has tracked the issue as CVE-2023-20592 and provided a microcode update fixing the vulnerability. For more detailed information, we recommend reading the official [AMD Security Bulletin](https://www.amd.com/en/resources/product-security/bulletin/amd-sb-3005.html).


## `INVD` Instruction

The microcode update is not available for 2nd Gen EPYC so you can still test the `INVD` there (or downgrade on 3rd Gen EPYC).

`INVD` is the same as `WBINVD` unless bit 4 of MSR 0xc0010015 is cleared.

Enable `INVD` for all cores:

```bash
sudo bash -c 'modprobe msr; CUR=$(rdmsr 0xc0010015); ENABLED=$(printf "%x" $((0x$CUR & ~16))); wrmsr -a 0xc0010015 0x$ENABLED'
```

Disable it again:

```bash
sudo bash -c 'modprobe msr; CUR=$(rdmsr 0xc0010015); ENABLED=$(printf "%x" $((0x$CUR | 16))); wrmsr -a 0xc0010015 0x$ENABLED'
```


## (RoadMap) Materials
This repository contains the following materials, which are step-by-step to reproduce the results in the paper.
All folders should be self-commented.
Even though we try our best to avoid freezing your system, I'm pretty sure you will probably encounter crashes somehow.
So make sure you are able to reboot your machine before testing it ;)

#### Without VM
1. `l2-l3-prime`: Builds eviction set for L2/L3 cache sets.
2. `timing-after-(wb)invd`: Analyse the scope of `WBINVD` and `INVD`.
3. `timing-with-dirty-cls`: We observe a clear linear relationship between the number of dirty cache lines and the timing of the instruction.
   
#### With VM
4. `kernel-patch`: We incorporate our own interrupt framework to KVM on Linux 6.1.0 (any version higher than 6.0 should work, as two-dimentional page feature is introduced), which provides reliable single-stepping.
5. `userspace-controller`: The code to interact with our interrupt framework.
6. `toy-examples`: Toy-examples we used to illustrate the attacking primitives, DropForge and Timewarp.
7. `rsa-crt`: Blindly drop (without single-stepping) to break rsa-crt implementation.
8. `sudo`: Bypass sudo authentication via DropForge.
9. `openssh`: Bypass openssh authentication via TimeWarp.


## Launch VM

We also provide the script (./lauch-qemu.sh) to launch SEV-ES VM, which is slightly changed from the official one.

```bash
cd AMDSEV
sudo taskset -c 7 ./launch-qemu.sh -hda focal.qcow2 -cdrom ubuntu-20.04.5-desktop-amd64.iso -vnc 1 -console serial -sev-es
```

## Environment Setup

As shown in launch script, the vCPU is pinned to core 7.
You can check the CPU complex (CCX) via `sudo cat /sys/devices/system/cpu/cpu7/cache/index3/shared_cpu_list`
The l3 cache is shared among the entire CCX. 
And from the results of `timing-after-(wb)invd`, you will know the `INVD` invalidates the entire L3 cache within the CCX.
Hence, we offline other cores within the same CCX to make cache cleaner and avoid crashes.

If the CCX contains 8 cores on your CPU, just offline other seven cores.
You can also disable all the prefetch features to reduce noise.

```bash
#!/bin/bash

# Offline other cores within the CCX
echo 0 | sudo tee /sys/devices/system/cpu/cpu14/online
echo 0 | sudo tee /sys/devices/system/cpu/cpu15/online
echo 0 | sudo tee /sys/devices/system/cpu/cpu6/online

sudo cpufreq-set -c 7 -g userspace
# We do not recommend using the maximum frequency (P0) to avoid thermal throttling
sudo cpufreq-set -c 7 -f 2.40GHz

# Enable INVD
sudo bash -c 'modprobe msr; CUR=$(rdmsr 0xc0010015); ENABLED=$(printf "%x" $((0x$CUR & ~16))); wrmsr -p 7 0xc0010015 0x$ENABLED'

# Disable all prefetchers (Optional. The MSR bits vary from Processors)
sudo bash -c 'modprobe msr; CUR=$(rdmsr 0xc0011022); ENABLED=$(printf "%x" $((0x$CUR | 40960))); wrmsr -p 7 0xc0011022 0x$ENABLED'
sudo bash -c 'modprobe msr; CUR=$(rdmsr 0xc001102b); ENABLED=$(printf "%x" $((0x$CUR | 458760))); wrmsr -p 7 0xc001102b 0x$ENABLED'
```

## Contact
If there are questions regarding these experiments, please send an email to `ruiyi.zhang (AT) cispa.de` or message `@Rayiizzz` on Twitter.

## Research Paper
The paper is available at the [USENIX website](https://www.usenix.org/conference/usenixsecurity24/presentation/zhang-ruiyi). 
You can cite our work with the following BibTeX entry:
```latex
@inproceedings{Zhang2024CacheWarp,
  year={2024},
  title={{CacheWarp: Software-based Fault Injection using Selective State Reset}},
  booktitle={USENIX Security},
  author={Ruiyi Zhang and Lukas Gerlach and Daniel Weber and Lorenz Hetterich and Youheng Lü and Andreas Kogler and Michael Schwarz}
}

```

## Disclaimer
We are providing this code as-is. 
You are responsible for protecting yourself, your property and data, and others from any risks caused by this code. 