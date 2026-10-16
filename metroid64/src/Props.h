#pragma once

// Props (not part of the original): models placed in the world, for decoration. They come from
// metroid64/props.json in any loaded archive or mod (the last one loaded wins; the editor writes it):
//
//   { "props": [ { "area": "brinstar", "map_x": 3, "map_y": 14, "x": 96, "y": 192, "z": -8,
//                  "scale": 1.0, "yaw": 0, "display_list": "mymod/props/tree" } ] }
//
// area / map_x / map_y: the world map screen (as for save points); x, y: where the model's origin goes, in that
// screen's pixels (x right, y down); z: towards the camera (blocks are -8 to 8); scale; yaw: degrees around the
// vertical axis; display_list: a DisplayList resource (e.g. a Fast64 export in Shipwright's XML), drawn with
// y up and one unit per pixel before the scale.

#include <cstdint>
#include <string>
#include <vector>

namespace Metroid {

struct Prop {
    int bank = 1;
    uint8_t mapX = 0, mapY = 0;
    float x = 0, y = 0, z = 0;
    float scale = 1, yaw = 0;
    std::string displayList;
};

// Reads the props from the loaded archives (again).
void LoadProps();
const std::vector<Prop>& Props();

} // namespace Metroid
