#include "Port.h"

#include "GameData.h"
#include "Menu.h"
#include "Picture.h"
#include "SavePoints.h"

#include "game/Game.h"
#include "game/Hw.h"
#include "game/Memory.h"
#include "game/Options.h"
#include "game/AllActive.h"
#include "game/Explored.h"
#include "game/Present.h"
#include "game/Ram.h"
#include "game/SaveState.h"

#include <algorithm>
#include <cstdlib>
#include <libultraship/bridge/consolevariablebridge.h>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <spdlog/spdlog.h>

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
    if (!Metroid::LoadGameData(error)) {
        return false;
    }
    LoadSavePoints();
    return true;
}

void Port::PowerOn() {
    sApu = &mApu;
    mApu.Reset();
    Game::Hw::SetApuSink(ApuSink, ApuStatus);
    Game::SetNmiHook([this](int) { Game::Hw::gPads[0] = mButtons; });
    Game::PowerOn();
    Game::Explored::Reset();
    ResetView();
    mLagTicks = 0;
    mFrame = 0;
}

void Port::Tick(uint8_t buttons) {
    mPrevButtons = mButtons;
    mButtons = buttons;
    // Select while paused saves (the original does nothing with it then).
    constexpr uint8_t kSelect = 0x04;
    if ((buttons & kSelect) && !(mPrevButtons & kSelect) && CanSave()) {
        SaveGame();
    }
    // Game over (MainRoutine 4, after the death animation): back to the save instead, if wanted.
    if (Game::Vars::GameMode == 0 && Game::Vars::MainRoutine == 0x04 &&
        CVarGetInteger(kCVarDeathToSave, 1) != 0 && HasSave() && LoadGame()) {
        mFrame++;
        mApu.Clock(kCpuCyclesPerFrame);
        return;
    }
    if (mLagTicks > 0) {
        // The original was still busy with the last frame (a lag frame): it spent this vblank too, so the
        // game waits here as long as it did on the console.
        mLagTicks--;
        mFrame++;
        mApu.Clock(kCpuCyclesPerFrame);
        return;
    }
    Game::AllActive::gEnabled = CVarGetInteger(kCVarOutsideView, 0) == 2;
    Game::Options::gNoFlicker = CVarGetInteger(kCVarNoFlicker, 1) != 0;
    Game::Options::gShotsPastEdge = CVarGetInteger(kCVarShotsPastEdge, 1) != 0;
    Game::Options::gNoPasswords = true; // the app saves the game instead
    Game::AllActive::BeforeFrame();
    int nmis = std::max(Game::RunFrame(), 1);
    mFrame++;
    mLagTicks = nmis - 1;
    mApu.Clock(kCpuCyclesPerFrame);
    Game::Explored::Update();
    // CONTINUE on the START/CONTINUE screen: the saved game, if there is one.
    if (Game::Options::gContinueRequested) {
        Game::Options::gContinueRequested = false;
        if (HasSave()) {
            LoadGame();
            return;
        }
    }
    // Up on a save point saves.
    constexpr uint8_t kUp = 0x10;
    if ((buttons & kUp) && !(mPrevButtons & kUp) && SavePointHere() != nullptr) {
        WriteSave();
    }
    for (int i = 0; i < 64; i++) {
        mOamTags[i] = { (SpriteOwner)Game::Present::gOamTags[i].owner, Game::Present::gOamTags[i].slot };
    }
    const auto& v = Game::Hw::gVideo;
    TrackScroll((v.ctrl & 0x01) * 256 + v.scrollX, ((v.ctrl >> 1) & 0x01) * 240 + v.scrollY);
    TrackObjects();
}

bool Port::Paused() const {
    return Game::Vars::GameMode == 0 && Game::Vars::MainRoutine == 0x05;
}

bool Port::Playing() const {
    return Game::Vars::GameMode == 0 && Game::Vars::MainRoutine == 0x03;
}

bool Port::CanSave() const {
    return !mSavePath.empty() && CVarGetInteger(kCVarPauseSave, 1) != 0 && Game::SaveState::CanSave();
}

const SavePoint* Port::SavePointHere() const {
    using namespace Game::Vars;
    if (mSavePath.empty() || !Playing() || (uint8_t)(EndTimerHi + 1) != 0) {
        return nullptr; // (not during the escape, like saving from the pause screen)
    }
    int mx = 0, my = 0;
    if (!Game::Explored::SamusScreen(mx, my)) {
        return nullptr;
    }
    int feet = ObjectY + ObjRadY;
    for (const SavePoint& p : SavePoints()) {
        if (p.bank == Game::Hw::gPrgBank && p.mapX == mx && p.mapY == my &&
            std::abs((int)ObjectX - p.x) <= kSavePointHalfWidth && std::abs(feet - p.y) <= kSavePointFloorSlack) {
            return &p;
        }
    }
    return nullptr;
}

bool Port::SaveGame() {
    return CanSave() && WriteSave();
}

bool Port::WriteSave() {
    std::string data = Game::SaveState::Capture();
    std::string tmp = mSavePath + ".tmp";
    {
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        if (!f.write(data.data(), (std::streamsize)data.size())) {
            SPDLOG_ERROR("Couldn't write {}", tmp);
            return false;
        }
    }
    std::error_code ec;
    std::filesystem::rename(tmp, mSavePath, ec); // replaces the old save only once the new one is whole
    if (ec) {
        SPDLOG_ERROR("Couldn't save to {}: {}", mSavePath, ec.message());
        return false;
    }
    mSavedAt = (int64_t)mFrame;
    SPDLOG_INFO("Saved the game to {}", mSavePath);
    return true;
}

bool Port::HasSave() const {
    std::error_code ec;
    return !mSavePath.empty() && std::filesystem::is_regular_file(mSavePath, ec);
}

bool Port::LoadGame() {
    std::ifstream f(mSavePath, std::ios::binary);
    std::string data((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (!f.good() && !f.eof()) {
        return false;
    }
    if (!Game::SaveState::Restore(data)) {
        SPDLOG_ERROR("{} is not a saved game", mSavePath);
        return false;
    }
    mApu.Reset();
    ResetView();
    mLagTicks = 0;
    mSavedAt = -1;
    // Buttons held now (e.g. the Start that chose CONTINUE) count as held already, not as new presses. The
    // game keeps them as the NES reads them: A in bit 7 ... Right in bit 0.
    uint8_t held = 0;
    for (int i = 0; i < 8; i++) {
        if (mButtons & (1 << i)) {
            held |= (uint8_t)(0x80 >> i);
        }
    }
    Game::Vars::Joy1Status = held;
    Game::Vars::Joy1Change = 0;
    SPDLOG_INFO("Loaded the game from {}", mSavePath);
    return true;
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
    RenderPicture(Game::Hw::gVideo, out, Game::Options::gNoFlicker ? 64 : 8);
}

} // namespace Metroid
