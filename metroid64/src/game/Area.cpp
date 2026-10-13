// The switchable area banks 1-5.

#include "game/Area.h"

#include "game/Engine.h"
#include "game/Hw.h"
#include "game/Ram.h"

namespace Game {

using namespace Vars;

Regs AreaHook(uint16_t addr, Regs in) {
    return Original(addr, in);
}

void AreaRoutine() {
    AreaHook(0x95C3);
}
void SamusEnterDoor() {
    AreaHook(0x8B13);
}
void DrawDoors() {
    AreaHook(0x8B79);
}

} // namespace Game
