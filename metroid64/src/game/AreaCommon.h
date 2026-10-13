#pragma once

// Code shared by the five area banks (identical bytes at the same addresses in banks 1-5): enemy
// movement ($8058-$8571) and doors ($8B13-$8D39). Tables they read come from the bank mapped at
// $8000, so each area gets its own data.

#include <cstdint>

namespace Game {

void EnemyMove8058();                          // $8058 move the enemy by its speeds, a pixel at a time
uint8_t EnemyType80B0(uint8_t x, bool& carry); // $80B0: ($977B + EnDataIndex) * 2; carry = bit 7
void EnemyBlockedUp80B8(bool carry);           // $80B8
void EnemyBlockedDown80FB(bool carry);         // $80FB
void EnemyBlockedRight8134(bool carry);        // $8134
void EnemyBlockedLeft816E(bool carry);         // $816E
uint8_t Enemy81B1(uint8_t x);                  // $81B1
uint8_t Enemy81C0(uint8_t x);                  // $81C0
void Enemy81C7(uint8_t x);                     // $81C7
void Enemy81D1(uint8_t x);                     // $81D1
void Enemy81DA(uint8_t x);                     // $81DA
void Enemy81FC(uint8_t x);                     // $81FC
void Enemy8206(uint8_t x);                     // $8206
void Enemy820F(uint8_t x);                     // $820F
void Enemy822B(uint8_t x);                     // $822B movement table pointer into $81/$82
// $8244: vertical speed into $00. Returns true when the original pops its caller's return address
// (movement code $FB): the routine that called $8244 is then left.
bool EnemyVertSpeed8244(uint8_t x);
uint8_t SpeedFromNibble8296(uint8_t a); // $8296: returns A (also in $00)
void EnemyHorzSpeed8318(uint8_t x);     // $8318: horizontal speed into $00
uint8_t SpeedFromByte832F(uint8_t a);   // $832F: returns A (also in $00)
void EnemyVertAccel833F(uint8_t x);     // $833F
void EnemyHorzAccel8395(uint8_t x);     // $8395
bool EnemyMoveUp83F5();                 // $83F5: returns the carry (set: moved)
bool EnemyMoveDown844B();               // $844B
bool EnemyMoveLeft84A7();               // $84A7
bool EnemyMoveRight84FE();              // $84FE
// The pointer table at $8048 (pushed and jumped to with RTS by some enemies): entry index -> routine.
bool EnemyMoveByTable8048(uint8_t index);
void ToggleEnemyNameTable855A(uint8_t x);       // $855A
uint8_t EnemyOtherNameTable8563(uint8_t x);     // $8563
void ToggleEnemyBits856B(uint8_t x, uint8_t a); // $856B

void DrawDoors8B79();      // $8B79
void SamusEnterDoor8B13(); // $8B13

} // namespace Game
