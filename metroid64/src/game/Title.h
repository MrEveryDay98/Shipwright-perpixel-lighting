#pragma once

// Bank 0: title screen, intro, password screens and the ending (Bank00.asm in the community
// disassembly). These routines run while bank 0 is mapped at $8000.

namespace Game {

void TitleMainRoutine(); // $8000: runs the current TitleRoutine
void NMIScreenWrite();   // $9A07
void DecSpriteYCoord();  // $988A
void EndGamePalWrite();  // $9F54
void StarPalSwitch();    // $8AC7
void CopyMap();          // $A93E: copies the world map to $7000-$73FF

} // namespace Game
