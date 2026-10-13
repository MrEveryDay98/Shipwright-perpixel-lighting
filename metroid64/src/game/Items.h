#pragma once

// Bank 7: Kraid and Ridley statues, power-up items and the unique item history.

#include <cstdint>

namespace Game {

void UpdateStatues();
void UpdateStatue(uint8_t x);
void StatueRise(uint8_t x);
void StatueSound(uint8_t x);
bool DrawStatueTiles(uint8_t y);
void CheckStatuesRaised();
uint8_t MakeBitMask(uint8_t y);
void UpdateItems();
void CheckOneItem(uint8_t x);
void MissileEnergyPickup(uint8_t item);
void GetItemXYPos();
void GetItemXYPosFrom(uint8_t mapX);
void AddItemToHistory();
void StoreItemInHistory();
void CreateItemID();
bool AreObjectsTouching(uint8_t x, uint8_t y);

} // namespace Game
