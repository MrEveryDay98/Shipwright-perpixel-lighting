#pragma once

// The map (not part of the original), in the style of Super Metroid's: the screens of the area Samus has
// been to as cells, with walls where a screen doesn't lead on, doors, elevators, the power-ups not taken
// yet, and Samus blinking where she is. The pause screen shows the whole area; a minimap in the corner
// shows the screens around her while playing.

#include <cstdint>
#include <imgui.h>

namespace Game::Data {
struct Level;
}

namespace Metroid {

class MapView {
  public:
    // The whole area Samus is in, fitted into the rectangle.
    void DrawArea(ImDrawList* dl, ImVec2 min, ImVec2 max, double time);
    // `cols` x `rows` screens around Samus.
    void DrawMinimap(ImDrawList* dl, ImVec2 min, float cell, int cols, int rows, double time);
    // False before a game is running (nothing to show).
    bool Ready();

  private:
    struct Cell {
        uint8_t bank = 0; // area bank (1 Brinstar ... 5 Ridley); 0: not part of any area
        uint8_t room = 0xFF;
        // Leads on to the next screen (left, right) if it scrolls horizontally, (up, down) if vertically: the
        // section the game scrolls through from here (FindSection) each way.
        bool hOpen[2] = {}, vOpen[2] = {};
        bool door[2] = {}; // left, right
        bool redDoor[2] = {};
        bool elevator = false;
        int8_t item = -1; // power-up type, or -1
    };
    void Build(const Game::Data::Level& level);
    void DrawCell(ImDrawList* dl, int x, int y, ImVec2 p, float size, bool showUnvisited);
    void DrawSamus(ImDrawList* dl, ImVec2 p, float size, double time);

    const Game::Data::Level* mBuiltFor = nullptr;
    Cell mCells[32][32];
};

} // namespace Metroid
