#pragma once

#include "GameView.h"
#include "nes/Nes.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace Metroid {

// Metroid running on the emulated NES, plus the bookkeeping the 3D presentation needs.
class Emulator : public Nes::Machine, public GameView {
  public:
    // Reads and checks the user's ROM (PRG CRC32 of Metroid (USA)).
    bool LoadRomFile(const std::string& path, std::string& error) override;
    void PowerOn() override;

    // Runs one 60 Hz frame with these NES buttons held.
    void Tick(uint8_t buttons) override;

    void Write(uint16_t addr, uint8_t value) override;

    bool Loaded() const {
        return mLoaded;
    }
    // The area bank mapped at $8000 (1 Brinstar, 2 Norfair, 3 Tourian, 4 Kraid, 5 Ridley).
    uint8_t AreaBank() const;

    const uint8_t* Chr() const override {
        return Machine::Chr();
    }
    const uint8_t* Palette() const override {
        return Machine::Palette();
    }
    const uint8_t* Oam() const override {
        return Machine::Oam();
    }
    uint8_t PpuCtrl() const override {
        return Machine::PpuCtrl();
    }
    uint8_t PpuMask() const override {
        return Machine::PpuMask();
    }
    uint8_t Mirroring() const override {
        return Machine::Mirroring();
    }
    uint8_t PpuPeek(uint16_t addr) const override {
        return Machine::PpuPeek(addr);
    }
    uint8_t Peek(uint16_t addr) const override {
        return Machine::Peek(addr);
    }
    uint64_t FrameCount() const override {
        return Machine::FrameCount();
    }
    void RenderFrame(uint32_t* out) const override {
        Machine::RenderFrame(out);
    }
    std::vector<int16_t>& AudioSamples() override {
        return Machine::AudioSamples();
    }

  private:
    SpriteTag TagForCurrentWrite() const;

    bool mLoaded = false;
    std::array<SpriteTag, 64> mBufferTags{};
};

} // namespace Metroid
