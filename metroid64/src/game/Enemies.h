#pragma once

// Bank 7 from $F1A7: enemies, enemy/object collisions, enemy-spawned objects and tile regrowth (plus
// $EB6E, a few lines of the room code the area banks call). Variables without a name in the community
// disassembly are in EnemyRam.h.

#include <cstdint>

namespace Game {

// --- Called from the game engine every frame (UpdateWorld) ---
void UpdateEnemies();       // $F345
void DestroyEnemyObjects(); // $FA9D enemy explosion pieces (DrawEnemy's pop just ends it early)
void UpdateMellows();       // $FC65 Mellows / Memus
// $F93B enemy projectiles. Returns true when the original leaves UpdateWorld here: the last slot
// ($60) is reached by falling through, so when its explosion ends DrawEnemy's pop takes UpdateWorld's
// return address (the caller must then return at once).
bool UpdateObjectsF93B();
void UpdateSpinners(); // $FBDD
// $FE1B. Returns true when the original leaves UpdateWorld here: the last tile slot (0) is reached by
// falling through, so when its animation ends UpdateTileAnim's pop takes UpdateWorld's return address.
bool UpdateTiles();
void UpdateObjectsFAF2(); // $FAF2 enemies spawned by the room (e.g. pipe bugs, Kraid/Ridley)
void UpdateObjectsFDE3(); // $FDE3 escape timer / Zebetite item history

// --- Crash detection pieces ($F034 SamusEnemyCollisions itself is in Room.cpp). carry: $F1FA's result.
void CrashDoorF277(uint8_t y, bool carry);                   // $F277
void CrashEnemySamusF282(uint8_t x, uint8_t y, bool carry);  // $F282
void CrashMemuSamusF2B4(uint8_t x, bool carry);              // $F2B4
void CrashEnemyBulletF2CA(uint8_t x, uint8_t y, bool carry); // $F2CA
void CrashSamusObjectF2ED(uint8_t x, uint8_t y, bool carry); // $F2ED
void CrashBlastF311(uint8_t y, bool carry);                  // $F311
void CrashMemuBulletF32A(uint8_t x, uint8_t y, bool carry);  // $F32A

// --- Distances (coordinates from Room.h's GetObject0CoordData etc.; results in $04-$0B, see $F1FA) ---
void DistFromObj0ToObj1(uint8_t x, uint8_t y); // $F1A7
void DistFromObj0ToEn1(uint8_t x, uint8_t y);  // $F1B3
void DistFromEn0ToObj1(uint8_t x, uint8_t y);  // $F1BF
void AddEnemy1XRadius(uint8_t a, uint8_t y);   // $F1CB
void AddObject1XRadius(uint8_t a, uint8_t y);  // $F1D9
void AddObject1YRadius(uint8_t a, uint8_t y);  // $F1E0
void AddEnemy1YRadius(uint8_t a, uint8_t y);   // $F1E7
// $F1FA: are objects 0 and 1 closer than the distances in $04 (y) and $05 (x)? Returns the carry:
// clear when they touch. Leaves the direction bits in $10 and the distances in $00/$11/$0F.
bool CheckObjectDistance();
// $DC7F / $DC82 (bank 7, made of the routines above): object x against object y.
bool TouchCheckDC7F(uint8_t x, uint8_t y);
bool TouchCheckDC82(uint8_t x, uint8_t y);

// --- Enemy helpers reached from the area banks ---
void DoOneEnemy(uint8_t x);                      // $F351
void EnemyAnimDraw(uint8_t a);                   // $F410 (area $8000); a = animation delay
void EnemyAnimDrawOnly(uint8_t a);               // $F438 (area $8003)
void EnemyDraw();                                // $F416 (area $8006)
uint8_t EnemyFrameCarry(uint8_t x, bool& carry); // $F852 (area $8009): returns A and the carry
uint8_t SetEnemyAnim(uint8_t x, uint8_t a);      // $F68D (area $800F): returns A (0)
void SetEnemyAnimIndex(uint8_t x, uint8_t a);    // $F690
void ClearEnemyAnimDelay(uint8_t x);             // $F693
uint8_t EnemyDirIndex2(uint8_t x);               // $F83E (area $8012): returns Y
uint8_t EnemyDirIndex(uint8_t x);                // $F844: returns Y
void ResetEnemyHitPoints(uint8_t x);             // $F85A (area $8015)
void EnemyDirAnim(uint8_t x, uint8_t a);         // $FBB9 (area $8018)
void EnemyTurnAnim();                            // $FB88 (area $801B)
void EnemyTurnAnim2();                           // $FBCA (area $801E)
void EnemyFireProjectile(uint8_t x);             // $F870 (area $8021)
bool MoveEnemyObject(uint8_t x);                 // $FA1E (area $8033): returns the carry
bool MoveObjectByDeltas();                       // $FD8F (area $8027): returns the carry
bool MoveEnemyByDeltas(uint8_t x);               // $FA41: returns the carry
void EnemyStorePosition(uint8_t x);              // $FA49
void SetEnemyBits(uint8_t x, uint8_t a);         // $F744: $0405 |= a
uint8_t EnemyDataF74B(uint8_t x);                // $F74B: returns A (and Y = EnDataIndex)
void KillObject(uint8_t x);                      // $FA18
void EnemyRestartEB6E(uint8_t x);                // $EB6E (area $802A)
void ClearHealthChangeF323();                    // $F323 (same as Samus.h ClearHealthChange)
bool DrawTileBlast();                            // $FEDC (area $803F): returns the carry

} // namespace Game
