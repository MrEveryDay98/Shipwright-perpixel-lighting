#include "nes/Nes.h"

#include <algorithm>

namespace Nes {

namespace {

// NTSC 2C02 colours.
constexpr uint32_t kPalette[64] = {
    0x666666, 0x002A88, 0x1412A7, 0x3B00A4, 0x5C007E, 0x6E0040, 0x6C0600, 0x561D00, 0x333500, 0x0B4800, 0x005200,
    0x004F08, 0x00404D, 0x000000, 0x000000, 0x000000, 0xADADAD, 0x155FD9, 0x4240FF, 0x7527FE, 0xA01ACC, 0xB71E7B,
    0xB53120, 0x994E00, 0x6B6D00, 0x388700, 0x0C9300, 0x008F32, 0x007C8D, 0x000000, 0x000000, 0x000000, 0xFFFEFF,
    0x64B0FF, 0x9290FF, 0xC676FF, 0xF36AFF, 0xFE6ECC, 0xFE8170, 0xEA9E22, 0xBCBE00, 0x88D800, 0x5CE430, 0x45E082,
    0x48CDDE, 0x4F4F4F, 0x000000, 0x000000, 0xFFFEFF, 0xC0DFFF, 0xD3D2FF, 0xE8C8FF, 0xFBC2FF, 0xFEC4EA, 0xFECCC5,
    0xF7D8A5, 0xE4E594, 0xCFEF96, 0xBDF4AB, 0xB3F3CC, 0xB5EBF2, 0xB8B8B8, 0x000000, 0x000000,
};

// 341 PPU dots per scanline, 3 dots per CPU cycle.
constexpr double kCyclesPerLine = 341.0 / 3.0;

} // namespace

Machine::Machine() : mCpu(*this), mApu([this](uint16_t addr) { return Read(addr); }) {
}

uint32_t Machine::Color(uint8_t index) {
    return (kPalette[index & 0x3F] << 8) | 0xFF;
}

bool Machine::Load(const std::vector<uint8_t>& ines) {
    if (ines.size() < 16 || ines[0] != 'N' || ines[1] != 'E' || ines[2] != 'S' || ines[3] != 0x1A) {
        return false;
    }
    int mapper = (ines[6] >> 4) | (ines[7] & 0xF0);
    size_t prgSize = ines[4] * 0x4000;
    size_t offset = 16 + ((ines[6] & 4) ? 512 : 0);
    if (mapper != 1 || prgSize == 0 || ines.size() < offset + prgSize) {
        return false;
    }
    mPrg.assign(ines.begin() + offset, ines.begin() + offset + prgSize);
    return true;
}

void Machine::Reset() {
    mRam.fill(0);
    mVram.fill(0);
    mPalette.fill(0);
    mOam.fill(0);
    mChr.fill(0);
    mShift = 0x10;
    mMmcControl = 0x0C;
    mChrBank0 = mChrBank1 = mPrgBank = 0;
    mCtrl = mMask = mStatus = mOamAddr = mReadBuffer = 0;
    mV = mT = 0;
    mFineX = 0;
    mW = false;
    mNmiPending = false;
    mCycleDebt = 0;
    mFrame = 0;
    mApu.Reset();
    mCpu.Reset();
}

// ---------------------------------------------------------------------------------------------------------------
// CPU bus

uint8_t Machine::Peek(uint16_t addr) const {
    if (addr < 0x2000) {
        return mRam[addr & 0x7FF];
    }
    if (addr >= 0x6000 && addr < 0x8000) {
        return mWram[addr - 0x6000];
    }
    if (addr >= 0x8000) {
        int banks = (int)(mPrg.size() / 0x4000);
        int mode = (mMmcControl >> 2) & 3;
        int bank;
        if (mode < 2) {
            bank = (mPrgBank & 0x0E) + (addr >= 0xC000 ? 1 : 0);
        } else if (mode == 2) {
            bank = addr >= 0xC000 ? (mPrgBank & 0x0F) : 0;
        } else {
            bank = addr >= 0xC000 ? banks - 1 : (mPrgBank & 0x0F);
        }
        size_t off = (size_t)(bank % banks) * 0x4000 + (addr & 0x3FFF);
        return mPrg[off];
    }
    return 0;
}

uint8_t Machine::Read(uint16_t addr) {
    if (addr < 0x2000 || addr >= 0x6000) {
        return Peek(addr);
    }
    if (addr < 0x4000) {
        switch (addr & 7) {
            case 2: {
                uint8_t v = (mStatus & 0xE0) | (mOpenBus & 0x1F);
                mStatus &= ~0x80;
                mW = false;
                mOpenBus = v;
                return v;
            }
            case 4:
                return mOam[mOamAddr];
            case 7: {
                uint16_t a = mV & 0x3FFF;
                uint8_t v;
                if (a >= 0x3F00) {
                    v = PpuRead(a);
                    mReadBuffer = PpuRead(a - 0x1000);
                } else {
                    v = mReadBuffer;
                    mReadBuffer = PpuRead(a);
                }
                mV = (mV + ((mCtrl & 0x04) ? 32 : 1)) & 0x7FFF;
                mOpenBus = v;
                return v;
            }
            default:
                return mOpenBus;
        }
    }
    if (addr == 0x4016) {
        uint8_t bit = mStrobe ? (mButtons & 1) : (mShiftReg & 1);
        if (!mStrobe) {
            mShiftReg = (mShiftReg >> 1) | 0x80;
        }
        return 0x40 | bit;
    }
    if (addr == 0x4017) {
        return 0x40;
    }
    if (addr == 0x4015) {
        return mApu.ReadStatus();
    }
    return 0;
}

void Machine::Write(uint16_t addr, uint8_t value) {
    if (addr < 0x2000) {
        mRam[addr & 0x7FF] = value;
        return;
    }
    if (addr < 0x4000) {
        mOpenBus = value;
        switch (addr & 7) {
            case 0:
                if (!(mCtrl & 0x80) && (value & 0x80) && (mStatus & 0x80)) {
                    mNmiPending = true;
                }
                mCtrl = value;
                mT = (mT & ~0x0C00) | ((value & 3) << 10);
                break;
            case 1:
                mMask = value;
                break;
            case 3:
                mOamAddr = value;
                break;
            case 4:
                mOam[mOamAddr++] = value;
                break;
            case 5:
                if (!mW) {
                    mT = (mT & ~0x001F) | (value >> 3);
                    mFineX = value & 7;
                } else {
                    mT = (mT & ~0x73E0) | ((value & 7) << 12) | ((value & 0xF8) << 2);
                }
                mW = !mW;
                break;
            case 6:
                if (!mW) {
                    mT = (mT & 0x00FF) | ((value & 0x3F) << 8);
                } else {
                    mT = (mT & 0xFF00) | value;
                    mV = mT;
                }
                mW = !mW;
                break;
            case 7:
                PpuWrite(mV & 0x3FFF, value);
                mV = (mV + ((mCtrl & 0x04) ? 32 : 1)) & 0x7FFF;
                break;
        }
        return;
    }
    if (addr == 0x4014) {
        uint16_t page = value << 8;
        for (int i = 0; i < 256; i++) {
            mOam[(mOamAddr + i) & 0xFF] = Read(page | i);
        }
        mCpu.AddCycles(513 + (mCpu.Cycles() & 1));
        return;
    }
    if (addr == 0x4016) {
        bool strobe = value & 1;
        if (mStrobe && !strobe) {
            mShiftReg = mButtons;
        }
        mStrobe = strobe;
        if (mStrobe) {
            mShiftReg = mButtons;
        }
        return;
    }
    if (addr <= 0x4013 || addr == 0x4015 || addr == 0x4017) {
        mApu.Write(addr, value);
        return;
    }
    if (addr < 0x6000) {
        return;
    }
    if (addr < 0x8000) {
        mWram[addr - 0x6000] = value;
        return;
    }
    MapperWrite(addr, value);
}

void Machine::MapperWrite(uint16_t addr, uint8_t value) {
    if (value & 0x80) {
        mShift = 0x10;
        mMmcControl |= 0x0C;
        return;
    }
    bool full = mShift & 1;
    mShift = (mShift >> 1) | ((value & 1) << 4);
    if (!full) {
        return;
    }
    uint8_t data = mShift;
    mShift = 0x10;
    switch ((addr >> 13) & 3) {
        case 0:
            mMmcControl = data;
            break;
        case 1:
            mChrBank0 = data;
            break;
        case 2:
            mChrBank1 = data;
            break;
        case 3:
            mPrgBank = data;
            break;
    }
}

// ---------------------------------------------------------------------------------------------------------------
// PPU memory

uint16_t Machine::NametableIndex(uint16_t addr) const {
    uint16_t a = addr & 0x0FFF;
    uint16_t table = a >> 10;
    uint16_t page;
    switch (mMmcControl & 3) {
        case 0:
            page = 0;
            break;
        case 1:
            page = 1;
            break;
        case 2: // vertical: $2000/$2800 share, $2400/$2C00 share
            page = table & 1;
            break;
        default: // horizontal
            page = table >> 1;
            break;
    }
    return (page << 10) | (a & 0x3FF);
}

uint8_t Machine::PpuPeek(uint16_t addr) const {
    addr &= 0x3FFF;
    if (addr < 0x2000) {
        return mChr[addr];
    }
    if (addr < 0x3F00) {
        return mVram[NametableIndex(addr)];
    }
    uint8_t i = addr & 0x1F;
    if ((i & 0x13) == 0x10) {
        i &= 0x0F;
    }
    return mPalette[i];
}

uint8_t Machine::PpuRead(uint16_t addr) {
    return PpuPeek(addr);
}

void Machine::PpuWrite(uint16_t addr, uint8_t value) {
    addr &= 0x3FFF;
    if (addr < 0x2000) {
        mChr[addr] = value;
    } else if (addr < 0x3F00) {
        mVram[NametableIndex(addr)] = value;
    } else {
        uint8_t i = addr & 0x1F;
        if ((i & 0x13) == 0x10) {
            i &= 0x0F;
        }
        mPalette[i] = value & 0x3F;
    }
}

// ---------------------------------------------------------------------------------------------------------------
// Timing

void Machine::RunCycles(double cycles) {
    mCycleDebt += cycles;
    while (mCycleDebt > 0) {
        uint64_t before = mCpu.Cycles();
        if (mNmiPending) {
            mNmiPending = false;
            mCpu.Nmi();
        } else if (mApu.Irq()) {
            mCpu.Irq(); // ignored while the I flag is set
        }
        mCpu.Step();
        int cycles = (int)(mCpu.Cycles() - before);
        mApu.Clock(cycles);
        mCycleDebt -= cycles;
    }
}

void Machine::Scanline(int line) {
    bool rendering = mMask & 0x18;
    if (line < 240) {
        mLines[line] = { mV, mFineX, mCtrl, mMask };
        // Sprite 0 hit, approximated as the first line sprite 0 covers.
        if (rendering && (mMask & 0x18) == 0x18 && !(mStatus & 0x40)) {
            int top = mOam[0] + 1;
            int h = (mCtrl & 0x20) ? 16 : 8;
            if (line >= top && line < top + h) {
                mStatus |= 0x40;
            }
        }
        RunCycles(kCyclesPerLine);
        if (rendering) {
            // Increment fine/coarse y, then reload the horizontal scroll from t (dots 256-257).
            if ((mV & 0x7000) != 0x7000) {
                mV += 0x1000;
            } else {
                mV &= ~0x7000;
                int y = (mV & 0x03E0) >> 5;
                if (y == 29) {
                    y = 0;
                    mV ^= 0x0800;
                } else if (y == 31) {
                    y = 0;
                } else {
                    y++;
                }
                mV = (mV & ~0x03E0) | (y << 5);
            }
            mV = (mV & ~0x041F) | (mT & 0x041F);
        }
        return;
    }
    if (line == 241) {
        mStatus |= 0x80;
        if (mCtrl & 0x80) {
            mNmiPending = true;
        }
    }
    if (line == 261) {
        mStatus &= ~0xE0;
        RunCycles(kCyclesPerLine);
        if (mMask & 0x18) {
            mV = mT;
        }
        return;
    }
    RunCycles(kCyclesPerLine);
}

void Machine::RunFrame() {
    // Vblank first, so the picture rendered by lines 0-239 matches the state the game leaves behind.
    for (int line = 241; line <= 261; line++) {
        Scanline(line);
    }
    for (int line = 0; line <= 240; line++) {
        Scanline(line);
    }
    mFrame++;
}

// ---------------------------------------------------------------------------------------------------------------
// Reference 2D renderer

void Machine::RenderLine(int line, uint32_t* out) const {
    const LineState& ls = mLines[line];
    uint8_t bgIdx[256];
    uint8_t bgOpaque[256];
    std::fill(bgIdx, bgIdx + 256, 0);
    std::fill(bgOpaque, bgOpaque + 256, 0);

    if (ls.mask & 0x08) {
        uint16_t patBase = (ls.ctrl & 0x10) ? 0x1000 : 0;
        int fineY = (ls.v >> 12) & 7;
        int coarseY = (ls.v >> 5) & 31;
        int ntY = (ls.v >> 11) & 1;
        int startX = ((ls.v >> 10) & 1) * 256 + (ls.v & 31) * 8 + ls.fineX;
        for (int x = 0; x < 256; x++) {
            if (x < 8 && !(ls.mask & 0x02)) {
                continue;
            }
            int sx = (startX + x) & 511;
            int ntX = sx >> 8;
            int coarseX = (sx >> 3) & 31;
            uint16_t nt = 0x2000 | (((ntY << 1) | ntX) << 10);
            uint8_t tile = PpuPeek(nt | (coarseY << 5) | coarseX);
            uint8_t attr = PpuPeek(nt | 0x3C0 | ((coarseY >> 2) << 3) | (coarseX >> 2));
            int shift = ((coarseY & 2) << 1) | (coarseX & 2);
            uint8_t pal = (attr >> shift) & 3;
            uint16_t p = patBase + tile * 16 + fineY;
            int bit = 7 - (sx & 7);
            uint8_t ci = ((mChr[p] >> bit) & 1) | (((mChr[p + 8] >> bit) & 1) << 1);
            if (ci) {
                bgIdx[x] = pal * 4 + ci;
                bgOpaque[x] = 1;
            }
        }
    }

    uint8_t spIdx[256];
    uint8_t spBehind[256];
    uint8_t spSet[256];
    std::fill(spSet, spSet + 256, 0);
    if (ls.mask & 0x10) {
        int h = (ls.ctrl & 0x20) ? 16 : 8;
        int found = 0;
        for (int s = 0; s < 64 && found < 8; s++) {
            const uint8_t* o = &mOam[s * 4];
            int row = line - (o[0] + 1);
            if (row < 0 || row >= h) {
                continue;
            }
            found++;
            uint8_t attr = o[2];
            if (attr & 0x80) {
                row = h - 1 - row;
            }
            uint16_t p;
            if (h == 16) {
                p = ((o[1] & 1) ? 0x1000 : 0) + (o[1] & 0xFE) * 16 + (row >= 8 ? 16 : 0) + (row & 7);
            } else {
                p = ((ls.ctrl & 0x08) ? 0x1000 : 0) + o[1] * 16 + row;
            }
            for (int i = 0; i < 8; i++) {
                int x = o[3] + i;
                if (x > 255 || spSet[x] || (x < 8 && !(ls.mask & 0x04))) {
                    continue;
                }
                int bit = (attr & 0x40) ? i : 7 - i;
                uint8_t ci = ((mChr[p] >> bit) & 1) | (((mChr[p + 8] >> bit) & 1) << 1);
                if (ci) {
                    spSet[x] = 1;
                    spIdx[x] = 16 + (attr & 3) * 4 + ci;
                    spBehind[x] = attr & 0x20;
                }
            }
        }
    }

    for (int x = 0; x < 256; x++) {
        uint8_t idx = bgIdx[x];
        if (spSet[x] && !(spBehind[x] && bgOpaque[x])) {
            idx = spIdx[x];
        }
        out[x] = Color(mPalette[idx]);
    }
}

void Machine::RenderFrame(uint32_t* out) const {
    for (int line = 0; line < 240; line++) {
        RenderLine(line, out + line * 256);
    }
}

} // namespace Nes
