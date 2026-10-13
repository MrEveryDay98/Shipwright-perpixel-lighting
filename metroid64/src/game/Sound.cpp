// The sound engine ($B3B4 in every bank).

#include "game/Sound.h"

#include "game/Engine.h"
#include "game/Hw.h"
#include "game/Original.h"
#include "game/Ram.h"

namespace Game {

using namespace Vars;

void SoundEngine() {
    Original(0xB3B4);
}

} // namespace Game
