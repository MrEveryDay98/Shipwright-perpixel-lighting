#pragma once

// Bank 7: object animation and sprite drawing.

#include <cstdint>

namespace Game {

void UpdateObjAnim(uint8_t delay);
uint8_t SignExtendTo06(uint8_t a);
uint8_t GetSpriteCntrlData();
uint8_t ReduceYRadius(uint8_t radius);
bool AnimDrawObject(uint8_t delay);
bool DrawFrame();
void DrawSpriteObject(uint8_t x);
void ClearObjectCntrl();
uint8_t YDisplacement(uint8_t y);
uint8_t XDisplacement(uint8_t y);
uint8_t NegativeDisplacement(uint8_t a);
uint8_t IsObjectVisible();
void SpriteFlipBitsOveride(uint8_t y);

} // namespace Game
