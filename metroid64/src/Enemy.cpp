#include "Enemy.h"

#include <algorithm>
#include <cmath>

namespace Metroid {

namespace {

constexpr float kActivateRangeX = 300.0f;
constexpr float kActivateRangeY = 260.0f;
constexpr float kDespawnRangeX = 420.0f;
constexpr float kDespawnRangeY = 360.0f;
constexpr int kFreezeTicks = 300;
constexpr int kPickupLife = 400;

enum SpawnState { SPAWN_IDLE = -1, SPAWN_ACTIVE = 1, SPAWN_DEAD = 2 };

using A = Archetype;
// Behaviour per area (1-5) and enemy type (low nibble of the spawn type), matched to the sprites
// each area's animation table assigns to that type.
const Archetype kArchetypes[AREA_COUNT][16] = {
    {},
    // Brinstar: Sidehopper, ceiling Sidehopper, Reo, Ripper, Skree, Zoomer, Rio, Zeb, -, -, -, Memu, Memu
    { A::Hopper, A::CeilingHopper, A::Reo, A::Ripper, A::Skree, A::Crawler, A::Swooper, A::PipeBug, A::None, A::None,
      A::None, A::Reo, A::Reo, A::None, A::None, A::None },
    // Norfair: Geruta x2, Ripper II, -, -, -, Nova, Gamet, -, -, -, Squeept, Multiviola, Dragon, Gamet
    { A::Swooper, A::Swooper, A::Ripper, A::None, A::None, A::None, A::Crawler, A::PipeBug, A::None, A::Hopper, A::None,
      A::Hopper, A::Bouncer, A::Spitter, A::PipeBug, A::None },
    // Tourian: Metroid x2, Rinka
    { A::Metroid, A::Metroid, A::Rinka, A::None, A::None, A::None, A::None, A::None, A::None, A::None, A::None, A::None,
      A::None, A::None, A::None, A::None },
    // Kraid's Lair: Sidehopper x2, Reo, Ripper, Skree, Zeela, Geega x2, Kraid
    { A::Hopper, A::CeilingHopper, A::Reo, A::Ripper, A::Skree, A::Crawler, A::PipeBug, A::PipeBug, A::Boss, A::None,
      A::None, A::None, A::None, A::None, A::None, A::None },
    // Ridley's Lair: Holtz x2, Dessgeega x2, -, -, Viola, Zebbo, -, Ridley, -, Holtz, Multiviola
    { A::Swooper, A::Swooper, A::Hopper, A::CeilingHopper, A::None, A::None, A::Crawler, A::PipeBug, A::None, A::Boss,
      A::None, A::Swooper, A::Bouncer, A::None, A::None, A::None },
};

int BaseHp(Archetype a) {
    switch (a) {
        case A::Ripper:
        case A::PipeBug:
        case A::Rinka:
            return 1;
        case A::Crawler:
        case A::Skree:
        case A::Reo:
            return 2;
        case A::Swooper:
            return 3;
        case A::Bouncer:
            return 4;
        case A::Metroid:
            return 5;
        case A::Hopper:
        case A::CeilingHopper:
        case A::Spitter:
            return 6;
        case A::Boss:
            return 60;
        default:
            return 1;
    }
}

int ContactDamage(const Enemy& e) {
    switch (e.arch) {
        case A::Boss:
            return 20;
        case A::Hopper:
        case A::CeilingHopper:
            return 12;
        case A::Metroid:
            return 0; // drains instead
        default:
            return e.variant ? 12 : 8;
    }
}

float Sign(float v) {
    return v < 0 ? -1.0f : 1.0f;
}

uint32_t sRng = 0x1234567;
uint32_t Rand() {
    sRng = sRng * 1103515245u + 12345u;
    return sRng >> 16;
}

} // namespace

Archetype EnemyLogic::ArchetypeFor(Area area, uint8_t type) {
    if (area <= AREA_NONE || area >= AREA_COUNT) {
        return A::None;
    }
    return kArchetypes[area][type & 0x0F];
}

void EnemyLogic::Update(Game& game) {
    Activate(game);
    for (Enemy& e : game.mEnemies) {
        if (!e.alive) {
            continue;
        }
        if (e.hurt > 0) {
            e.hurt--;
        }
        if (e.frozen > 0) {
            e.frozen--;
        } else {
            e.ticks++;
            Think(game, e);
        }
        TouchSamus(game, e);
    }

    // Retire dead and far-away enemies.
    const Samus& s = game.mSamus;
    for (Enemy& e : game.mEnemies) {
        bool far = std::fabs(e.x - s.x) > kDespawnRangeX || std::fabs(e.y - s.y) > kDespawnRangeY;
        bool homeFar = std::fabs(e.homeX - s.x) > kDespawnRangeX || std::fabs(e.homeY - s.y) > kDespawnRangeY;
        if (!e.alive) {
            game.mSpawnActive[e.spawn] = SPAWN_DEAD;
            if (game.GetRom().Enemies()[e.spawn].pipe) {
                game.mSpawnActive[e.spawn] = SPAWN_IDLE;
                game.mSpawnCooldown[e.spawn] = 90;
            }
        } else if (far && homeFar) {
            game.mSpawnActive[e.spawn] = SPAWN_IDLE;
            e.alive = false;
        } else if (e.arch == A::PipeBug && std::fabs(e.x - e.homeX) > 320) {
            game.mSpawnActive[e.spawn] = SPAWN_IDLE;
            game.mSpawnCooldown[e.spawn] = 60;
            e.alive = false;
        }
    }
    game.mEnemies.erase(
        std::remove_if(game.mEnemies.begin(), game.mEnemies.end(), [](const Enemy& e) { return !e.alive; }),
        game.mEnemies.end());

    UpdateShots(game);
    UpdatePickups(game);
}

void EnemyLogic::Activate(Game& game) {
    const Samus& s = game.mSamus;
    const auto& spawns = game.GetRom().Enemies();
    for (size_t i = 0; i < spawns.size(); i++) {
        const EnemySpawn& sp = spawns[i];
        float dx = std::fabs(sp.worldPxX - s.x), dy = std::fabs(sp.worldPxY - s.y);
        if (game.mSpawnCooldown[i] > 0) {
            game.mSpawnCooldown[i]--;
            continue;
        }
        // Killed enemies come back once Samus has left the area, like screens reloading.
        if (game.mSpawnActive[i] == SPAWN_DEAD && (dx > kDespawnRangeX || dy > kDespawnRangeY)) {
            game.mSpawnActive[i] = SPAWN_IDLE;
        }
        if (game.mSpawnActive[i] != SPAWN_IDLE || dx > kActivateRangeX || dy > kActivateRangeY) {
            continue;
        }
        if (game.mBossesDefeated.count((int)i) || ArchetypeFor(sp.area, sp.type) == A::None) {
            continue;
        }
        // Only spawn in the area Samus is in (neighbouring areas share map edges).
        int ssx = (int)s.x / kScreenPxW, ssy = (int)(s.y - 8) / kScreenPxH;
        if (game.GetRom().ScreenArea(ssx, ssy) != sp.area) {
            continue;
        }
        Spawn(game, (int)i);
    }
}

void EnemyLogic::Spawn(Game& game, int spawnIndex) {
    const EnemySpawn& sp = game.GetRom().Enemies()[spawnIndex];
    Enemy e;
    e.spawn = spawnIndex;
    e.area = sp.area;
    e.type = sp.type & 0x0F;
    e.variant = (sp.type & 0x80) != 0;
    e.arch = ArchetypeFor(sp.area, sp.type);
    e.x = e.prevX = e.homeX = (float)sp.worldPxX;
    e.y = e.prevY = e.homeY = (float)sp.worldPxY;
    e.hp = BaseHp(e.arch) * (e.variant ? 2 : 1);
    e.facing = game.mSamus.x < e.x ? -1 : 1;

    // Hitbox from the first frame of the enemy's animation.
    const Rom& rom = game.GetRom();
    std::vector<uint8_t> anim = rom.EnemyAnim(e.area, e.type, true);
    if (!anim.empty()) {
        const SpriteFrame& f = rom.EnemyFrame(e.area, anim[0]);
        if (!f.Empty()) {
            e.halfW = std::max(4.0f, (f.maxX - f.minX + 1) * 0.5f - 1.0f);
            e.halfH = std::max(4.0f, (f.maxY - f.minY + 1) * 0.5f - 1.0f);
        }
    }

    switch (e.arch) {
        case A::Crawler: {
            e.halfW = e.halfH = 7;
            // Find the surface the crawler sits on.
            const int dirs[4][2] = { { 0, 1 }, { 0, -1 }, { -1, 0 }, { 1, 0 } };
            bool found = false;
            for (auto& d : dirs) {
                if (game.TerrainSolidAt(e.x + d[0] * 9, e.y + d[1] * 9)) {
                    e.wallX = d[0];
                    e.wallY = d[1];
                    found = true;
                    break;
                }
            }
            if (!found) {
                // Drop onto the floor below.
                for (int i = 0; i < 128 && !game.TerrainSolidAt(e.x, e.y + 9); i++) {
                    e.y += 1;
                }
                e.wallX = 0;
                e.wallY = 1;
            }
            e.dirX = e.wallY != 0 ? e.facing : 0;
            e.dirY = e.wallX != 0 ? 1 : 0;
            break;
        }
        case A::Bouncer:
            e.vx = (Rand() & 1) ? 1.0f : -1.0f;
            e.vy = 1.0f;
            break;
        case A::PipeBug:
            e.state = 0;
            break;
        case A::Rinka: {
            float dx = game.mSamus.x - e.x, dy = game.mSamus.y - 16 - e.y;
            float len = std::max(1.0f, std::sqrt(dx * dx + dy * dy));
            e.vx = dx / len * 1.2f;
            e.vy = dy / len * 1.2f;
            break;
        }
        default:
            break;
    }
    e.prevX = e.x;
    e.prevY = e.y;
    game.mEnemies.push_back(e);
    game.mSpawnActive[spawnIndex] = SPAWN_ACTIVE;
}

void EnemyLogic::Crawl(Game& game, Enemy& e, float speed) {
    // Follows walls on the 8 px tile grid: turn up an inner corner when blocked ahead, wrap around an
    // outer corner when the surface runs out underneath.
    auto solid = [&](float x, float y) { return game.TerrainSolidAt(x, y); };
    const float r = 8.0f;
    float aheadX = e.x + e.dirX * (r + 0.5f), aheadY = e.y + e.dirY * (r + 0.5f);
    bool blocked =
        solid(aheadX + e.wallX * 6, aheadY + e.wallY * 6) || solid(aheadX - e.wallX * 4, aheadY - e.wallY * 4);
    if (blocked) {
        // Snap flush against the face ahead, then climb it.
        if (e.dirX > 0) {
            e.x = std::floor((e.x + r + 0.5f) / 8) * 8 - r;
        } else if (e.dirX < 0) {
            e.x = std::ceil((e.x - r - 0.5f) / 8) * 8 + r;
        } else if (e.dirY > 0) {
            e.y = std::floor((e.y + r + 0.5f) / 8) * 8 - r;
        } else {
            e.y = std::ceil((e.y - r - 0.5f) / 8) * 8 + r;
        }
        int nx = -e.wallX, ny = -e.wallY;
        e.wallX = e.dirX;
        e.wallY = e.dirY;
        e.dirX = nx;
        e.dirY = ny;
    } else {
        e.x += e.dirX * speed;
        e.y += e.dirY * speed;
        float underX = e.x + e.wallX * (r + 0.5f), underY = e.y + e.wallY * (r + 0.5f);
        bool supportMid = solid(underX, underY);
        bool supportBack = solid(underX - e.dirX * 6, underY - e.dirY * 6);
        if (!supportMid && !supportBack) {
            // Past an outer corner: wrap around it.
            if (e.dirX > 0) {
                e.x = std::floor((e.x - 6) / 8) * 8 + r;
            } else if (e.dirX < 0) {
                e.x = std::ceil((e.x + 6) / 8) * 8 - r;
            } else if (e.dirY > 0) {
                e.y = std::floor((e.y - 6) / 8) * 8 + r;
            } else {
                e.y = std::ceil((e.y + 6) / 8) * 8 - r;
            }
            e.x += e.wallX * 2 * r;
            e.y += e.wallY * 2 * r;
            int nx = e.wallX, ny = e.wallY;
            e.wallX = -e.dirX;
            e.wallY = -e.dirY;
            e.dirX = nx;
            e.dirY = ny;
            // Lost the wall entirely (e.g. a tile was blasted): fall back to the floor.
            if (!solid(e.x + e.wallX * (r + 0.5f), e.y + e.wallY * (r + 0.5f))) {
                e.wallX = 0;
                e.wallY = 1;
                e.dirX = e.facing;
                e.dirY = 0;
                for (int i = 0; i < 4 && !solid(e.x, e.y + r + 0.5f); i++) {
                    e.y += 1;
                }
            }
        }
    }
    // Sprite "up" points away from the wall (world y is up when rendering).
    e.angle = std::atan2((float)e.wallY, (float)-e.wallX) * 180.0f / (float)M_PI - 90.0f;
    if (e.dirX != 0) {
        e.facing = e.dirX;
    }
}

void EnemyLogic::Think(Game& game, Enemy& e) {
    const Samus& s = game.mSamus;
    float dx = s.x - e.x, dy = (s.y - 15) - e.y;
    auto terrain = [&](float x, float y) { return game.TerrainSolidAt(x, y); };

    switch (e.arch) {
        case A::Crawler:
            Crawl(game, e, e.variant ? 0.75f : 0.5f);
            break;

        case A::Skree:
            if (e.state == 0) {
                if (std::fabs(dx) < 40 && dy > 0) {
                    e.state = 1;
                }
            } else {
                e.vy = std::min(e.vy + 0.25f, 3.5f);
                e.vx = std::clamp(dx * 0.05f, -1.0f, 1.0f);
                e.x += e.vx;
                e.y += e.vy;
                if (terrain(e.x, e.y + e.halfH)) {
                    // Burst into four fragments.
                    const float v[4][2] = { { -1.5f, -2.0f }, { 1.5f, -2.0f }, { -0.7f, -3.0f }, { 0.7f, -3.0f } };
                    for (auto& d : v) {
                        game.mEnemyShots.push_back({ e.x, e.y, d[0], d[1], 0.15f, 40, 8, e.x, e.y });
                    }
                    e.alive = false;
                    game.Spawn(e.x, e.y, 0xFFC040FF, 16);
                }
            }
            break;

        case A::Swooper:
            if (e.state == 0) {
                if (e.timer > 0) {
                    e.timer--;
                } else if (std::fabs(dx) < 96 && dy > 0) {
                    e.state = 1;
                    e.vx = Sign(dx) * 1.6f;
                    e.vy = 3.2f;
                    e.facing = (int)Sign(dx);
                }
            } else {
                e.vy -= 0.09f;
                if (terrain(e.x + Sign(e.vx) * (e.halfW + 1), e.y)) {
                    e.vx = -e.vx;
                    e.facing = -e.facing;
                }
                if (e.vy > 0 && terrain(e.x, e.y + e.halfH + e.vy)) {
                    e.vy = -e.vy * 0.5f;
                }
                e.x += e.vx;
                e.y += e.vy;
                if (e.vy < 0 && (e.y <= e.homeY || terrain(e.x, e.y - e.halfH - 1))) {
                    e.y = std::max(e.y, e.homeY);
                    e.vx = e.vy = 0;
                    e.state = 0;
                    e.timer = 40;
                }
            }
            break;

        case A::Ripper: {
            float speed = e.variant ? 1.0f : 0.6f;
            if (terrain(e.x + e.facing * (e.halfW + 1), e.y)) {
                e.facing = -e.facing;
            }
            e.x += e.facing * speed;
            break;
        }

        case A::Reo:
            if (e.state == 0) {
                e.y = e.homeY + std::sin(e.ticks * 0.08f) * 4;
                if (e.timer > 0) {
                    e.timer--;
                } else if (std::fabs(dx) < 80 && std::fabs(dy) < 120) {
                    float len = std::max(1.0f, std::sqrt(dx * dx + dy * dy));
                    e.vx = dx / len * 1.4f;
                    e.vy = dy / len * 1.4f;
                    e.state = 1;
                    e.timer = 50;
                    e.facing = (int)Sign(dx);
                }
            } else if (e.state == 1) {
                if (!terrain(e.x + e.vx, e.y + e.vy)) {
                    e.x += e.vx;
                    e.y += e.vy;
                }
                if (--e.timer <= 0) {
                    e.state = 2;
                }
            } else {
                float hx = e.homeX - e.x, hy = e.homeY - e.y;
                float len = std::sqrt(hx * hx + hy * hy);
                if (len < 1.5f) {
                    e.x = e.homeX;
                    e.y = e.homeY;
                    e.state = 0;
                    e.timer = 60;
                } else {
                    e.x += hx / len;
                    e.y += hy / len;
                }
            }
            break;

        case A::Hopper:
        case A::CeilingHopper: {
            float g = e.arch == A::Hopper ? 1.0f : -1.0f;
            bool grounded = terrain(e.x, e.y + g * (e.halfH + 1));
            if (grounded && e.vy * g >= 0) {
                e.vx = 0;
                e.vy = 0;
                if (++e.timer > 40) {
                    e.timer = 0;
                    e.vy = -g * 3.6f;
                    e.vx = Sign(dx) * 1.2f;
                    e.facing = (int)Sign(dx);
                }
            } else {
                e.vy += g * 0.18f;
            }
            if (terrain(e.x + e.vx + Sign(e.vx) * e.halfW, e.y)) {
                e.vx = 0;
            }
            e.x += e.vx;
            if (terrain(e.x, e.y + e.vy + Sign(e.vy) * e.halfH)) {
                e.vy = 0;
            } else {
                e.y += e.vy;
            }
            break;
        }

        case A::PipeBug:
            if (e.state == 0) {
                if (std::fabs(dx) < 160) {
                    e.state = 1;
                }
            } else if (e.state == 1) {
                e.y -= 1.5f;
                if (e.y <= s.y - 14 || e.y <= e.homeY - 96 || terrain(e.x, e.y - e.halfH)) {
                    e.state = 2;
                    e.facing = (int)Sign(dx);
                }
            } else {
                e.x += e.facing * 2.0f;
            }
            break;

        case A::Metroid:
            if (e.latched) {
                e.x = s.x;
                e.y = s.y - 30;
                if (e.ticks % 8 == 0) {
                    game.mSamus.energy -= 1;
                }
                if (s.morph) {
                    // Rolling up shakes it loose (bombs knock it away too).
                    e.latched = false;
                    e.vy = -2.0f;
                    e.vx = (Rand() & 1) ? 1.5f : -1.5f;
                }
            } else {
                e.vx = std::clamp(e.vx + Sign(dx) * 0.05f, -1.2f, 1.2f);
                e.vy = std::clamp(e.vy + Sign(dy - 10) * 0.05f, -1.2f, 1.2f);
                if (!terrain(e.x + e.vx, e.y)) {
                    e.x += e.vx;
                } else {
                    e.vx = -e.vx * 0.5f;
                }
                if (!terrain(e.x, e.y + e.vy)) {
                    e.y += e.vy;
                } else {
                    e.vy = -e.vy * 0.5f;
                }
            }
            break;

        case A::Bouncer:
            if (terrain(e.x + Sign(e.vx) * (e.halfW + 1), e.y)) {
                e.vx = -e.vx;
            }
            if (terrain(e.x, e.y + Sign(e.vy) * (e.halfH + 1))) {
                e.vy = -e.vy;
            }
            e.x += e.vx;
            e.y += e.vy;
            break;

        case A::Spitter:
            e.facing = (int)Sign(dx);
            if (++e.timer >= 110) {
                e.timer = 0;
                game.mEnemyShots.push_back({ e.x, e.y - 4, Sign(dx) * 1.5f, -3.0f, 0.12f, 120, 10, e.x, e.y - 4 });
            }
            break;

        case A::Rinka:
            e.x += e.vx;
            e.y += e.vy;
            if (terrain(e.x, e.y)) {
                e.alive = false;
            }
            break;

        case A::Boss: {
            e.facing = (int)Sign(dx);
            if (e.area == AREA_KRAID) {
                // Kraid: shuffles back and forth and throws spikes.
                e.vx = (std::fabs(e.x - e.homeX) > 32) ? Sign(e.homeX - e.x) * 0.3f : (e.vx == 0 ? 0.3f : e.vx);
                if (!terrain(e.x + e.vx + Sign(e.vx) * e.halfW, e.y)) {
                    e.x += e.vx;
                }
                if (++e.timer >= 80) {
                    e.timer = 0;
                    for (int i = -1; i <= 1; i++) {
                        game.mEnemyShots.push_back(
                            { e.x, e.y + i * 10.0f, Sign(dx) * 2.0f, 0, 0, 160, 12, e.x, e.y + i * 10.0f });
                    }
                }
            } else {
                // Ridley: hops and spits fireballs.
                bool grounded = terrain(e.x, e.y + e.halfH + 1);
                if (grounded && e.vy >= 0) {
                    e.vy = 0;
                    if (e.ticks % 70 == 0) {
                        e.vy = -3.5f;
                    }
                } else {
                    e.vy = std::min(e.vy + 0.15f, 4.0f);
                }
                if (!terrain(e.x, e.y + e.vy + Sign(e.vy) * e.halfH)) {
                    e.y += e.vy;
                } else {
                    e.vy = 0;
                }
                if (++e.timer >= 50) {
                    e.timer = 0;
                    game.mEnemyShots.push_back({ e.x, e.y - 10, Sign(dx) * 1.6f, -2.5f, 0.1f, 160, 12, e.x, e.y - 10 });
                }
            }
            break;
        }

        default:
            break;
    }
}

void EnemyLogic::TouchSamus(Game& game, Enemy& e) {
    if (!e.alive || e.frozen > 0) {
        return;
    }
    Samus& s = game.mSamus;
    float sx0 = s.x - 6, sx1 = s.x + 6, sy0 = s.y - s.Height(), sy1 = s.y;
    if (e.x + e.halfW < sx0 || e.x - e.halfW > sx1 || e.y + e.halfH < sy0 || e.y - e.halfH > sy1) {
        return;
    }
    if (s.spinning && s.Has(PU_SCREW_ATTACK) && e.arch != A::Boss) {
        Kill(game, e);
        return;
    }
    if (e.arch == A::Metroid) {
        if (!s.morph) {
            e.latched = true;
        }
        return;
    }
    game.HurtSamus(ContactDamage(e), e.x);
}

void EnemyLogic::Damage(Game& game, Enemy& e, int amount, bool missile, bool ice) {
    if (ice && e.frozen == 0 && e.arch != A::Boss) {
        e.frozen = kFreezeTicks;
        e.latched = false;
        return;
    }
    if (e.arch == A::Ripper && !missile) {
        return; // beams bounce off
    }
    if (e.arch == A::Metroid && e.frozen == 0) {
        return; // only hurt while frozen
    }
    e.hp -= amount;
    e.hurt = 8;
    if (e.hp <= 0) {
        Kill(game, e);
    }
}

void EnemyLogic::Kill(Game& game, Enemy& e) {
    e.alive = false;
    game.Spawn(e.x, e.y, 0xFFE0A0FF, 20);
    if (e.arch == A::Boss) {
        game.mBossesDefeated.insert(e.spawn);
        game.Say(e.area == AREA_KRAID ? "KRAID DEFEATED" : "RIDLEY DEFEATED");
        for (int i = 0; i < 4; i++) {
            game.mPickups.push_back({ e.x - 12 + i * 8.0f, e.y, i % 2 == 1, kPickupLife });
        }
        return;
    }
    uint32_t r = Rand() % 6;
    if (r < 3) {
        bool missile = game.mSamus.maxMissiles > 0 && r == 0;
        game.mPickups.push_back({ e.x, e.y, missile, kPickupLife });
    }
}

void EnemyLogic::HitByProjectile(Game& game, const Projectile& p, bool& consumed) {
    for (Enemy& e : game.mEnemies) {
        if (!e.alive || p.x < e.x - e.halfW - 3 || p.x > e.x + e.halfW + 3 || p.y < e.y - e.halfH - 3 ||
            p.y > e.y + e.halfH + 3) {
            continue;
        }
        consumed = true;
        int amount = p.missile ? 5 : 1;
        if (!p.missile && game.mSamus.Has(PU_WAVE_BEAM)) {
            amount = 2;
        }
        Damage(game, e, amount, p.missile, !p.missile && game.mSamus.Has(PU_ICE_BEAM));
        return;
    }
}

void EnemyLogic::BombBlast(Game& game, float x, float y) {
    for (Enemy& e : game.mEnemies) {
        if (!e.alive || std::fabs(e.x - x) > e.halfW + 12 || std::fabs(e.y - y) > e.halfH + 12) {
            continue;
        }
        if (e.latched) {
            e.latched = false;
            e.vy = -2.5f;
        }
        Damage(game, e, 2, false, false);
    }
}

void EnemyLogic::UpdateShots(Game& game) {
    Samus& s = game.mSamus;
    for (EnemyShot& p : game.mEnemyShots) {
        p.vy += p.gravity;
        p.x += p.vx;
        p.y += p.vy;
        p.life--;
        if (game.TerrainSolidAt(p.x, p.y)) {
            p.life = 0;
            continue;
        }
        if (p.x > s.x - 7 && p.x < s.x + 7 && p.y > s.y - s.Height() && p.y < s.y) {
            game.HurtSamus(p.damage, p.x);
            p.life = 0;
        }
    }
    game.mEnemyShots.erase(std::remove_if(game.mEnemyShots.begin(), game.mEnemyShots.end(),
                                          [](const EnemyShot& p) { return p.life <= 0; }),
                           game.mEnemyShots.end());
}

void EnemyLogic::UpdatePickups(Game& game) {
    Samus& s = game.mSamus;
    for (Pickup& p : game.mPickups) {
        p.life--;
        if (std::fabs(p.x - s.x) < 10 && p.y > s.y - s.Height() - 6 && p.y < s.y + 6) {
            if (p.missile) {
                s.missiles = std::min(s.maxMissiles, s.missiles + 2);
            } else {
                s.energy = std::min(s.maxEnergy, s.energy + 5);
            }
            p.life = 0;
        }
    }
    game.mPickups.erase(
        std::remove_if(game.mPickups.begin(), game.mPickups.end(), [](const Pickup& p) { return p.life <= 0; }),
        game.mPickups.end());
}

} // namespace Metroid
