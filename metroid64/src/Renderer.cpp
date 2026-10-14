#include "Renderer.h"

#include "Picture.h"

#include "Menu.h"
#include "game/AllActive.h"
#include "game/data/Data.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <libultraship/bridge/consolevariablebridge.h>
#include <libultraship/libultraship.h>
#include <map>

namespace Metroid {

namespace {

constexpr int kNativeW = 320;
constexpr int kNativeH = 240;
constexpr int kScreenW = 256; // NES picture
constexpr int kScreenH = 240;
constexpr float kBlockFront = Game::Data::kBlockFront; // solid blocks span z = -8..+8 around the play plane
constexpr float kBlockBack = Game::Data::kBlockBack;
constexpr float kDecorZ = Game::Data::kDecorZ; // passable background tiles sit just behind the play plane
constexpr float kBehindZ = -7.0f;              // sprites the game puts behind the background
constexpr int kChunkPx = 64;
constexpr size_t kFrameGfxCapacity = 1 << 16;
constexpr size_t kFrameVtxCapacity = 1 << 16;

using Game::Data::kFirstPassableTile;
using Game::Data::kShadeBottom;
using Game::Data::kShadeDecor;
using Game::Data::kShadeFront;
using Game::Data::kShadeSide;
using Game::Data::kShadeTop;

void SetVtx(Vtx& v, float x, float y, float z, int s, int t, uint8_t shade) {
    v.v.ob[0] = (short)std::lround(x);
    v.v.ob[1] = (short)std::lround(y);
    v.v.ob[2] = (short)std::lround(z);
    v.v.flag = 0;
    v.v.tc[0] = (short)(s << 5);
    v.v.tc[1] = (short)(t << 5);
    v.v.cn[0] = shade;
    v.v.cn[1] = shade;
    v.v.cn[2] = shade;
    v.v.cn[3] = 0xFF;
}

uint64_t Fnv(uint64_t h, const uint8_t* data, size_t n) {
    for (size_t i = 0; i < n; i++) {
        h = (h ^ data[i]) * 0x100000001B3ull;
    }
    return h;
}

Mat4 Ortho(float l, float r, float b, float t, float n, float f) {
    Mat4 m;
    m.m[0][0] = 2.0f / (r - l);
    m.m[1][1] = 2.0f / (t - b);
    m.m[2][2] = -2.0f / (f - n);
    m.m[3][0] = -(r + l) / (r - l);
    m.m[3][1] = -(t + b) / (t - b);
    m.m[3][2] = -(f + n) / (f - n);
    m.m[3][3] = 1.0f;
    return m;
}

// Wraps v into [lo, lo + period).
int WrapInto(int v, int lo, int period) {
    return lo + (((v - lo) % period) + period) % period;
}

Lights1 sModelLights = gdSPDefLights1(0x50, 0x50, 0x50, 0xFF, 0xFF, 0xFF, 0x28, 0x50, 0x50);

} // namespace

void Renderer::Init() {
    mTextures.clear();
    for (auto& c : mChunks) {
        c = Chunk{};
    }
    mModels.Reset();
    mFrameDl.resize(kFrameGfxCapacity);
    mFrameVtx.reserve(kFrameVtxCapacity);
    mFrameMtx.reserve(512);
    for (auto& t : mFrameTex) {
        t = std::make_unique<uint8_t[]>(kChunkPx * kChunkPx * 4);
        memset(t.get(), 0, kChunkPx * kChunkPx * 4);
    }
    mFramePixels.assign(kScreenW * kScreenH, 0);
    mViewport.vp.vscale[0] = kNativeW * 2;
    mViewport.vp.vscale[1] = kNativeH * 2;
    mViewport.vp.vscale[2] = G_MAXZ / 2;
    mViewport.vp.vscale[3] = 0;
    mViewport.vp.vtrans[0] = kNativeW * 2;
    mViewport.vp.vtrans[1] = kNativeH * 2;
    mViewport.vp.vtrans[2] = G_MAXZ / 2;
    mViewport.vp.vtrans[3] = 0;
    ToMtx(Ortho(0, (float)kScreenW, -(float)kScreenH, 0, -16, 16), &mOrtho);
    ToMtx(Identity(), &mIdentity);
    mSamusAnim = 0xFF;
}

// --- Textures ---------------------------------------------------------------------------------

bool Renderer::TileBlank(const GameView& emu, uint16_t patternAddr) const {
    const uint8_t* p = emu.Chr() + (patternAddr & 0x1FFF);
    for (int i = 0; i < 16; i++) {
        if (p[i]) {
            return false;
        }
    }
    return true;
}

const uint8_t* Renderer::TileTexture(const GameView& emu, uint16_t patternAddr, const uint8_t* palette4,
                                     bool transparent) {
    // Keyed by content: CHR is RAM in Metroid, so a tile number alone doesn't identify a picture.
    const uint8_t* p = emu.Chr() + (patternAddr & 0x1FFF);
    std::string key(21, '\0');
    memcpy(&key[0], p, 16);
    memcpy(&key[16], palette4, 4);
    key[20] = transparent ? 1 : 0;
    auto it = mTextures.find(key);
    if (it != mTextures.end()) {
        return it->second.get();
    }
    auto tex = std::make_unique<uint8_t[]>(64 * 4);
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            int bit = 7 - x;
            uint8_t ci = ((p[y] >> bit) & 1) | (((p[y + 8] >> bit) & 1) << 1);
            uint32_t c = (ci == 0 && transparent) ? 0 : NesColor(palette4[ci]);
            uint8_t* o = &tex[(y * 8 + x) * 4];
            o[0] = c >> 24;
            o[1] = c >> 16;
            o[2] = c >> 8;
            o[3] = c;
        }
    }
    const uint8_t* ptr = tex.get();
    mTextures.emplace(std::move(key), std::move(tex));
    return ptr;
}

// --- Display list helpers ---------------------------------------------------------------------

void Renderer::EmitTexturedQuads(Gfx*& g, const uint8_t* tex, int size, const std::vector<Quad>& quads,
                                 std::vector<Vtx>& store) {
    if (quads.empty()) {
        return;
    }
    if (size == 8) {
        gDPLoadTextureBlock(g++, tex, G_IM_FMT_RGBA, G_IM_SIZ_32b, 8, 8, 0, G_TX_NOMIRROR | G_TX_CLAMP,
                            G_TX_NOMIRROR | G_TX_CLAMP, 3, 3, G_TX_NOLOD, G_TX_NOLOD);
    } else {
        gDPLoadTextureBlock(g++, tex, G_IM_FMT_RGBA, G_IM_SIZ_32b, kChunkPx, kChunkPx, 0, G_TX_NOMIRROR | G_TX_CLAMP,
                            G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    }
    for (size_t i = 0; i < quads.size(); i += 8) {
        size_t n = std::min<size_t>(8, quads.size() - i);
        if (store.size() + n * 4 > store.capacity()) {
            return; // out of vertex space; drop the rest rather than reallocate under the DL
        }
        Vtx* base = store.data() + store.size();
        for (size_t k = 0; k < n; k++) {
            store.insert(store.end(), quads[i + k].v, quads[i + k].v + 4);
        }
        gSPVertex(g++, base, (int)(n * 4), 0);
        for (size_t k = 0; k < n; k++) {
            int o = (int)k * 4;
            gSP2Triangles(g++, o, o + 1, o + 2, 0, o, o + 2, o + 3, 0);
        }
    }
}

Mtx* Renderer::AllocMtx(const Mat4& m) {
    if (mFrameMtx.size() >= mFrameMtx.capacity()) {
        return nullptr;
    }
    mFrameMtx.emplace_back();
    ToMtx(m, &mFrameMtx.back());
    return &mFrameMtx.back();
}

void Renderer::SetTexturedState(Gfx*& g, bool zbuffer) {
    gDPPipeSync(g++);
    gSPClearGeometryMode(g++, G_LIGHTING | G_FOG | G_TEXTURE_GEN | G_TEXTURE_GEN_LINEAR | G_CULL_BOTH | G_ZBUFFER);
    gSPSetGeometryMode(g++, G_SHADE | G_SHADING_SMOOTH | (zbuffer ? G_ZBUFFER : 0));
    gDPSetCycleType(g++, G_CYC_1CYCLE);
    gDPSetTextureFilter(g++, G_TF_POINT);
    gDPSetTexturePersp(g++, G_TP_PERSP);
    gDPSetTextureLUT(g++, G_TT_NONE);
    gSPTexture(g++, 0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_ON);
    gDPSetCombineMode(g++, G_CC_MODULATERGBA, G_CC_MODULATERGBA);
    if (zbuffer) {
        gDPSetRenderMode(g++, G_RM_AA_ZB_TEX_EDGE, G_RM_AA_ZB_TEX_EDGE2);
    } else {
        gDPSetRenderMode(g++, G_RM_AA_TEX_EDGE, G_RM_AA_TEX_EDGE2);
    }
}

// What Ship of Harkinian's actor drawing sets up before an actor's display lists (models exported for it
// expect this): two-cycle mode, which their combiners and render modes use (a second cycle tinting with
// the primitive colour, fog in the first), one light, and fog pushed to the far plane.
void Renderer::SetModelState(Gfx*& g) {
    gDPPipeSync(g++);
    gDPSetCycleType(g++, G_CYC_2CYCLE);
    gSPSetLights1(g++, sModelLights);
    gDPSetFogColor(g++, 0, 0, 0, 255);
    gSPFogPosition(g++, 996, 1000);
}

void Renderer::DrawModel(Gfx*& g, const Gfx* dl, const Mat4& model) {
    Mtx* m = AllocMtx(model);
    if (m == nullptr || dl == nullptr || g + 32 >= mFrameEnd) {
        return;
    }
    SetModelState(g);
    gSPMatrix(g++, m, G_MTX_MODELVIEW | G_MTX_MUL | G_MTX_PUSH);
    gSPDisplayList(g++, dl);
    gSPPopMatrix(g++, G_MTX_MODELVIEW);
    SetTexturedState(g, true);
}

void Renderer::DrawSkeleton(Gfx*& g, const Skeleton& skel, SkeletonPose& pose, const Mat4& model,
                            const std::function<const Gfx*(const std::string&)>& limbList) {
    if (g + 16 + skel.limbCount * 4 >= mFrameEnd) {
        return;
    }
    SetModelState(g);
    pose.Draw(g, skel, model, mViewMat, [this](const Mat4& m) { return AllocMtx(m); }, limbList);
    gSPMatrix(g++, &mView, G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH);
    SetTexturedState(g, true);
}

// --- Play field -------------------------------------------------------------------------------

void Renderer::BuildChunk(const GameView& emu, Chunk& chunk, uint16_t ntBase, int cx, int cy) {
    chunk.vtx.clear();
    chunk.dl.clear();
    chunk.valid = true;

    const uint16_t patBase = (emu.PpuCtrl() & 0x10) ? 0x1000 : 0x0000;
    const uint8_t* pal = emu.Palette();
    const int tx0 = cx * 8, ty0 = cy * 8;
    const int tx1 = tx0 + 8, ty1 = std::min(ty0 + 8, 30);

    auto tileAt = [&](int tx, int ty) { return emu.PpuPeek(ntBase + ty * 32 + tx); };
    auto solidAt = [&](int tx, int ty) {
        // Only neighbours inside this chunk may hide a face: the chunk is rebuilt only when its own
        // tiles change.
        if (tx < tx0 || tx >= tx1 || ty < ty0 || ty >= ty1) {
            return false;
        }
        return tileAt(tx, ty) < kFirstPassableTile;
    };

    std::map<const uint8_t*, std::vector<Quad>> groups;
    for (int ty = ty0; ty < ty1; ty++) {
        for (int tx = tx0; tx < tx1; tx++) {
            uint8_t tile = tileAt(tx, ty);
            uint8_t attr = emu.PpuPeek(ntBase + 0x3C0 + (ty >> 2) * 8 + (tx >> 2));
            int sel = (attr >> (((ty & 2) << 1) | (tx & 2))) & 3;
            uint8_t pal4[4] = { pal[0], pal[sel * 4 + 1], pal[sel * 4 + 2], pal[sel * 4 + 3] };
            uint16_t pat = patBase + tile * 16;
            float x0 = (float)((tx - tx0) * 8), x1 = x0 + 8;
            float yt = -(float)((ty - ty0) * 8), yb = yt - 8;
            Quad q;
            if (tile >= kFirstPassableTile) {
                if (TileBlank(emu, pat)) {
                    continue;
                }
                SetVtx(q.v[0], x0, yb, kDecorZ, 0, 8, kShadeDecor);
                SetVtx(q.v[1], x1, yb, kDecorZ, 8, 8, kShadeDecor);
                SetVtx(q.v[2], x1, yt, kDecorZ, 8, 0, kShadeDecor);
                SetVtx(q.v[3], x0, yt, kDecorZ, 0, 0, kShadeDecor);
                groups[TileTexture(emu, pat, pal4, true)].push_back(q);
                continue;
            }
            auto& out = groups[TileTexture(emu, pat, pal4, false)];
            SetVtx(q.v[0], x0, yb, kBlockFront, 0, 8, kShadeFront);
            SetVtx(q.v[1], x1, yb, kBlockFront, 8, 8, kShadeFront);
            SetVtx(q.v[2], x1, yt, kBlockFront, 8, 0, kShadeFront);
            SetVtx(q.v[3], x0, yt, kBlockFront, 0, 0, kShadeFront);
            out.push_back(q);
            if (!solidAt(tx, ty - 1)) { // top
                SetVtx(q.v[0], x0, yt, kBlockFront, 0, 8, kShadeTop);
                SetVtx(q.v[1], x1, yt, kBlockFront, 8, 8, kShadeTop);
                SetVtx(q.v[2], x1, yt, kBlockBack, 8, 0, kShadeTop);
                SetVtx(q.v[3], x0, yt, kBlockBack, 0, 0, kShadeTop);
                out.push_back(q);
            }
            if (!solidAt(tx, ty + 1)) { // bottom
                SetVtx(q.v[0], x0, yb, kBlockBack, 0, 8, kShadeBottom);
                SetVtx(q.v[1], x1, yb, kBlockBack, 8, 8, kShadeBottom);
                SetVtx(q.v[2], x1, yb, kBlockFront, 8, 0, kShadeBottom);
                SetVtx(q.v[3], x0, yb, kBlockFront, 0, 0, kShadeBottom);
                out.push_back(q);
            }
            if (!solidAt(tx - 1, ty)) { // left
                SetVtx(q.v[0], x0, yb, kBlockBack, 0, 8, kShadeSide);
                SetVtx(q.v[1], x0, yb, kBlockFront, 8, 8, kShadeSide);
                SetVtx(q.v[2], x0, yt, kBlockFront, 8, 0, kShadeSide);
                SetVtx(q.v[3], x0, yt, kBlockBack, 0, 0, kShadeSide);
                out.push_back(q);
            }
            if (!solidAt(tx + 1, ty)) { // right
                SetVtx(q.v[0], x1, yb, kBlockFront, 0, 8, kShadeSide);
                SetVtx(q.v[1], x1, yb, kBlockBack, 8, 8, kShadeSide);
                SetVtx(q.v[2], x1, yt, kBlockBack, 8, 0, kShadeSide);
                SetVtx(q.v[3], x1, yt, kBlockFront, 0, 0, kShadeSide);
                out.push_back(q);
            }
        }
    }

    size_t quadCount = 0;
    for (auto& [tex, quads] : groups) {
        quadCount += quads.size();
    }
    chunk.vtx.reserve(quadCount * 4);
    // Worst case: 8 commands per texture load, then a vertex load and 8 triangle pairs per 8 quads.
    chunk.dl.resize(groups.size() * 8 + (quadCount / 8 + 1) * 10 + groups.size() * 10 + 4);
    Gfx* g = chunk.dl.data();
    for (auto& [tex, quads] : groups) {
        EmitTexturedQuads(g, tex, 8, quads, chunk.vtx);
    }
    gSPEndDisplayList(g++);
    chunk.dl.resize(g - chunk.dl.data());
}

void Renderer::DrawPlayField(Gfx*& g, const GameView& emu) {
    // Anything that changes how tiles look invalidates every chunk.
    const uint16_t patBase = (emu.PpuCtrl() & 0x10) ? 0x1000 : 0x0000;
    uint64_t gfx = Fnv(0xCBF29CE484222325ull, emu.Chr() + patBase, 0x1000);
    gfx = Fnv(gfx, emu.Palette(), 16);
    gfx ^= patBase;

    // The nametable plane is 512x480, but mirroring decides how much of it is distinct: a horizontal
    // room (vertical mirroring) is 512x240, a vertical shaft 256x480. Each distinct chunk is drawn
    // once, at the copy closest to the middle of the screen.
    uint8_t mirroring = emu.Mirroring();
    int periodX = mirroring == 2 ? 512 : 256;
    int periodY = mirroring == 3 ? 480 : 240;
    int scrollX = emu.PlaneScrollX(), scrollY = emu.PlaneScrollY();

    for (int ly = 0; ly < periodY / 240; ly++) {
        for (int lx = 0; lx < periodX / 256; lx++) {
            int page = mirroring == 0 ? 0 : mirroring == 1 ? 1 : mirroring == 2 ? lx : ly;
            // A room with custom geometry: draw its model at the page's position.
            RoomInfo room = emu.RoomOnPage(page);
            const Model* roomModel = room.valid ? mModels.Room(room.bank, room.mapX, room.mapY, room.room) : nullptr;
            if (roomModel != nullptr) {
                ModelInstance m;
                m.anim = "Idle";
                m.time = (float)emu.FrameCount() / 60.0f;
                m.x = (float)(WrapInto(lx * 256 + 128 - scrollX, kScreenW / 2 - periodX / 2, periodX) - 128);
                m.y = (float)(WrapInto(ly * 240 + 120 - scrollY, kScreenH / 2 - periodY / 2, periodY) - 120);
                roomModel->Draw(*this, g, m);
                if (!roomModel->KeepTiles()) {
                    continue;
                }
            }
            for (int cy = 0; cy < 4; cy++) {
                for (int cx = 0; cx < 4; cx++) {
                    Chunk& chunk = mChunks[page * 16 + cy * 4 + cx];
                    uint64_t h = gfx;
                    // A logical nametable address that maps to this physical page.
                    uint16_t ntBase = 0x2000 | (((ly << 1) | lx) << 10);
                    for (int ty = cy * 8; ty < std::min(cy * 8 + 8, 30); ty++) {
                        for (int tx = cx * 8; tx < cx * 8 + 8; tx++) {
                            uint8_t t = emu.PpuPeek(ntBase + ty * 32 + tx);
                            h = (h ^ t) * 0x100000001B3ull;
                        }
                    }
                    for (int ay = cy * 2; ay < cy * 2 + 2; ay++) {
                        for (int ax = cx * 2; ax < cx * 2 + 2; ax++) {
                            h = (h ^ emu.PpuPeek(ntBase + 0x3C0 + ay * 8 + ax)) * 0x100000001B3ull;
                        }
                    }
                    if (!chunk.valid || chunk.hash != h) {
                        BuildChunk(emu, chunk, ntBase, cx, cy);
                        chunk.hash = h;
                    }
                    if (chunk.dl.size() <= 1 || g + 8 >= mFrameEnd) {
                        continue;
                    }
                    int chunkH = cy == 3 ? 48 : 64;
                    int px = lx * 256 + cx * kChunkPx, py = ly * 240 + cy * kChunkPx;
                    // Offset of the chunk's centre from the screen's centre, wrapped to the nearest copy.
                    int dx = WrapInto(px + kChunkPx / 2 - scrollX, kScreenW / 2 - periodX / 2, periodX) - kChunkPx / 2;
                    int dy = WrapInto(py + chunkH / 2 - scrollY, kScreenH / 2 - periodY / 2, periodY) - chunkH / 2;
                    Mtx* m = AllocMtx(Translate((float)dx, (float)-dy, 0));
                    if (m == nullptr) {
                        continue;
                    }
                    gSPMatrix(g++, m, G_MTX_MODELVIEW | G_MTX_MUL | G_MTX_PUSH);
                    gSPDisplayList(g++, chunk.dl.data());
                    gSPPopMatrix(g++, G_MTX_MODELVIEW);
                }
            }
        }
    }
}

// --- Sections ------------------------------------------------------------------------------------

namespace {

constexpr int kFadeFrames = 48; // game frames for a section to fade in or out

std::shared_ptr<Ship::File> LoadArchiveFile(const std::string& path) {
    return Ship::Context::GetRawInstance()->GetResourceManager()->GetArchiveManager()->LoadFile(path);
}

// Index of map screen (x, y) in the section, or -1.
int SectionIndex(const Game::Data::Section& s, uint8_t x, uint8_t y) {
    for (int i = 0; i < s.length; i++) {
        if (s.ScreenX(i) == x && s.ScreenY(i) == y) {
            return i;
        }
    }
    return -1;
}

int WrapDelta(int d, int size) {
    d %= size;
    if (d < -size / 2) {
        d += size;
    } else if (d >= size / 2) {
        d -= size;
    }
    return d;
}

} // namespace

bool Renderer::LoadSection(const GameView& emu, const Game::Data::Section& s, SectionView& out) {
    const Game::Data::Level* level = Game::Data::InstalledLevel();
    out = SectionView{};
    if (level == nullptr) {
        return false;
    }
    auto engine = [&emu](uint16_t a) { return emu.Peek(a); };
    if (!Game::Data::SectionScreens(*level, s, engine, out.layout)) {
        return false;
    }
    // The mesh from the game's data; made here if it's missing or was made from other tiles (a mod
    // changed the rooms).
    Game::Data::RoomMesh mesh;
    auto file = LoadArchiveFile(Game::Data::RoomMeshPath(s));
    bool loaded = false;
    if (file != nullptr && file->IsLoaded && file->Buffer != nullptr) {
        std::string data(file->Buffer->begin() + file->BufferOffset, file->Buffer->end());
        loaded = Game::Data::DecodeRoomMesh(data, mesh) && mesh.section == s &&
                 mesh.layoutHash == Game::Data::LayoutHash(out.layout);
    }
    if (!loaded) {
        mesh = Game::Data::BuildRoomMesh(s, out.layout);
    }
    out.section = s;
    out.live = out.layout;
    out.chunks.resize((size_t)s.length * 16);
    for (Game::Data::MeshChunk& c : mesh.chunks) {
        size_t i = (size_t)c.screen * 16 + c.cy * 4 + c.cx;
        if (i < out.chunks.size()) {
            out.chunks[i].base = std::move(c);
        }
    }
    out.valid = true;
    return true;
}

void Renderer::UpdateSection(const GameView& emu) {
    uint64_t frame = emu.FrameCount();
    int steps = (int)std::min<uint64_t>(frame - mSectionFrame, 60);
    mSectionFrame = frame;

    const Game::Data::Level* level = Game::Data::InstalledLevel();
    int bank = emu.AreaBank();
    if (level == nullptr || bank < 1 || bank > 5) {
        mSection.valid = mOutgoing.valid = false;
        return;
    }
    // In a door (DoorStatus set, until Samus comes out: 5), the section fades out; the next one is
    // known once the door has scrolled the screen to it and the scrolling direction is set.
    uint8_t door = emu.Peek(Ram::kDoorStatus);
    bool inDoor = door != 0 && door != 5;
    if (inDoor) {
        if (!mInDoor && mSection.valid) {
            mOutgoing = std::move(mSection);
            mSection = SectionView{};
        }
        // The next section, as soon as the door's direction is known (SamusEnterDoor/ScrollDoor: 1 right,
        // 2 left; 3 and 4 first centre a vertical shaft, then go on as 1 or 2), so it can fade in while the
        // door scrolls to it. From a horizontal section the scrolling turns vertical through an $A0 door
        // and stays horizontal through an $A1 door; from a vertical one it turns horizontal.
        uint8_t dir = door & 0x0F;
        if (!mSection.valid && mOutgoing.valid && (dir == 1 || dir == 2)) {
            uint8_t mx = emu.Peek(Ram::kMapPosX) & 0x1F, my = emu.Peek(Ram::kMapPosY) & 0x1F;
            int nx = dir == 1 ? mx + 1 : mx - 1;
            if (nx >= 0 && nx <= 31 && level->world.rooms[my][nx] != 0xFF) {
                bool horizontal = true;
                if (mOutgoing.section.horizontal) {
                    auto engine = [&emu](uint16_t a) { return emu.Peek(a); };
                    horizontal = Game::Data::DoorTile(*level, bank, level->world.rooms[my][mx], dir == 1 ? 1 : 2,
                                                      engine) == 0xA1;
                }
                LoadSection(emu, Game::Data::FindSection(*level, bank, (uint8_t)nx, my, horizontal), mSection);
            }
        }
    } else {
        uint8_t mx = emu.Peek(Ram::kMapPosX) & 0x1F, my = emu.Peek(Ram::kMapPosY) & 0x1F;
        if (level->world.rooms[my][mx] != 0xFF) {
            bool horizontal = (emu.Peek(Ram::kScrollDir) & 0x02) != 0;
            Game::Data::Section s = Game::Data::FindSection(*level, bank, mx, my, horizontal);
            if (!mSection.valid || !(mSection.section == s)) {
                if (mSection.valid && !mOutgoing.valid) {
                    mOutgoing = std::move(mSection); // a change without a door (an elevator, a new area)
                }
                LoadSection(emu, s, mSection);
            }
        }
    }
    mInDoor = inDoor;
    float step = (float)steps / kFadeFrames;
    if (mOutgoing.valid) {
        mOutgoing.fade = std::max(0.0f, mOutgoing.fade - step);
        if (mOutgoing.fade <= 0.0f) {
            mOutgoing.valid = false;
        }
    }
    if (mSection.valid) {
        mSection.fade = std::min(1.0f, mSection.fade + step);
    }
    if (mSection.valid) {
        UpdateLiveTiles(emu, mSection);
    }
}

void Renderer::UpdateLiveTiles(const GameView& emu, SectionView& v) {
    const Game::Data::Section& s = v.section;
    for (int page = 0; page < 2; page++) {
        RoomInfo room = emu.RoomOnPage(page);
        if (!room.valid || !room.complete || room.bank != s.bank) {
            continue;
        }
        int i = SectionIndex(s, room.mapX, room.mapY);
        if (i < 0) {
            continue;
        }
        Game::Data::ScreenTiles ram;
        for (int k = 0; k < 0x400; k++) {
            ram.ram[k] = emu.Peek((uint16_t)(0x6000 + page * 0x400 + k));
        }
        uint64_t h = Fnv(0xCBF29CE484222325ull ^ (uint64_t)i, ram.ram, sizeof(ram.ram));
        if (h == v.pageHash[page]) {
            continue;
        }
        v.pageHash[page] = h;
        v.live[i] = ram;
        // Blocks whose tiles (or the tiles around them, which decide their faces) differ from the
        // room mesh's are made from room RAM.
        for (int cy = 0; cy < 4; cy++) {
            for (int cx = 0; cx < 4; cx++) {
                bool differs = false;
                for (int ty = std::max(0, cy * 8 - 1); ty < std::min(30, cy * 8 + 9) && !differs; ty++) {
                    for (int tx = std::max(0, cx * 8 - 1); tx < std::min(32, cx * 8 + 9); tx++) {
                        if (ram.Tile(tx, ty) != v.layout[i].Tile(tx, ty) ||
                            ram.Palette(tx, ty) != v.layout[i].Palette(tx, ty)) {
                            differs = true;
                            break;
                        }
                    }
                }
                SectionChunk& c = v.chunks[(size_t)i * 16 + cy * 4 + cx];
                if (differs) {
                    c.live = Game::Data::BuildMeshChunk(s, v.live, i, cx, cy);
                    c.useLive = true;
                    c.dlHash = 0;
                } else if (c.useLive) {
                    c.useLive = false;
                    c.dlHash = 0;
                }
            }
        }
    }
}

bool Renderer::SectionScreenOrigin(const GameView& emu, const SectionView& v, int& x, int& y) const {
    // Where the top-left of the screen is in the section: found from a name table page that holds one of
    // the section's rooms, and where the scroll is relative to that page.
    uint8_t mirroring = emu.Mirroring();
    int planeX = emu.PlaneScrollX(), planeY = emu.PlaneScrollY();
    for (int page = 0; page < 2; page++) {
        RoomInfo room = emu.RoomOnPage(page);
        if (!room.valid || room.bank != v.section.bank) {
            continue;
        }
        int i = SectionIndex(v.section, room.mapX, room.mapY);
        if (i < 0) {
            continue;
        }
        int originX = mirroring == 2 ? page * 256 : 0;
        int originY = mirroring == 3 ? page * 240 : 0;
        int dx = WrapDelta(planeX - originX, mirroring == 2 ? 512 : 256);
        int dy = WrapDelta(planeY - originY, mirroring == 3 ? 480 : 240);
        x = (v.section.horizontal ? i * 256 : 0) + dx;
        y = (v.section.horizontal ? 0 : i * 240) + dy;
        return true;
    }
    return false;
}

void Renderer::DrawSection(Gfx*& g, const GameView& emu, SectionView& v) {
    int sx = 0, sy = 0;
    if (!v.valid || v.fade <= 0.0f || !SectionScreenOrigin(emu, v, sx, sy)) {
        return;
    }
    const uint8_t* pal = emu.Palette();
    uint64_t gfx = Fnv(0xCBF29CE484222325ull, emu.Chr() + 0x1000, 0x1000);
    gfx = Fnv(gfx, pal, 16) | 1;

    // Blocks the camera can see, with room to spare so none appear or vanish in view.
    float fov = CVarGetFloat("gMetroid64.Camera.Fov", 52.0f);
    float dist = CVarGetFloat("gMetroid64.Camera.Distance", 270.0f);
    float halfH = (dist + kBlockFront + 64) * std::tan(fov * 3.14159265f / 360.0f);
    float halfW = halfH * kNativeW / kNativeH;
    float cx = sx + kScreenW / 2.0f, cy = sy + kScreenH / 2.0f;

    Mtx* m = AllocMtx(Translate((float)-sx, (float)sy, 0));
    if (m == nullptr) {
        return;
    }
    gDPPipeSync(g++);
    if (v.fade < 1.0f) {
        gDPSetCombineLERP(g++, TEXEL0, 0, SHADE, 0, TEXEL0, 0, PRIMITIVE, 0, TEXEL0, 0, SHADE, 0, TEXEL0, 0, PRIMITIVE,
                          0);
        gDPSetPrimColor(g++, 0, 0, 255, 255, 255, (uint8_t)std::lround(v.fade * 255));
        gDPSetRenderMode(g++, G_RM_AA_ZB_XLU_SURF, G_RM_AA_ZB_XLU_SURF2);
    }
    gSPMatrix(g++, m, G_MTX_MODELVIEW | G_MTX_MUL | G_MTX_PUSH);
    const Game::Data::Section& s = v.section;
    const Game::Data::Level* level = Game::Data::InstalledLevel();
    for (int i = 0; i < s.length; i++) {
        // A room model (MODELS.md) replaces the screen's blocks, or adds to them with keep_tiles.
        uint8_t mapX = s.ScreenX(i), mapY = s.ScreenY(i);
        uint8_t roomNumber = level != nullptr ? level->world.rooms[mapY & 0x1F][mapX & 0x1F] : 0xFF;
        const Model* roomModel = mModels.Room((uint8_t)s.bank, mapX, mapY, roomNumber);
        if (roomModel != nullptr) {
            ModelInstance mi;
            mi.anim = "Idle";
            mi.time = (float)emu.FrameCount() / 60.0f;
            mi.x = (float)(s.horizontal ? i * 256 : 0);
            mi.y = (float)(s.horizontal ? 0 : i * 240);
            roomModel->Draw(*this, g, mi);
            if (v.fade < 1.0f) {
                gDPSetCombineLERP(g++, TEXEL0, 0, SHADE, 0, TEXEL0, 0, PRIMITIVE, 0, TEXEL0, 0, SHADE, 0, TEXEL0, 0,
                                  PRIMITIVE, 0);
                gDPSetRenderMode(g++, G_RM_AA_ZB_XLU_SURF, G_RM_AA_ZB_XLU_SURF2);
            }
            if (!roomModel->KeepTiles()) {
                continue;
            }
        }
        for (int cyi = 0; cyi < 4; cyi++) {
            for (int cxi = 0; cxi < 4; cxi++) {
                float x0 = (float)((s.horizontal ? i * 256 : 0) + cxi * 64);
                float y0 = (float)((s.horizontal ? 0 : i * 240) + cyi * 64);
                if (x0 + 64 < cx - halfW || x0 > cx + halfW || y0 + 64 < cy - halfH || y0 > cy + halfH) {
                    continue;
                }
                SectionChunk& c = v.chunks[(size_t)i * 16 + cyi * 4 + cxi];
                if (c.dlHash != gfx) {
                    const Game::Data::MeshChunk& mesh = c.useLive ? c.live : c.base;
                    std::map<const uint8_t*, std::vector<Quad>> groups;
                    size_t quadCount = 0;
                    for (const Game::Data::MeshGroup& grp : mesh.groups) {
                        uint16_t pat = 0x1000 + grp.tile * 16;
                        bool decor = grp.tile >= Game::Data::kFirstPassableTile;
                        if (decor && TileBlank(emu, pat)) {
                            continue;
                        }
                        uint8_t pal4[4] = { pal[0], pal[grp.palette * 4 + 1], pal[grp.palette * 4 + 2],
                                            pal[grp.palette * 4 + 3] };
                        auto& out = groups[TileTexture(emu, pat, pal4, decor)];
                        for (const Game::Data::MeshQuad& mq : grp.quads) {
                            Quad q;
                            for (int k = 0; k < 4; k++) {
                                SetVtx(q.v[k], mq.pos[k][0], mq.pos[k][1], mq.pos[k][2], mq.uv[k][0], mq.uv[k][1],
                                       mq.shade);
                            }
                            out.push_back(q);
                        }
                        quadCount += grp.quads.size();
                    }
                    c.vtx.clear();
                    c.vtx.reserve(quadCount * 4);
                    c.dl.assign(groups.size() * 18 + (quadCount / 8 + 1) * 10 + 4, Gfx{});
                    Gfx* d = c.dl.data();
                    for (auto& [tex, quads] : groups) {
                        EmitTexturedQuads(d, tex, 8, quads, c.vtx);
                    }
                    gSPEndDisplayList(d++);
                    c.dl.resize(d - c.dl.data());
                    c.dlHash = gfx;
                }
                if (c.dl.size() <= 1 || g + 8 >= mFrameEnd) {
                    continue;
                }
                gSPDisplayList(g++, c.dl.data());
            }
        }
    }
    gSPPopMatrix(g++, G_MTX_MODELVIEW);
    SetTexturedState(g, true);
}

void Renderer::DrawOffscreenEnemies(Gfx*& g, const GameView& emu) {
    // Enemies running outside the game's two screens (everything active), at their place in the section.
    const auto& sprites = Game::AllActive::OffscreenSprites();
    int sx = 0, sy = 0;
    if (sprites.empty() || !mSection.valid || !SectionScreenOrigin(emu, mSection, sx, sy)) {
        return;
    }
    const uint8_t* pal = emu.Palette();
    uint16_t base = (emu.PpuCtrl() & 0x08) ? 0x1000 : 0;
    std::map<const uint8_t*, std::vector<Quad>> groups;
    for (const auto& sp : sprites) {
        uint8_t attr = sp.attr;
        uint8_t pal4[4] = { 0, pal[16 + (attr & 3) * 4 + 1], pal[16 + (attr & 3) * 4 + 2],
                            pal[16 + (attr & 3) * 4 + 3] };
        float x0 = (float)(sp.x - sx), x1 = x0 + 8;
        float yt = -(float)(sp.y - sy), yb = yt - 8;
        bool hflip = attr & 0x40, vflip = attr & 0x80;
        int s0 = hflip ? 8 : 0, s1 = 8 - s0;
        int t0 = vflip ? 8 : 0, t1 = 8 - t0;
        float z = (attr & 0x20) ? kBehindZ : 1.0f;
        Quad q;
        SetVtx(q.v[0], x0, yb, z, s0, t1, 255);
        SetVtx(q.v[1], x1, yb, z, s1, t1, 255);
        SetVtx(q.v[2], x1, yt, z, s1, t0, 255);
        SetVtx(q.v[3], x0, yt, z, s0, t0, 255);
        groups[TileTexture(emu, base + sp.tile * 16, pal4, true)].push_back(q);
    }
    SetTexturedState(g, true);
    for (auto& [tex, quads] : groups) {
        if (g + 64 >= mFrameEnd) {
            break;
        }
        EmitTexturedQuads(g, tex, 8, quads, mFrameVtx);
    }
}

void Renderer::DrawOutsideFog(Gfx*& g) {
    // Darkness over everything outside the original screen (0..256 x 0..240), in front of the blocks,
    // fading in over a few pixels from its edge.
    constexpr float kEdge = 24.0f, kFar = 4096.0f, kZ = kBlockFront + 1;
    constexpr uint8_t kDark = 230;
    const float inner[4] = { 0, (float)kScreenW, 0, -(float)kScreenH }; // left, right, top, bottom
    const float outer[4] = { -kEdge, kScreenW + kEdge, kEdge, -(kScreenH + kEdge) };
    const float distant[4] = { -kFar, kScreenW + kFar, kFar, -(kScreenH + kFar) };
    gDPPipeSync(g++);
    gSPTexture(g++, 0, 0, 0, G_TX_RENDERTILE, G_OFF);
    gDPSetCombineMode(g++, G_CC_SHADE, G_CC_SHADE);
    gDPSetRenderMode(g++, G_RM_ZB_XLU_SURF, G_RM_ZB_XLU_SURF2);
    auto vtx = [](Vtx& v, float x, float y, uint8_t a) {
        SetVtx(v, x, y, kZ, 0, 0, 0);
        v.v.cn[3] = a;
    };
    // A ring of quads: (inner -> outer) fades in, (outer -> far) is solid. Corners go clockwise.
    auto ring = [&](const float* a, const float* b, uint8_t alphaA, uint8_t alphaB) {
        const float ax[4] = { a[0], a[1], a[1], a[0] }, ay[4] = { a[2], a[2], a[3], a[3] };
        const float bx[4] = { b[0], b[1], b[1], b[0] }, by[4] = { b[2], b[2], b[3], b[3] };
        if (mFrameVtx.size() + 16 > mFrameVtx.capacity()) {
            return;
        }
        Vtx* base = mFrameVtx.data() + mFrameVtx.size();
        for (int k = 0; k < 4; k++) {
            Vtx va, vb;
            vtx(va, ax[k], ay[k], alphaA);
            vtx(vb, bx[k], by[k], alphaB);
            mFrameVtx.push_back(va);
            mFrameVtx.push_back(vb);
        }
        gSPVertex(g++, base, 8, 0);
        for (int k = 0; k < 4; k++) {
            int n = (k + 1) % 4;
            gSP2Triangles(g++, k * 2, k * 2 + 1, n * 2 + 1, 0, k * 2, n * 2 + 1, n * 2, 0);
        }
    };
    ring(inner, outer, 0, kDark);
    ring(outer, distant, kDark, kDark);
    SetTexturedState(g, true);
}

// --- Sprites ----------------------------------------------------------------------------------

void Renderer::DrawSprites(Gfx*& g, const GameView& emu, bool hudPass, bool hideSamus) {
    const uint8_t* oam = emu.Oam();
    const uint8_t* pal = emu.Palette();
    const auto& tags = emu.SpriteTags();
    bool tall = emu.PpuCtrl() & 0x20;
    std::map<const uint8_t*, std::vector<Quad>> groups;

    for (int i = 0; i < 64; i++) {
        const uint8_t* o = &oam[i * 4];
        if (o[0] >= 0xEF) {
            continue;
        }
        SpriteOwner owner = tags[i].owner;
        if ((owner == SpriteOwner::Hud) != hudPass || (hideSamus && owner == SpriteOwner::Samus)) {
            continue;
        }
        uint8_t attr = o[2];
        uint8_t pal4[4] = { 0, pal[16 + (attr & 3) * 4 + 1], pal[16 + (attr & 3) * 4 + 2],
                            pal[16 + (attr & 3) * 4 + 3] };
        // Depth per kind of sprite, so overlapping sprites don't fight; priority sprites go behind
        // the background like on the NES.
        float z = owner == SpriteOwner::Samus ? 2.0f : owner == SpriteOwner::Projectile ? 3.0f : 1.0f;
        if (owner == SpriteOwner::Scenery) {
            // Door bubbles: behind the background on the NES, where they show through the door
            // frame's transparent pixels. Blocks are opaque, so put them on the wall's face instead.
            z = kBlockFront + 1;
        } else if (attr & 0x20) {
            z = kBehindZ;
        }
        if (hudPass) {
            z = 0;
        }
        bool hflip = attr & 0x40, vflip = attr & 0x80;
        int parts = tall ? 2 : 1;
        for (int part = 0; part < parts; part++) {
            uint16_t pat;
            if (tall) {
                int half = vflip ? 1 - part : part;
                pat = ((o[1] & 1) ? 0x1000 : 0) + (o[1] & 0xFE) * 16 + half * 16;
            } else {
                pat = ((emu.PpuCtrl() & 0x08) ? 0x1000 : 0) + o[1] * 16;
            }
            float x0 = o[3], x1 = x0 + 8;
            float yt = -(float)(o[0] + 1 + part * 8), yb = yt - 8;
            int s0 = hflip ? 8 : 0, s1 = 8 - s0;
            int t0 = vflip ? 8 : 0, t1 = 8 - t0; // t at the top edge, then the bottom edge
            Quad q;
            SetVtx(q.v[0], x0, yb, z, s0, t1, 255);
            SetVtx(q.v[1], x1, yb, z, s1, t1, 255);
            SetVtx(q.v[2], x1, yt, z, s1, t0, 255);
            SetVtx(q.v[3], x0, yt, z, s0, t0, 255);
            groups[TileTexture(emu, pat, pal4, true)].push_back(q);
        }
    }
    for (auto& [tex, quads] : groups) {
        if (g + 64 >= mFrameEnd) {
            break;
        }
        EmitTexturedQuads(g, tex, 8, quads, mFrameVtx);
    }
}

void Renderer::DrawSamusModel(Gfx*& g, const GameView& emu, const Model& model, float alpha) {
    uint8_t anim = emu.Peek(Ram::kSamusAnimReset);
    if (anim != mSamusAnim) {
        mSamusAnim = anim;
        mSamusAnimStart = emu.FrameCount();
    }
    ModelInstance m;
    m.anim = SamusAnimName(anim);
    m.time = ((float)(emu.FrameCount() - mSamusAnimStart) + alpha) / 60.0f;
    m.x = emu.SamusScreenX();
    m.y = emu.SamusScreenY() + emu.Peek(Ram::kSamusRadY) + 1; // models stand on their origin
    m.facingLeft = emu.SamusFacingLeft();
    model.Draw(*this, g, m);
}

// --- 2D -----------------------------------------------------------------------------------------

void Renderer::BeginOverlay(Gfx*& g) {
    gSPMatrix(g++, &mOrtho, G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
    gSPMatrix(g++, &mIdentity, G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH);
    SetTexturedState(g, false);
}

void Renderer::Draw2DFrame(Gfx*& g, const GameView& emu) {
    emu.RenderFrame(mFramePixels.data());
    BeginOverlay(g);
    gDPPipeSync(g++);
    gDPSetCombineMode(g++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPSetRenderMode(g++, G_RM_OPA_SURF, G_RM_OPA_SURF2);
    for (int j = 0; j < 4; j++) {
        for (int i = 0; i < 4; i++) {
            uint8_t* tex = mFrameTex[j * 4 + i].get();
            for (int y = 0; y < kChunkPx; y++) {
                int sy = j * kChunkPx + y;
                for (int x = 0; x < kChunkPx; x++) {
                    uint32_t c = sy < kScreenH ? mFramePixels[sy * kScreenW + i * kChunkPx + x] : 0x000000FF;
                    uint8_t* o = &tex[(y * kChunkPx + x) * 4];
                    o[0] = c >> 24;
                    o[1] = c >> 16;
                    o[2] = c >> 8;
                    o[3] = 0xFF;
                }
            }
            // The texture is reused every frame, so tell the renderer its contents changed.
            __gSPInvalidateTexCache(g++, (uintptr_t)tex);
            float x0 = (float)(i * kChunkPx), x1 = x0 + kChunkPx;
            float yt = -(float)(j * kChunkPx), yb = yt - kChunkPx;
            Quad q;
            SetVtx(q.v[0], x0, yb, 0, 0, kChunkPx, 255);
            SetVtx(q.v[1], x1, yb, 0, kChunkPx, kChunkPx, 255);
            SetVtx(q.v[2], x1, yt, 0, kChunkPx, 0, 255);
            SetVtx(q.v[3], x0, yt, 0, 0, 0, 255);
            EmitTexturedQuads(g, tex, kChunkPx, { q }, mFrameVtx);
        }
    }
}

// --- Frame --------------------------------------------------------------------------------------

Gfx* Renderer::BuildFrame(const GameView& emu, float alpha) {
    mFrameVtx.clear();
    mFrameMtx.clear();
    Gfx* g = mFrameDl.data();
    mFrameEnd = mFrameDl.data() + mFrameDl.size() - 64;

    // Fast3D clears colour and depth at the start of every frame, so no fill rectangle is needed (one
    // drawn with z-buffering still enabled would stamp the depth buffer).
    gDPPipeSync(g++);
    gDPSetScissor(g++, G_SC_NON_INTERLACE, 0, 0, kNativeW, kNativeH);
    gSPViewport(g++, &mViewport);
    gDPSetTextureLOD(g++, G_TL_TILE);
    gDPSetTextureDetail(g++, G_TD_CLAMP);
    gDPSetTextureConvert(g++, G_TC_FILT);
    gDPSetCombineKey(g++, G_CK_NONE);
    gDPSetAlphaCompare(g++, G_AC_NONE);

    bool threeD = CVarGetInteger(kCVarDisplay3D, 1) != 0 && emu.InGameplay();
    if (!threeD) {
        Draw2DFrame(g, emu);
    } else {
        // The camera looks at the middle of the NES screen; between game frames it is
        // interpolated along the (unwrapped) scroll.
        float offX = (float)((emu.PrevWorldScrollX() - emu.WorldScrollX()) * (1.0f - alpha));
        float offY = (float)((emu.PrevWorldScrollY() - emu.WorldScrollY()) * (1.0f - alpha));
        float fov = CVarGetFloat("gMetroid64.Camera.Fov", 52.0f);
        float dist = CVarGetFloat("gMetroid64.Camera.Distance", 270.0f);
        float height = CVarGetFloat("gMetroid64.Camera.Height", 30.0f);
        float cx = kScreenW / 2.0f + offX, cy = -(kScreenH / 2.0f + offY);
        ToMtx(Perspective(fov, (float)kNativeW / kNativeH, 16.0f, 4000.0f), &mProjection);
        mViewMat = LookAt({ cx, cy + height, dist }, { cx, cy, 0.0f }, { 0, 1, 0 });
        ToMtx(mViewMat, &mView);
        gSPMatrix(g++, &mProjection, G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
        gSPMatrix(g++, &mView, G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH);
        SetTexturedState(g, true);

        if (Game::Data::InstalledLevel() != nullptr && emu.AreaBank() >= 1 && emu.AreaBank() <= 5) {
            UpdateSection(emu);
            DrawSection(g, emu, mOutgoing);
            DrawSection(g, emu, mSection);
        } else {
            DrawPlayField(g, emu); // no level data: the name tables as the game shows them
        }

        // The ball while the game shows Samus rolled up, the Varia once she has it (SamusGear bit 5, the
        // bit the game picks her palette by), else the power suit.
        ModelLibrary::SamusForm form = ModelLibrary::SamusForm::Power;
        if (std::string(SamusAnimName(emu.Peek(Ram::kSamusAnimReset))) == "Morph") {
            form = ModelLibrary::SamusForm::Ball;
        } else if (emu.Peek(Ram::kSamusGear) & 0x20) {
            form = ModelLibrary::SamusForm::Varia;
        }
        const Model* samus = mModels.Samus(form);
        bool samusVisible = false;
        for (int i = 0; i < 64; i++) {
            samusVisible |= emu.SpriteTags()[i].owner == SpriteOwner::Samus;
        }
        SetTexturedState(g, true);
        DrawSprites(g, emu, false, samus != nullptr);
        if (samus != nullptr && samusVisible) {
            DrawSamusModel(g, emu, *samus, alpha);
        }

        DrawOffscreenEnemies(g, emu);
        if (CVarGetInteger(kCVarOutsideView, 0) == 0) {
            DrawOutsideFog(g);
        }

        BeginOverlay(g);
        DrawSprites(g, emu, true, false);
    }

    gDPPipeSync(g++);
    gDPFullSync(g++);
    gSPEndDisplayList(g++);
    return mFrameDl.data();
}

} // namespace Metroid
