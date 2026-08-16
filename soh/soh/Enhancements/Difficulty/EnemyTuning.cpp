#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/ShipInit.hpp"
#include "functions.h"
#include "macros.h"

#include "EnemyTuningTable.h"

extern "C" PlayState* gPlayState;

#define CVAR_ENEMY_FALLBACK_SPEED_MULT_NAME CVAR_ENHANCEMENT("EnemyTuning.Fallback.Speed")
#define CVAR_ENEMY_FALLBACK_ATTACK_SPEED_MULT_NAME CVAR_ENHANCEMENT("EnemyTuning.Fallback.AttackSpeed")
#define CVAR_ENEMY_FALLBACK_DAMAGE_MULT_NAME CVAR_ENHANCEMENT("EnemyTuning.Fallback.Damage")

// Same eligibility rules as Hyper Enemies: most ACTORCAT_ENEMY actors, plus wall/floor torches
// that get lit as an "enemy" action, excluding actors that break when updated off-cycle. A
// curated actor is always eligible regardless of category -- e.g. Blade Trap (ACTOR_BG_HAKA_SGAMI)
// is ACTORCAT_PROP, not ACTORCAT_ENEMY, so without this it would never be affected no matter what
// its own curated sliders say. Being explicitly curated is a stronger signal than the category
// heuristic below, which exists only to cover the *uncurated* fallback case sanely.
static bool IsAffectedEnemy(Actor* actor) {
    if (FindEnemyTuningEntry(actor->id) != nullptr) {
        return true;
    }
    bool isEnemy = actor->category == ACTORCAT_ENEMY || actor->id == ACTOR_EN_TORCH2;
    bool isExcludedEnemy = actor->id == ACTOR_EN_FIRE_ROCK || actor->id == ACTOR_EN_ENCOUNT2;
    return isEnemy && !isExcludedEnemy;
}

// A curated enemy uses its own multiplier; everything else affected falls back to the global one.
static float GetSpeedMult(Actor* actor) {
    if (const EnemyTuningEntry* entry = FindEnemyTuningEntry(actor->id)) {
        return CVarGetFloat(entry->speedCvarName.c_str(), 1.0f);
    }
    return CVarGetFloat(CVAR_ENEMY_FALLBACK_SPEED_MULT_NAME, 1.0f);
}

static float GetAttackSpeedMult(Actor* actor) {
    if (const EnemyTuningEntry* entry = FindEnemyTuningEntry(actor->id)) {
        return CVarGetFloat(entry->attackSpeedCvarName.c_str(), 1.0f);
    }
    return CVarGetFloat(CVAR_ENEMY_FALLBACK_ATTACK_SPEED_MULT_NAME, 1.0f);
}

// Splits a multiplier like 2.4 into 2 guaranteed extra steps plus a 40% chance of a 3rd,
// so the slider's average effect over time matches the requested value continuously.
static int32_t RollExtraSteps(float mult) {
    float extraSteps = mult - 1.0f;
    int32_t wholeSteps = (int32_t)extraSteps;
    float fractionalStep = extraSteps - wholeSteps;
    return wholeSteps + ((Rand_ZeroOne() < fractionalStep) ? 1 : 0);
}

// Gives enemies extra horizontal movement steps along their current heading so they cover
// ground faster, without advancing their action/attack timers.
static void ApplyEnemySpeedMult(void* refActor) {
    Player* player = GET_PLAYER(gPlayState);
    Actor* actor = static_cast<Actor*>(refActor);

    if (!IsAffectedEnemy(actor) || Player_InBlockingCsMode(gPlayState, player)) {
        return;
    }

    int32_t extraSteps = RollExtraSteps(GetSpeedMult(actor));
    if (extraSteps <= 0) {
        return;
    }

    // Deliberately don't call Actor_MoveXZGravity/Actor_UpdateVelocityXZGravity here: that
    // function does `velocity.y += gravity`, so extra calls would re-accumulate gravity on
    // top of what the actor's real update already applied this frame, making enemies fall
    // faster than their once-per-frame floor collision check can correct for and sink into
    // the ground. Only extend the X/Z displacement using this frame's already-computed
    // horizontal velocity, leaving Y untouched.
    // Matches Actor_UpdatePos's speedRate (R_UPDATE_RATE * 0.5f) at its default value of 1.0f,
    // without pulling in the gGameInfo debug-register global that macro depends on.
    f32 speedRate = 1.0f;
    for (int32_t i = 0; i < extraSteps; i++) {
        actor->world.pos.x += actor->velocity.x * speedRate;
        actor->world.pos.z += actor->velocity.z * speedRate;
    }
}

// Re-runs the enemy's full update an extra time (like Hyper Enemies) so all of its action/attack
// timers advance faster -- everything it does plays out quicker, attacks included, which is why
// this is "Attack Speed" rather than "Attack Frequency": it doesn't change how often the enemy
// *chooses* to attack, only how fast whatever it's already doing executes. Rewinds the position
// change it caused so the effect doesn't also read as faster movement (which has its own slider).
static void ApplyEnemyAttackSpeedMult(void* refActor) {
    Player* player = GET_PLAYER(gPlayState);
    Actor* actor = static_cast<Actor*>(refActor);

    if (!IsAffectedEnemy(actor) || Player_InBlockingCsMode(gPlayState, player)) {
        return;
    }

    int32_t extraTicks = RollExtraSteps(GetAttackSpeedMult(actor));
    for (int32_t i = 0; i < extraTicks; i++) {
        Vec3f posBefore = actor->world.pos;
        Vec3f prevPosBefore = actor->prevPos;
        Vec3f velocityBefore = actor->velocity;
        f32 speedXZBefore = actor->speedXZ;

        GameInteractor::RawAction::UpdateActor(actor);

        if (actor->update == NULL) {
            // The actor died/despawned during the extra update; nothing left to rewind.
            break;
        }

        actor->world.pos = posBefore;
        actor->prevPos = prevPosBefore;
        // Also restore velocity (in particular velocity.y, which accumulates gravity every
        // tick) so the extra tick doesn't leave the actor "primed" to fall further than normal
        // next frame and sink into the floor before collision can catch it.
        actor->velocity = velocityBefore;
        actor->speedXZ = speedXZBefore;
    }
}

// Whether any curated or fallback multiplier for the given getter is high enough to be worth
// hooking OnActorUpdate for at all.
template <typename GetCuratedMult>
static bool AnyMultActive(GetCuratedMult getCurated, const char* fallbackCvarName) {
    for (const auto& entry : GetEnemyTuningTable()) {
        if (getCurated(entry) > 1.0f) {
            return true;
        }
    }
    return CVarGetFloat(fallbackCvarName, 1.0f) > 1.0f;
}

static void UpdateEnemyBehaviorMultsState() {
    bool speedActive = AnyMultActive(
        [](const EnemyTuningEntry& entry) { return CVarGetFloat(entry.speedCvarName.c_str(), 1.0f); },
        CVAR_ENEMY_FALLBACK_SPEED_MULT_NAME);
    bool attackSpeedActive = AnyMultActive(
        [](const EnemyTuningEntry& entry) { return CVarGetFloat(entry.attackSpeedCvarName.c_str(), 1.0f); },
        CVAR_ENEMY_FALLBACK_ATTACK_SPEED_MULT_NAME);

    COND_HOOK(OnActorUpdate, speedActive, ApplyEnemySpeedMult);
    COND_HOOK(OnActorUpdate, attackSpeedActive, ApplyEnemyAttackSpeedMult);
}

static std::set<std::string> BuildWatchedCvarSet() {
    std::set<std::string> cvars = { CVAR_ENEMY_FALLBACK_SPEED_MULT_NAME, CVAR_ENEMY_FALLBACK_ATTACK_SPEED_MULT_NAME };
    for (const auto& entry : GetEnemyTuningTable()) {
        cvars.insert(entry.speedCvarName);
        cvars.insert(entry.attackSpeedCvarName);
    }
    return cvars;
}

static void RegisterEnemyDamageMult() {
    // Registered unconditionally (unlike the speed/attack-speed hooks above): this only ever
    // reads CVars fresh on each call, so there's no per-frame cost worth conditionally
    // registering/unregistering for, and it means damage CVars don't need to be watched by
    // RegisterShipInitFunc below to take effect immediately.
    COND_VB_SHOULD(VB_MODIFY_ENEMY_DAMAGE_TO_PLAYER, true, {
        Actor* victim = va_arg(args, Actor*);
        Actor* attacker = va_arg(args, Actor*);
        f32* damage = va_arg(args, f32*);

        if (victim == NULL || victim->category != ACTORCAT_PLAYER || attacker == NULL) {
            return;
        }

        if (const EnemyTuningEntry* entry = FindEnemyTuningEntry(attacker->id, attacker->params)) {
            // Base Damage's CVar default is this enemy's own vanilla value, so leaving it
            // untouched reproduces vanilla exactly; changing it overrides the damage this enemy
            // deals outright (note: for enemies that go through the standard collision system,
            // this replaces the value after the player's defense/double-defense reduction has
            // already been subtracted, rather than scaling around it). baseDamageCvarName is
            // bound to an int slider, so it must be read with CVarGetInteger to match -- this
            // project's CVar storage is a strictly-typed union and CVarGetFloat on an
            // Integer-typed CVar silently returns its own fallback instead of converting.
            *damage = (f32)CVarGetInteger(entry->baseDamageCvarName.c_str(), entry->defaultDamage);
            *damage *= CVarGetFloat(entry->damageCvarName.c_str(), 1.0f);
        } else if (attacker->category == ACTORCAT_ENEMY) {
            *damage *= CVarGetFloat(CVAR_ENEMY_FALLBACK_DAMAGE_MULT_NAME, 1.0f);
        }
    });
}

static void RegisterEnemyTuning() {
    UpdateEnemyBehaviorMultsState();
    RegisterEnemyDamageMult();
}

static RegisterShipInitFunc initFunc(RegisterEnemyTuning, BuildWatchedCvarSet());

extern "C" void ResetEnemyTuningToDefaults() {
    CVarClearBlock(CVAR_ENHANCEMENT("EnemyTuning"));
    CVarSave();
    RegisterEnemyTuning();
}
