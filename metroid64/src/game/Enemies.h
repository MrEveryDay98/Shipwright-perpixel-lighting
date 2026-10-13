#pragma once

// Bank 7 from $F1A7: enemies, enemy/object collisions, enemy-spawned objects and tile regrowth.

namespace Game {

void UpdateEnemies(); // $F345
void DestroyEnemyObjects(); // $FA9D
void UpdateMellows(); // $FC65
void UpdateObjectsF93B(); // $F93B
void UpdateSpinners(); // $FBDD
void UpdateTiles(); // $FE1B
void SamusEnemyCollisions(); // $F034
void UpdateObjectsFAF2(); // $FAF2
void UpdateObjectsFDE3(); // $FDE3

} // namespace Game
