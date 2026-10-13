#pragma once

#include <cstdint>
#include <functional>

namespace Game {

// Power-on: the original's reset routine.
void PowerOn();

// One 60 Hz frame: the vblank (NMI) work, then one pass of the main loop. Returns how many NMIs ran;
// more than one when a routine waited for the screen to finish drawing (while loading a room with the
// screen off, for instance).
int RunFrame();

// Called at the start of every NMI, before the controllers are read. The app sets Hw::gPads here.
void SetNmiHook(std::function<void(int nmiIndex)> hook);

// The NMI handler, and running it in the middle of a routine that waits for it.
void Nmi();
void WaitForNmi();

// Total NMIs since power-on.
uint64_t NmiCount();

} // namespace Game
