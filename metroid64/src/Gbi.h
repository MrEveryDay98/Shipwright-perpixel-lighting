#pragma once

// libultraship leaves the pointer-taking GBI commands to the port (they are where a port would
// resolve archive paths). Metroid64 only ever passes real heap pointers, so these forward directly.

#include <libultraship/libultraship.h>

inline void gSPVertex(Gfx* pkt, const Vtx* v, int n, int v0) {
    __gSPVertex(pkt, (uintptr_t)v, n, v0);
}

inline void gSPDisplayList(Gfx* pkt, const Gfx* dl) {
    __gSPDisplayList(pkt, (uintptr_t)dl);
}
