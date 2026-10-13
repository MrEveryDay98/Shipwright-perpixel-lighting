#!/bin/bash
# Builds the lockstep tool and runs the standard checks against the original game.
#   metroid64/tools/lockstep/check.sh "path/to/Metroid (USA).nes" [random seeds...]
set -e
ROM="$1"
shift || true
HERE="$(cd "$(dirname "$0")" && pwd)"
REPO="$(cd "$HERE/../../.." && pwd)"
BUILD="$REPO/build-lockstep"
if [ ! -f "$BUILD/build.ninja" ] && [ ! -f "$BUILD/Makefile" ]; then
    mkdir -p "$BUILD"
    cat > "$BUILD/CMakeLists.txt" <<CML
cmake_minimum_required(VERSION 3.16)
project(lockstep_only CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_BUILD_TYPE RelWithDebInfo)
add_subdirectory($HERE lockstep)
CML
    cmake -S "$BUILD" -B "$BUILD" -G Ninja > /dev/null
fi
cmake --build "$BUILD" 2>&1 | grep -E "error|Error" && exit 1 || true
LS="$BUILD/lockstep/lockstep"
cd "$REPO"
status=0
run() {
    echo "== $*"
    "$LS" "$ROM" --ram-h "$REPO/metroid64/src/game/Ram.h" "$@" | tail -40 || status=1
}
run --sync 30 --frames 400 --script "120:08,125:00,200:08,205:00"
run --script "120:08,125:00,200:08,205:00,600:80,1100:40,1148:81,1188:80,1208:81,1248:80,1600:10" --sync 640 --frames 1300
for seed in "${@:-1 2 3}"; do
    run --script "120:08,125:00,200:08,205:00" --sync 650 --frames 3000 --random $seed
done
exit $status
