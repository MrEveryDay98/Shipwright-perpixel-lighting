#pragma once

// The switchable area banks 1-5 (Brinstar, Norfair, Tourian, Kraid, Ridley): enemy behaviour, doors,
// area-specific objects and Tourian's bosses. Most of this code is shared by all five banks at the
// same addresses; the rest differs per area.

#include "game/Original.h"

#include <cstdint>

namespace Game {

// Calls the area bank routine at addr ($8000-$BFFF) for the bank currently mapped, with the
// registers the original passes, and returns the registers the routine leaves.
Regs AreaHook(uint16_t addr, Regs in = {});

void AreaRoutine();    // $95C3
void SamusEnterDoor(); // $8B13
void DrawDoors();      // $8B79

} // namespace Game
