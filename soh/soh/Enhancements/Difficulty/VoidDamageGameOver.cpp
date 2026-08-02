#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/ShipInit.hpp"
#include "functions.h"
#include "macros.h"

extern "C" {
extern SaveContext gSaveContext;
}

#define CVAR_VOID_DAMAGE_GAME_OVER_NAME CVAR_ENHANCEMENT("VoidDamageGameOver")
#define CVAR_VOID_DAMAGE_GAME_OVER_DEFAULT 0
#define CVAR_VOID_DAMAGE_GAME_OVER_VALUE CVarGetInteger(CVAR_VOID_DAMAGE_GAME_OVER_NAME, CVAR_VOID_DAMAGE_GAME_OVER_DEFAULT)

// Every void-out call site gates its teleport (Play_TriggerVoidOut/Play_TriggerRespawn) behind
// VB_TRIGGER_VOIDOUT, but the actual void-damage application (func_808382DC's
// Player_InflictDamageModified call, the only place that reads VoidDamageMult) doesn't
// consistently happen before that gate check -- some void floors teleport first and only apply
// damage afterward, at the destination, on a frame that kills the player before any second
// teleport decision (and thus this hook) ever runs again. So rather than reactively checking
// whether damage has ALREADY landed, this predicts whether the standard void hit -- using the
// exact same formula func_808382DC itself uses -- would be lethal given current health,
// regardless of whether that specific call's damage has been applied yet.
//
// Blocking the teleport and forcing health to 0 still isn't enough on its own: the game's normal
// per-frame health==0 check (Player_UpdateCommon) only actually starts the game over sequence
// when the player is grounded or in water, and void falls happen mid-air, so health==0 would
// otherwise just sit there with nothing visibly happening until the player eventually lands
// somewhere. sPendingForcedGameOver flags that this specific health==0 came from a blocked void
// hit, so the paired VB_FORCE_GAME_OVER_AIRBORNE hook can let the death sequence start
// immediately regardless of airborne state, one time only.
static bool sPendingForcedGameOver = false;

// If VoidDamageMult is high enough, expectedVoidDamage below can exceed any health the player
// could realistically have, meaning literally any void touch is "lethal." That's a problem right
// after "Continue" from a forced void death: entrances can spawn the player right back on or next
// to the same void-triggering floor, re-triggering this hook before they've had a chance to move
// off it, forcing another game over on the very next life. sPostRespawnGraceFrames gives the
// player a short window immediately after such a respawn where void touches are ignored entirely
// (no teleport, no damage, no game over) so they have a moment to step away.
static bool sDiedFromForcedVoidOut = false;
static int32_t sPostRespawnGraceFrames = 0;
static bool sPrevHealthWasZero = false;

static void UpdateVoidRespawnGraceState() {
    if (sPostRespawnGraceFrames > 0) {
        sPostRespawnGraceFrames--;
    }

    bool healthIsZero = (gSaveContext.health == 0);
    if (sDiedFromForcedVoidOut && sPrevHealthWasZero && !healthIsZero) {
        sDiedFromForcedVoidOut = false;
        sPostRespawnGraceFrames = 90;
    }
    sPrevHealthWasZero = healthIsZero;
}

static void RegisterVoidDamageGameOver() {
    COND_HOOK(OnPlayerUpdate, CVAR_VOID_DAMAGE_GAME_OVER_VALUE, UpdateVoidRespawnGraceState);

    COND_VB_SHOULD(VB_TRIGGER_VOIDOUT, CVAR_VOID_DAMAGE_GAME_OVER_VALUE, {
        Player* player = va_arg(args, Player*);
        if (sPostRespawnGraceFrames > 0) {
            *should = false;
        } else {
            int32_t expectedVoidDamage = 16 * (1 << CVarGetInteger(CVAR_ENHANCEMENT("VoidDamageMult"), 0));
            bool lethal = gSaveContext.health <= expectedVoidDamage;
            if (lethal) {
                gSaveContext.health = 0;
                // The void-fall/walk-out action sets PLAYER_STATE1_IN_CUTSCENE (and, for some
                // triggers, PLAYER_STATE1_LOADING) to lock out player control while it plays out,
                // and normally clears them once the teleport/respawn completes. Since we're
                // blocking that, nothing else will ever clear them, which would otherwise
                // permanently lock Player_InBlockingCsMode (and thus the death sequence) out for
                // good -- Player_InBlockingCsMode treats either flag as blocking on its own.
                // PLAYER_STATE1_FLOOR_DISABLED has to go too: as long as it's set, func_80838FB8
                // re-triggers the entire void-fall action (re-setting IN_CUTSCENE right back) every
                // single frame there's no active transition, which is exactly the state blocking
                // the transition leaves us in -- an infinite re-fall loop otherwise.
                player->stateFlags1 &=
                    ~(PLAYER_STATE1_IN_CUTSCENE | PLAYER_STATE1_FLOOR_DISABLED | PLAYER_STATE1_LOADING);
                // gSaveContext.respawnFlag gets set (e.g. to -2 for the FLOOR_EFFECT_2 surface-void
                // case) BEFORE this hook even runs, as part of detecting the void floor -- regardless
                // of whether we go on to block the teleport. It's part of the persisted save data, so
                // if left set, "Continue" after a forced death reloads with it still pending, and the
                // game's own spawn logic reads it as "resume the void respawn" and immediately
                // re-triggers the whole sequence again on the fresh life.
                gSaveContext.respawnFlag = 0;
                sPendingForcedGameOver = true;
                sDiedFromForcedVoidOut = true;
                *should = false;
            }
        }
    });

    COND_VB_SHOULD(VB_FORCE_GAME_OVER_AIRBORNE, CVAR_VOID_DAMAGE_GAME_OVER_VALUE, {
        if (sPendingForcedGameOver) {
            sPendingForcedGameOver = false;
            *should = true;
        }
    });
}

static RegisterShipInitFunc initFunc(RegisterVoidDamageGameOver, { CVAR_VOID_DAMAGE_GAME_OVER_NAME });
