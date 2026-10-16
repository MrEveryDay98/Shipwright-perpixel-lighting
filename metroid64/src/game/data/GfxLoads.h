#pragma once

// The graphics blocks (GFXInfo entries, $C6E0: 7 bytes each, [bank][source][PPU destination][length]) the
// game loads into the pattern tables: the title's, Samus's when a game starts (LoadSamusGFX, suited), then each
// area's
// when it is entered (InitBrinstarGFX ...). Engine.cpp loads them; the extractor uses them to write each
// area's tiles for the editor.

#include <array>
#include <cstdint>

namespace Game {

constexpr uint16_t kGFXInfo = 0xC6E0;

// The title screen's block (InitTitleGFX), loaded before any game starts; some of it stays (the areas
// but Tourian don't load over all of it).
constexpr uint8_t kTitleGFX = 0x15;
inline constexpr std::array<uint8_t, 6> kSamusGFX{ 0x00, 0x14, 0x17, 0x18, 0x19, 0x16 };
inline constexpr std::array<uint8_t, 6> kBrinstarGFX{ 0x03, 0x04, 0x05, 0x06, 0x19, 0x16 };
inline constexpr std::array<uint8_t, 7> kNorfairGFX{ 0x04, 0x05, 0x07, 0x08, 0x09, 0x19, 0x16 };
inline constexpr std::array<uint8_t, 10> kTourianGFX{ 0x05, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x1A, 0x1C, 0x19, 0x16 };
inline constexpr std::array<uint8_t, 8> kKraidGFX{ 0x04, 0x05, 0x0A, 0x0F, 0x10, 0x11, 0x19, 0x16 };
inline constexpr std::array<uint8_t, 7> kRidleyGFX{ 0x04, 0x05, 0x0A, 0x12, 0x13, 0x19, 0x16 };

} // namespace Game
