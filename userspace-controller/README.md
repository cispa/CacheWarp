We use [libtea](https://github.com/libtea/frameworks/tree/master/libtea) to write to APIC and remap a prefixed 4KB physical page for sync. Please first install libtea and compile with `libtea-x86-interrupts`. 

Add `paddr &= 0x7fffffffffff;` to libtea_remap_address() and `value &= 0x7ffffffff;` to libtea_get_physical_address(). As thats the bit indicating the encrypted page on machines support SEV.

Then put the folder `cachewarp` under `libtea`.