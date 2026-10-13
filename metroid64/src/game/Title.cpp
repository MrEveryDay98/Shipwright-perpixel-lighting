// Bank 0: title screen, intro, password screens and the ending.

#include "game/Title.h"

#include "game/Engine.h"
#include "game/Hw.h"
#include "game/Original.h"
#include "game/Ram.h"

namespace Game {

using namespace Vars;

void TitleMainRoutine() {
    Original(0x8000);
}
void NMIScreenWrite() {
    Original(0x9A07);
}
void DecSpriteYCoord() {
    Original(0x988A);
}
void EndGamePalWrite() {
    Original(0x9F54);
}
void StarPalSwitch() {
    Original(0x8AC7);
}
void CopyMap() {
    Original(0xA93E);
}

} // namespace Game
