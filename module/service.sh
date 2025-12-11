#!/bin/sh
# service.sh
# No warranty.
# No rights reserved.
# This is free software; you can redistribute it and/or modify it under the terms of The Unlicense.
PATH=/data/adb/ksu/bin:$PATH
MODDIR="/data/adb/modules/ksu_toolkit"
KSUDIR="/data/adb/ksu"

if [ -f "$KSUDIR/.manager_uid" ]; then
	uid=$(head -n1 "$KSUDIR/.manager_uid")

	# just pull it out from /data/system/packages.list
	[ -n "$uid" ] && "$MODDIR/toolkit" --setuid $uid > /dev/null 2>&1
fi

if [ -f "$KSUDIR/.umount_list" ]; then
	for i in $(grep -v "^#" "$KSUDIR/.umount_list"); do
		/data/adb/ksud kernel umount add "$i" -f 2
	done
	/data/adb/ksud kernel notify-module-mounted
fi

# EOF
