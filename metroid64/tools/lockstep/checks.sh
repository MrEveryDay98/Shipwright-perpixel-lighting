#!/bin/bash
# The lockstep battery: run it before every commit that touches the game.
#
#   metroid64/tools/lockstep/checks.sh "path/to/Metroid (USA).nes" [path/to/lockstep]
#
# Vanilla runs compare the port with the emulated original frame by frame (they must all say OK); the solo
# runs exercise the optional changes (saves, Everything active), which the comparison leaves off.
ROM="$1"
LS="${2:-build-lockstep/lockstep/lockstep}"
RAMH="$(dirname "$0")/../../src/game/Ram.h"
if [ -z "$ROM" ] || [ ! -f "$ROM" ] || [ ! -x "$LS" ] && [ ! -x "$LS.exe" ]; then
    echo "usage: $0 <rom> [lockstep binary]   (build it with -DMETROID64_TOOLS=ON)"
    exit 2
fi
START=120:08,125:00,200:08,205:00
ITEMS=0107=09,6878=FF,6879=40,687A=40,6877=06
fail=0
run() {
    local want="$1"
    shift
    local out
    out="$("$LS" "$ROM" --ram-h "$RAMH" "$@" 2>&1 | grep -E "^(OK|FAIL|ERROR|PROGRESS-TEST|SAVE-TEST|ALL-ACTIVE)" | tail -1)"
    if [[ "$out" != *"$want"* ]]; then
        fail=1
        echo "FAILED: $* -> $out"
    else
        echo "ok: $* -> ${out:0:110}"
    fi
}
echo "== vanilla, against the original"
run OK --sync 30 --frames 6000
run OK --sync 30 --frames 400 --script $START
run OK --script "$START,600:80,1100:40,1148:81,1188:80,1208:81,1248:80,1600:10" --sync 640 --frames 1300
for s in 1 2 3 4 5; do
    run OK --script $START --sync 650 --frames 3000 --random $s
done
for a in 11 12 13 14; do
    run OK --password $a,FF,40 --frames 3000 --random 1 --poke $ITEMS
done
echo "== saves"
run "SAVE-TEST: OK" --solo --script $START --random 2 --save-test 1500 --frames 2500
for e in "" --progress-entry; do
    for a in 00 11 22 33 44; do
        run "PROGRESS-TEST: OK" --solo --progress-test 2000 $e --password $a,FF,40 --random 1 --frames 1 --poke $ITEMS
    done
done
echo "== everything active"
for a in 11 13 14; do
    run "ALL-ACTIVE" --password $a,FF,40 --frames 5000 --random 1 --poke $ITEMS --all-active
done
[ $fail = 0 ] && echo "ALL CHECKS PASSED" || echo "SOME CHECKS FAILED"
exit $fail
