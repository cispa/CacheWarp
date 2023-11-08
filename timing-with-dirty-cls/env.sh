#!/bin/bash

echo 0 | sudo tee /sys/devices/system/cpu/cpu14/online
echo 0 | sudo tee /sys/devices/system/cpu/cpu15/online
echo 0 | sudo tee /sys/devices/system/cpu/cpu6/online
sudo cpufreq-set -c 7 -g userspace
sudo cpufreq-set -c 7 -f 2.40GHz

sudo bash -c 'modprobe msr; CUR=$(rdmsr 0xc0010015); ENABLED=$(printf "%x" $((0x$CUR & ~16))); wrmsr -a 0xc0010015 0x$ENABLED'