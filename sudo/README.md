Run exp like the demo video.

``` bash
sudo ./sudo_exp <single-stepping Interval> <Num of steps to end> <IGNORE> <END_FLAG>
```

We provide a sequence for locating the target. If that doesn't work, you can simply debugging in a local environment with the same version of target.

If the exploit finishes, you can see a message via `sudo dmesg`. And the APIC timer will stop being hooked. So the `<Num of steps to end>` is provided with a big number just to make sure the exp can finish. If it takes too long or you think the sequence is not correct, you can just break it `ctrl+c` and clean up the shared page (the one for synchronization) by `sudo ./sudo_exp <> <> <> 1` .