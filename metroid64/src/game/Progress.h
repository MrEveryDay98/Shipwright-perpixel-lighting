#pragma once

// Saved games (not part of the original, which has passwords): what a save keeps is Samus's progress, as in
// Super Metroid, not a snapshot of the game. Loading starts the game the way a password does (InitializeGame:
// a fresh area start, the rooms loaded anew, Samus appearing), but in the room she was saved in, where she
// stood, with the energy she had.

#include "game/Explored.h"

#include <cstdint>
#include <string>

namespace Game::Progress {

struct Data {
    uint8_t stats[0x87] = {}; // $6876-$68FC: tanks, gear, missiles, statues, age, the unique item history
    uint8_t inArea = 0x10, bailey = 0;
    uint8_t healthLo = 0, healthHi = 0x03;
    uint8_t mapX = 0, mapY = 0; // the map screen she was in
    uint8_t samusX = 0x80, samusY = 0x80;
    bool horizontal = true; // how that screen scrolls
    uint8_t explored[Explored::kBytes] = {}; // the map
};

// Samus's progress and place now (playing or paused). False if she isn't in a room (not in play). With
// atEntry, the place is where she came into the room she is in (TrackEntry) instead of where she stands: a
// save away from a save point starts her at the door she last came through.
bool Capture(Data& out, bool atEntry = false);
// Keeps where Samus came into the room she's in: through a door, off an elevator, or where the game started
// her. Call after every frame.
void TrackEntry();
// Starts the game from it: like InitializeGame from a password, then MoreInit and SamusInit put her where she
// was saved (StartOverride) instead of at the area's start, with her energy.
void Start(const Data& d);

// The bytes of a save file, and back (false if it isn't one). Decode also reads the snapshot saves of earlier
// Metroid64 versions (SaveState.h), taking the progress out of them.
std::string Encode(const Data& d);
bool Decode(const std::string& bytes, Data& out);

// Set by Start for MoreInit and SamusInit (Engine.cpp), which use it once.
struct StartOverride {
    bool active = false;
    Data data;
};
extern StartOverride gStart;

} // namespace Game::Progress
