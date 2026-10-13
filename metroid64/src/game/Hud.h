#pragma once

// Bank 7: Samus's status bar (energy, tanks, missiles, escape timer).

#include <cstdint>

namespace Game {

void DisplayBar();
uint8_t SPRWriteDigit(uint8_t x, uint8_t digit);
uint8_t AddOneTank(uint8_t x, uint8_t tile);
uint8_t HexToDec(uint8_t a);
uint8_t GetDigit(uint8_t& a);

} // namespace Game
