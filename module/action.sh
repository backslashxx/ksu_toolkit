#!/bin/sh
# action.sh
# No warranty.
# No rights reserved.
# This is free software; you can redistribute it and/or modify it under the terms of The Unlicense.
PATH=/data/adb/ksu/bin:$PATH
MODDIR="/data/adb/modules/ksu_toolkit"
KSUDIR="/data/adb/ksu"

"$MODDIR/toolkit" --bench

sleep 20

