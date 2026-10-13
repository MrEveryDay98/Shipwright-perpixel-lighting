// Bank 4 (Kraid): the area's own code.

#include "game/Area.h"

namespace Game {

bool KraidHook(uint16_t addr, const Regs& in, Regs& out) {
    return NoAreaHook(addr, in, out);
}

} // namespace Game
