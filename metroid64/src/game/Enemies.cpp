// Bank 7 from $F1A7: enemies, enemy/object collisions, enemy-spawned objects and tile regrowth.

#include "game/Enemies.h"

#include "game/Area.h"
#include "game/Engine.h"
#include "game/Original.h"
#include "game/Ram.h"

namespace Game {

using namespace Vars;

void UpdateEnemies() {
    Original(0xF345);
}
void DestroyEnemyObjects() {
    Original(0xFA9D);
}
void UpdateMellows() {
    Original(0xFC65);
}
void UpdateObjectsF93B() {
    Original(0xF93B);
}
void UpdateSpinners() {
    Original(0xFBDD);
}
void UpdateTiles() {
    Original(0xFE1B);
}
void UpdateObjectsFAF2() {
    Original(0xFAF2);
}
void UpdateObjectsFDE3() {
    Original(0xFDE3);
}

} // namespace Game
