#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/ShipInit.hpp"
#include "functions.h"
#include "macros.h"
#include "variables.h"

extern "C" {
extern PlayState* gPlayState;
// SaveManager.h's free-function declarations (as opposed to the SaveManager class's own,
// C++-only methods) are only exposed to plain C translation units; Save_Exist's underlying
// symbol is still plain extern "C" though; see soh/SaveManager.cpp.
extern bool Save_Exist(int fileNum);
}

#define CVAR_GOSSIP_STONE_SAVE_NAME CVAR_ENHANCEMENT("GossipStoneManualSave")
#define CVAR_GOSSIP_STONE_SAVE_DEFAULT 0
#define CVAR_GOSSIP_STONE_SAVE_VALUE CVarGetInteger(CVAR_GOSSIP_STONE_SAVE_NAME, CVAR_GOSSIP_STONE_SAVE_DEFAULT)

#define CVAR_GAME_OVER_RELOAD_SAVE_NAME CVAR_ENHANCEMENT("GameOverSkipPromptsReload")
#define CVAR_GAME_OVER_RELOAD_SAVE_DEFAULT 0
#define CVAR_GAME_OVER_RELOAD_SAVE_VALUE CVarGetInteger(CVAR_GAME_OVER_RELOAD_SAVE_NAME, CVAR_GAME_OVER_RELOAD_SAVE_DEFAULT)

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

// Mirrors the transient-state reset FileChoose_LoadGame (z_file_choose.c) performs right after
// Sram_OpenSave(), since that's normally the only entry point into "start playing from a freshly
// loaded save file" and this hook is a second one, invoked mid-game from the death screen instead
// of the file-select screen.
static void ResetTransientStateForFreshLoad() {
    gSaveContext.respawn[RESPAWN_MODE_DOWN].entranceIndex = ENTR_LOAD_OPENING;
    gSaveContext.respawnFlag = 0;
    gSaveContext.seqId = (u8)NA_BGM_DISABLED;
    gSaveContext.natureAmbienceId = 0xFF;
    gSaveContext.showTitleCard = true;
    if (!CVarGetInteger(CVAR_ENHANCEMENT("DogFollowsEverywhere"), 0)) {
        gSaveContext.dogParams = 0;
    }
    gSaveContext.timerState = TIMER_STATE_OFF;
    gSaveContext.subTimerState = SUBTIMER_STATE_OFF;
    gSaveContext.eventInf[0] = 0;
    gSaveContext.eventInf[1] = 0;
    gSaveContext.eventInf[2] = 0;
    gSaveContext.eventInf[3] = 0;
    gSaveContext.prevHudVisibilityMode = 0x32;
    gSaveContext.nayrusLoveTimer = 0;
    gSaveContext.healthAccumulator = 0;
    gSaveContext.magicState = MAGIC_STATE_IDLE;
    gSaveContext.prevMagicState = MAGIC_STATE_IDLE;
    gSaveContext.forcedSeqId = NA_BGM_GENERAL_SFX;
    gSaveContext.skyboxTime = 0;
    gSaveContext.nextTransitionType = TRANS_NEXT_TYPE_DEFAULT;
    gSaveContext.nextCutsceneIndex = 0xFFEF;
    gSaveContext.cutsceneTrigger = 0;
    gSaveContext.chamberCutsceneNum = 0;
    gSaveContext.nextDayTime = 0xFFFF;
    gSaveContext.retainWeatherMode = 0;

    for (int buttonIndex = 0; buttonIndex < ARRAY_COUNT(gSaveContext.buttonStatus); buttonIndex++) {
        gSaveContext.buttonStatus[buttonIndex] = BTN_ENABLED;
    }
    gSaveContext.forceRisingButtonAlphas = 0;
    gSaveContext.nextHudVisibilityMode = 0;
    gSaveContext.hudVisibilityMode = 0;
    gSaveContext.hudVisibilityModeTimer = 0;
    gSaveContext.magicCapacity = 0;
    gSaveContext.magicFillTarget = gSaveContext.magic;
    gSaveContext.magic = 0;
    gSaveContext.magicLevel = gSaveContext.magic;
    gSaveContext.naviTimer = 0;
}

// Save_LoadFile()/Sram_OpenSave() assume a save file actually exists for gSaveContext.fileNum and
// crash trying to parse one that doesn't (e.g. fileNum 255, the debug save, which only ever
// exists in memory and is never written to disk). Guard both reload hooks below with this so an
// empty/nonexistent slot degrades to the normal vanilla prompts/in-place respawn instead of
// crashing.
static bool CurrentSaveFileExistsOnDisk() {
    return Save_Exist(gSaveContext.fileNum);
}

// Actually transitions into the save file already loaded by VB_TRANSITION_TO_SAVE_SCREEN_ON_DEATH
// below (at the point "Save?" would have shown, a whole fade-animation's worth of frames ago),
// instead of the vanilla death-screen "Continue" which just respawns using whatever's still in
// memory. Called from the VB_CONTINUE_RESPAWNS_IN_PLACE hook below, which blocks that in-memory
// respawn so this can take over instead.
//
// This deliberately does NOT call Sram_OpenSave() itself: doing the disk load and the gamestate
// flip back to back, on the same frame, crashed. BetterSaveMenu.cpp's equivalent flow (the only
// other place in the codebase that reloads a save mid-game) calls Sram_OpenSave() a full
// fade-animation's worth of frames before its own SET_NEXT_GAMESTATE, so gSaveContext has already
// been sitting in its new, loaded state for a while by the time anything else that frame --
// rendering included -- reads it, instead of changing out from under whatever's still mid-use.
// Loading the save at the same earlier point (right as the death screen would show "Save?")
// matches that same gap.
static void FinishSaveReloadContinue(PlayState* play) {
    ResetTransientStateForFreshLoad();

    // Matches the same fields the vanilla in-place respawn this replaces would otherwise have set.
    play->gameplayFrames = 0;
    gSaveContext.nextTransitionType = TRANS_TYPE_FADE_BLACK;
    Audio_QueueSeqCmd(0xF << 28 | SEQ_PLAYER_BGM_MAIN << 24 | 0xA);

    play->state.running = 0;
    SET_NEXT_GAMESTATE(&play->state, Play_Init, PlayState);
    GameInteractor_ExecuteOnLoadGame(gSaveContext.fileNum);
}

static void RegisterGossipStoneManualSave() {
    COND_HOOK(OnGossipStoneSwordHit, CVAR_GOSSIP_STONE_SAVE_VALUE, OnGossipStoneSwordHit);

    COND_VB_SHOULD(VB_PERFORM_SAVE, CVAR_GOSSIP_STONE_SAVE_VALUE, { *should = sGossipStoneSaveInProgress; });

    // Once saving only happens at Gossip Stones, the pause-menu "Save?" prompt no longer does
    // anything meaningful, so skip straight past it (same as pressing Start already does).
    COND_VB_SHOULD(VB_OPEN_PAUSE_SAVE_PROMPT, CVAR_GOSSIP_STONE_SAVE_VALUE, { *should = false; });

    COND_VB_SHOULD(VB_TRANSITION_TO_SAVE_SCREEN_ON_DEATH, CVAR_GAME_OVER_RELOAD_SAVE_VALUE, {
        // No file to reload from (e.g. playing on the in-memory-only debug save) -- fall back to
        // the normal "Save?"/"Continue?" prompts and in-place respawn entirely.
        if (CurrentSaveFileExistsOnDisk()) {
            PauseContext* pauseCtx = va_arg(args, PauseContext*);
            // Load the save file here rather than at the "Continue?" fade below, so gSaveContext
            // has settled into its new state well before the actual gamestate transition -- see
            // FinishSaveReloadContinue's comment for why that gap matters.
            Sram_OpenSave();
            // Skip past both the "Save?" prompt and the "Saved!" confirmation (state 0xF, which
            // vanilla otherwise shows for a few seconds or until a button press) straight to
            // "Continue?". The "Continue?" prompt itself still shows normally; pressing it is what
            // VB_CONTINUE_RESPAWNS_IN_PLACE below turns into finishing the save-file reload
            // instead of an in-place respawn.
            pauseCtx->state = 0x10;
            *should = false;
        }
    });

    COND_VB_SHOULD(VB_CONTINUE_RESPAWNS_IN_PLACE, CVAR_GAME_OVER_RELOAD_SAVE_VALUE, {
        if (CurrentSaveFileExistsOnDisk()) {
            PlayState* play = va_arg(args, PlayState*);
            FinishSaveReloadContinue(play);
            *should = false;
        }
    });
}

static RegisterShipInitFunc initFunc(RegisterGossipStoneManualSave,
                                     { CVAR_GOSSIP_STONE_SAVE_NAME, CVAR_GAME_OVER_RELOAD_SAVE_NAME });
