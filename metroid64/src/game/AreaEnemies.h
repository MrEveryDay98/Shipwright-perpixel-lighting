#pragma once

// Enemy routines that several area banks contain at different addresses (AreaEnemies.cpp).

#include <cstdint>

namespace Game {

// $99A1 (Brinstar; $9839 Norfair, $97E2 Tourian, $9905 Kraid, $9830 Ridley): animate and draw the
// enemy by its status ($81), with the animation delays in $00 / $01.
void CommonEnemyJump();
void Delays6And8();                          // $99C8: delays 6 and 8, then CommonEnemyJump
void HopperAI(uint8_t x, uint8_t anims);     // $99B8: hoppers ($85 = $86 = anims)
void EnemyPosTo08(uint8_t x);                // $9CA8
void EnemyPosFrom08(uint8_t x);              // $9C96
void RipperAI(uint8_t x, uint16_t speedTbl); // $9B32: rippers
// $9A4C: crawlers. bottom: the room position where they turn; onlyDelay: delay when status 1.
void CrawlerAI(uint8_t x, uint16_t animTbl, uint8_t bottom, uint8_t onlyDelay);

} // namespace Game
