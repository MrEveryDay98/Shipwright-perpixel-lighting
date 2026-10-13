#include "Emulator.h"

#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iterator>

namespace Metroid {

namespace {

constexpr uint32_t kUsaPrgCrc32 = 0x70080810;
constexpr size_t kPrgSize = 8 * 0x4000;

uint32_t Crc32(const uint8_t* data, size_t len) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int k = 0; k < 8; k++) {
            crc = (crc >> 1) ^ (0xEDB88320 & (0 - (crc & 1)));
        }
    }
    return ~crc;
}

// Return addresses (as pushed by JSR: address of the JSR's last byte) of the sprite-drawing calls in
// the game engine's main loop at $CB2D.
struct Caller {
    uint16_t pushed;
    SpriteOwner owner;
};
constexpr Caller kCallers[] = {
    { 0xCB2F, SpriteOwner::Enemy },      // JSR $F345: enemies
    { 0xCB32, SpriteOwner::Projectile }, // JSR $D4BF: Samus's projectiles
    { 0xCB35, SpriteOwner::Samus },      // JSR $CC0D: Samus
    { 0xCB50, SpriteOwner::Scenery },    // JSR $8B79: doors
    { 0xCB59, SpriteOwner::Hud },        // JSR $E0C1: status display
};

} // namespace

bool Emulator::LoadRomFile(const std::string& path, std::string& error) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        error = "Could not open " + path;
        return false;
    }
    std::vector<uint8_t> file((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (file.size() < 16 + kPrgSize || file[0] != 'N' || file[1] != 'E' || file[2] != 'S' || file[3] != 0x1A) {
        error = "Not an iNES ROM (.nes)";
        return false;
    }
    size_t start = 16 + ((file[6] & 4) ? 512 : 0);
    if (file.size() < start + kPrgSize || Crc32(&file[start], kPrgSize) != kUsaPrgCrc32) {
        error = "This is not Metroid (USA) (PRG CRC32 mismatch)";
        return false;
    }
    if (!Load(file)) {
        error = "Unsupported cartridge header";
        return false;
    }
    mLoaded = true;
    return true;
}

void Emulator::PowerOn() {
    Reset();
    mBufferTags.fill({});
    ResetView();
}

SpriteTag Emulator::TagForCurrentWrite() const {
    // Walk the stack from the bottom (outermost call) and take the first main-loop call site.
    for (int s = 0xFE; s > Cpu().S(); s--) {
        uint16_t pushed = Peek(0x100 + s) | (Peek(0x100 + s + 1) << 8);
        for (const Caller& c : kCallers) {
            if (pushed == c.pushed) {
                return { c.owner, Peek(0x4B) };
            }
        }
    }
    return { SpriteOwner::Other, 0 };
}

void Emulator::Write(uint16_t addr, uint8_t value) {
    if (addr >= 0x0200 && addr < 0x0300 && (addr & 3) == 0) {
        // The game marks unused entries by moving them off screen ($F4).
        mBufferTags[(addr & 0xFF) >> 2] = value >= 0xEF ? SpriteTag{} : TagForCurrentWrite();
    } else if (addr == 0x4014 && value == 0x02) {
        mOamTags = mBufferTags;
    }
    Nes::Machine::Write(addr, value);
}

void Emulator::Tick(uint8_t buttons) {
    SetButtons(buttons);
    RunFrame();

    // Scroll at the top of the picture (Metroid has no split screen).
    const Nes::LineState& ls = Lines()[0];
    int x = ((ls.v >> 10) & 1) * 256 + (ls.v & 31) * 8 + ls.fineX;
    int y = ((ls.v >> 11) & 1) * 240 + ((ls.v >> 5) & 31) * 8 + ((ls.v >> 12) & 7);
    TrackScroll(x, y);
}

uint8_t Emulator::AreaBank() const {
    return PrgBank();
}

} // namespace Metroid
