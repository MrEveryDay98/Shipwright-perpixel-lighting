#pragma once

#include <cstdint>

namespace Game {

// CPU registers at a call into, or return from, a routine of the original game.
struct Regs {
    uint8_t a = 0, x = 0, y = 0;
    bool c = false, z = false, n = false, v = false;
};

// While the port is in progress, routines not translated yet are reached through this. The lockstep
// test tool runs the original routine against the port's memory; the game itself has no way to run
// original code, so a finished port never calls it.
Regs Original(uint16_t addr, Regs in = {});

} // namespace Game
