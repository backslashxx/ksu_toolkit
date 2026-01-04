#!/bin/sh
# service.sh
# No warranty.
# No rights reserved.
# This is free software; you can redistribute it and/or modify it under the terms of The Unlicense.
PATH=/data/adb/ksu/bin:$PATH
MODDIR="/data/adb/modules/ksu_toolkit"
KSUDIR="/data/adb/ksu"
PACKAGES_LIST="/data/system/packages.list"

echo "RELEASE=$(uname -r)" > "$MODDIR/default_uname"
echo "VERSION=$(uname -v)" >> "$MODDIR/default_uname"

if [ -f "$KSUDIR/.uname" ] && grep -qE "RELEASE=|VERSION=" "$KSUDIR/.uname"; then
	. "$KSUDIR/.uname"
	"$MODDIR/toolkit" --fkuname $RELEASE $VERSION > /dev/null 2>&1
fi

if [ -f "$KSUDIR/.manager_uid" ]; then
	uid=$(head -n1 "$KSUDIR/.manager_uid")

	# only set this when it is still installed
	# this way we dont get into a situation where theres a manager uid is set
	# but theres no manager
	if grep -q "$uid" "$PACKAGES_LIST" > /dev/null 2>&1; then
		"$MODDIR/toolkit" --setuid "$uid" > /dev/null 2>&1
	fi
fi

if [ -f "$KSUDIR/.manager_version" ]; then
	version=$(head -n1 "$KSUDIR/.manager_version")
	[ -z $version ] || "$MODDIR/toolkit" --setver "$version" > /dev/null 2>&1
fi

# wait for boot-complete
until [ "$(getprop sys.boot_completed)" = "1" ]; do
	sleep 1
done

apply_sepolicy_rule()
{
# if no zn, we need an extra rule to umount stuff from /data
# for shit like revanced
zygisksu_dir="/data/adb/modules/zygisksu"
toolkit_rule="/dev/ksu_toolkit_sepolicy"
if [ ! -d "$zygisksu_dir" ] || [ -f "$zygisksu_dir/remove" ] || 
	[ -f "$zygisksu_dir/disable" ] || [ ! -f "/data/adb/zygisksu/denylist_enforce" ]; then

	echo "ksu_toolkit: applying umount sepolicy rule" >> /dev/kmsg

	# from susfs 1.2.2
	echo "allow zygote labeledfs filesystem unmount" > "$toolkit_rule"
	/data/adb/ksud sepolicy apply "$toolkit_rule"
	busybox rm "$toolkit_rule"
fi
} # apply_sepolicy_rule

if [ -f "$KSUDIR/.umount_list" ]; then
	for i in $(grep -v "^#" "$KSUDIR/.umount_list"); do
		/data/adb/ksud kernel umount add "$i" -f 2
	done
	/data/adb/ksud kernel notify-module-mounted

	# we do this since rezygisk will disable it
	# I wonder who did it bro
	if [ "$(grep -cv "^#" "$KSUDIR/.umount_list")" -gt 0  ]; then
		/data/adb/ksud feature set 1 1
		apply_sepolicy_rule
	fi
fi

# EOF
