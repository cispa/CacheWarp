``` bash
# Config
vm> sudo su
vm> adduser <new_account>
vm> su new_account
```

Run exp like the demo video.

``` bash
sudo ./sudo_exp <single-stepping Interval> <Num of steps to end> <IGNORE> <END_FLAG>
```


Victime Code in the sudo binary:
```
endbr64
mov rax, 0x66
syscall 
ret
mov [...], rax <-- TARGET
```

While it looks simple, the `syscall` actually takes 266 instructions.
We provide two sequences (the context swicth of the syscall) for locating the target. If that doesn't work, it means that the register state might be a little bit differ which leads to one or multiple steps of changes dismatch.
You can decrease the number of `vec` (and increase `count` at the same time since the sum should be the same) and use `verbose` and offset `2052` to debug a little bit more.


If the exploit finishes, you can see a message via `sudo dmesg`. And the APIC timer will stop being hooked. So the `<Num of steps to end>` is provided with a big number just to make sure the exp can finish. If it takes too long or you think the sequence is not correct, you can just break it `ctrl+c` and clean up the shared page (the one for synchronization) by `sudo ./sudo_exp <> <> <> 1` .