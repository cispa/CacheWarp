# Overview

We introduce a new software-based fault attack on AMD SEV-ES, exploiting inconsistencies between the cache and memory to selectively drop dirty data by an undocumented behavior of the `INVD` instruction. This attack allows the hypervisor to erase any generic memory writes from guest. Moreover, instruction pointer or other general-purpose registers that are on stack can be reset to an earlier state. Based on that, our PoCs show the hypervisor can replace the return value of arbitrary functions with the one of any following functions. 

## Platform

We tested the attack on AMD EPYC 7252 for SEV-ES, AMD EPYC 7313P and 7443 CPUs for SEV-SNP. 

As suggested by AMD, on AMD EPYC 7252, the host OS, QEMU, and OVMF are built with the master branch (Linux kernel 6.1.0, QEMU v7.2.0-2-g5204b499a6, OVMF commit cda98df, firmware 0.24.15). 

For AMD SEV-SNP, we use the snp-latest branch (commit ad91624, firmware 1.54.01). 
The victim VMs are configured with a single virtual CPU and 4 GB of main memory. 

The victim system is running Ubuntu 20.04 LTS (Linux kernel 5.15.0).

## Mitigation

AMD has tracked the issue as CVE-2023-20592 and provided a microcode update fixing the vulnerability. For more detailed information, we recommend reading the official [AMD Security Bulletin](https://www.amd.com/en/resources/product-security/bulletin/amd-sb-3005.html).


## `INVD` Instruction

`INVD` is the same as `WBINVD` unless bit 4 of MSR 0xc0010015 is cleared.

Enable `INVD` for all cores:

```bash
sudo bash -c 'modprobe msr; CUR=$(rdmsr 0xc0010015); ENABLED=$(printf "%x" $((0x$CUR & ~16))); wrmsr -a 0xc0010015 0x$ENABLED'
```

Disable it again:

```bash
sudo bash -c 'modprobe msr; CUR=$(rdmsr 0xc0010015); ENABLED=$(printf "%x" $((0x$CUR | 16))); wrmsr -a 0xc0010015 0x$ENABLED'
```

The microcode update is not available for 1st/2nd Gen EPYC so you can still test the `INVD` there (or downgrade on 3rd Gen EPYC by adding `dis_ucode_ldr` to kernel commandline).

```bash
Edit /etc/default/grub (as root), and run update-grub afterwards (also as root). Changes require a reboot of the system.
```


## (RoadMap) Materials

#### Without VM
1. `l2-l3-prime`: Builds eviction set for L2/L3 cache sets.

#### With VM
1. `kernel-patch`: Some change for the attacker (malicious hypervisor) on Linux 6.1.0
2. `attacker`: The code to interact with our interrupt framework.
3. `pocs`: Toy-examples we used to illustrate the attacking primitives, Selective State Reset, DropForge, and Timewarp.
4. `rsa-crt`: Blindly drop (without single-stepping) to break rsa-crt implementation.

## Environment Setup

Please pin the vCPU to a fixed core.
You can check the CPU complex (CCX) via `sudo cat /sys/devices/system/cpu/cpu<X>/cache/index3/shared_cpu_list`
The l3 cache is shared among the entire CCX. 
The `INVD` invalidates the entire L3 cache within the CCX.
Hence, we offline other cores within the same CCX to make cache cleaner and avoid crashes.

If the CCX contains 8 cores on your CPU, just offline other seven cores.


```bash
#!/bin/bash

# Offline other cores within the CCX, for example, pin the vCPU to core 7
echo 0 | sudo tee /sys/devices/system/cpu/cpu14/online
echo 0 | sudo tee /sys/devices/system/cpu/cpu15/online
echo 0 | sudo tee /sys/devices/system/cpu/cpu6/online

sudo cpufreq-set -c 7 -g userspace
# We do not recommend using the maximum frequency (P0) to avoid thermal throttling
sudo cpufreq-set -c 7 -f 2.40GHz

# Enable INVD
sudo bash -c 'modprobe msr; CUR=$(rdmsr 0xc0010015); ENABLED=$(printf "%x" $((0x$CUR & ~16))); wrmsr -p 7 0xc0010015 0x$ENABLED'
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