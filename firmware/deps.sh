#!/bin/sh
# Fetches the pinned dependencies into deps/ (not committed). The pins are the ones TinyUSB 0.21.0's
# tools/get_deps.py uses for the imxrt family.
set -e
cd "$(dirname "$0")"
fetch() {
    [ -d "deps/$1/.git" ] && return
    mkdir -p "deps/$1"
    git -C "deps/$1" init -q
    git -C "deps/$1" remote add origin "$2"
    git -C "deps/$1" fetch -q --depth 1 origin "$3"
    git -C "deps/$1" checkout -q FETCH_HEAD
}
fetch tinyusb https://github.com/hathach/tinyusb dae3f9a366bfcddbf9dcf1b48d7500286a849539  # 0.21.0
fetch mcuxsdk-core https://github.com/nxp-mcuxpresso/mcuxsdk-core 0c5c6b16deb211110e06bde896cdff59ab213e16
fetch mcux-devices-rt https://github.com/nxp-mcuxpresso/mcux-devices-rt dba2b523c9df61f3330bd186242f8210a8e47c45
fetch CMSIS_6 https://github.com/ARM-software/CMSIS_6.git 6f0a58d01aa9bd2feba212097f9afe7acd991d52
