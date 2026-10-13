#include "game/Hw.h"

#include "game/Memory.h"

namespace Game::Hw {

Video gVideo;
uint8_t gPrgBank = 0;
uint8_t gMmcControl = 0x0C;
uint8_t gPads[2] = {};

namespace {
ApuSink sApuSink = nullptr;
uint8_t (*sApuStatus)() = nullptr;
uint8_t sPadShift[2] = {};
bool sStrobe = false;
} // namespace

uint8_t& Video::Nametable(uint16_t ppuAddr) {
    uint16_t a = ppuAddr & 0x0FFF;
    uint16_t table = a >> 10;
    uint16_t page;
    switch (mirroring) {
        case 0:
            page = 0;
            break;
        case 1:
            page = 1;
            break;
        case 2:
            page = table & 1;
            break;
        default:
            page = table >> 1;
            break;
    }
    return nametables[(page << 10) | (a & 0x3FF)];
}

void PpuCtrl(uint8_t v) {
    gVideo.ctrl = v;
}

void PpuMask(uint8_t v) {
    gVideo.mask = v;
}

uint8_t PpuStatus() {
    // Reading the status register resets the address latch. The game only polls the vblank flag to wait
    // for vblank to start or end; it reads alternately set and clear so such waits finish at once.
    static bool vblank = false;
    vblank = !vblank;
    gVideo.latch = false;
    return vblank ? 0x80 : 0x00;
}

void PpuScroll(uint8_t v) {
    if (!gVideo.latch) {
        gVideo.scrollX = v;
    } else {
        gVideo.scrollY = v;
    }
    gVideo.latch = !gVideo.latch;
}

void PpuAddr(uint8_t v) {
    if (!gVideo.latch) {
        gVideo.addr = (uint16_t)((gVideo.addr & 0x00FF) | ((v & 0x3F) << 8));
    } else {
        gVideo.addr = (uint16_t)((gVideo.addr & 0xFF00) | v);
    }
    gVideo.latch = !gVideo.latch;
}

static uint8_t& VideoByte(uint16_t a, bool& palette) {
    a &= 0x3FFF;
    palette = false;
    if (a < 0x2000) {
        return gVideo.chr[a];
    }
    if (a < 0x3F00) {
        return gVideo.Nametable(a);
    }
    palette = true;
    uint8_t i = a & 0x1F;
    if ((i & 0x13) == 0x10) {
        i &= 0x0F; // $3F10/$3F14/$3F18/$3F1C mirror the background entries
    }
    return gVideo.palette[i];
}

static void Advance() {
    gVideo.addr = (uint16_t)((gVideo.addr + ((gVideo.ctrl & 0x04) ? 32 : 1)) & 0x3FFF);
}

void PpuData(uint8_t v) {
    bool palette;
    uint8_t& b = VideoByte(gVideo.addr, palette);
    b = palette ? (v & 0x3F) : v;
    Advance();
}

uint8_t PpuDataRead() {
    bool palette;
    uint8_t v = VideoByte(gVideo.addr, palette);
    uint8_t result;
    if (palette) {
        result = v;
        bool unused;
        gVideo.readBuffer = VideoByte(gVideo.addr - 0x1000, unused);
    } else {
        result = gVideo.readBuffer;
        gVideo.readBuffer = v;
    }
    Advance();
    return result;
}

void OamAddr(uint8_t v) {
    gVideo.oamAddr = v;
}

void OamDma(uint8_t page) {
    for (int i = 0; i < 256; i++) {
        gVideo.oam[(gVideo.oamAddr + i) & 0xFF] = Read((uint16_t)((page << 8) | i));
    }
}

void SetPrgBank(uint8_t bank) {
    gPrgBank = bank & 0x0F;
}

void SetMmcControl(uint8_t value) {
    gMmcControl = value & 0x1F;
    gVideo.mirroring = value & 3;
}

void ApuWrite(uint16_t reg, uint8_t v) {
    if (sApuSink != nullptr) {
        sApuSink(reg, v);
    }
}

uint8_t ApuStatus() {
    return sApuStatus != nullptr ? sApuStatus() : 0;
}

void SetApuSink(ApuSink sink, uint8_t (*status)()) {
    sApuSink = sink;
    sApuStatus = status;
}

void JoypadStrobe(uint8_t v) {
    bool strobe = v & 1;
    if (strobe || sStrobe) {
        sPadShift[0] = gPads[0];
        sPadShift[1] = gPads[1];
    }
    sStrobe = strobe;
}

uint8_t JoypadRead(int pad) {
    uint8_t bit = sStrobe ? (gPads[pad] & 1) : (sPadShift[pad] & 1);
    if (!sStrobe) {
        sPadShift[pad] = (uint8_t)((sPadShift[pad] >> 1) | 0x80);
    }
    return 0x40 | bit;
}

} // namespace Game::Hw
