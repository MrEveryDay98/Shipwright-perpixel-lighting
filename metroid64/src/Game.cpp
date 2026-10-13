#include "Game.h"
#include "Enemy.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace Metroid {

namespace {

// Movement tuning, in NES pixels per 60 Hz frame.
constexpr float kRunSpeed = 1.5f;
constexpr float kRunAccel = 0.25f;
constexpr float kAirAccel = 0.12f;
constexpr float kGravity = 0.17f;
constexpr float kJumpSpeed = 5.35f;    // ~84 px apex, enough to clear the outcrop left of the start
constexpr float kHighJumpSpeed = 6.6f; // ~128 px apex
constexpr float kJumpCutSpeed = -1.0f;
constexpr float kMaxFall = 5.0f;
constexpr float kHalfWidth = 6.0f;
constexpr float kBallHalfWidth = 6.0f;

constexpr int kBeamSpeed = 5;
constexpr int kShortBeamLife = 14;
constexpr int kLongBeamLife = 48;
constexpr int kMissileLife = 60;
constexpr int kMaxShots = 3;
constexpr int kBombFuse = 40;
constexpr int kRegrowFrames = 300;
constexpr int kRideFrames = 90;

const char* const kItemNames[PU_COUNT] = {
    "BOMBS", "HIGH JUMP", "LONG BEAM", "SCREW ATTACK", "MARU MARI",
    "VARIA", "WAVE BEAM", "ICE BEAM",  "ENERGY TANK",  "MISSILES",
};

int TileIndex(int tx, int ty) {
    return ty * kWorldTilesW + tx;
}

} // namespace

const char* PoseName(Pose pose) {
    static const char* const names[(int)Pose::Count] = {
        "Stand", "Run", "Jump", "Spin", "Morph", "RunShoot", "JumpShoot", "StandUp", "RunUp", "JumpUp", "Front", "Move",
    };
    return names[(int)pose];
}

void Game::Init(const Rom* rom) {
    mRom = rom;
    mSamus = Samus{};
    mEnemies.clear();
    mEnemyShots.clear();
    mPickups.clear();
    mSpawnActive.assign(rom->Enemies().size(), -1);
    mSpawnCooldown.assign(rom->Enemies().size(), 0);
    mBossesDefeated.clear();
    mDeathTimer = 0;
    mPaused = false;
    mShots.clear();
    mBombs.clear();
    mEffects.clear();
    mBlasted.clear();
    mFrame = 0;
    mRide = -1;
    mFade = 0;

    mDoors.clear();
    for (const Door& d : rom->Doors()) {
        DoorState s;
        s.door = d;
        s.hitsLeft = d.type == DOOR_RED ? 5 : d.type == DOOR_ORANGE ? 10 : 1;
        // The door bubble sits in the 16 px door-frame column at the screen edge, macro rows 5-7.
        int sx = d.screenX * kScreenPxW, sy = d.screenY * kScreenPxH;
        s.x0 = d.rightSide ? sx + kScreenPxW - 8 : sx;
        s.x1 = s.x0 + 8;
        s.y0 = sy + 80;
        s.y1 = sy + 128;
        mDoors.push_back(s);
    }

    mItems.clear();
    for (const Item& it : rom->Items()) {
        mItems.push_back({ it, false });
    }

    mElevators.clear();
    for (const Elevator& e : rom->Elevators()) {
        ElevatorState s;
        s.elevator = e;
        int sx = e.screenX * kScreenPxW, sy = e.screenY * kScreenPxH;
        s.platX0 = sx + 112;
        s.platX1 = sx + 144;
        // Arrival rooms have a floor under the elevator; departure rooms have an open shaft, where
        // the elevator rests level with the floor beside the shaft.
        s.platY = sy + 128;
        bool found = false;
        for (int column : { sx + 128, sx + 80 }) {
            for (int row = 4; row < 15 && !found; row++) {
                if (rom->Tile(column / 8, (sy + row * 16) / 8).kind == TILE_SOLID) {
                    s.platY = sy + row * 16;
                    found = true;
                }
            }
        }
        mElevators.push_back(s);
    }

    // Start like the original: dropped into the start screen of Brinstar.
    int startX = rom->StartScreenX(), startY = rom->StartScreenY();
    // Developer warp: METROID64_WARP=x,y starts in that map screen instead.
    if (const char* warp = getenv("METROID64_WARP")) {
        sscanf(warp, "%d,%d", &startX, &startY);
    }
    mCheckpointX = startX;
    mCheckpointY = startY;
    PlaceSamusAt(startX, startY);
    Say("EMERGENCY ORDER: DEFEAT THE METROID OF THE PLANET ZEBETH");
    // Developer switch: METROID64_ALLITEMS=1 starts with every upgrade.
    if (getenv("METROID64_ALLITEMS")) {
        GiveAllItems();
    }
}

void Game::PlaceSamusAt(int screenX, int screenY) {
    mSamus.x = screenX * kScreenPxW + 128;
    mSamus.y = screenY * kScreenPxH + 64;
    for (int i = 0; i < 160 && !SolidAt(mSamus.x, mSamus.y + 1); i++) {
        mSamus.y += 1;
    }
    mSamus.prevX = mSamus.x;
    mSamus.prevY = mSamus.y;
    mSamus.vx = mSamus.vy = 0;
    mSamus.morph = false;
}

void Game::GiveAllItems() {
    for (int i = 0; i < PU_COUNT; i++) {
        if (i != PU_ICE_BEAM && i != PU_ENERGY_TANK && i != PU_MISSILES) {
            mSamus.items |= 1 << i;
        }
    }
    mSamus.maxEnergy = 699;
    mSamus.energy = mSamus.maxEnergy;
    mSamus.maxMissiles = mSamus.missiles = 255;
    Say("ALL ITEMS");
}

const char* Game::AreaName() const {
    static const char* const names[AREA_COUNT] = {
        "", "BRINSTAR", "NORFAIR", "TOURIAN", "KRAID'S LAIR", "RIDLEY'S LAIR"
    };
    int sx = (int)mSamus.x / kScreenPxW, sy = (int)(mSamus.y - 8) / kScreenPxH;
    return names[mRom->ScreenArea(sx, sy)];
}

void Game::Say(const std::string& msg) {
    mMessage = msg;
    mMessageTimer = 180;
}

void Game::Spawn(float x, float y, uint32_t color, int life) {
    mEffects.push_back({ x, y, 0, life, color });
}

bool Game::TileIntact(int tx, int ty) const {
    return mBlasted.find(TileIndex(tx, ty)) == mBlasted.end();
}

bool Game::TerrainSolidAt(float x, float y) const {
    if (x < 0 || y < 0 || x >= kWorldTilesW * 8 || y >= kWorldTilesH * 8) {
        return true;
    }
    int tx = (int)x >> 3, ty = (int)y >> 3;
    const WorldTile& t = mRom->Tile(tx, ty);
    return t.kind == TILE_SOLID || (t.kind == TILE_BLAST && TileIntact(tx, ty));
}

bool Game::SolidAt(float x, float y) const {
    if (TerrainSolidAt(x, y)) {
        return true;
    }
    // Frozen enemies are platforms, as in the original.
    for (const Enemy& e : mEnemies) {
        if (e.alive && e.frozen > 0 && x >= e.x - e.halfW && x < e.x + e.halfW && y >= e.y - e.halfH &&
            y < e.y + e.halfH) {
            return true;
        }
    }
    for (const DoorState& d : mDoors) {
        if (!d.open && x >= d.x0 && x < d.x1 && y >= d.y0 && y < d.y1) {
            return true;
        }
    }
    for (const ElevatorState& e : mElevators) {
        if (x >= e.platX0 && x < e.platX1 && y >= e.platY && y < e.platY + 8) {
            return true;
        }
    }
    return false;
}

bool Game::BoxBlocked(float x0, float y0, float x1, float y1) const {
    for (float y = y0; y < y1; y += 4.0f) {
        for (float x = x0; x < x1; x += 4.0f) {
            if (SolidAt(x, y)) {
                return true;
            }
        }
        if (SolidAt(x1 - 0.01f, y)) {
            return true;
        }
    }
    for (float x = x0; x < x1; x += 4.0f) {
        if (SolidAt(x, y1 - 0.01f)) {
            return true;
        }
    }
    return SolidAt(x1 - 0.01f, y1 - 0.01f);
}

void Game::Update(const Input& in) {
    // Positions at the start of the tick, for render interpolation.
    mSamus.prevX = mSamus.x;
    mSamus.prevY = mSamus.y;
    for (Enemy& e : mEnemies) {
        e.prevX = e.x;
        e.prevY = e.y;
    }
    for (Projectile& p : mShots) {
        p.prevX = p.x;
        p.prevY = p.y;
    }
    for (EnemyShot& p : mEnemyShots) {
        p.prevX = p.x;
        p.prevY = p.y;
    }

    if (in.startPressed && mRide < 0 && mDeathTimer == 0) {
        mPaused = !mPaused;
    }
    if (mPaused) {
        return;
    }
    mFrame++;
    if (mMessageTimer > 0) {
        mMessageTimer--;
    }
    if (mCheats.infiniteEnergy) {
        mSamus.energy = std::max(mSamus.energy, 99);
    }
    if (mCheats.infiniteMissiles && mSamus.maxMissiles > 0) {
        mSamus.missiles = mSamus.maxMissiles;
    }
    if (mDeathTimer > 0) {
        UpdateDeath();
        return;
    }
    if (mRide >= 0) {
        UpdateRide();
        UpdatePose();
        return;
    }
    if (mSamus.invuln > 0) {
        mSamus.invuln--;
    }

    if (in.toggleMissilePressed) {
        if (mSamus.maxMissiles > 0) {
            mSamus.missileMode = !mSamus.missileMode;
        }
    }
    if (mSamus.missiles == 0) {
        mSamus.missileMode = false;
    }

    MoveSamus(in);
    if (in.shootPressed) {
        if (mSamus.morph) {
            if (mSamus.Has(PU_BOMBS) && mBombs.size() < 3) {
                mBombs.push_back({ mSamus.x, mSamus.y - 7, kBombFuse });
            }
        } else {
            Shoot();
        }
    }
    UpdateShots();
    UpdateBombs();
    UpdateBlasted();
    UpdateDoors();
    EnemyLogic::Update(*this);
    CheckItems();
    CheckElevators(in);
    UpdatePose();
    if (mSamus.energy <= 0 && mDeathTimer == 0) {
        mDeathTimer = 1;
        mShots.clear();
        mBombs.clear();
        Spawn(mSamus.x, mSamus.y - 16, 0xFFFFFFFF, 40);
    }

    for (Effect& e : mEffects) {
        e.age++;
    }
    mEffects.erase(std::remove_if(mEffects.begin(), mEffects.end(), [](const Effect& e) { return e.age >= e.life; }),
                   mEffects.end());
}

void Game::MoveSamus(const Input& in) {
    Samus& s = mSamus;
    if (s.shootTimer > 0) {
        s.shootTimer--;
    }

    // Morph / unmorph
    if (in.down && !s.morph && s.grounded && s.Has(PU_MORPH_BALL)) {
        s.morph = true;
        s.spinning = false;
    } else if ((in.up || in.jumpPressed) && s.morph) {
        if (!BoxBlocked(s.x - kHalfWidth, s.y - 30, s.x + kHalfWidth, s.y - 14)) {
            s.morph = false;
        }
    }
    s.aimUp = in.up && !s.morph;

    // Horizontal (no steering for a moment after being knocked back)
    float target = 0;
    if (s.invuln > 45) {
        target = s.vx;
    } else if (in.left) {
        target = -kRunSpeed;
        s.facing = -1;
    } else if (in.right) {
        target = kRunSpeed;
        s.facing = 1;
    }
    float accel = s.grounded ? kRunAccel : kAirAccel;
    if (s.vx < target) {
        s.vx = std::min(s.vx + accel, target);
    } else if (s.vx > target) {
        s.vx = std::max(s.vx - accel, target);
    }

    // Jump
    if (in.jumpPressed && s.grounded && !s.morph) {
        s.vy = -(s.Has(PU_HIGH_JUMP) ? kHighJumpSpeed : kJumpSpeed);
        s.grounded = false;
        s.spinning = std::fabs(s.vx) > 0.5f;
    }
    if (!in.jump && s.vy < kJumpCutSpeed) {
        s.vy = kJumpCutSpeed;
    }
    s.vy = std::min(s.vy + kGravity, kMaxFall);

    float hw = s.morph ? kBallHalfWidth : kHalfWidth;
    float h = s.Height();

    // Move X in small steps so we never tunnel through 8 px tiles.
    float dx = s.vx;
    while (dx != 0) {
        float step = std::clamp(dx, -2.0f, 2.0f);
        if (!BoxBlocked(s.x + step - hw, s.y - h, s.x + step + hw, s.y)) {
            s.x += step;
            dx -= step;
        } else {
            // Step up one-pixel ledges smoothly (keeps rolling the ball over seams).
            if (s.grounded && !BoxBlocked(s.x + step - hw, s.y - h - 2, s.x + step + hw, s.y - 2)) {
                s.y -= 2;
                s.x += step;
                dx -= step;
                continue;
            }
            s.vx = 0;
            break;
        }
    }

    // Move Y
    float dy = s.vy;
    bool landed = false;
    while (dy != 0) {
        float step = std::clamp(dy, -2.0f, 2.0f);
        if (!BoxBlocked(s.x - hw, s.y + step - h, s.x + hw, s.y + step)) {
            s.y += step;
            dy -= step;
        } else {
            if (step > 0) {
                landed = true;
                s.y = std::floor(s.y);
                while (!BoxBlocked(s.x - hw, s.y + 1 - h, s.x + hw, s.y + 1)) {
                    s.y += 1;
                }
            }
            s.vy = 0;
            break;
        }
    }
    s.grounded = landed || BoxBlocked(s.x - hw, s.y + 1 - h, s.x + hw, s.y + 1);
    if (s.grounded) {
        s.spinning = false;
    }
}

void Game::UpdatePose() {
    Samus& s = mSamus;
    Pose pose;
    bool moving = std::fabs(s.vx) > 0.2f;
    if (mRide >= 0) {
        pose = Pose::Front;
    } else if (s.morph) {
        pose = Pose::Morph;
    } else if (!s.grounded) {
        if (s.spinning && s.shootTimer == 0) {
            pose = Pose::Spin;
        } else if (s.aimUp) {
            pose = Pose::JumpUp;
        } else {
            pose = s.shootTimer > 0 ? Pose::JumpShoot : Pose::Jump;
        }
    } else if (moving) {
        pose = s.aimUp ? Pose::RunUp : (s.shootTimer > 0 ? Pose::RunShoot : Pose::Run);
    } else {
        pose = s.aimUp ? Pose::StandUp : Pose::Stand;
    }
    // Run/shoot variants share timing so the legs don't skip when firing.
    bool sameFamily = (pose == Pose::Run || pose == Pose::RunShoot || pose == Pose::RunUp) &&
                      (s.pose == Pose::Run || s.pose == Pose::RunShoot || s.pose == Pose::RunUp);
    if (pose != s.pose && !sameFamily) {
        s.poseTicks = 0;
    }
    s.pose = pose;
    // The ball only rolls while it moves.
    if (pose != Pose::Morph || moving) {
        s.poseTicks++;
    }
}

void Game::HurtSamus(int damage, float fromX) {
    Samus& s = mSamus;
    if (s.invuln > 0 || mDeathTimer > 0 || mRide >= 0) {
        return;
    }
    if (s.Has(PU_VARIA)) {
        damage = (damage + 1) / 2;
    }
    s.energy -= damage;
    s.invuln = 60;
    s.vx = (s.x < fromX ? -1.5f : 1.5f);
    if (!s.morph) {
        s.vy = -2.0f;
        s.grounded = false;
    }
}

void Game::UpdateDeath() {
    mDeathTimer++;
    mFade = std::min(1.0f, mDeathTimer / 60.0f);
    if (mDeathTimer == 30) {
        Say("SAMUS WAS DEFEATED");
    }
    if (mDeathTimer >= 120) {
        mDeathTimer = 0;
        mFade = 0;
        mSamus.energy = 30;
        mSamus.invuln = 90;
        mEnemies.clear();
        mEnemyShots.clear();
        std::fill(mSpawnActive.begin(), mSpawnActive.end(), -1);
        PlaceSamusAt(mCheckpointX, mCheckpointY);
        Say(AreaName());
    }
}

void Game::Shoot() {
    Samus& s = mSamus;
    if ((int)mShots.size() >= kMaxShots) {
        return;
    }
    bool missile = s.missileMode && s.missiles > 0;
    if (missile) {
        s.missiles--;
    }
    Projectile p{};
    p.missile = missile;
    p.age = 0;
    float speed = missile ? 5.0f : (float)kBeamSpeed;
    if (s.aimUp) {
        p.x = s.x + s.facing * 2;
        p.y = s.y - 34;
        p.vx = 0;
        p.vy = -speed;
    } else {
        p.x = s.x + s.facing * 10;
        p.y = s.y - 21;
        p.vx = s.facing * speed;
        p.vy = 0;
    }
    p.life = missile ? kMissileLife : (s.Has(PU_LONG_BEAM) || s.Has(PU_WAVE_BEAM) ? kLongBeamLife : kShortBeamLife);
    p.prevX = p.x;
    p.prevY = p.y;
    mShots.push_back(p);
    s.shootTimer = 12;
}

void Game::Blast(int tx, int ty) {
    if (tx < 0 || ty < 0 || tx >= kWorldTilesW || ty >= kWorldTilesH) {
        return;
    }
    if (mRom->Tile(tx, ty).kind != TILE_BLAST || !TileIntact(tx, ty)) {
        return;
    }
    mBlasted[TileIndex(tx, ty)] = kRegrowFrames;
    Spawn(tx * 8 + 4.0f, ty * 8 + 4.0f, 0xFFB040FF, 14);
}

void Game::HitDoors(const Projectile& p, bool& consumed) {
    for (size_t i = 0; i < mDoors.size(); i++) {
        DoorState& d = mDoors[i];
        if (d.open || p.x < d.x0 - 2 || p.x >= d.x1 + 2 || p.y < d.y0 || p.y >= d.y1) {
            continue;
        }
        consumed = true;
        bool counts = d.door.type == DOOR_BLUE || d.door.type == DOOR_TOURIAN || p.missile;
        if (counts && --d.hitsLeft <= 0) {
            d.open = true;
            // The neighbouring screen's door shares this doorway; open it too.
            for (DoorState& o : mDoors) {
                if (!o.open && std::abs(o.x0 - d.x0) <= 8 && o.y0 == d.y0) {
                    o.open = true;
                }
            }
            Spawn((d.x0 + d.x1) * 0.5f, (d.y0 + d.y1) * 0.5f, 0x60A0FFFF, 20);
        }
        return;
    }
}

void Game::UpdateShots() {
    for (Projectile& p : mShots) {
        p.age++;
        float wave = 0;
        if (!p.missile && mSamus.Has(PU_WAVE_BEAM)) {
            wave = std::sin(p.age * 0.5f) * 1.6f;
        }
        p.x += p.vx + (p.vx == 0 ? wave : 0);
        p.y += p.vy + (p.vy == 0 ? wave : 0);
        p.life--;
        bool consumed = false;
        HitDoors(p, consumed);
        if (!consumed) {
            EnemyLogic::HitByProjectile(*this, p, consumed);
        }
        if (consumed) {
            p.life = 0;
            continue;
        }
        if (SolidAt(p.x, p.y)) {
            int tx = (int)p.x >> 3, ty = (int)p.y >> 3;
            Blast(tx, ty);
            if (p.missile) {
                Blast(tx + (p.vx > 0 ? 1 : -1), ty);
            }
            Spawn(p.x, p.y, p.missile ? 0xFFFFFFFF : 0xFFE060FF, 8);
            p.life = 0;
        }
    }
    mShots.erase(std::remove_if(mShots.begin(), mShots.end(), [](const Projectile& p) { return p.life <= 0; }),
                 mShots.end());
}

void Game::UpdateBombs() {
    for (Bomb& b : mBombs) {
        if (--b.fuse > 0) {
            continue;
        }
        int tx = (int)b.x >> 3, ty = (int)b.y >> 3;
        for (int dy = -2; dy <= 2; dy++) {
            for (int dx = -2; dx <= 2; dx++) {
                if (std::abs(dx) + std::abs(dy) <= 2) {
                    Blast(tx + dx, ty + dy);
                }
            }
        }
        Projectile fake{ b.x, b.y, 0, 0, 0, 0, false };
        bool consumed = false;
        HitDoors(fake, consumed);
        EnemyLogic::BombBlast(*this, b.x, b.y);
        Spawn(b.x, b.y, 0xFFFFA0FF, 18);
        // Bomb jump
        if (mSamus.morph && std::fabs(mSamus.x - b.x) < 18 && std::fabs(mSamus.y - 7 - b.y) < 18) {
            mSamus.vy = -3.2f;
            mSamus.grounded = false;
        }
    }
    mBombs.erase(std::remove_if(mBombs.begin(), mBombs.end(), [](const Bomb& b) { return b.fuse <= 0; }), mBombs.end());
}

void Game::UpdateBlasted() {
    float hw = kHalfWidth, h = mSamus.Height();
    for (auto it = mBlasted.begin(); it != mBlasted.end();) {
        if (--it->second <= 0) {
            int tx = it->first % kWorldTilesW, ty = it->first / kWorldTilesW;
            bool overlapsSamus =
                tx * 8 < mSamus.x + hw && tx * 8 + 8 > mSamus.x - hw && ty * 8 < mSamus.y && ty * 8 + 8 > mSamus.y - h;
            if (overlapsSamus) {
                it->second = 30;
                ++it;
                continue;
            }
            it = mBlasted.erase(it);
        } else {
            ++it;
        }
    }
}

void Game::UpdateDoors() {
    // Doors close again once Samus is well away from them.
    for (DoorState& d : mDoors) {
        if (d.open && (std::fabs(mSamus.x - d.x0) > 300 || std::fabs(mSamus.y - d.y0) > 260)) {
            d.open = false;
            d.hitsLeft = d.door.type == DOOR_RED ? 5 : d.door.type == DOOR_ORANGE ? 10 : 1;
        }
    }
}

void Game::CheckItems() {
    Samus& s = mSamus;
    for (ItemState& it : mItems) {
        if (it.taken) {
            continue;
        }
        float cx = it.item.worldPxX, cy = it.item.worldPxY;
        if (std::fabs(cx - s.x) < 12 && cy > s.y - s.Height() - 8 && cy < s.y + 8) {
            it.taken = true;
            switch (it.item.type) {
                case PU_ENERGY_TANK:
                    s.maxEnergy += 100;
                    s.energy = s.maxEnergy;
                    break;
                case PU_MISSILES:
                    s.maxMissiles += 5;
                    s.missiles += 5;
                    break;
                case PU_ICE_BEAM:
                    s.items &= ~(1 << PU_WAVE_BEAM);
                    s.items |= 1 << PU_ICE_BEAM;
                    break;
                case PU_WAVE_BEAM:
                    s.items &= ~(1 << PU_ICE_BEAM);
                    s.items |= 1 << PU_WAVE_BEAM;
                    break;
                default:
                    s.items |= 1 << it.item.type;
                    break;
            }
            Say(kItemNames[it.item.type]);
            Spawn(cx, cy, 0xFFFFFFFF, 30);
        }
    }
}

void Game::CheckElevators(const Input& in) {
    Samus& s = mSamus;
    if (!s.grounded || s.morph) {
        return;
    }
    for (size_t i = 0; i < mElevators.size(); i++) {
        const ElevatorState& e = mElevators[i];
        if (s.x < e.platX0 || s.x >= e.platX1 || std::fabs(s.y - e.platY) > 1.5f) {
            continue;
        }
        bool wants = e.elevator.goesUp ? in.up : in.down;
        if (!wants) {
            return;
        }
        // Destination: next elevator in the same map column, in the direction of travel,
        // travelling the other way.
        int best = -1, bestDist = 999;
        for (size_t j = 0; j < mElevators.size(); j++) {
            const Elevator& o = mElevators[j].elevator;
            int dist = (o.screenY - e.elevator.screenY) * (e.elevator.goesUp ? -1 : 1);
            if (j != i && o.screenX == e.elevator.screenX && o.goesUp != e.elevator.goesUp && dist > 0 &&
                dist < bestDist) {
                best = (int)j;
                bestDist = dist;
            }
        }
        if (best < 0) {
            Say(e.elevator.dest == 0x0F ? "THE ESCAPE ELEVATOR IS NOT READY YET" : "THIS ELEVATOR GOES NOWHERE (YET)");
            return;
        }
        mRide = (int)i;
        mRideDest = best;
        mRideTimer = 0;
        s.vx = s.vy = 0;
        mShots.clear();
        return;
    }
}

void Game::UpdateRide() {
    Samus& s = mSamus;
    const ElevatorState& from = mElevators[mRide];
    const ElevatorState& to = mElevators[mRideDest];
    mRideTimer++;
    float dir = from.elevator.goesUp ? -1.0f : 1.0f;
    if (mRideTimer < kRideFrames / 2) {
        s.y += dir * 1.5f;
        mFade = std::min(1.0f, mRideTimer / (kRideFrames * 0.4f));
    } else {
        if (mRideTimer == kRideFrames / 2) {
            s.x = (to.platX0 + to.platX1) * 0.5f;
            s.y = to.platY - dir * (kRideFrames / 2) * 1.5f;
            s.prevX = s.x; // teleport: don't interpolate across the cut
            s.prevY = s.y;
            mEnemies.clear();
            mEnemyShots.clear();
            std::fill(mSpawnActive.begin(), mSpawnActive.end(), -1);
        }
        s.y += dir * 1.5f;
        mFade = std::max(0.0f, 1.0f - (mRideTimer - kRideFrames / 2) / (kRideFrames * 0.4f));
    }
    if (mRideTimer >= kRideFrames) {
        s.y = (float)to.platY;
        s.prevY = s.y;
        s.grounded = true;
        mRide = -1;
        mFade = 0;
        mCheckpointX = to.elevator.screenX;
        mCheckpointY = to.elevator.screenY;
        Say(AreaName());
    }
}

} // namespace Metroid
