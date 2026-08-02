#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/ShipInit.hpp"
#include "functions.h"
#include "macros.h"

extern "C" {
extern PlayState* gPlayState;
}

#define CVAR_PLAYER_HURT_BLOOD_NAME CVAR_ENHANCEMENT("PlayerHurtBloodEffect")
#define CVAR_PLAYER_HURT_BLOOD_DEFAULT 0
#define CVAR_PLAYER_HURT_BLOOD_VALUE CVarGetInteger(CVAR_PLAYER_HURT_BLOOD_NAME, CVAR_PLAYER_HURT_BLOOD_DEFAULT)

// Deliberately NOT nested under "PlayerHurtBloodEffect" (e.g. "PlayerHurtBloodEffect.GameOverInterval")
// -- that CVar is itself a leaf value (the checkbox above), and the underlying Config store
// (libultraship's Config::Save, which flattens/unflattens to a nested JSON tree) can't have a key
// be both a leaf and a parent of other keys at once. Doing so throws inside nlohmann::json's
// unflatten() the next time anything saves, uncaught, after Config::Save() has already truncated
// the file open for writing -- crashing the game and wiping the whole settings file with it.
#define CVAR_PLAYER_HURT_BLOOD_GAME_OVER_INTERVAL_NAME CVAR_ENHANCEMENT("PlayerHurtBloodGameOverInterval")
#define CVAR_PLAYER_HURT_BLOOD_GAME_OVER_INTERVAL_DEFAULT 0
#define CVAR_PLAYER_HURT_BLOOD_GAME_OVER_INTERVAL_VALUE \
    CVarGetInteger(CVAR_PLAYER_HURT_BLOOD_GAME_OVER_INTERVAL_NAME, CVAR_PLAYER_HURT_BLOOD_GAME_OVER_INTERVAL_DEFAULT)

// Modeled on CollisionCheck_BlueBlood/CollisionCheck_RedBlood (z_collision_check.c), which spawn
// this same EFFECT_SPARK burst for sword hits on flesh-type colliders (e.g. Redead/Gibdo,
// Jabu-Jabu's inner walls) -- recolored dark red here since Link's own blood shouldn't read as
// either of those (blue, or red-with-orange-highlights).
static void SpawnPlayerHurtBlood(PlayState* play, Vec3f* pos) {
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

// Counts down between spurts while PLAYER_STATE1_DEAD is set (i.e. for the duration of the Game
// Over death animation, from func_80836448 until a fresh Player_Init on respawn/reload clears it).
static s32 sGameOverBloodTimer = 0;

static void UpdatePlayerHurtBloodGameOver() {
    s32 interval = CVAR_PLAYER_HURT_BLOOD_GAME_OVER_INTERVAL_VALUE;
    Player* player = GET_PLAYER(gPlayState);

    if (interval <= 0 || !(player->stateFlags1 & PLAYER_STATE1_DEAD)) {
        sGameOverBloodTimer = 0;
        return;
    }

    if (sGameOverBloodTimer <= 0) {
        SpawnPlayerHurtBlood(gPlayState, &player->bodyPartsPos[PLAYER_BODYPART_WAIST]);
        sGameOverBloodTimer = interval;
    } else {
        sGameOverBloodTimer--;
    }
}

static void RegisterPlayerHurtBlood() {
    COND_VB_SHOULD(VB_PLAYER_TAKE_DAMAGE_EFFECT, CVAR_PLAYER_HURT_BLOOD_VALUE, {
        PlayState* play = va_arg(args, PlayState*);
        Player* player = va_arg(args, Player*);
        SpawnPlayerHurtBlood(play, &player->bodyPartsPos[PLAYER_BODYPART_WAIST]);
    });

    COND_HOOK(OnPlayerUpdate, CVAR_PLAYER_HURT_BLOOD_GAME_OVER_INTERVAL_VALUE > 0, UpdatePlayerHurtBloodGameOver);
}

static RegisterShipInitFunc initFunc(RegisterPlayerHurtBlood,
                                     { CVAR_PLAYER_HURT_BLOOD_NAME, CVAR_PLAYER_HURT_BLOOD_GAME_OVER_INTERVAL_NAME });
