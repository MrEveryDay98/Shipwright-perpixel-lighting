#pragma once

// "Everything active": an optional way to play (not part of the original game) where the enemies of the
// whole scrolling section Samus is in (game/data/Layout.h) keep running, not only those on the two screens
// the original keeps loaded.
//
// When Samus enters a section, every enemy its rooms place is put in a roster, with its position in the
// section. Enemies near the screen are moved into the game's own enemy slots and run as usual; the others
// stay in the roster, and each frame each of them runs through the game's own enemy code
// (DoOneEnemy) in a stand-in for the game: two screens of room RAM around the enemy, the screen looking
// at it, and the enemy in a slot. Its state is then kept and everything else put back. The original
// spawns enemies when their room is drawn; with this on, the roster's enemies take their place, so an
// enemy is where it went, and stays dead, until Samus leaves the section.
//
// Not covered yet: Tourian, Kraid and Ridley themselves, Memus, Zeb holes' enemies off screen, and enemy
// shots fired off screen (they are dropped).

#include <cstdint>
#include <vector>

namespace Game::AllActive {

// Set by the app each frame from the setting.
extern bool gEnabled;

// Called before each game frame: finds the section, builds its roster on entry, moves enemies between
// the roster and the game's slots, and runs the roster's enemies.
void BeforeFrame();

// True while the room loader must not place enemies (the roster has them).
bool SuppressRoomEnemies();

// A sprite of an enemy running off screen, in the section's pixels (x right, y down from its top-left).
struct Sprite {
    int x, y;
    uint8_t tile, attr;
};
// The off-screen enemies' sprites of the last frame (empty when off).
const std::vector<Sprite>& OffscreenSprites();

// For tests: enemies in the roster now, and rosters built so far.
size_t RosterSize();
int RostersBuilt();

} // namespace Game::AllActive
