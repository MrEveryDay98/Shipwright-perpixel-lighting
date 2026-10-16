#pragma once

// Which screens of the world map Samus has been to, for the map (not part of the original). Kept outside
// the game's RAM, cleared for a new game and kept in saved games (SaveState.h).

#include <cstdint>

namespace Game::Explored {

// Forgets everything (power on, a new game).
void Reset();
// After each frame: marks the map screen Samus is in while playing, and how the screen scrolled there.
void Update();
bool Visited(int mapX, int mapY);
// How a visited screen scrolls (ScrollDir bit 1 when Samus was last there): true horizontally. A screen's
// room data doesn't say; the doors that lead into it decide (an $A0 door turns horizontal scrolling
// vertical), so the map takes it from play.
bool Horizontal(int mapX, int mapY);

// The map screen Samus is in: the room drawn on the name table she's on (Present::gRooms). False if
// that isn't known.
bool SamusScreen(int& mapX, int& mapY);

// True if the item history has the unique item of this type at this map screen (a power-up taken, a red
// door opened: CreateItemID's ID, looked up like CheckForItem).
bool ItemTaken(uint8_t type, uint8_t mapX, uint8_t mapY);

// The visited screens, then their scrolling, as 128 bytes each (bit x of row y), for saves.
constexpr int kBytes = 2 * 32 * 32 / 8;
void Get(uint8_t* out);
void Set(const uint8_t* in);

} // namespace Game::Explored
