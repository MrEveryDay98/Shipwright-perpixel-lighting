#pragma once

// Bank 7: Samus's projectiles and bombs, elevators, starting the area music.

#include <cstdint>

namespace Game {

void UpdateProjectiles();
void DoOneProjectile(uint8_t x);
void UpdateBullet();
void CheckBulletStat(bool carry);
void DrawBullet();
void UpdateWaveBullet();
void UpdateIceBullet();
void BulletExplode();
void CheckProjectileHit();
void ExplodeProjectile(uint8_t x);
void KillIfOffScreen();
// Not part of the original (Options::gShotsPastEdge): see Projectiles.cpp.
bool PastEdge(uint8_t hi, uint8_t x, uint8_t y, int& rx, int& ry);
bool PastEdgePosition(uint8_t x, int& rx, int& ry);
bool MoveProjectile();
bool MoveObjectByTiles();
void StoreObjectCoords(uint8_t x);
void ToggleObjectHi(uint8_t x);
bool IsBlastableRange(uint8_t tile);
void LayBomb();
void DrawBomb();
void BombCountdown();
void BombExplode();
void BombBlastTiles();
void BlastTileAt04(uint8_t x);
void TileOffsetDown(uint8_t a);
void TileOffsetUp(uint8_t a);
void GetObjCoords();
void UpdateElevator();
void ElevatorIdle();
void ShowElevator();
void ElevatorCenter();
void ElevatorMove();
void ElevatorScroll();
void ElevScrollRoom();
void ElevatorNext();
void ElevatorChangeArea();
void StartMusic();
void ElevatorStop();
uint8_t ToggleScroll();
void SamusOnElevatorOrEnemy();
bool SamusOnTop(uint8_t& y);
void WaitNMIPass_();

} // namespace Game
