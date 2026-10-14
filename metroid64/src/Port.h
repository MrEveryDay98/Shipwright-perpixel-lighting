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

  private:
    Nes::Apu mApu;
    uint8_t mButtons = 0;
    uint64_t mFrame = 0;
    int mLagTicks = 0; // vblanks the frame just run also took
};

} // namespace Metroid
