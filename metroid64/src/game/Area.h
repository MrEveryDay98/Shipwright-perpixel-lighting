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

// Per-area dispatchers: return true (and the registers in out) when addr is handled natively.
bool BrinstarHook(uint16_t addr, const Regs& in, Regs& out); // bank 1
bool NorfairHook(uint16_t addr, const Regs& in, Regs& out);  // bank 2
bool TourianHook(uint16_t addr, const Regs& in, Regs& out);  // bank 3
bool KraidHook(uint16_t addr, const Regs& in, Regs& out);    // bank 4
bool RidleyHook(uint16_t addr, const Regs& in, Regs& out);   // bank 5
// The $95A8-$95C0 hooks of banks 1, 2, 4 and 5 (all RTS).
bool NoAreaHook(uint16_t addr, const Regs& in, Regs& out);

} // namespace Game
