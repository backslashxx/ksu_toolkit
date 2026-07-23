#!/bin/sh
# action.sh
# No warranty.
# No rights reserved.
# This is free software; you can redistribute it and/or modify it under the terms of The Unlicense.
PATH=/data/adb/ksu/bin:$PATH
MODDIR="/data/adb/modules/ksu_toolkit"
KSUDIR="/data/adb/ksu"

# 0 = stable mode (lowest cpu freq)
# 1 = fast mode (highest cpu freq, can cpu throttle)
benchmark_mode=1

echo "[?] press any volume key w/in 3s for stable mode"
KEY_EVENT=$(busybox timeout 3 /system/bin/getevent -lq | head -n1 2>/dev/null)

if echo "$KEY_EVENT" | grep -q "KEY_VOLUMEUP"; then
	echo "[!] vol+ press detected"
	echo "[+] benchmarking in stable mode"
	benchmark_mode=0
elif echo "$KEY_EVENT" | grep -q "KEY_VOLUMEDOWN"; then
	# keep it like this for now, maybe we can use this for another mode.
	echo "[!] vol- press detected"
	echo "[+] benchmarking in stable mode"
	benchmark_mode=0
else
	echo "[!] no volume interaction detected!"
	echo "[+] benchmarking in fast mode"
fi

HIGHEST_CPU=$(awk -F'-' '{print $NF}' /sys/devices/system/cpu/online)
CPUFREQ_DIR="/sys/devices/system/cpu/cpu$HIGHEST_CPU/cpufreq"
MAX_FREQ_NODE="$CPUFREQ_DIR/scaling_max_freq"
MIN_FREQ_NODE="$CPUFREQ_DIR/scaling_min_freq"
# last_cpu=$(expr $(busybox nproc) - 1 )
highest_freq=$(cat /sys/devices/system/cpu/cpu"$HIGHEST_CPU"/cpufreq/scaling_available_frequencies | busybox rev | cut -f2 -d " " | busybox rev)
lowest_freq=$(cat /sys/devices/system/cpu/cpu"$HIGHEST_CPU"/cpufreq/scaling_available_frequencies | cut -f1 -d " ")
original_octal_perm=$(busybox stat -c '%a' /sys/devices/system/cpu/cpu"$HIGHEST_CPU"/cpufreq/scaling_max_freq)

# grab old setting
original_sucompat_setting=$(ksud feature get 0 | grep "Value" | awk {'print $2'})
original_adbroot_setting=$(ksud feature get 3 | grep "Value" | awk {'print $2'})
original_sulog_setting=$(ksud feature get 2 | grep "Value" | awk {'print $2'})
original_max_freq=$(cat "$MAX_FREQ_NODE")

# set the highest CPU to its lowest available freq
busybox chmod +w "$MAX_FREQ_NODE"
if [ "$benchmark_mode" -eq 0 ]; then
	echo "$lowest_freq" > "$MAX_FREQ_NODE"
else
	echo "$highest_freq" > "$MAX_FREQ_NODE"
fi
busybox chmod 444 "$MAX_FREQ_NODE"
echo "[!] cpu clock pinned at $(expr $(cat $MAX_FREQ_NODE) / 1000) MHz"

# disable sulog and adb root
ksud feature set 2 0 > /dev/null 2>&1
ksud feature set 3 0 > /dev/null 2>&1

# enable sucompat
ksud feature set 0 1 > /dev/null 2>&1

"$MODDIR/toolkit" --bench "$HIGHEST_CPU"

echo ""

ksud feature set 0 0 > /dev/null 2>&1
"$MODDIR/toolkit" --bench "$HIGHEST_CPU"

# restore original highest CPU's max freq
busybox chmod +w "$MAX_FREQ_NODE"
echo "$original_max_freq" > "$MAX_FREQ_NODE"
busybox chmod "$original_octal_perm" "$MAX_FREQ_NODE"

ksud feature set 0 "$original_sucompat_setting" > /dev/null 2>&1
ksud feature set 2 "$original_sulog_setting" > /dev/null 2>&1
ksud feature set 3 "$original_adbroot_setting" > /dev/null 2>&1

echo "[+] benchmark finished!"

sleep 20
