#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/ShipInit.hpp"
#include "functions.h"
#include "macros.h"

extern "C" {
extern PlayState* gPlayState;
}

#define CVAR_DEATH_DECAPITATE_NAME CVAR_ENHANCEMENT("DeathByBladeTrapEffect")
#define CVAR_DEATH_DECAPITATE_DEFAULT 0
#define CVAR_DEATH_DECAPITATE_VALUE CVarGetInteger(CVAR_DEATH_DECAPITATE_NAME, CVAR_DEATH_DECAPITATE_DEFAULT)

// Bg_Haka_Trap is a shared "Shadow Temple hazard" actor whose `params` selects between several
// unrelated mechanics -- spiked wall, spiked crusher, fan blade, and the guillotine (see
// BgHakaTrap_Init in z_bg_haka_trap.c and HAKA_TRAP_GUILLOTINE_SLOW/FAST in its header). Only the
// guillotine should trigger this effect, so matching on actor ID alone isn't enough -- see
// sLastDamageAttackerParams below. BgHakaTrap_Init rewrites HAKA_TRAP_GUILLOTINE_FAST to
// HAKA_TRAP_GUILLOTINE_SLOW (0) on spawn, so checking for that one value alone covers both.
#define HAKA_TRAP_GUILLOTINE_SLOW_PARAM 0

// Same dark-red spark burst as PlayerHurtBlood.cpp's SpawnPlayerHurtBlood (kept as a separate
// copy here rather than shared, since that one's file-scoped static and this is a one-shot use
// rather than a per-hit hook).
static void SpawnDeathBlood(PlayState* play, Vec3f* pos) {
    static EffectSparkInit sparkInit;
    s32 effectIndex;

    sparkInit.position.x = (s16)pos->x;
    sparkInit.position.y = (s16)pos->y;
    sparkInit.position.z = (s16)pos->z;
    sparkInit.uDiv = 5;
    sparkInit.vDiv = 5;
    for (s32 i = 0; i < 4; i++) {
        sparkInit.colorStart[i].r = 139;
        sparkInit.colorStart[i].g = 0;
        sparkInit.colorStart[i].b = 0;
        sparkInit.colorStart[i].a = 255;
        sparkInit.colorEnd[i].r = 40;
        sparkInit.colorEnd[i].g = 0;
        sparkInit.colorEnd[i].b = 0;
        sparkInit.colorEnd[i].a = 0;
    }
    sparkInit.timer = 0;
    sparkInit.duration = 16;
    sparkInit.speed = 8.0f;
    sparkInit.gravity = -1.0f;

    Effect_Add(play, &effectIndex, EFFECT_SPARK, 0, 1, &sparkInit);
}

// Updated on every hit that goes through the standard AT-vs-AC damage path (CollisionCheck_
// ApplyDamage), which is also what the Enemy Tuning table's own damage hook relies on for these
// same two actors (see EnemyTuning.cpp's RegisterEnemyDamageMult and the "Blade Trap"/"Guillotine
// (Shadow Temple)" entries in EnemyTuningTable.cpp) -- so it's a reliable signal for exactly this
// case. Holds whatever hit Link most recently, lethal or not; only meaningful read at the instant
// death is detected below, since it's simply overwritten by the next hit otherwise.
static s16 sLastDamageAttackerId = -1;
static s16 sLastDamageAttackerParams = -1;

// Latched once at the PLAYER_STATE1_DEAD rising edge (see the OnPlayerUpdate handler below)
// rather than recomputed every frame of the death animation.
static s32 sDiedDecapitated = false;
static s32 sWasDead = false;

static void RegisterDeathBySource() {
    COND_VB_SHOULD(VB_MODIFY_ENEMY_DAMAGE_TO_PLAYER, CVAR_DEATH_DECAPITATE_VALUE, {
        Actor* victim = va_arg(args, Actor*);
        Actor* attacker = va_arg(args, Actor*);
        va_arg(args, f32*); // damage, unused here

        if (victim == NULL || victim->category != ACTORCAT_PLAYER || attacker == NULL) {
            return;
        }

        sLastDamageAttackerId = attacker->id;
        sLastDamageAttackerParams = attacker->params;
    });

    COND_VB_SHOULD(VB_PLAYER_OVERRIDE_LIMB_DRAW, CVAR_DEATH_DECAPITATE_VALUE, {
        s32 limbIndex = va_arg(args, s32);
        Gfx** dList = va_arg(args, Gfx**);
        va_arg(args, void*);      // thisx, unused here
        va_arg(args, PlayState*); // play, unused here

        if (sDiedDecapitated) {
            switch (limbIndex) {
                // Everything above the waist -- head/hat, torso, sheath, and both arms. WAIST
                // itself and the leg limbs are left alone, so what's left standing still reads
                // as Link's lower body rather than nothing at all.
                case PLAYER_LIMB_HEAD:
                case PLAYER_LIMB_HAT:
                case PLAYER_LIMB_COLLAR:
                case PLAYER_LIMB_TORSO:
                case PLAYER_LIMB_SHEATH:
                case PLAYER_LIMB_L_SHOULDER:
                case PLAYER_LIMB_L_FOREARM:
                case PLAYER_LIMB_L_HAND:
                case PLAYER_LIMB_R_SHOULDER:
                case PLAYER_LIMB_R_FOREARM:
                case PLAYER_LIMB_R_HAND:
                    *dList = NULL;
                    break;
            }
        }
    });

    COND_HOOK(OnPlayerUpdate, CVAR_DEATH_DECAPITATE_VALUE, []() {
        Player* player = GET_PLAYER(gPlayState);
        s32 isDead = (player->stateFlags1 & PLAYER_STATE1_DEAD) != 0;

        if (isDead && !sWasDead) {
            bool killedByScytheTrap = sLastDamageAttackerId == ACTOR_BG_HAKA_SGAMI;
            bool killedByGuillotine = (sLastDamageAttackerId == ACTOR_BG_HAKA_TRAP) &&
                                      (sLastDamageAttackerParams == HAKA_TRAP_GUILLOTINE_SLOW_PARAM);
            sDiedDecapitated = killedByScytheTrap || killedByGuillotine;

            if (sDiedDecapitated) {
                SpawnDeathBlood(gPlayState, &player->bodyPartsPos[PLAYER_BODYPART_COLLAR]);
            }
        } else if (!isDead) {
            sDiedDecapitated = false;
        }

        sWasDead = isDead;
    });
}

static RegisterShipInitFunc initFunc(RegisterDeathBySource, { CVAR_DEATH_DECAPITATE_NAME });
