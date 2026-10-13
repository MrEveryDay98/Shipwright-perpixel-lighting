#include "Renderer.h"

#include "Menu.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <libultraship/bridge/consolevariablebridge.h>
#include <map>

namespace Metroid {

namespace {

constexpr int kNativeW = 320;
constexpr int kNativeH = 240;
constexpr int kScreenW = 256; // NES picture
constexpr int kScreenH = 240;
constexpr float kBlockFront = 8.0f; // solid blocks span z = -8..+8 around the play plane
constexpr float kBlockBack = -8.0f;
constexpr float kDecorZ = -6.0f;  // passable background tiles sit just behind the play plane
constexpr float kBehindZ = -7.0f; // sprites the game puts behind the background
constexpr int kChunkPx = 64;
constexpr size_t kFrameGfxCapacity = 1 << 16;
constexpr size_t kFrameVtxCapacity = 1 << 16;

// Metroid's collision rule (bank 7 $E7F9): patterns below $A0 are solid.
constexpr uint8_t kFirstPassableTile = 0xA0;

// Face brightness gives the blocks depth without needing lights.
constexpr uint8_t kShadeFront = 255, kShadeTop = 210, kShadeSide = 160, kShadeBottom = 110, kShadeDecor = 190;

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

bool Renderer::TileBlank(const Emulator& emu, uint16_t patternAddr) const {
    const uint8_t* p = emu.Chr() + (patternAddr & 0x1FFF);
    for (int i = 0; i < 16; i++) {
        if (p[i]) {
            return false;
        }
    }
    return true;
}

const uint8_t* Renderer::TileTexture(const Emulator& emu, uint16_t patternAddr, const uint8_t* palette4,
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
            uint32_t c = (ci == 0 && transparent) ? 0 : Nes::Machine::Color(palette4[ci]);
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

void Renderer::DrawModel(Gfx*& g, const Gfx* dl, const Mat4& model) {
    Mtx* m = AllocMtx(model);
    if (m == nullptr || dl == nullptr || g + 32 >= mFrameEnd) {
        return;
    }
    gDPPipeSync(g++);
    gSPSetLights1(g++, sModelLights);
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
    gDPPipeSync(g++);
    gSPSetLights1(g++, sModelLights);
    pose.Draw(g, skel, model, mViewMat, [this](const Mat4& m) { return AllocMtx(m); }, limbList);
    gSPMatrix(g++, &mView, G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH);
    SetTexturedState(g, true);
}

// --- Play field -------------------------------------------------------------------------------

void Renderer::BuildChunk(const Emulator& emu, Chunk& chunk, uint16_t ntBase, int cx, int cy) {
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

void Renderer::DrawPlayField(Gfx*& g, const Emulator& emu) {
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
                    int dx = WrapInto(px + kChunkPx / 2 - scrollX, kScreenW / 2 - periodX / 2, periodX) -
                             kChunkPx / 2;
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

// --- Sprites ----------------------------------------------------------------------------------

void Renderer::DrawSprites(Gfx*& g, const Emulator& emu, bool hudPass, bool hideSamus) {
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

void Renderer::DrawSamusModel(Gfx*& g, const Emulator& emu, const Model& model, float alpha) {
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

void Renderer::Draw2DFrame(Gfx*& g, const Emulator& emu) {
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

Gfx* Renderer::BuildFrame(const Emulator& emu, float alpha) {
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
        // The camera looks at the middle of the NES screen; between emulated frames it is
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

        DrawPlayField(g, emu);

        const Model* samus = mModels.Samus();
        bool samusVisible = false;
        for (int i = 0; i < 64; i++) {
            samusVisible |= emu.SpriteTags()[i].owner == SpriteOwner::Samus;
        }
        SetTexturedState(g, true);
        DrawSprites(g, emu, false, samus != nullptr);
        if (samus != nullptr && samusVisible) {
            DrawSamusModel(g, emu, *samus, alpha);
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
