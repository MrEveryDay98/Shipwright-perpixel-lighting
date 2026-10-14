#pragma once

// What the renderer and audio output see of the running game: picture memory, sprites and who drew
// them, Samus and the scroll. Implemented by the native game (Port).

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

namespace Metroid {

// Controller bits in the order the NES shifts them out.
enum Button : uint8_t {
    BtnA = 0x01,
    BtnB = 0x02,
    BtnSelect = 0x04,
    BtnStart = 0x08,
    BtnUp = 0x10,
    BtnDown = 0x20,
    BtnLeft = 0x40,
    BtnRight = 0x80,
};

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
constexpr uint16_t kMainRoutine = 0x1E;  // 3 playing, 5 paused, 8 Samus appearing, 9 timer
constexpr uint16_t kSamusDir = 0x4D;     // 0 right, 1 left
constexpr uint16_t kSamusRadY = 0x0301;  // half height
constexpr uint16_t kSamusAnimFrame = 0x0303;
constexpr uint16_t kSamusAnimReset = 0x0305; // animation offset into the object animation table ($8572)
constexpr uint16_t kSamusHi = 0x030C;        // bit 0: which nametable Samus is in
constexpr uint16_t kSamusY = 0x030D;
constexpr uint16_t kSamusX = 0x030E;
constexpr uint16_t kEnergy = 0x0106; // BCD: low byte, high byte
constexpr uint16_t kSamusGear = 0x6878; // power-ups Samus has; bit 5 the Varia
} // namespace Ram

// The world-map room drawn in one physical name table page (256x240), for per-room custom geometry.
struct RoomInfo {
    bool valid = false;
    uint8_t bank = 0; // area bank: 1 Brinstar, 2 Norfair, 3 Tourian, 4 Kraid, 5 Ridley
    uint8_t mapX = 0, mapY = 0;
    uint8_t room = 0;
};

class GameView {
  public:
    virtual ~GameView() = default;

    // Loads the game's data (resources made from the player's ROM).
    virtual bool LoadGameData(std::string& error) = 0;
    virtual void PowerOn() = 0;
    // Runs one 60 Hz frame with these NES buttons held (bit 0 A ... bit 7 Right).
    virtual void Tick(uint8_t buttons) = 0;

    // Picture memory.
    virtual const uint8_t* Chr() const = 0;     // $0000-$1FFF
    virtual const uint8_t* Palette() const = 0; // 32 bytes
    virtual const uint8_t* Oam() const = 0;     // 256 bytes
    virtual uint8_t PpuCtrl() const = 0;
    virtual uint8_t PpuMask() const = 0;
    // 0 one-screen low, 1 one-screen high, 2 vertical, 3 horizontal.
    virtual uint8_t Mirroring() const = 0;
    // A nametable byte ($2000-$2FFF, mirrored).
    virtual uint8_t PpuPeek(uint16_t addr) const = 0;
    // The game's RAM and cartridge RAM.
    virtual uint8_t Peek(uint16_t addr) const = 0;
    virtual uint64_t FrameCount() const = 0;
    // The picture as a television would show it, 0xRRGGBBAA pixels (256x240).
    virtual void RenderFrame(uint32_t* out) const = 0;
    // Audio produced so far (signed 16-bit mono at 44100 Hz). The caller drains it.
    virtual std::vector<int16_t>& AudioSamples() = 0;
    // The room drawn in name table page 0 or 1.
    virtual RoomInfo RoomOnPage(int page) const {
        (void)page;
        return {};
    }

    // OAM ownership, for the frame on screen.
    const std::array<SpriteTag, 64>& SpriteTags() const {
        return mOamTags;
    }
    // True while the game engine draws the play field (as opposed to title, password, ending screens).
    bool InGameplay() const;

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

  protected:
    // Called after each frame with the scroll at the top of the picture, in the nametable plane.
    void TrackScroll(int x, int y);
    void ResetView();

    std::array<SpriteTag, 64> mOamTags{};

  private:
    int mPlaneX = 0, mPlaneY = 0;
    double mWorldX = 0, mWorldY = 0, mPrevWorldX = 0, mPrevWorldY = 0;
    bool mHaveScroll = false;
};

} // namespace Metroid
