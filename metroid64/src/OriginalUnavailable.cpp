// The game has no way to run the original's code: a routine still reached through Game::Original is
// missing from the port. This only logs it; the lockstep test tool provides the real Original().

#include "game/Original.h"

#include <cstdio>
#include <set>

namespace Game {

Regs Original(uint16_t addr, Regs in) {
    static std::set<uint16_t> reported;
    if (reported.insert(addr).second) {
        fprintf(stderr, "Metroid64: routine $%04X has not been ported\n", addr);
    }
    return in;
}

} // namespace Game
