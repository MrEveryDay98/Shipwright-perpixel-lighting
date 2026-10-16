#pragma once

#include "GameView.h"
#include "Gbi.h"
#include "Mat4.h"
#include "Model.h"
#include "game/data/RoomMesh.h"

#include <array>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace Metroid {

// Presents the game in 3D. The play field is the scrolling section Samus is in (game/data/Layout.h), drawn
// whole from its room mesh: solid 8x8 background tiles are textured blocks, passable tiles cards set back
// from the play plane. What the game has drawn differently in room RAM (blasted blocks...) replaces the
// mesh there. Going through a door, the section fades out and the next one fades in. Sprites come from
// OAM and are drawn as flat cards, or replaced by models. Screens that aren't the play field (title,
// password, ending) are shown as the original 2D picture.
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

    // A section's 8x8-tile block: the mesh drawn there, and its display list for the current tiles.
    struct SectionChunk {
        Game::Data::MeshChunk base; // from the room mesh
        Game::Data::MeshChunk live; // remade from room RAM, when that differs
        bool useLive = false;
        std::vector<Vtx> vtx;
        std::vector<Gfx> dl;
        uint64_t dlHash = 0; // the tile pictures the list was made with (0: not made)
    };
    struct SectionView {
        bool valid = false;
        Game::Data::Section section;
        std::vector<Game::Data::ScreenTiles> layout, live;
        std::vector<SectionChunk> chunks; // [screen * 16 + cy * 4 + cx]
        uint64_t pageHash[2] = {};
        float fade = 0; // opacity
        // Where the top-left of the screen is in the section: found once, when a name table page shows one
        // of its rooms at the left (or top) of the screen, then moved by the scroll each game frame (wrapped
        // by the name tables the mirroring makes distinct, so flipping a mirrored bit moves nothing). A
        // page that has started on the next room doesn't move it.
        bool anchored = false;
        int camX = 0, camY = 0;
        int lastPlaneX = 0, lastPlaneY = 0;
        uint8_t lastMirroring = 0; // a change of mirroring changes what the scroll means: anchored again
    };
    // How far to draw an object back from where the game has it, for the interpolation.
    float MotionX(const GameView::Motion& m) const;
    float MotionY(const GameView::Motion& m) const;
    float mAlpha = 1.0f;
    void UpdateSection(const GameView& emu);
    bool LoadSection(const GameView& emu, const Game::Data::Section& s, SectionView& out);
    void UpdateLiveTiles(const GameView& emu, SectionView& v);
    bool SectionScreenOrigin(const GameView& emu, const SectionView& v, int& x, int& y) const;
    // doorDir: 1 or 2 while a door scrolls right or left to this section (it is then beside the screen).
    static void AnchorSection(const GameView& emu, SectionView& v, int doorDir = 0);
    static void FollowScroll(const GameView& emu, SectionView& v);
    void DrawSection(Gfx*& g, const GameView& emu, SectionView& v);
    void DrawOutsideFog(Gfx*& g);
    void DrawOffscreenEnemies(Gfx*& g, const GameView& emu);
    void DrawSavePoints(Gfx*& g, const GameView& emu);
    void DrawProps(Gfx*& g, const GameView& emu);
    // A DisplayList resource by path (null if missing), kept loaded.
    const Gfx* PropDisplayList(const std::string& path);

    void DrawPlayField(Gfx*& g, const GameView& emu);
    void DrawSprites(Gfx*& g, const GameView& emu, bool hudPass, bool hideSamus);
    void DrawSamusModel(Gfx*& g, const GameView& emu, const Model& model, float alpha);
    void Draw2DFrame(Gfx*& g, const GameView& emu);
    void BeginOverlay(Gfx*& g);

    ModelLibrary mModels;
    std::unordered_map<std::string, std::unique_ptr<uint8_t[]>> mTextures;
    std::array<Chunk, 4 * 4 * 2> mChunks;
    uint64_t mGfxHash = 0;
    SectionView mSection, mOutgoing;
    bool mInDoor = false;
    int mDoorDir = 0;
    uint64_t mSectionFrame = 0;
    uint32_t mGeneration = 0; // of the game's state the sections follow (GameView::Generation)
    bool mSnapSection = false; // the next section shows at once (the game's state was replaced)

    // The original picture, for 2D screens: 4x4 textures of 64x64 (one texture load each).
    std::array<std::unique_ptr<uint8_t[]>, 16> mFrameTex;
    std::vector<uint32_t> mFramePixels;

    // Props' display lists by path (null: missing), and the resources that keep them loaded.
    std::map<std::string, const Gfx*> mPropLists;
    std::vector<std::shared_ptr<Ship::IResource>> mPropResources;

    // Samus's model animation clock.
    uint8_t mSamusAnim = 0xFF;
    uint64_t mSamusAnimStart = 0;
    // Smooth animation: the pose last drawn, the one blended from since the animation changed (at
    // mSamusBlendStart, in ticks), the model they belong to, and the facing turning from right (0) to left (1).
    SkeletonPose mSamusPose, mSamusFromPose;
    double mSamusBlendStart = -1e9, mSamusLastTime = 0;
    const Model* mSamusModel = nullptr;
    float mSamusTurn = 0;

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
