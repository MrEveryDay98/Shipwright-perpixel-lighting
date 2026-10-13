// Bank 7: room lookup on the world map, collisions with the background and blastable tiles
// ($E720-$EA04), and the name table helpers they share ($EB85, $EC93).

#include "game/Collision.h"

#include "game/Constants.h"
#include "game/Engine.h"
#include "game/Memory.h"
#include "game/Original.h"
#include "game/Ram.h"

namespace Game {

using namespace Vars;
using namespace Const;

// $E720: carry (true) when there is no room at the map position, or a door is in the way.
bool GetRoomNum() {
    return Original(0xE720).c;
}

// $E9B7: flips to the other name table.
void SwitchNameTable() {
    PPUCNT0ZP ^= 0x03;
}

// $EB85: 1 when new objects go on name table 3, 0 for name table 0.
uint8_t GetNameTable() {
    return (PPUCNT0ZP ^ ScrollDir) & 0x01;
}

// $EC93: 1 when the screen shows name table 0, 0 for name table 3.
uint8_t OnNameTable0() {
    return (PPUCNT0ZP ^ 0x01) & 0x01;
}

} // namespace Game
