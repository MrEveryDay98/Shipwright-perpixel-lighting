#pragma once

#include "Game.h"
#include "Gbi.h"
#include "Mat4.h"

#include <array>
#include <memory>
#include <unordered_map>
#include <vector>

namespace Metroid {

// Turns the decoded NES world into 3D: every 8x8 background tile becomes a textured block
// (solid tiles) or a textured card set back from the play plane (decoration), viewed by a
// perspective camera that follows Samus.
class Renderer {
  public:
    void Init(const Rom* rom);
    // Builds this frame's display list. Valid until the next call.
    Gfx* BuildFrame(const Game& game);

  private:
    struct Quad {
        Vtx v[4];
    };
    struct ScreenMesh {
        bool built = false;
        std::vector<Vtx> vtx;
        std::vector<Gfx> dl;
    };

    const uint8_t* TileTexture(Area area, bool alt, uint8_t chr, uint8_t pal, bool transparent);
    const uint8_t* SamusTexture(int frame, bool flip);
    void BuildScreen(int sx, int sy);
    void EmitTexturedQuads(Gfx*& g, const uint8_t* tex, int w, int h, const std::vector<Quad>& quads,
                           std::vector<Vtx>& store);
    void EmitColorQuads(Gfx*& g, const std::vector<Quad>& quads);

    void AddTileBlock(std::vector<Quad>& out, int tx, int ty, bool cullNeighbours);
    void AddBox(std::vector<Quad>& out, float x0, float y0, float z0, float x1, float y1, float z1, uint32_t rgba);

    const Rom* mRom = nullptr;
    std::array<ScreenMesh, kMapW * kMapH> mScreens;
    std::unordered_map<uint64_t, std::unique_ptr<uint8_t[]>> mTextures;
    std::unordered_map<int, std::unique_ptr<uint8_t[]>> mSamusTextures;

    // Per-frame storage (the interpreter consumes the list synchronously each frame).
    std::vector<Gfx> mFrameDl;
    std::vector<Vtx> mFrameVtx;
    Mtx mProjection;
    Mtx mView;
    Vp mViewport;
    Vec3 mCamera{};
    bool mCameraInit = false;
};

} // namespace Metroid
