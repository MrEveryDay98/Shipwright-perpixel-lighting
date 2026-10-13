#pragma once

#include "Rom.h"

#include <string>
#include <unordered_map>
#include <unordered_set>
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

// Named animation poses. Sprite visuals map these to the ROM's animation tables; model visuals map
// them to animations in a model manifest.
enum class Pose : uint8_t {
    Stand,
    Run,
    Jump,
    Spin,
    Morph,
    RunShoot,
    JumpShoot,
    StandUp,
    RunUp,
    JumpUp,
    Front,
    Move, // enemies
    Count
};
const char* PoseName(Pose pose);

struct Projectile {
    float x, y, vx, vy;
    int life;
    int age = 0;
    bool missile;
    float prevX = 0, prevY = 0;
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

// How an enemy behaves. Assigned per area and enemy type (see Enemy.cpp).
enum class Archetype : uint8_t {
    None,
    Crawler,       // Zoomer, Zeela, Viola, Nova: crawl around surfaces
    Skree,         // hang from the ceiling, dive at Samus and burst
    Swooper,       // Rio, Holtz, Geruta: swoop down from the ceiling and back up
    Ripper,        // fly back and forth; beams can't hurt them
    Reo,           // hover, then dive at Samus
    Hopper,        // Sidehopper / Dessgeega
    CeilingHopper, // the upside-down variant
    PipeBug,       // Zeb, Gamet, Geega, Zebbo: rise from a pipe, then fly at Samus
    Metroid,       // float towards Samus and latch on
    Bouncer,       // Multiviola: bounce around diagonally
    Spitter,       // Dragon: stays put and spits fireballs
    Rinka,         // fly straight at Samus
    Boss,          // Kraid, Ridley
};

struct Enemy {
    int spawn = -1; // index into Rom::Enemies()
    Area area = AREA_NONE;
    uint8_t type = 0;
    bool variant = false;
    Archetype arch = Archetype::None;
    float x = 0, y = 0, prevX = 0, prevY = 0; // centre, world pixels
    float vx = 0, vy = 0;
    float homeX = 0, homeY = 0;
    float halfW = 6, halfH = 6;
    int hp = 1;
    int state = 0, timer = 0;
    int facing = 1;
    int frozen = 0, hurt = 0, ticks = 0;
    // Crawlers: direction of travel and the side the surface is on (unit axis vectors).
    int dirX = 1, dirY = 0, wallX = 0, wallY = 1;
    float angle = 0; // render rotation in degrees
    bool alive = true;
    bool latched = false; // Metroid holding on to Samus
};

struct EnemyShot {
    float x, y, vx, vy;
    float gravity;
    int life;
    int damage;
    float prevX = 0, prevY = 0;
};

struct Pickup {
    float x, y;
    bool missile;
    int life;
};

struct Samus {
    float x = 0, y = 0; // centre of feet, world pixels (y down)
    float prevX = 0, prevY = 0;
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
    int shootTimer = 0;
    int invuln = 0;
    Pose pose = Pose::Stand;
    int poseTicks = 0;

    bool Has(PowerUp p) const {
        return items & (1 << p);
    }
    float Height() const {
        return morph ? 14.0f : 30.0f;
    }
};

// Developer/cheat switches set from the menu.
struct Cheats {
    bool infiniteEnergy = false;
    bool infiniteMissiles = false;
};

class Game {
  public:
    void Init(const Rom* rom);
    void Update(const Input& in);
    Cheats& GetCheats() {
        return mCheats;
    }
    void GiveAllItems();

    const Rom& GetRom() const {
        return *mRom;
    }
    const Samus& GetSamus() const {
        return mSamus;
    }
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
    const std::vector<Enemy>& Enemies() const {
        return mEnemies;
    }
    const std::vector<EnemyShot>& EnemyShots() const {
        return mEnemyShots;
    }
    const std::vector<Pickup>& Pickups() const {
        return mPickups;
    }
    bool TileIntact(int tx, int ty) const;
    bool SolidAt(float x, float y) const;
    // Terrain only (no doors, platforms or frozen enemies); used by crawlers.
    bool TerrainSolidAt(float x, float y) const;
    int FrameCount() const {
        return mFrame;
    }
    // Elevator ride / death screen fade (0 = none, 1 = black).
    float Fade() const {
        return mFade;
    }
    bool Riding() const {
        return mRide >= 0;
    }
    bool Paused() const {
        return mPaused;
    }
    void SetPaused(bool paused) {
        mPaused = paused;
    }
    const std::string& Message() const {
        return mMessage;
    }
    int MessageTimer() const {
        return mMessageTimer;
    }
    const char* AreaName() const;

  private:
    friend class EnemyLogic;

    bool BoxBlocked(float x0, float y0, float x1, float y1) const;
    void MoveSamus(const Input& in);
    void UpdatePose();
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
    void UpdateDeath();
    void Say(const std::string& msg);
    void Spawn(float x, float y, uint32_t color, int life);
    void HurtSamus(int damage, float fromX);
    void PlaceSamusAt(int screenX, int screenY);

    const Rom* mRom = nullptr;
    Samus mSamus;
    Cheats mCheats;
    std::vector<Projectile> mShots;
    std::vector<Bomb> mBombs;
    std::vector<Effect> mEffects;
    std::vector<DoorState> mDoors;
    std::vector<ItemState> mItems;
    std::vector<ElevatorState> mElevators;
    std::vector<Enemy> mEnemies;
    std::vector<EnemyShot> mEnemyShots;
    std::vector<Pickup> mPickups;
    std::vector<int> mSpawnActive;           // per Rom spawn: index into mEnemies or -1
    std::vector<int> mSpawnCooldown;         // per Rom spawn: ticks before a pipe/killed enemy can return
    std::unordered_set<int> mBossesDefeated; // Rom spawn indices
    std::unordered_map<int, int> mBlasted;   // world tile index -> frames until it regrows
    int mFrame = 0;
    bool mPaused = false;
    std::string mMessage;
    int mMessageTimer = 0;

    // Respawn point (last elevator arrival or the start)
    int mCheckpointX = 0, mCheckpointY = 0;
    int mDeathTimer = 0;

    // Elevator ride
    int mRide = -1;     // index into mElevators while riding
    int mRideDest = -1; // destination elevator
    int mRideTimer = 0;
    float mFade = 0.0f;
};

} // namespace Metroid
