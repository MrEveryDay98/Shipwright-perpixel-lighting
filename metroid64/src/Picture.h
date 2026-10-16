#pragma once

#include "game/Hw.h"

#include <cstdint>

namespace Metroid {

// The NES's colour for a palette entry ($00-$3F), as 0xRRGGBBAA.
uint32_t NesColor(uint8_t index);

// Draws the picture the game's video memory shows, as 0xRRGGBBAA pixels (256x240). The NES shows up to 8
// sprites a line; `spritesPerLine` 64 shows them all.
void RenderPicture(const Game::Hw::Video& video, uint32_t* out, int spritesPerLine = 8);

} // namespace Metroid
