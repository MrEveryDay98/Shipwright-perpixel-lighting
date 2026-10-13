#pragma once

// Bank 0: title screen, intro, password screens and the ending (Bank00.asm in the community
// disassembly). These routines run while bank 0 is mapped at $8000.
//
//   Title.cpp     intro/attract sequence, START/CONTINUE, password entry and display, game over
//   Password.cpp  password encoding and decoding, the unique item list
//   Ending.cpp    the ending and its credits
//
// The sound engine of bank 0 ($B3B4 on) is in Sound.cpp.

#include <cstdint>

namespace Game {

// --- Called from bank 7 ---

void TitleMainRoutine(); // $8000: runs the current TitleRoutine
void NMIScreenWrite();   // $9A07: the ending's name table writes, from the NMI
void DecSpriteYCoord();  // $988A
void EndGamePalWrite();  // $9F54
void StarPalSwitch();    // $8AC7: twinkles the intro stars
void CopyMap();          // $A93E: copies the world map to $7000-$73FF

// --- Shared between the bank 0 files ---

// Title.cpp
void ClearAll();                              // $909F
void InitializeGame();                        // $92D4
void PrepPPUProcess_(uint8_t lo, uint8_t hi); // $C20E (bank 7) and $9449 PreparePPUProcess

// Password.cpp
void ProcessUniqueItems();             // $8B79
void UniqueItemSearch();               // $8B9C
void UniqueItemFound(uint8_t x);       // $8BB5
void LoadUniqueItems();                // $8BD4
void SamusHasItem();                   // $8C39
void CheckPassword();                  // $8C5E
void CalculatePassword();              // $8C7A
void LoadPasswordData();               // $8D12
void LdTanksAndMissiles();             // $8D3D
bool ValidatePassword(bool carry);     // $8DDE: returns the carry, set for a wrong password
void PswrdChksmNScrmbl();              // $8E17
uint8_t PasswordChecksum(bool& carry); // $8E21
void PasswordScramble(bool carry);     // $8E2D
void UnscramblePassword(bool carry);   // $8E4E
void LoadPasswordChar();               // $8E6C
bool ConsolidatePassword();            // $8F60: returns the carry it leaves

// Ending.cpp
void EndGame(); // $9AA7
void Restart(); // $9A39

} // namespace Game
