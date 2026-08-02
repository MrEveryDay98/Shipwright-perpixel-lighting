#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/ShipInit.hpp"
#include "functions.h"
#include "macros.h"

extern "C" PlayState* gPlayState;

#define CVAR_PLAYER_DAMAGE_FREQ_MULT_NAME CVAR_ENHANCEMENT("PlayerDamageFrequencyMult")
#define CVAR_PLAYER_DAMAGE_FREQ_MULT_DEFAULT 1.0f
#define CVAR_PLAYER_DAMAGE_FREQ_MULT_VALUE CVarGetFloat(CVAR_PLAYER_DAMAGE_FREQ_MULT_NAME, CVAR_PLAYER_DAMAGE_FREQ_MULT_DEFAULT)

// Runs each frame after Player_UpdateCommon has already applied its normal invincibilityTimer
// decrement, and ticks it down extra times so it reaches 0 (and Link can be hit again) sooner.
static void ApplyPlayerDamageFrequencyMult() {
    Player* player = GET_PLAYER(gPlayState);

    // Positive invincibilityTimer is post-hit intangibility; negative is dodge/roll invulnerability,
    // which this slider intentionally leaves alone.
    if (player->invincibilityTimer <= 0) {
        return;
    }

    float extraDecrements = CVAR_PLAYER_DAMAGE_FREQ_MULT_VALUE - 1.0f;
    int32_t wholeDecrements = (int32_t)extraDecrements;
    float fractionalDecrement = extraDecrements - wholeDecrements;
    int32_t decrementsToApply = wholeDecrements + ((Rand_ZeroOne() < fractionalDecrement) ? 1 : 0);

    for (int32_t i = 0; i < decrementsToApply && player->invincibilityTimer > 0; i++) {
        player->invincibilityTimer--;
    }
}

static void UpdatePlayerDamageFrequencyState() {
    COND_HOOK(OnPlayerUpdate, CVAR_PLAYER_DAMAGE_FREQ_MULT_VALUE > 1.0f, ApplyPlayerDamageFrequencyMult);
}

static RegisterShipInitFunc initFunc(UpdatePlayerDamageFrequencyState, { CVAR_PLAYER_DAMAGE_FREQ_MULT_NAME });
