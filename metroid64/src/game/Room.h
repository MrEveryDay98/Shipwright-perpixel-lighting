#pragma once

// Bank 7: building a room in room RAM (structures, macros, attribute bits), loading its enemies, doors,
// elevators, statues and special items, clearing the objects of the room left behind, and the
// collision checks between Samus, her projectiles, enemies and Memus ($EA05-$F1A6).

#include "game/Original.h"

#include <cstdint>

namespace Game {

void SelectRoomRAM();                              // $EA05
void AttribTableWrite();                           // $EA13
void RoomFinished();                               // $EA26
void SetupRoom();                                  // $EA2B
void DrawObject(uint8_t pos);                      // $EA60
void DrawRoom();                                   // $EAAA
void AddToRoomPtr(uint8_t a);                      // $EAC0
void EndOfObjs();                                  // $EACA
void EndOfRoom();                                  // $EAF4
uint8_t GetEnemyData(uint8_t& x);                  // $EB0C
bool GetEnemyType(uint8_t x, uint8_t type);        // $EB28
void SetEnemyPosition(uint8_t x, uint8_t pos);     // $EB4D
uint8_t IsSlotTaken(uint8_t x);                    // $EB7A
uint8_t LoadDoorData(uint8_t& x);                  // $EB92
uint8_t LoadElevatorData();                        // $EC09
uint8_t LoadStatues();                             // $EC2F
uint8_t ZebHole();                                 // $EC57
uint8_t UpdateRoomSpriteInfo();                    // $EC9B
void UpdateDoorData(uint8_t x, uint8_t y);         // $ED51
void ClearDoorDataOtherTable();                    // $ED5B
void ClearOffScreenObjects();                      // $ED65
void ClearProjectileOnTable(uint8_t x, uint8_t y); // $ED7A
void ClearPowerUpOnTable(uint8_t x, uint8_t y);    // $ED8C
void ScanForItems(uint8_t x);                      // $ED98
uint8_t PowerUpHandler(uint8_t& x);                // $EDFE
void PrepareItemID(uint8_t type);                  // $EE3D
void SetItemID(uint8_t mapX);                      // $EE41
bool CheckForItem();                               // $EE4A
uint8_t SpecEnemyHandler(uint8_t& x);              // $EE63
void InitMemu(uint8_t x, bool& carry);             // $EE86
uint8_t CannonHandler(uint8_t& x);                 // $EEA6
uint8_t MotherBrainHandler(uint8_t& x);            // $EEAE
uint8_t ZeebetiteHandler(uint8_t& x);              // $EECA
uint8_t RinkaHandler(uint8_t& x);                  // $EEEE
bool AnotherItem(uint8_t y);                       // $EF00
void AddToPtr00(uint8_t a);                        // $EF09
bool DrawStructRow(uint8_t a);                     // $EF13
void AdvanceRow(uint8_t a);                        // $EF78
void DrawStruct();                                 // $EF8C
void UpdateAttrib();                               // $EF9E
void InitTables();                                 // $EFF8
void FillRoomRAM(uint8_t a, uint8_t x, uint8_t y); // $F01C
void SamusEnemyCollisions();                       // $F034
Regs EnemyCollision(uint8_t x, uint8_t y);         // $F140
Regs MemuCollision(uint8_t x, uint8_t y);          // $F149
void GetEnemy0CoordData(uint8_t x);                // $F152
void GetEnemy1CoordData(uint8_t y);                // $F162
void GetObject0CoordData(uint8_t x);               // $F172
void SetObject0NameTable(uint8_t hi);              // $F17F
void GetObject1CoordData(uint8_t y);               // $F186
void SetObject1NameTable(uint8_t hi);              // $F193
void GetMemu0CoordData(uint8_t x);                 // $F19A

} // namespace Game
