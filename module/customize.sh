#!/bin/sh
PATH=/data/adb/ksu/bin:$PATH

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
	busybox ln -sf "/data/adb/modules/ksu_toolkit/toolkit" "$KSU_BIN_DIR/toolkit"
fi

OLD_MODULE_DIR="/data/adb/modules/ksu_switch_manager"
if [ -d "$OLD_MODULE_DIR" ]; then
	touch "$OLD_MODULE_DIR/remove"
fi

# EOF
