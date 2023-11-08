
Access time for reading data from different hierarchies, as well as the access time after wbinvd.
Note: CORE 6 and CORE 14 are silbling cores. 

``` bash
# Load the kernel module for executing `WBINVD`

sudo insmmod ../module/leaky.ko
```

Simply run `sudo ./hist`, and the result is in `invd-scope-hist.csv`.

Fixing the frequency might also help:
`sudo cpufreq-set -c <core> -g performance`
or
`sudo cpufreq-set -c <core> -g userspace; sudo cpufreq-set -c <core> -f <>GHz`