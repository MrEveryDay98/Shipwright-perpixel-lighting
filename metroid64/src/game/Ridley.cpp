// Bank 5 (Ridley): the area's own code.

#include "game/Area.h"

namespace Game {

bool RidleyHook(uint16_t addr, const Regs& in, Regs& out) {
    return NoAreaHook(addr, in, out);
}

} // namespace Game
