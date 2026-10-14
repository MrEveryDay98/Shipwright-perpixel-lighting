#include "Port.h"

#include "GameData.h"
#include "Menu.h"
#include "Picture.h"

#include "game/Game.h"
#include "game/Hw.h"
#include "game/Memory.h"
#include "game/AllActive.h"
#include "game/Present.h"

#include <algorithm>
#include <libultraship/bridge/consolevariablebridge.h>
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

bool Port::LoadGameData(std::string& error) {
    return Metroid::LoadGameData(error);
}

void Port::PowerOn() {
    sApu = &mApu;
    mApu.Reset();
    Game::Hw::SetApuSink(ApuSink, ApuStatus);
    Game::SetNmiHook([this](int) { Game::Hw::gPads[0] = mButtons; });
    Game::PowerOn();
    ResetView();
    mLagTicks = 0;
    mFrame = 0;
}

void Port::Tick(uint8_t buttons) {
    mButtons = buttons;
    if (mLagTicks > 0) {
        // The original was still busy with the last frame (a lag frame): it spent this vblank too, so the
        // game waits here as long as it did on the console.
        mLagTicks--;
        mFrame++;
        mApu.Clock(kCpuCyclesPerFrame);
        return;
    }
    Game::AllActive::gEnabled = CVarGetInteger(kCVarOutsideView, 0) == 2;
    Game::AllActive::BeforeFrame();
    int nmis = std::max(Game::RunFrame(), 1);
    mFrame++;
    mLagTicks = nmis - 1;
    mApu.Clock(kCpuCyclesPerFrame);
    for (int i = 0; i < 64; i++) {
        mOamTags[i] = { (SpriteOwner)Game::Present::gOamTags[i].owner, Game::Present::gOamTags[i].slot };
    }
    const auto& v = Game::Hw::gVideo;
    TrackScroll((v.ctrl & 0x01) * 256 + v.scrollX, ((v.ctrl >> 1) & 0x01) * 240 + v.scrollY);
    TrackObjects();
}

int Port::AreaBank() const {
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
    return { t.valid, t.bank, t.mapX, t.mapY, t.room, t.complete };
}

uint8_t Port::Peek(uint16_t addr) const {
    return Game::Read(addr);
}

void Port::RenderFrame(uint32_t* out) const {
    RenderPicture(Game::Hw::gVideo, out);
}

} // namespace Metroid
