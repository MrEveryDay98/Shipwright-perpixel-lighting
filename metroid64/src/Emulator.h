#pragma once

#include "nes/Nes.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace Metroid {

// Which part of the game drew an OAM entry. Found by looking at which main-loop routine is on the
// stack when the game writes its sprite buffer at $0200, so it works for any sprite the game draws.
enum class SpriteOwner : uint8_t {
    None,       // unused entry (y >= $EF)
    Samus,      // $CC0D: Samus herself
    Enemy,      // $F345: enemies; slot is the enemy's RAM offset ($4B)
    Projectile, // $D4BF: beams, missiles, bombs
    Hud,        // $E0C1: energy, missiles, energy tanks
    Scenery,    // $8B79 (area bank): door bubbles
    Other,      // anything else (doors, intro, effects)
};

struct SpriteTag {
    SpriteOwner owner = SpriteOwner::None;
    uint8_t slot = 0;
};

// RAM addresses (Metroid (USA)), confirmed by running the game.
namespace Ram {
constexpr uint16_t kTitleRoutine = 0x1D; // 0 while the game engine runs
constexpr uint16_t kMainRoutine = 0x1E;  // 3 = playing, 8 = Samus appearing
constexpr uint16_t kSamusDir = 0x4D;     // 0 right, 1 left
constexpr uint16_t kSamusRadY = 0x0301; // half height
constexpr uint16_t kSamusAnimFrame = 0x0303;
constexpr uint16_t kSamusAnimReset = 0x0305; // animation offset into the object animation table ($8572)
constexpr uint16_t kSamusHi = 0x030C;        // bit 0: which nametable Samus is in
constexpr uint16_t kSamusY = 0x030D;
constexpr uint16_t kSamusX = 0x030E;
constexpr uint16_t kEnergy = 0x0106; // BCD: low byte, high byte
} // namespace Ram

// Metroid running on the emulated NES, plus the bookkeeping the 3D presentation needs.
class Emulator : public Nes::Machine {
  public:
    // Reads and checks the user's ROM (PRG CRC32 of Metroid (USA)).
    bool LoadRomFile(const std::string& path, std::string& error);
    void PowerOn();

    // Runs one 60 Hz frame with these NES buttons held.
    void Tick(uint8_t buttons);

    void Write(uint16_t addr, uint8_t value) override;

    bool Loaded() const {
        return mLoaded;
    }
    // True while the game engine draws the play field (as opposed to title, password, ending screens).
    bool InGameplay() const;
    // The area bank mapped at $8000 (1 Brinstar, 2 Norfair, 3 Tourian, 4 Kraid, 5 Ridley).
    uint8_t AreaBank() const;

    // OAM ownership, for the frame on screen.
    const std::array<SpriteTag, 64>& SpriteTags() const {
        return mOamTags;
    }

    // Scroll of the frame on screen, as a position in the 512x480 nametable plane.
    int PlaneScrollX() const {
        return mPlaneX;
    }
    int PlaneScrollY() const {
        return mPlaneY;
    }
    // Unwrapped scroll, so the camera can move continuously. Previous values are for interpolation.
    double WorldScrollX() const {
        return mWorldX;
    }
    double WorldScrollY() const {
        return mWorldY;
    }
    double PrevWorldScrollX() const {
        return mPrevWorldX;
    }
    double PrevWorldScrollY() const {
        return mPrevWorldY;
    }

    // Samus's position (her centre) relative to the top-left of the screen, from RAM.
    float SamusScreenX() const;
    float SamusScreenY() const;
    bool SamusFacingLeft() const {
        return Peek(Ram::kSamusDir) & 1;
    }

  private:
    SpriteTag TagForCurrentWrite() const;

    bool mLoaded = false;
    std::array<SpriteTag, 64> mBufferTags{};
    std::array<SpriteTag, 64> mOamTags{};
    int mPlaneX = 0, mPlaneY = 0;
    double mWorldX = 0, mWorldY = 0, mPrevWorldX = 0, mPrevWorldY = 0;
    bool mHaveScroll = false;
};

} // namespace Metroid
