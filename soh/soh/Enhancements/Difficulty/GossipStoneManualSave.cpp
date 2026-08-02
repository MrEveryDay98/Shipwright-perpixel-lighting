#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/ShipInit.hpp"
#include "functions.h"
#include "macros.h"
#include "variables.h"

extern "C" PlayState* gPlayState;

#define CVAR_GOSSIP_STONE_SAVE_NAME CVAR_ENHANCEMENT("GossipStoneManualSave")
#define CVAR_GOSSIP_STONE_SAVE_DEFAULT 0
#define CVAR_GOSSIP_STONE_SAVE_VALUE CVarGetInteger(CVAR_GOSSIP_STONE_SAVE_NAME, CVAR_GOSSIP_STONE_SAVE_DEFAULT)

// Set true only for the duration of the Play_PerformSave call below, so the VB_PERFORM_SAVE
// hook (which wraps the actual save-file write for every caller: pause menu, forced death
// save, periodic autosave, and this one) can tell this specific call apart from all the
// others without needing to touch any of those other call sites individually. This means the
// vanilla pause-menu/death-screen UI flow (including its "Saved!" confirmation) runs
// completely untouched -- it just silently doesn't write a file when triggered any other way.
static bool sGossipStoneSaveInProgress = false;

static void OnGossipStoneSwordHit(void* refActor) {
    sGossipStoneSaveInProgress = true;
    Play_PerformSave(gPlayState);
    sGossipStoneSaveInProgress = false;

    // Same jingle vanilla plays on a successful pause-menu/death-screen save, so hitting a
    // stone gives the same audible "saved" confirmation those normally would.
    Audio_PlaySoundGeneral(NA_SE_SY_PIECE_OF_HEART, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                           &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
}

static void RegisterGossipStoneManualSave() {
    COND_HOOK(OnGossipStoneSwordHit, CVAR_GOSSIP_STONE_SAVE_VALUE, OnGossipStoneSwordHit);

    COND_VB_SHOULD(VB_PERFORM_SAVE, CVAR_GOSSIP_STONE_SAVE_VALUE, { *should = sGossipStoneSaveInProgress; });
}

static RegisterShipInitFunc initFunc(RegisterGossipStoneManualSave, { CVAR_GOSSIP_STONE_SAVE_NAME });
