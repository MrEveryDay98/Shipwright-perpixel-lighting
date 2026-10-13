#pragma once

// Enemy and enemy-object variables the community disassembly leaves unnamed. They are named after their
// address; the comments say what the enemy code uses them for. Enemy slots are 16 bytes apart ($00-$50
// enemies, $60-$B0 enemy projectiles, $C0-$D8 explosion pieces 8 bytes apart, $F0 Mellows).

#include "game/Memory.h"

namespace Game::Vars {

inline constexpr Var En0402{ 0x0402 }; // Vertical speed (or table index for table-driven movement).
inline constexpr Var En0403{ 0x0403 }; // Horizontal speed.
inline constexpr Var En0404{ 0x0404 }; // Hit flags: what touched the enemy this frame.
inline constexpr Var En0405{ 0x0405 }; // Direction and state bits (bit 1: on screen).
inline constexpr Var En0407{ 0x0407 }; // Horizontal fraction / piece counter.
inline constexpr Var En0408{ 0x0408 }; // Movement pattern index.
inline constexpr Var En040A{ 0x040A }; // Projectile pattern / direction.
inline constexpr Var En040C{ 0x040C }; // Status saved while frozen or hit.
inline constexpr Var En040D{ 0x040D }; // Timer (frozen, item drop, attack delay).
inline constexpr Var En040E{ 0x040E }; // Weapon (ObjAction) that hit the enemy.
inline constexpr Var En6AFC{ 0x6AFC }; // Vertical sub-pixel accumulator.
inline constexpr Var En6AFD{ 0x6AFD }; // Horizontal sub-pixel accumulator.
inline constexpr Var En6AFE{ 0x6AFE }; // Vertical acceleration.
inline constexpr Var En6AFF{ 0x6AFF }; // Horizontal acceleration.
inline constexpr Var En6B00{ 0x6B00 };
inline constexpr Var En6B01{ 0x6B01 };
inline constexpr Var En6B03{ 0x6B03 };

// Memus ($B0-$CF, four of 8 bytes) and the spinning "skree" pieces ($A0-$AF, four of 4 bytes).
inline constexpr Var MemuStatus{ 0x00B0 };
inline constexpr Var MemuY{ 0x00B1 };
inline constexpr Var MemuX{ 0x00B2 };
inline constexpr Var MemuHi{ 0x00B3 };
inline constexpr Var MemuDir{ 0x00B4 };
inline constexpr Var MemuCounter{ 0x00B5 };
inline constexpr Var MemuHit{ 0x00B6 };
inline constexpr Var PieceTimer{ 0x00A0 };
inline constexpr Var PieceY{ 0x00A1 };
inline constexpr Var PieceX{ 0x00A2 };
inline constexpr Var PieceHi{ 0x00A3 };

inline constexpr Var ObjHitBy{ 0x010F }; // What hit Samus last ($80 enemy, $C0 Memu, $E0 blast/fire).

} // namespace Game::Vars
