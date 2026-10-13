#pragma once

// Bank 7: room lookup on the world map, collisions with the background, blastable tiles and the name
// table helpers they share.

#include <cstdint>

namespace Game {

bool GetRoomNum();
bool EnemyCheckMoveUp();
bool EnemyCheckMoveDown();
bool EnemyCheckMoveVert();
void EnemyCoordsToTemp(uint8_t x);
bool CheckMoveUp();
bool CheckMoveDown();
bool CheckVertCrash(uint8_t radX);
bool CheckCrashLoop(uint8_t a, uint8_t stepY, uint8_t stepX);
bool DoorTileHit();
bool CheckMoveLeft();
bool CheckMoveRight();
bool CheckHorzCrash(uint8_t radY);
uint8_t ObjCoordsToTemp(uint8_t x);
uint8_t TilesToCheck(uint8_t a, uint8_t x, uint8_t y);
bool EnemyCheckMoveLeft();
bool EnemyCheckMoveRight();
void CrashCoords();
void MakeCartRAMPtr();
void NextCrashTile();
void SwitchNameTable();
bool IsBlastTile(uint8_t tile, uint8_t x, bool carry);
bool BlastTile(uint8_t tile, uint8_t x);
uint8_t GetNameTable();
uint8_t OnNameTable0();

} // namespace Game
