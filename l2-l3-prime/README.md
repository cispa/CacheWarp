Find a pattern to evict a L1/L2 cache line to non-inclusive L3 and then evict it to memory.
The threat model allows the attacker to use huge pages. 
So it is much easier to build eviction sets as long as we know the physical address of the target.

### Usage
``` Bash
# Enable huge pages (here 32)
sudo sysctl -w vm.nr_hugepages=32

sudo ./hist <CORE>
```

### Detail

CPUID outputs the number of L2 sets is 1024 and the number of L3 sets is 16384.
```
L2 index bits: 06-15 | L3 index bits: 06-19
```
We use 32 * 2MB huge pages for evict buffer and then extract the physical address of the target.
Then just use the address with same offset within the huge pages as candidate of eviction set. 

Find a L2 prime pattern should be trival. 
The L2 has 8 ways and L3 has 16 ways. Here we just use 14 address for L2 eviction set (line 130). 

As the L3 cache is non-inclusive, the timing of reloading the target now should be a L3 hit.
We calculate the threshold of L3 miss via `(2*L3 hit + 3*DRAM Access) / 5`.

Then the brute force part just to find a reliable pattern for L2+L3 prime.
Note that, this L2_L3 prime pattern which is reliable on your CPUs should be ported into kernel later. 

### Expected Output

```
evict_buffer phys: 0x1e1a00000
target phys: 0x1d33d3000
L3_index: 34c0
After L2 Prime: 235
After Flush: 675
Fail Times: 57393 | Evict Duration: 3139 | C:1 D:1 L:1 S:25
Fail Times: 33567 | Evict Duration: 3369 | C:1 D:1 L:1 S:26
Fail Times: 23227 | Evict Duration: 3628 | C:1 D:1 L:1 S:27
Fail Times: 17241 | Evict Duration: 3831 | C:1 D:1 L:1 S:28
Fail Times: 10113 | Evict Duration: 4059 | C:1 D:1 L:1 S:29
Fail Times:  6307 | Evict Duration: 4247 | C:1 D:1 L:1 S:30
Fail Times:  3489 | Evict Duration: 4429 | C:1 D:1 L:1 S:31
Fail Times:  2078 | Evict Duration: 4533 | C:1 D:1 L:1 S:32
Fail Times:  1480 | Evict Duration: 4651 | C:1 D:1 L:1 S:33
Fail Times:  1557 | Evict Duration: 4898 | C:1 D:1 L:1 S:34
Fail Times:   749 | Evict Duration: 5021 | C:1 D:1 L:1 S:35
Fail Times:    60 | Evict Duration: 5102 | C:1 D:1 L:1 S:36
Fail Times:     3 | Evict Duration: 5191 | C:1 D:1 L:1 S:37
Fail Times:     0 | Evict Duration: 5139 | C:1 D:1 L:1 S:38
```