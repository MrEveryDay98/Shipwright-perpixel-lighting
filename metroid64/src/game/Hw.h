#pragma once

// What the game's code talks to besides memory: where the original wrote to the PPU, APU, mapper and
// controller ports, the port updates these plain models instead. Nothing here runs game code or
// draws; the renderer reads Video and the audio output plays the APU register writes.

#include <cstdint>

namespace Game::Hw {

struct Video {
    // Picture memory the game fills: 2 KiB of nametables, palette, pattern tables (CHR RAM), sprites.
    uint8_t nametables[0x800] = {};
    uint8_t palette[0x20] = {};
    uint8_t chr[0x2000] = {};
    uint8_t oam[0x100] = {};

    uint8_t ctrl = 0; // $2000
    uint8_t mask = 0; // $2001
    // Scroll as last written to $2005 (x then y); the nametable bits come from ctrl.
    uint8_t scrollX = 0, scrollY = 0;
    // 0 one-screen low, 1 one-screen high, 2 vertical, 3 horizontal (MMC1 control bits 0-1).
    uint8_t mirroring = 2;

    // Address port state.
    uint16_t addr = 0;
    bool latch = false;
    uint8_t readBuffer = 0;
    uint8_t oamAddr = 0;

    // The nametable byte for a PPU address in $2000-$2FFF, after mirroring.
    uint8_t& Nametable(uint16_t ppuAddr);
    uint8_t Nametable(uint16_t ppuAddr) const {
        return const_cast<Video*>(this)->Nametable(ppuAddr);
    }
};

extern Video gVideo;

// $2000-$2007
void PpuCtrl(uint8_t v);
void PpuMask(uint8_t v);
uint8_t PpuStatus();
void PpuScroll(uint8_t v);
void PpuAddr(uint8_t v);
void PpuData(uint8_t v);
uint8_t PpuDataRead();
void OamAddr(uint8_t v);
// $4014: copies a page of RAM into sprite memory.
void OamDma(uint8_t page);

// The 16 KiB bank mapped at $8000, and the MMC1 control register (mirroring and bank modes).
extern uint8_t gPrgBank;
extern uint8_t gMmcControl;
void SetPrgBank(uint8_t bank);
void SetMmcControl(uint8_t value);

// APU registers $4000-$4013, $4015, $4017.
void ApuWrite(uint16_t reg, uint8_t v);
uint8_t ApuStatus();
// Receives APU writes (the audio output); may be null.
using ApuSink = void (*)(uint16_t reg, uint8_t v);
void SetApuSink(ApuSink sink, uint8_t (*status)());

// Controllers: the buttons held, in the order the NES shifts them out (bit 0 A, 1 B, 2 Select, 3 Start,
// 4 Up, 5 Down, 6 Left, 7 Right).
extern uint8_t gPads[2];
void JoypadStrobe(uint8_t v);
uint8_t JoypadRead(int pad);

} // namespace Game::Hw
