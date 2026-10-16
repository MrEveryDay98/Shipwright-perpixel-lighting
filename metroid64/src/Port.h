#pragma once

#include "GameView.h"
#include "nes/Apu.h"

#include <string>

namespace Metroid {

// Metroid itself: the native C++ port of the game (game/), with the picture and sound it produces.
class Port : public GameView {
  public:
    Port();

    // Builds the game's data from the loaded resources (metroid.o2r and mods).
    bool LoadGameData(std::string& error) override;
    void PowerOn() override;

    // Runs one 60 Hz frame with these NES buttons held (bit 0 A ... bit 7 Right).
    void Tick(uint8_t buttons) override;

    const uint8_t* Chr() const override;
    const uint8_t* Palette() const override;
    const uint8_t* Oam() const override;
    uint8_t PpuCtrl() const override;
    uint8_t PpuMask() const override;
    uint8_t Mirroring() const override;
    uint8_t PpuPeek(uint16_t addr) const override;
    uint8_t Peek(uint16_t addr) const override;
    uint64_t FrameCount() const override {
        return mFrame;
    }
    void RenderFrame(uint32_t* out) const override;
    RoomInfo RoomOnPage(int page) const override;
    int AreaBank() const override;
    std::vector<int16_t>& AudioSamples() override {
        return mApu.Samples();
    }

    // Saved games (game/SaveState.h), in a file. SaveGame works while the game is paused in play
    // (CanSave); LoadGame puts the game back as it was saved (paused). False if that couldn't be done.
    void SetSavePath(const std::string& path) {
        mSavePath = path;
    }
    // Paused in play (Start).
    bool Paused() const;
    bool CanSave() const;
    bool SaveGame();
    bool HasSave() const;
    bool LoadGame();
    // Ticks since the last save, or -1 if none was made since starting (for the "saved" message).
    int64_t TicksSinceSave() const {
        return mSavedAt < 0 ? -1 : (int64_t)(mFrame - (uint64_t)mSavedAt);
    }

  private:
    Nes::Apu mApu;
    uint8_t mButtons = 0, mPrevButtons = 0;
    std::string mSavePath;
    int64_t mSavedAt = -1;
    uint64_t mFrame = 0;
    int mLagTicks = 0; // vblanks the frame just run also took
};

} // namespace Metroid
