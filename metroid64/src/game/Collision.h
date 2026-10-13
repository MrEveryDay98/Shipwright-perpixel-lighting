#pragma once

// Bank 7: room lookup on the world map, collisions with the background, blastable tiles and the name
// table helpers they share.

#include <cstdint>

namespace Game {

bool GetRoomNum();
void SwitchNameTable();
uint8_t GetNameTable();
uint8_t OnNameTable0();

} // namespace Game
