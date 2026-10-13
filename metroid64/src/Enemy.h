#pragma once

#include "Game.h"

namespace Metroid {

// Enemy spawning, AI, combat and drops. Spawn positions and types come from the ROM's room data;
// sprites and animations from the ROM's per-area enemy tables. Behaviour is reimplemented per
// archetype rather than emulated.
class EnemyLogic {
  public:
    static Archetype ArchetypeFor(Area area, uint8_t type);
    static void Update(Game& game);
    static void HitByProjectile(Game& game, const Projectile& p, bool& consumed);
    static void BombBlast(Game& game, float x, float y);

  private:
    static void Activate(Game& game);
    static void Spawn(Game& game, int spawnIndex);
    static void Think(Game& game, Enemy& e);
    static void Crawl(Game& game, Enemy& e, float speed);
    static void Damage(Game& game, Enemy& e, int amount, bool missile, bool ice);
    static void Kill(Game& game, Enemy& e);
    static void TouchSamus(Game& game, Enemy& e);
    static void UpdateShots(Game& game);
    static void UpdatePickups(Game& game);
};

} // namespace Metroid
