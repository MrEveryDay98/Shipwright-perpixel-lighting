#pragma once

#include "Rom.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace Metroid {

// Controller state for one tick, already reduced to what the game needs.
struct Input {
    bool left = false, right = false, up = false, down = false;
    bool jump = false, jumpPressed = false;
    bool shootPressed = false;
    bool toggleMissilePressed = false;
    bool startPressed = false;
};

struct Projectile {
    float x, y, vx, vy;
    int life;
    int age = 0;
    bool missile;
};

struct Bomb {
    float x, y;
    int fuse;
};

struct Effect {
    float x, y;
    int age, life;
    uint32_t color;
};

struct DoorState {
    Door door;
    int hitsLeft;
    bool open = false;
    int x0, y0, x1, y1; // barrier rectangle in world pixels
};

struct ItemState {
    Item item;
    bool taken = false;
};

struct ElevatorState {
    Elevator elevator;
    int platX0, platX1, platY; // standing surface
};

struct Samus {
    float x = 0, y = 0; // centre of feet, world pixels (y down)
    float vx = 0, vy = 0;
    int facing = 1;
    bool grounded = false;
    bool morph = false;
    bool spinning = false;
    bool aimUp = false;
    int energy = 30, maxEnergy = 99;
    int missiles = 0, maxMissiles = 0;
    bool missileMode = false;
    uint16_t items = 0; // PowerUp bits
    int animTimer = 0;
    int shootTimer = 0;

    bool Has(PowerUp p) const {
        return items & (1 << p);
    }
    float Height() const {
        return morph ? 14.0f : 30.0f;
    }
};

class Game {
  public:
    void Init(const Rom* rom);
    void Update(const Input& in);

    const Rom& GetRom() const {
        return *mRom;
    }
    const Samus& GetSamus() const {
        return mSamus;
    }
    int SamusFrame() const;
    const std::vector<Projectile>& Projectiles() const {
        return mShots;
    }
    const std::vector<Bomb>& Bombs() const {
        return mBombs;
    }
    const std::vector<Effect>& Effects() const {
        return mEffects;
    }
    const std::vector<DoorState>& Doors() const {
        return mDoors;
    }
    const std::vector<ItemState>& Items() const {
        return mItems;
    }
    const std::vector<ElevatorState>& Elevators() const {
        return mElevators;
    }
    bool TileIntact(int tx, int ty) const;
    int FrameCount() const {
        return mFrame;
    }
    // Elevator ride / screen fade (0 = none, 1 = black).
    float Fade() const {
        return mFade;
    }
    bool Riding() const {
        return mRide >= 0;
    }
    bool Paused() const {
        return mPaused;
    }
    const std::string& Message() const {
        return mMessage;
    }
    int MessageTimer() const {
        return mMessageTimer;
    }
    const char* AreaName() const;

  private:
    bool SolidAt(float x, float y) const;
    bool BoxBlocked(float x0, float y0, float x1, float y1) const;
    void MoveSamus(const Input& in);
    void Shoot();
    void UpdateShots();
    void UpdateBombs();
    void UpdateBlasted();
    void Blast(int tx, int ty);
    void HitDoors(const Projectile& p, bool& consumed);
    void CheckItems();
    void CheckElevators(const Input& in);
    void UpdateRide();
    void UpdateDoors();
    void Say(const std::string& msg);
    void Spawn(float x, float y, uint32_t color, int life);

    const Rom* mRom = nullptr;
    Samus mSamus;
    std::vector<Projectile> mShots;
    std::vector<Bomb> mBombs;
    std::vector<Effect> mEffects;
    std::vector<DoorState> mDoors;
    std::vector<ItemState> mItems;
    std::vector<ElevatorState> mElevators;
    std::unordered_map<int, int> mBlasted; // world tile index -> frames until it regrows
    int mFrame = 0;
    bool mPaused = false;
    std::string mMessage;
    int mMessageTimer = 0;

    // Elevator ride
    int mRide = -1;     // index into mElevators while riding
    int mRideDest = -1; // destination elevator
    int mRideTimer = 0;
    float mFade = 0.0f;
};

} // namespace Metroid
