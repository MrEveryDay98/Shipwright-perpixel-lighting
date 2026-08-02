#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/ShipInit.hpp"
#include "functions.h"
#include "macros.h"

extern "C" PlayState* gPlayState;

#define CVAR_ENEMY_SPEED_MULT_NAME CVAR_ENHANCEMENT("EnemySpeedMult")
#define CVAR_ENEMY_SPEED_MULT_DEFAULT 1.0f
#define CVAR_ENEMY_SPEED_MULT_VALUE CVarGetFloat(CVAR_ENEMY_SPEED_MULT_NAME, CVAR_ENEMY_SPEED_MULT_DEFAULT)

#define CVAR_ENEMY_ATTACK_FREQ_MULT_NAME CVAR_ENHANCEMENT("EnemyAttackFrequencyMult")
#define CVAR_ENEMY_ATTACK_FREQ_MULT_DEFAULT 1.0f
#define CVAR_ENEMY_ATTACK_FREQ_MULT_VALUE CVarGetFloat(CVAR_ENEMY_ATTACK_FREQ_MULT_NAME, CVAR_ENEMY_ATTACK_FREQ_MULT_DEFAULT)

// Same eligibility rules as Hyper Enemies: most ACTORCAT_ENEMY actors, plus wall/floor torches
// that get lit as an "enemy" action, excluding actors that break when updated off-cycle.
static bool IsAffectedEnemy(Actor* actor) {
    bool isEnemy = actor->category == ACTORCAT_ENEMY || actor->id == ACTOR_EN_TORCH2;
    bool isExcludedEnemy = actor->id == ACTOR_EN_FIRE_ROCK || actor->id == ACTOR_EN_ENCOUNT2;
    return isEnemy && !isExcludedEnemy;
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

    int32_t extraSteps = RollExtraSteps(CVAR_ENEMY_SPEED_MULT_VALUE);
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

// Re-runs the enemy's full update an extra time (like Hyper Enemies) so action/attack timers
// advance faster, then rewinds the position change it caused so the effect reads as more
// frequent attacks rather than faster movement (which has its own slider above).
static void ApplyEnemyAttackFrequencyMult(void* refActor) {
    Player* player = GET_PLAYER(gPlayState);
    Actor* actor = static_cast<Actor*>(refActor);

    if (!IsAffectedEnemy(actor) || Player_InBlockingCsMode(gPlayState, player)) {
        return;
    }

    int32_t extraTicks = RollExtraSteps(CVAR_ENEMY_ATTACK_FREQ_MULT_VALUE);
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

static void UpdateEnemyBehaviorMultsState() {
    COND_HOOK(OnActorUpdate, CVAR_ENEMY_SPEED_MULT_VALUE > 1.0f, ApplyEnemySpeedMult);
    COND_HOOK(OnActorUpdate, CVAR_ENEMY_ATTACK_FREQ_MULT_VALUE > 1.0f, ApplyEnemyAttackFrequencyMult);
}

static RegisterShipInitFunc initFunc(UpdateEnemyBehaviorMultsState,
                                      { CVAR_ENEMY_SPEED_MULT_NAME, CVAR_ENEMY_ATTACK_FREQ_MULT_NAME });
