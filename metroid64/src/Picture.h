#pragma once

#include "game/Hw.h"

#include <cstdint>

namespace Metroid {

// Draws the picture the game's video memory shows, as 0xRRGGBBAA pixels (256x240).
void RenderPicture(const Game::Hw::Video& video, uint32_t* out);

} // namespace Metroid
