#pragma once

#include "nes/Apu.h"
#include "nes/Cpu6502.h"

#include <array>
#include <cstdint>
#include <vector>

namespace Nes {

// Controller bits in the order the NES shifts them out.
enum Button : uint8_t {
    BtnA = 0x01,
    BtnB = 0x02,
    BtnSelect = 0x04,
    BtnStart = 0x08,
    BtnUp = 0x10,
    BtnDown = 0x20,
    BtnLeft = 0x40,
    BtnRight = 0x80,
};

// Scroll and register state the PPU actually used for one visible scanline.
struct LineState {
    uint16_t v = 0;    // loopy v at the start of the line (nametable, coarse x/y, fine y)
    uint8_t fineX = 0; // fine x scroll
    uint8_t ctrl = 0;  // $2000
    uint8_t mask = 0;  // $2001
};

// An NES with an MMC1 cartridge (SNROM-style: PRG ROM, CHR RAM, 8 KiB WRAM), which is what Metroid is.
// The CPU is instruction-accurate; the PPU is scanline-accurate, which is enough for a game that only
// changes scroll and mirroring during vblank.
class Machine : public Bus {
  public:
    Machine();

    // Takes an iNES image. Returns false if it isn't a mapper 1 cartridge.
    bool Load(const std::vector<uint8_t>& ines);
    void Reset();
    // Runs until the next vblank begins (one 60 Hz frame).
    void RunFrame();

    void SetButtons(uint8_t buttons) {
        mButtons = buttons;
    }

    uint8_t Read(uint16_t addr) override;
    void Write(uint16_t addr, uint8_t value) override;

    // Side-effect-free CPU memory read, for inspecting game state.
    uint8_t Peek(uint16_t addr) const;
    const uint8_t* Ram() const {
        return mRam.data();
    }

    // PPU memory.
    uint8_t PpuPeek(uint16_t addr) const;
    const uint8_t* Chr() const {
        return mChr.data();
    }
    const uint8_t* Palette() const {
        return mPalette.data();
    }
    const uint8_t* Oam() const {
        return mOam.data();
    }
    uint8_t PpuCtrl() const {
        return mCtrl;
    }
    uint8_t PpuMask() const {
        return mMask;
    }
    // 0 one-screen low, 1 one-screen high, 2 vertical, 3 horizontal (MMC1 control bits 0-1).
    uint8_t Mirroring() const {
        return mMmcControl & 3;
    }
    const uint8_t* Vram() const {
        return mVram.data();
    }
    uint8_t MmcControl() const {
        return mMmcControl;
    }
    // Scroll as the game last set it ($2005 writes and the nametable bits of $2000), from the t register.
    uint8_t ScrollRegX() const {
        return (uint8_t)(((mT & 31) << 3) | mFineX);
    }
    uint8_t ScrollRegY() const {
        return (uint8_t)((((mT >> 5) & 31) << 3) | ((mT >> 12) & 7));
    }
    uint8_t OamAddrReg() const {
        return mOamAddr;
    }
    const uint8_t* Wram() const {
        return mWram.data();
    }
    // MMC1 PRG bank register (the 16 KiB bank at $8000 in Metroid's mode).
    uint8_t PrgBank() const {
        return mPrgBank & 0x0F;
    }
    // Per-scanline scroll state of the frame that just ran.
    const std::array<LineState, 240>& Lines() const {
        return mLines;
    }

    // Draws the last frame the way a television would show it, as 0xRRGGBBAA pixels (256x240).
    void RenderFrame(uint32_t* out) const;
    // NES palette index to 0xRRGGBBAA.
    static uint32_t Color(uint8_t index);

    // Audio produced so far (signed 16-bit mono at Apu::kSampleRate). The caller drains it.
    std::vector<int16_t>& AudioSamples() {
        return mApu.Samples();
    }

    const Cpu6502& Cpu() const {
        return mCpu;
    }
    uint64_t FrameCount() const {
        return mFrame;
    }

  private:
    uint16_t NametableIndex(uint16_t addr) const;
    uint8_t PpuRead(uint16_t addr);
    void PpuWrite(uint16_t addr, uint8_t value);
    void MapperWrite(uint16_t addr, uint8_t value);
    void RunCycles(double cycles);
    void Scanline(int line);
    void RenderLine(int line, uint32_t* out) const;

    Cpu6502 mCpu;
    Apu mApu;
    std::array<uint8_t, 0x800> mRam{};
    std::array<uint8_t, 0x2000> mWram{};
    std::vector<uint8_t> mPrg;

    // MMC1
    uint8_t mShift = 0x10;
    uint8_t mMmcControl = 0x0C;
    uint8_t mChrBank0 = 0, mChrBank1 = 0, mPrgBank = 0;

    // PPU
    std::array<uint8_t, 0x2000> mChr{};
    std::array<uint8_t, 0x800> mVram{};
    std::array<uint8_t, 0x20> mPalette{};
    std::array<uint8_t, 0x100> mOam{};
    uint8_t mCtrl = 0, mMask = 0, mStatus = 0, mOamAddr = 0, mReadBuffer = 0, mOpenBus = 0;
    uint16_t mV = 0, mT = 0;
    uint8_t mFineX = 0;
    bool mW = false;
    bool mNmiPending = false;
    std::array<LineState, 240> mLines{};

    // Controller
    uint8_t mButtons = 0, mShiftReg = 0;
    bool mStrobe = false;

    double mCycleDebt = 0;
    uint64_t mFrame = 0;
};

} // namespace Nes
