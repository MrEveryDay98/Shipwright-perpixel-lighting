#include "Port.h"

#include "Picture.h"

#include "game/Game.h"
#include "game/Hw.h"
#include "game/Memory.h"
#include "game/Present.h"
#include "nes/Nes.h"

#include <algorithm>
#include <fstream>
#include <iterator>

namespace Metroid {

namespace {

// The APU synth plays the register writes the game makes during a frame.
Nes::Apu* sApu = nullptr;

void ApuSink(uint16_t reg, uint8_t v) {
    if (sApu) {
        sApu->Write(reg, v);
    }
}

uint8_t ApuStatus() {
    return sApu ? sApu->ReadStatus() : 0;
}

constexpr int kCpuCyclesPerFrame = 29781;

} // namespace

Port::Port() : mApu([](uint16_t addr) { return Game::Read(addr); }) {
}

bool Port::LoadRomFile(const std::string& path, std::string& error) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        error = "Could not open " + path;
        return false;
    }
    std::vector<uint8_t> file((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    const char* why = nullptr;
    if (!Game::LoadRom(file, &why)) {
        error = why ? why : "Not Metroid (USA)";
        return false;
    }
    return true;
}

void Port::PowerOn() {
    sApu = &mApu;
    mApu.Reset();
    Game::Hw::SetApuSink(ApuSink, ApuStatus);
    Game::SetNmiHook([this](int) { Game::Hw::gPads[0] = mButtons; });
    Game::PowerOn();
    ResetView();
    mFrame = 0;
}

void Port::Tick(uint8_t buttons) {
    mButtons = buttons;
    int nmis = Game::RunFrame();
    mFrame += nmis;
    // The sound engine runs once per NMI; the synth renders the time those frames took.
    mApu.Clock(kCpuCyclesPerFrame * std::max(nmis, 1));
    for (int i = 0; i < 64; i++) {
        mOamTags[i] = { (SpriteOwner)Game::Present::gOamTags[i].owner, Game::Present::gOamTags[i].slot };
    }
    const auto& v = Game::Hw::gVideo;
    TrackScroll((v.ctrl & 0x01) * 256 + v.scrollX, ((v.ctrl >> 1) & 0x01) * 240 + v.scrollY);
}

uint8_t Port::AreaBank() const {
    return Game::Hw::gPrgBank;
}

const uint8_t* Port::Chr() const {
    return Game::Hw::gVideo.chr;
}
const uint8_t* Port::Palette() const {
    return Game::Hw::gVideo.palette;
}
const uint8_t* Port::Oam() const {
    return Game::Hw::gVideo.oam;
}
uint8_t Port::PpuCtrl() const {
    return Game::Hw::gVideo.ctrl;
}
uint8_t Port::PpuMask() const {
    return Game::Hw::gVideo.mask;
}
uint8_t Port::Mirroring() const {
    return Game::Hw::gVideo.mirroring;
}
uint8_t Port::PpuPeek(uint16_t addr) const {
    return Game::Hw::gVideo.Nametable(addr);
}
RoomInfo Port::RoomOnPage(int page) const {
    const auto& t = Game::Present::gRooms[page & 1];
    return { t.valid, t.bank, t.mapX, t.mapY, t.room };
}

uint8_t Port::Peek(uint16_t addr) const {
    return Game::Read(addr);
}

void Port::RenderFrame(uint32_t* out) const {
    RenderPicture(Game::Hw::gVideo, out);
}

} // namespace Metroid
