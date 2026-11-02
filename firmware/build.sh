#!/bin/sh
# Builds the firmware and runs the host tests in docker (no bind mounts: the tree goes in as a tar,
# build/out comes back as one). Outputs: build/out/openmusicplayer.{elf,bin,hex,map}.
#   ./build.sh          check pins, host tests, target build
#   ./build.sh test     host tests only
set -e
cd "$(dirname "$0")"
IMAGE=omp-fw:bookworm

[ -d deps/tinyusb/src ] || ./deps.sh
python3 tools/gen_pins.py --check
docker image inspect $IMAGE >/dev/null 2>&1 || docker build -q -t $IMAGE docker >/dev/null

MODE=${1:-all}
mkdir -p build/out
tar --exclude=.git --exclude=./build -cf - . | docker run --rm -i $IMAGE sh -c "
    set -e
    mkdir /w && cd /w && tar xf -
    cmake -S . -B build/host -G Ninja >&2
    cmake --build build/host >&2
    ctest --test-dir build/host --output-on-failure >&2
    if [ $MODE != test ]; then
        cmake -S . -B build/target -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake >&2
        cmake --build build/target >&2
        cd build/target && tar cf - openmusicplayer.elf openmusicplayer.bin openmusicplayer.hex openmusicplayer.map
    fi
" | tar -C build/out -xf -
[ "$MODE" = test ] || ls -l build/out
