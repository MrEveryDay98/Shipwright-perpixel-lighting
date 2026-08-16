#include "EnemyTuningTable.h"

#include "functions.h"
#include "macros.h"
#include "soh/cvar_prefixes.h"

namespace {
void AddEntry(std::vector<EnemyTuningEntry>& table, int16_t actorId, const char* key, const char* displayName,
              int32_t defaultDamage, int32_t requiredParams = -1) {
    std::string base = std::string(CVAR_PREFIX_ENHANCEMENT) + ".EnemyTuning." + key;
    table.push_back({
        actorId,
        requiredParams,
        key,
        displayName,
        defaultDamage,
        base + ".Speed",
        base + ".AttackSpeed",
        base + ".Damage",
        base + ".BaseDamage",
    });
}
} // namespace

const std::vector<EnemyTuningEntry>& GetEnemyTuningTable() {
    static const std::vector<EnemyTuningEntry> table = [] {
        std::vector<EnemyTuningEntry> t;
        // Entries are kept in alphabetical order by display name to match the menu's listing
        // order (see SohMenuEnhancements.cpp's "Enemy Tuning" page, which just iterates this
        // table in order).
        //
        // Default damage values are each enemy's vanilla toucher/damagePlayer value (see
        // EnemyTuning.cpp's damage hook for how these get applied). Where an enemy has multiple
        // distinct attack-damage values in vanilla (different colliders/moves), the most common
        // or primary one is used as the representative default -- overriding Base Damage sets a
        // single fixed value for all of that enemy's attacks, not just the one the default was
        // sourced from. Entries marked NONE_FOUND below don't deal contact/melee damage to the
        // player in vanilla at all (their threat comes from a projectile actor they spawn, or a
        // non-damaging grab/effect) -- Base Damage and the Damage multiplier are harmless no-ops
        // for these, kept at a placeholder of 8 for consistency; Speed and Attack Speed still
        // apply normally.
        AddEntry(t, ACTOR_EN_ANUBICE_FIRE, "EnAnubiceFire", "Anubis Fire Attack", 4);
        AddEntry(t, ACTOR_EN_AM, "EnAm", "Armos Statue", 8);
        AddEntry(t, ACTOR_EN_DODOJR, "EnDodojr", "Baby Dodongo", 8);
        AddEntry(t, ACTOR_EN_VALI, "EnVali", "Bari", 8);
        AddEntry(t, ACTOR_EN_VM, "EnVm", "Beamos", 16);
        AddEntry(t, ACTOR_EN_BIGOKUTA, "EnBigokuta", "Big Octo", 8);
        AddEntry(t, ACTOR_EN_PO_FIELD, "EnPoField", "Big/Small Poe Spawn Point", 4);
        AddEntry(t, ACTOR_EN_BILI, "EnBili", "Biri", 8);
        AddEntry(t, ACTOR_BG_HAKA_SGAMI, "BgHakaSgami", "Blade Trap", 4);
        AddEntry(t, ACTOR_EN_BB, "EnBb", "Bubble (Skull)", 8);
        AddEntry(t, ACTOR_EN_ATTACK_NIW, "EnAttackNiw", "Cucco (Attacking)", 8); // NONE_FOUND
        AddEntry(t, ACTOR_EN_IK, "EnIk", "Dark Nut / Iron Knuckle", 64);
        AddEntry(t, ACTOR_EN_DH, "EnDh", "Dead Hand", 8); // MULTIPLE: 8 grab (primary) / 4 arm swipe
        AddEntry(t, ACTOR_EN_DHA, "EnDha", "Dead Hand's Hand", 8); // NONE_FOUND
        AddEntry(t, ACTOR_EN_DEKUBABA, "EnDekubaba", "Deku Baba", 8);
        AddEntry(t, ACTOR_EN_DODONGO, "EnDodongo", "Dodongo", 16); // MULTIPLE: 16 fire breath (primary) / 8 tail swipe
        AddEntry(t, ACTOR_EN_TP, "EnTp", "Electric Tailpasaran", 8);
        AddEntry(t, ACTOR_EN_BX, "EnBx", "Electrified Tentacle (Jabu-Jabu's Belly)", 4);
        AddEntry(t, ACTOR_EN_FIRE_ROCK, "EnFireRock", "Falling Burning Rocks", 8);
        AddEntry(t, ACTOR_EN_ENCOUNT2, "EnEncount2", "Falling Rock Spawner", 8); // NONE_FOUND
        AddEntry(t, ACTOR_EN_FD, "EnFd", "Flare Dancer", 4);
        AddEntry(t, ACTOR_EN_FW, "EnFw", "Flare Dancer Core", 8); // NONE_FOUND
        AddEntry(t, ACTOR_EN_FD_FIRE, "EnFdFire", "Flare Dancer Fire Attack", 8);
        AddEntry(t, ACTOR_EN_BROB, "EnBrob", "Flobbery Muscle Block (Jabu-Jabu's Belly)", 8);
        AddEntry(t, ACTOR_EN_FLOORMAS, "EnFloormas", "Floormaster", 16); // MULTIPLE: 16 grab / 8 hold tick
        AddEntry(t, ACTOR_EN_YUKABYUN, "EnYukabyun", "Flying Floor Tile", 4);
        AddEntry(t, ACTOR_EN_FZ, "EnFz", "Frezzard", 8); // MULTIPLE: 8 breath (primary) / 0 body touch (freeze only)
        AddEntry(t, ACTOR_EN_GELDB, "EnGeldb", "Gerudo Fighter", 8);
        AddEntry(t, ACTOR_EN_GOMA, "EnGoma", "Gohma Larva", 8);
        AddEntry(t, ACTOR_EN_SHOPNUTS, "EnShopnuts", "Grounded Sales Scrub", 8); // NONE_FOUND
        AddEntry(t, ACTOR_EN_CROW, "EnCrow", "Guay", 8);
        // Bg_Haka_Trap is a shared "Shadow Temple hazard" actor whose `params` selects between
        // several unrelated mechanics -- spiked wall, spiked crusher, fan blade, and the
        // guillotine (see BgHakaTrap_Init in z_bg_haka_trap.c). requiredParams scopes this entry
        // to just the guillotine (HAKA_TRAP_GUILLOTINE_SLOW == 0 in that file's own header;
        // BgHakaTrap_Init rewrites HAKA_TRAP_GUILLOTINE_FAST to that same value on spawn, so this
        // covers both variants). Speed/Attack Speed are no-ops here regardless -- see
        // FindEnemyTuningEntry's params handling -- since only the damage hook looks entries up
        // with real params; treat this entry as damage-only, the same as the NONE_FOUND ones
        // above are speed/attack-speed-only.
        AddEntry(t, ACTOR_BG_HAKA_TRAP, "BgHakaTrapGuillotine", "Guillotine (Shadow Temple)", 4,
                 /*requiredParams=*/0);
        AddEntry(t, ACTOR_EN_HINTNUTS, "EnHintnuts", "Hint Deku Scrubs (Deku Tree)", 8); // NONE_FOUND
        AddEntry(t, ACTOR_EN_FIREFLY, "EnFirefly", "Keese", 8);
        AddEntry(t, ACTOR_EN_BDFIRE, "EnBdfire", "King Dodongo's Fire Breath", 8); // NONE_FOUND
        AddEntry(t, ACTOR_EN_RR, "EnRr", "Like-Like", 8);
        AddEntry(t, ACTOR_EN_ZF, "EnZf", "Lizalfos and Dinolfos", 8);
        AddEntry(t, ACTOR_EN_DEKUNUTS, "EnDekunuts", "Mad Scrub", 8); // NONE_FOUND
        AddEntry(t, ACTOR_EN_TRAP, "EnTrap", "Metal Spike Trap", 4);
        AddEntry(t, ACTOR_EN_MB, "EnMb", "Moblins", 8);
        AddEntry(t, ACTOR_EN_OKUTA, "EnOkuta", "Octorok", 8);
        AddEntry(t, ACTOR_EN_PEEHAT, "EnPeehat", "Peahat and Larva", 16);
        AddEntry(t, ACTOR_EN_POH, "EnPoh", "Poe", 8);
        AddEntry(t, ACTOR_EN_PO_SISTERS, "EnPoSisters", "Poe Sisters", 8);
        AddEntry(t, ACTOR_EN_RD, "EnRd", "Redead and Gibdo", 8);
        AddEntry(t, ACTOR_EN_BUBBLE, "EnBubble", "Shabom", 4); // DIRECT_CALL
        AddEntry(t, ACTOR_EN_SB, "EnSb", "Shell Blade", 8);
        AddEntry(t, ACTOR_EN_SKJ, "EnSkj", "Skullkid", 8);
        AddEntry(t, ACTOR_EN_SKJNEEDLE, "EnSkjneedle", "Skullkid Needle Attack", 8);
        AddEntry(t, ACTOR_EN_ST, "EnSt", "Skulltula", 4); // MULTIPLE: 4 drop/lunge (primary) / 8 body slam
        AddEntry(t, ACTOR_EN_NY, "EnNy", "Spike (Enemy)", 8);
        AddEntry(t, ACTOR_EN_SKB, "EnSkb", "Stalchild", 4);
        // Stalfos varies its attack damage between 16 (regular/down slash) and 32 (jump slash) in
        // vanilla; 16 is used here as the representative default.
        AddEntry(t, ACTOR_EN_TEST, "EnTest", "Stalfos", 16);
        AddEntry(t, ACTOR_EN_EIYER, "EnEiyer", "Stinger (Land)", 8);
        AddEntry(t, ACTOR_EN_WEIYER, "EnWeiyer", "Stinger (Water)", 8);
        AddEntry(t, ACTOR_EN_TITE, "EnTite", "Tektite", 8);
        AddEntry(t, ACTOR_EN_BA, "EnBa", "Tentacle (Inside Jabu-Jabu's Belly)", 4);
        AddEntry(t, ACTOR_EN_BW, "EnBw", "Torch Slug", 8);
        AddEntry(t, ACTOR_EN_WALLMAS, "EnWallmas", "Wallmaster", 8); // NONE_FOUND
        AddEntry(t, ACTOR_EN_KAREBABA, "EnKarebaba", "Withered Deku Baba", 8);
        // Regular Wolfos deals 4 (its static default); the White Wolfos variant (same actor,
        // selected via spawn params) overrides this to 8 at runtime -- 4 is used here as the
        // representative default since it's the more commonly encountered variant.
        AddEntry(t, ACTOR_EN_WF, "EnWf", "Wolfos", 4);
        return t;
    }();
    return table;
}

const EnemyTuningEntry* FindEnemyTuningEntry(int16_t actorId, int32_t params) {
    for (const auto& entry : GetEnemyTuningTable()) {
        if (entry.actorId == actorId && (entry.requiredParams == -1 || entry.requiredParams == params)) {
            return &entry;
        }
    }
    return nullptr;
}
