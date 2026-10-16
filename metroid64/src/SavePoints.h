#pragma once

// Save points (not part of the original, which has passwords): places where Samus saves the game by
// pressing Up while standing there. They come from metroid64/savepoints.json in any loaded archive or mod
// (the last one loaded wins), so mods and the editor can add them; without that file there are none.
//
//   { "save_points": [ { "area": "brinstar", "map_x": 3, "map_y": 14, "x": 128, "y": 176 } ] }
//
// area: brinstar, norfair, tourian, kraid or ridley (or the area bank number 1-5); map_x / map_y: the
// world map screen; x: the middle of the save point and y: the floor it stands on, in that screen's
// pixels (0-255, 0-239).

#include <cstdint>
#include <vector>

namespace Metroid {

struct SavePoint {
    int bank = 1;
    uint8_t mapX = 0, mapY = 0;
    int x = 0, y = 0;
};

// Reads the save points from the loaded archives (again).
void LoadSavePoints();
const std::vector<SavePoint>& SavePoints();

// Samus counts as on a save point when her feet are within this of its floor and her middle within this of
// its middle.
constexpr int kSavePointHalfWidth = 12;
constexpr int kSavePointFloorSlack = 4;

} // namespace Metroid
