#pragma once

#include "Game.h"
#include "Gbi.h"
#include "Mat4.h"
#include "Visual.h"

#include <array>
#include <memory>
#include <unordered_map>
#include <vector>

namespace Metroid {

// Turns the decoded NES world into 3D: every 8x8 background tile becomes a textured block
// (solid tiles) or a textured card set back from the play plane (decoration). Characters are
// drawn through Visuals: flat ROM sprites, or models when provided.
class Renderer {
  public:
    void Init(const Rom* rom);
    // Builds this frame's display list. alpha (0-1) interpolates between the previous and current
    // game tick. Valid until the next call.
    Gfx* BuildFrame(const Game& game, float alpha);

    // Used by Visuals.
    void DrawSprite(Gfx*& g, const SpriteFrame& f, float originX, float originY, bool mirror, float angleDeg,
                    uint32_t tint, float z = 0.0f);
    void DrawModel(Gfx*& g, const Gfx* dl, const Mat4& model);
    VisualLibrary& Visuals() {
        return mVisuals;
    }

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
    const uint8_t* SpriteTexture(const SpriteFrame& f);
    void BuildScreen(int sx, int sy);
    void EmitTexturedQuads(Gfx*& g, const uint8_t* tex, int size, const std::vector<Quad>& quads,
                           std::vector<Vtx>& store);
    void EmitColorQuads(Gfx*& g, const std::vector<Quad>& quads);
    void SetSpriteState(Gfx*& g);
    void SetColorState(Gfx*& g);
    Mtx* AllocMtx(const Mat4& m);

    void AddTileBlock(std::vector<Quad>& out, int tx, int ty, bool cullNeighbours);
    void AddBox(std::vector<Quad>& out, float x0, float y0, float z0, float x1, float y1, float z1, uint32_t rgba);
    void AddFlat(std::vector<Quad>& out, float cx, float cy, float half, float z, uint32_t rgba);
    void UpdateCamera(const Game& game);

    const Rom* mRom = nullptr;
    VisualLibrary mVisuals;
    std::array<ScreenMesh, kMapW * kMapH> mScreens;
    std::unordered_map<uint64_t, std::unique_ptr<uint8_t[]>> mTextures;
    std::unordered_map<const SpriteFrame*, std::unique_ptr<uint8_t[]>> mSpriteTextures;

    // Per-frame storage (the interpreter consumes the list synchronously each frame).
    std::vector<Gfx> mFrameDl;
    std::vector<Vtx> mFrameVtx;
    std::vector<Mtx> mFrameMtx;
    Gfx* mFrameEnd = nullptr;
    Mtx mProjection;
    Mtx mView;
    Vp mViewport;

    // Camera, stepped once per game tick and interpolated per rendered frame.
    Vec3 mCamPrev{}, mCamCur{};
    int mCamTick = -1;
};

} // namespace Metroid
