#pragma once

#include "GameView.h"
#include "Gbi.h"
#include "Mat4.h"
#include "Model.h"

#include <array>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace Metroid {

// Presents the game in 3D. The play field comes straight from the video memory's nametables: every
// solid 8x8 background tile becomes a textured block, passable tiles become cards set back from
// the play plane. Sprites come from OAM and are drawn as flat cards, or replaced by models. Screens
// that aren't the play field (title, password, ending) are shown as the original 2D picture.
class Renderer {
  public:
    void Init();
    // Builds this frame's display list. alpha (0-1) interpolates between the previous and current
    // game frame. Valid until the next call.
    Gfx* BuildFrame(const GameView& emu, float alpha);

    void SetModelState(Gfx*& g);
    void DrawModel(Gfx*& g, const Gfx* dl, const Mat4& model);
    void DrawSkeleton(Gfx*& g, const Skeleton& skel, SkeletonPose& pose, const Mat4& model,
                      const std::function<const Gfx*(const std::string&)>& limbList);
    ModelLibrary& Models() {
        return mModels;
    }

  private:
    struct Quad {
        Vtx v[4];
    };
    // Geometry for one 64x64-pixel piece of a physical nametable, cached until its contents change.
    struct Chunk {
        uint64_t hash = 0;
        bool valid = false;
        std::vector<Vtx> vtx;
        std::vector<Gfx> dl;
    };

    const uint8_t* TileTexture(const GameView& emu, uint16_t patternAddr, const uint8_t* palette4, bool transparent);
    bool TileBlank(const GameView& emu, uint16_t patternAddr) const;
    void BuildChunk(const GameView& emu, Chunk& chunk, uint16_t ntBase, int cx, int cy);
    void EmitTexturedQuads(Gfx*& g, const uint8_t* tex, int size, const std::vector<Quad>& quads,
                           std::vector<Vtx>& store);
    void SetTexturedState(Gfx*& g, bool zbuffer);
    Mtx* AllocMtx(const Mat4& m);

    void DrawPlayField(Gfx*& g, const GameView& emu);
    void DrawSprites(Gfx*& g, const GameView& emu, bool hudPass, bool hideSamus);
    void DrawSamusModel(Gfx*& g, const GameView& emu, const Model& model, float alpha);
    void Draw2DFrame(Gfx*& g, const GameView& emu);
    void BeginOverlay(Gfx*& g);

    ModelLibrary mModels;
    std::unordered_map<std::string, std::unique_ptr<uint8_t[]>> mTextures;
    std::array<Chunk, 4 * 4 * 2> mChunks;
    uint64_t mGfxHash = 0;

    // The original picture, for 2D screens: 4x4 textures of 64x64 (one texture load each).
    std::array<std::unique_ptr<uint8_t[]>, 16> mFrameTex;
    std::vector<uint32_t> mFramePixels;

    // Samus's model animation clock.
    uint8_t mSamusAnim = 0xFF;
    uint64_t mSamusAnimStart = 0;

    // Per-frame storage (the interpreter consumes the list synchronously each frame).
    std::vector<Gfx> mFrameDl;
    std::vector<Vtx> mFrameVtx;
    std::vector<Mtx> mFrameMtx;
    Gfx* mFrameEnd = nullptr;
    Mtx mProjection;
    Mtx mView;
    Mat4 mViewMat;
    Mtx mOrtho;
    Mtx mIdentity;
    Vp mViewport;
};

} // namespace Metroid
