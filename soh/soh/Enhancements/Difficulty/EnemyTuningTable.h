#pragma once

#include <string>
#include <vector>
#include <cstdint>

// Curated list of enemy actor types each configurable with their own Speed/Attack Speed/Damage
// multipliers (see EnemyTuning.cpp), independent of the global fallback multipliers that
// apply to every other ACTORCAT_ENEMY actor. Building the table's CVar name strings is deferred
// to first use (a function-local static, i.e. a Meyers singleton) rather than a global, to avoid
// static-initialization-order issues between this translation unit and the ones that consume it.
struct EnemyTuningEntry {
    int16_t actorId;
    const char* key;          // Stable slug used in CVar names, e.g. "EnIk" -- never rename once shipped.
    const char* displayName;  // Player-facing name shown in the menu, e.g. "Dark Nut / Iron Knuckle".
    // This enemy's vanilla damage value -- also baseDamageCvarName's CVar default, so leaving
    // that slider untouched reproduces vanilla exactly. An int (not float): baseDamageCvarName
    // is bound to an int slider (WIDGET_CVAR_SLIDER_INT -> CVarSetInteger), and this project's
    // CVar storage is a strictly-typed union -- CVarGetFloat on an Integer-typed CVar always
    // silently returns its fallback rather than converting, so the getter/storage types must
    // match exactly.
    int32_t defaultDamage;
    std::string speedCvarName;
    std::string attackSpeedCvarName;
    std::string damageCvarName;
    std::string baseDamageCvarName;
};

const std::vector<EnemyTuningEntry>& GetEnemyTuningTable();

// Returns nullptr if actorId isn't in the curated list, meaning the caller should fall back to
// the global multipliers instead.
const EnemyTuningEntry* FindEnemyTuningEntry(int16_t actorId);

// Defined in EnemyTuning.cpp. Clears every Enemy Tuning CVar (curated and Global Fallback) back
// to its default and re-registers hooks to match.
extern "C" void ResetEnemyTuningToDefaults();
