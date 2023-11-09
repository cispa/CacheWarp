## Run

``` bash
# You will observe a correct signature
vm> make all 

# Keep signing until a fault leads to a different signature
vm> make fault
```

## Blindly drop  
``` bash
hv> sudo ./blind <randomly choose a multi-step interval> 3 <L2 sets index to drop> 0
```
If the L2 sets index is within 1~1024 then the single specified L2 set will be dropped.

Otherwise you can use 0xAAABBB to drop L2 sets whose index is within 0xAAA ~ 0xBBB


``` bash
vm> python3 exploit.py
```
