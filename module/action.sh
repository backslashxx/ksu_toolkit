#!/bin/sh
# action.sh
# No warranty.
# No rights reserved.
# This is free software; you can redistribute it and/or modify it under the terms of The Unlicense.
PATH=/data/adb/ksu/bin:$PATH
MODDIR="/data/adb/modules/ksu_toolkit"
KSUDIR="/data/adb/ksu"
HIGHEST_CPU=$(awk -F'-' '{print $NF}' /sys/devices/system/cpu/online)
CPUFREQ_DIR="/sys/devices/system/cpu/cpu$HIGHEST_CPU/cpufreq"
MAX_FREQ_NODE="$CPUFREQ_DIR/scaling_max_freq"
MIN_FREQ_NODE="$CPUFREQ_DIR/scaling_min_freq"

# grab old setting
original_sucompat_setting=$(ksud feature get 0 | grep "Value" | awk {'print $2'})
original_adbroot_setting=$(ksud feature get 3 | grep "Value" | awk {'print $2'})
original_sulog_setting=$(ksud feature get 2 | grep "Value" | awk {'print $2'})
original_max_freq=$(cat "$MAX_FREQ_NODE")

# set the highest CPU to its lowest available freq
lowest_freq=$(cat "MIN_FREQ_NODE")
echo "$lowest_freq" > "$MAX_FREQ_NODE"

# disable sulog and adb root
ksud feature set 2 0 > /dev/null 2>&1
ksud feature set 3 0 > /dev/null 2>&1

# enable sucompat
ksud feature set 0 1 > /dev/null 2>&1

"$MODDIR/toolkit" --bench

echo ""

ksud feature set 0 0 > /dev/null 2>&1
"$MODDIR/toolkit" --bench

# restore original highest CPU's max freq
echo "$original_max_freq" > "$MAX_FREQ_NODE"

ksud feature set 0 "$original_sucompat_setting" > /dev/null 2>&1
ksud feature set 2 "$original_sulog_setting" > /dev/null 2>&1
ksud feature set 3 "$original_adbroot_setting" > /dev/null 2>&1

echo "[+] benchmark finished!"

sleep 20
