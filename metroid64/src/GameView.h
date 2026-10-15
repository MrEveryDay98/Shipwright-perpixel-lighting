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
    Item,       // power-ups, the elevator, the statues
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
constexpr uint16_t kEnergy = 0x0106;    // BCD: low byte, high byte
constexpr uint16_t kSamusGear = 0x6878; // power-ups Samus has; bit 5 the Varia
constexpr uint16_t kScrollDir = 0x49;   // 0 up, 1 down, 2 left, 3 right
constexpr uint16_t kMapPosY = 0x4F;
constexpr uint16_t kMapPosX = 0x50;
constexpr uint16_t kDoorStatus = 0x56; // 0 not in a door; 5 coming out of one
} // namespace Ram

// The world-map room drawn in one physical name table page (256x240), for per-room custom geometry.
struct RoomInfo {
    bool valid = false;
    uint8_t bank = 0; // area bank: 1 Brinstar, 2 Norfair, 3 Tourian, 4 Kraid, 5 Ridley
    uint8_t mapX = 0, mapY = 0;
    uint8_t room = 0;
    bool complete = false; // the whole room is in room RAM ($6000 for page 0, $6400 for page 1)
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
    // The area bank mapped in (1 Brinstar, 2 Norfair, 3 Tourian, 4 Kraid, 5 Ridley; 0 title).
    virtual int AreaBank() const {
        return 0;
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
    // How far from the current scroll to draw the camera between game frames (alpha 0-1 from the last
    // game frame to the next): between the averages of the last three game frames' scroll, two by two.
    // The game scrolls in uneven steps (1, 2, 1, 2 pixels while running); averaging pairs evens them out,
    // half a frame late. Objects are drawn the same way (Motion), so they keep their place on screen.
    double SmoothScrollOffsetX(float alpha) const;
    double SmoothScrollOffsetY(float alpha) const;
    double PrevWorldScrollY() const {
        return mPrevWorldY;
    }

    // Where a position in the game's name table plane (name table bit, x, y) is, relative to the top-left of
    // the screen.
    float ScreenXOf(uint8_t hi, uint8_t x) const;
    float ScreenYOf(uint8_t hi, uint8_t y) const;
    // Samus's position (her centre) relative to the top-left of the screen, from RAM.
    float SamusScreenX() const;
    float SamusScreenY() const;
    bool SamusFacingLeft() const {
        return Peek(Ram::kSamusDir) & 1;
    }

    // An object's position at the last two game frames, in the same unwrapped space as the scroll, so it can
    // be drawn between them the way the camera is.
    struct Motion {
        float prev2X = 0, prev2Y = 0, prevX = 0, prevY = 0, x = 0, y = 0;
        int have = 0; // frames in a row it was there without a jump (up to 3)
        void Set(bool present, float nx, float ny, bool cut);
        // How far from where the game has it now to draw it (like SmoothScrollOffset).
        float OffsetX(float alpha) const;
        float OffsetY(float alpha) const;
    };
    // Where Samus is in the picture on screen (relative to its top-left): her position of the game frame
    // the picture comes from (a frame behind RAM).
    float SamusDrawX() const;
    float SamusDrawY() const;
    const Motion& SamusMotion() const {
        return mSamusMotion;
    }
    // Enemy slots $00-$50 by index 0-5.
    const Motion& EnemyMotion(int i) const {
        return mEnemyMotion[i];
    }

  protected:
    // Called after each frame with the scroll at the top of the picture, in the nametable plane; then the
    // objects' motion is recorded with it (TrackObjects).
    void TrackScroll(int x, int y);
    void TrackObjects();
    void ResetView();

    std::array<SpriteTag, 64> mOamTags{};

  private:
    int mPlaneX = 0, mPlaneY = 0;
    double mWorldX = 0, mWorldY = 0, mPrevWorldX = 0, mPrevWorldY = 0, mPrev2WorldX = 0, mPrev2WorldY = 0;
    bool mHaveScroll = false;
    bool mScrollCut = false;
    Motion mSamusMotion;
    Motion mEnemyMotion[6];
    // Positions read from RAM at the last frame, for the next one (name table bit, x, y).
    struct PendingObject {
        bool present = false;
        uint8_t hi = 0, x = 0, y = 0;
    };
    PendingObject mPendingSamus;
    PendingObject mPendingEnemies[6];
};

} // namespace Metroid
