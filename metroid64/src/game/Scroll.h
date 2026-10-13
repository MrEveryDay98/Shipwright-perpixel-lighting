#pragma once

// Bank 7: door scrolling, lava, Samus's movement and screen scrolling with name table updates.

#include <cstdint>

namespace Game {

void ScrollDoor();
uint8_t VertRoomCentered();
bool IsSamusInLava();
void LavaAndMoveCheck();
void SamusMoveVertically();
void SamusMoveHorizontally();
void CheckStopHorzMvmt(bool moved);
void VertAccelerate();
void HorzAccelerate();
bool MoveSamusUp();
bool MoveSamusDown();
bool MoveSamusLeft();
bool MoveSamusRight();
bool ScrollUp();
bool ScrollDown();
bool ScrollLeft();
bool ScrollRight();
void CheckNameTableUpdate();
void VertNameTableUpdate();
void HorzNameTableUpdate();
void WriteNameTableLine(uint8_t hi);
uint8_t GetNameAddrs(uint8_t& wramHi);
void WritePPUAttribTbl();

} // namespace Game
