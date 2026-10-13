#pragma once

// Constants of the original game (named after the community disassembly).

#include <cstdint>

namespace Game::Const {

// --- Constants
// Bitmask defs used for SamusGear.
inline constexpr uint16_t gr_BOMBS = 0x01;
inline constexpr uint16_t gr_HIGHJUMP = 0x02;
inline constexpr uint16_t gr_LONGBEAM = 0x04;
inline constexpr uint16_t gr_SCREWATTACK = 0x08;
inline constexpr uint16_t gr_MARUMARI = 0x10;
inline constexpr uint16_t gr_VARIA = 0x20;
inline constexpr uint16_t gr_WAVEBEAM = 0x40;
inline constexpr uint16_t gr_ICEBEAM = 0x80;
// Samus action handlers.
inline constexpr uint16_t sa_Stand = 0x00;
inline constexpr uint16_t sa_Run = 0x01; // Also run and jump.
inline constexpr uint16_t sa_Jump = 0x02;
inline constexpr uint16_t sa_Roll = 0x03;
inline constexpr uint16_t sa_PntUp = 0x04;
inline constexpr uint16_t sa_Door = 0x05;
inline constexpr uint16_t sa_PntJump = 0x06;
inline constexpr uint16_t sa_Dead = 0x07;
inline constexpr uint16_t sa_Dead2 = 0x08;
inline constexpr uint16_t sa_Elevator = 0x09;
inline constexpr uint16_t sa_FadeIn0 = 0x14;
inline constexpr uint16_t sa_FadeIn1 = 0x15;
inline constexpr uint16_t sa_FadeIn2 = 0x16;
inline constexpr uint16_t sa_FadeIn3 = 0x17;
inline constexpr uint16_t sa_FadeIn4 = 0x18;
inline constexpr uint16_t sa_Begin = 0xFF;
// Animations
inline constexpr uint16_t an_SamusRun = 0x00;
inline constexpr uint16_t an_SamusFront = 0x04;
inline constexpr uint16_t an_SamusStand = 0x07;
inline constexpr uint16_t an_SamusJump = 0x0C;
inline constexpr uint16_t an_SamusSalto = 0x0E;
inline constexpr uint16_t an_SamusRunJump = 0x13;
inline constexpr uint16_t an_SamusRoll = 0x16;
inline constexpr uint16_t an_Bullet = 0x1B;
inline constexpr uint16_t an_SamusFireJump = 0x20;
inline constexpr uint16_t an_SamusFireRun = 0x22;
inline constexpr uint16_t an_SamusPntUp = 0x27;
inline constexpr uint16_t an_Explode = 0x32;
inline constexpr uint16_t an_SamusJmpPntUp = 0x35;
inline constexpr uint16_t an_SamusRunPntUp = 0x37;
inline constexpr uint16_t an_WaveBeam = 0x7D;
inline constexpr uint16_t an_BombTick = 0x7F;
inline constexpr uint16_t an_BombExplode = 0x82;
inline constexpr uint16_t an_MissileLeft = 0x8B;
inline constexpr uint16_t an_MissileRight = 0x8D;
inline constexpr uint16_t an_MissileExpld = 0x91;
// Weapon action handlers.
inline constexpr uint16_t wa_RegularBeam = 0x01;
inline constexpr uint16_t wa_WaveBeam = 0x02;
inline constexpr uint16_t wa_IceBeam = 0x03;
inline constexpr uint16_t wa_BulletExplode = 0x04;
inline constexpr uint16_t wa_LayBomb = 0x08;
inline constexpr uint16_t wa_BombCount = 0x09;
inline constexpr uint16_t wa_BombExplode = 0x0A;
inline constexpr uint16_t wa_Missile = 0x0B;
// Hardware registers.
inline constexpr uint16_t PPU_PT0_UB = 0x00; // Base address of pattern table 0, upper byte.
inline constexpr uint16_t PPU_PT0_LB = 0x00; // Base address of pattern table 0, lower byte.
inline constexpr uint16_t PPU_PT1_UB = 0x10; // Base address of pattern table 1, upper byte.
inline constexpr uint16_t PPU_PT1_LB = 0x00; // Base address of pattern table 1, lower byte.
inline constexpr uint16_t PPU_NT0_UB = 0x20; // Base address of name table 0, upper byte.
inline constexpr uint16_t PPU_NT0_LB = 0x00; // Base address of name table 0, lower byte.
inline constexpr uint16_t PPU_AT0_UB = 0x23; // Base address of attribute table 0, upper byte.
inline constexpr uint16_t PPU_AT0_LB = 0xC0; // Base address of attribute table 0, lower byte.
inline constexpr uint16_t PPU_NT1_UB = 0x24; // Base address of name table 1, upper byte.
inline constexpr uint16_t PPU_NT1_LB = 0x00; // Base address of name table 1, lower byte.
inline constexpr uint16_t PPU_PAL_UB = 0x3F; // Base address of palettes, upper byte.
inline constexpr uint16_t PPU_PAL_LB = 0x00; // Base address of palettes, lower byte.
inline constexpr uint16_t SUIT_ON = 0x00; // Samus suit on.
inline constexpr uint16_t SUIT_OFF = 0x01; // Samus suit off.
// Music and SFX flags.
inline constexpr uint16_t SFX_SMS_DIE = 0x80; // Samus die SFX.
inline constexpr uint16_t SFX_MSL_PKUP = 0x80; // Missile pickup SFX.
inline constexpr uint16_t SFX_SCRW_ATK = 0x40; // Screw attack SFX.
inline constexpr uint16_t SFX_ENGY_PKUP = 0x40; // Energy pickup SFX.
inline constexpr uint16_t SFX_DOOR = 0x40; // Door open/close SFX.
inline constexpr uint16_t SFX_MSL_LNCH = 0x20; // Missile launch SFX.
inline constexpr uint16_t SFX_METAL = 0x20; // Invincible enemy hit SFX.
inline constexpr uint16_t SFX_MTRD_HIT = 0x20; // Metroid hit SFX.
inline constexpr uint16_t MUS_END = 0x20; // End game music.
inline constexpr uint16_t SFX_BMB_XPLD = 0x10; // Bomb explode SFX.
inline constexpr uint16_t SFX_BLT_FIRE = 0x10; // Bullet fire SFX.
inline constexpr uint16_t SFX_ST_RAISE = 0x10; // Statue raise SFX.
inline constexpr uint16_t SFX_SMS_WLK = 0x08; // Samus walk SFX.
inline constexpr uint16_t SFX_EN_REGEN = 0x08; // Enemy regenerate SFX.
inline constexpr uint16_t SFX_BEEP = 0x08; // Low health beep SFX.
inline constexpr uint16_t SFX_SPT_FLM = 0x04; // Spit flame SFX.
inline constexpr uint16_t SFX_EN_HIT = 0x04; // Enemy hit SFX.
inline constexpr uint16_t SFX_BIG_EN_HIT = 0x04; // Big enemy hit SFX.
inline constexpr uint16_t SFX_SMS_HIT = 0x04; // Samus hit SFX.
inline constexpr uint16_t SFX_SMS_JMP = 0x02; // Samus jump SFX.
inline constexpr uint16_t SFX_SMS_BALL = 0x02; // Samus morph into ball SFX.
inline constexpr uint16_t SFX_BOSS_HIT = 0x02; // Boss hit SFX.
inline constexpr uint16_t SFX_WV_FIRE = 0x01; // Wave beam SFX.
inline constexpr uint16_t SFX_BMB_SET = 0x01; // Bomb launced SFX.
inline constexpr uint16_t SFX_BAD_PSWD = 0x01; // Incorrect password SFX.
inline constexpr uint16_t MUS_FADE_IN = 0x80; // Fade in music.
inline constexpr uint16_t MUS_RIDLEY = 0x80; // Ridley hideout music.
inline constexpr uint16_t MUS_PWR_UP = 0x40; // Power up music.
inline constexpr uint16_t MUS_TOURIAN = 0x40; // Tourian area music
inline constexpr uint16_t MUS_ITM_ROOM = 0x20; // Item room music.
inline constexpr uint16_t MUS_INTRO = 0x10; // Intro music.
inline constexpr uint16_t MUS_KRAID = 0x10; // Kraid hideout music.
inline constexpr uint16_t MUS_NORFAIR = 0x08; // Norfair area music.
inline constexpr uint16_t MUS_ESCAPE = 0x04; // End escape music.
inline constexpr uint16_t MUS_BOSS = 0x02; // Boss room music.
inline constexpr uint16_t MUS_PAUSE = 0x02; // Pause music.
inline constexpr uint16_t MUS_BRINSTAR = 0x01; // Brinstar area music.
inline constexpr uint16_t MUS_NONE = 0x01; // Silence music.
// Music control.
inline constexpr uint16_t MUS_REPEAT = 0xFF; // Repeat music.
inline constexpr uint16_t MUS_NO_REPEAT = 0x00; // Do not repeat music.
inline constexpr uint16_t MUS_NLT_0 = 0x00; // Note length table 0.
inline constexpr uint16_t MUS_NLT_1 = 0x0B; // Note length table 1.
inline constexpr uint16_t MUS_NLT_2 = 0x17; // Note length table 2.
inline constexpr uint16_t VOL_TBL_NONE = 0x00; // Channel does not use volume data table.
inline constexpr uint16_t VOL_TBL1 = 0x01; // Channel uses volume data table 1.
inline constexpr uint16_t VOL_TBL2 = 0x02; // Channel uses volume data table 2.
inline constexpr uint16_t VOL_TBL3 = 0x03; // Channel uses volume data table 5.
inline constexpr uint16_t VOL_TBL4 = 0x04; // Channel uses volume data table 4.
inline constexpr uint16_t VOL_TBL5 = 0x05; // Channel uses volume data table 5.
inline constexpr uint16_t NSE_NONE = 0x00; // No noise channel music data.
inline constexpr uint16_t SQ1_NONE = 0x100; // No SQ1 channel music data.
inline constexpr uint16_t SQ2_NONE = 0x300; // No SQ2 channel music data.
inline constexpr uint16_t TRI_NONE = 0x500; // No triangle channel music data.
inline constexpr uint16_t NSE_NONE1 = 0x700; // No noise channel music data.
inline constexpr uint16_t CHN_NOISE = 0x00; // Noise channel indicator.
inline constexpr uint16_t CHN_SQ1 = 0x01; // SQ1 channel indicator.
inline constexpr uint16_t CHN_TRI = 0x03; // Triangle channel indicator.
inline constexpr uint16_t CHN_MULTI = 0x04; // Multiple channels indicator.
// Controller inputs.
inline constexpr uint16_t D_PAD = 0x0F; // All bits for D-pad input.
inline constexpr uint16_t BTN_RIGHT = 0x01; // Controller D-pad right.
inline constexpr uint16_t BTN_LEFT = 0x02; // Controller D-pad left.
inline constexpr uint16_t BTN_DOWN = 0x04; // Controller D-pad down.
inline constexpr uint16_t BTN_UP = 0x08; // Controller D-pad up.
inline constexpr uint16_t BTN_START = 0x10; // Controller button start.
inline constexpr uint16_t BTN_SELECT = 0x20; // Controller button select.
inline constexpr uint16_t BTN_B = 0x40; // Controller button B.
inline constexpr uint16_t BTN_A = 0x80; // Controller button A.
inline constexpr uint16_t ___ = 0xFF; // Used in world map to make it more readable.

} // namespace Game::Const
