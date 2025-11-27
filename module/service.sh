#!/bin/sh
# service.sh
# No warranty.
# No rights reserved.
# This is free software; you can redistribute it and/or modify it under the terms of The Unlicense.
PATH=/data/adb/ksu/bin:$PATH
MODDIR="/data/adb/modules/ksu_switch_manager"

# just pull it out from /data/asystem/packages.list
"$MODDIR/switch_uid" 12345 > /dev/null 2>&1

# EOF
