#!/bin/sh
PATH=/data/adb/ksu/bin:$PATH
MODDIR="/data/adb/modules/ksu_toolkit"

if [ ! "$KSU" = true ]; then
	abort "[!] KernelSU only!"
fi

# this assumes CONFIG_COMPAT=y on CONFIG_ARM
arch=$(busybox uname -m)
echo "[+] detected: $arch"

case "$arch" in
	aarch64 | arm64 )
		ELF_BINARY="toolkit-arm64"
		;;
	armv7l | armv8l )
		ELF_BINARY="toolkit-arm"
		;;
	*)
		abort "[!] $arch not supported!"
		;;
esac

mv "$MODPATH/bin/$ELF_BINARY" "$MODPATH/toolkit"
rm -rf "$MODPATH/bin"

chmod 755 "$MODPATH/toolkit"

current_uid=$("$MODPATH/toolkit" --getuid)

if ! "$MODPATH/toolkit" --setuid "$current_uid" >/dev/null 2>&1; then
	abort "[!] custom interface not available!"
fi

# add symlink
KSU_BIN_DIR="/data/adb/ksu/bin"
if [ -d "$KSU_BIN_DIR" ]; then
	echo "[+] creating symlink in $KSU_BIN_DIR"
	busybox ln -sf "$MODDIR/toolkit" "$KSU_BIN_DIR/toolkit"
fi

OLD_MODULE_DIR="/data/adb/modules/ksu_switch_manager"
if [ -d "$OLD_MODULE_DIR" ]; then
	touch "$OLD_MODULE_DIR/remove"
fi

# we troll a little

hot_install() {
	( sleep 3 ; 
		busybox rm -rf "$MODDIR" ; 
		mkdir -p "$MODDIR" ; 
		busybox cp -Lrf "$MODPATH"/* "$MODDIR" ; 
		busybox rm -rf "$MODPATH"
	) & # fork in background
}

echo "[?] hot install?"
echo "[!] press volume up within 6 seconds if so."
if [ "$(busybox timeout 3 /system/bin/getevent -lq | grep -q KEY_VOLUMEUP 2>/dev/null ; echo $?)" -eq 0 ]; then
	hot_install
	echo "[+] hot install forked to background"
else
	echo "[!] no volume up within 5 seconds"
	echo "[+] performing hot install anyway"
	hot_install
fi
echo "[+] no need to reboot"

# EOF
