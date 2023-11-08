## Wbinvd timing kernel module
Measures how long a `WBINVD`/`INVD` instruction takes depending on the nubmer of dirty cache lines.

`WBIND` is a safe instruction. So we measure it via [r0e](https://github.com/misc0110/r0e)

``` bash
make

# Env setup. Please follow the guidance before to port on your own machine
sudo ./env.sh

sudo ./wbinvd_bench
```

See `res.csv` for the results, which show there is a linear dependency between the number of dirty cache lines and the timing of `wbinvd`

For `INVD`, we use our leaky kernel module.
``` bash
# Make sure you insmod leaky kernel module already
sudo insmod ../module/leaky.ko

sudo ./invd_bench

# See results
sudo dmesg
```