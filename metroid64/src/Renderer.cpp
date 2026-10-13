#include "Renderer.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>

namespace Metroid {

namespace {

constexpr int kNativeW = 320;
constexpr int kNativeH = 240;
constexpr float kBlockFront = 8.0f; // solid blocks span z = -8..+8 around the play plane
constexpr float kBlockBack = -8.0f;
constexpr float kDecorZ = -6.0f;     // passable background tiles sit just behind Samus
constexpr float kBackWallZ = -48.0f; // dim wall closing off each screen
constexpr int kSamusTexW = 32;
constexpr int kSamusTexH = 48;
constexpr size_t kFrameGfxCapacity = 1 << 16;
constexpr size_t kFrameVtxCapacity = 1 << 16;

// Face brightness gives the blocks depth without needing lights.
constexpr uint8_t kShadeFront = 255, kShadeTop = 210, kShadeSide = 160, kShadeBottom = 110;

const uint32_t kAreaWallColor[AREA_COUNT] = { 0x000000FF, 0x0C1426FF, 0x1E0A18FF, 0x101016FF, 0x0C1A10FF, 0x1C0810FF };

const uint32_t kItemColor[PU_COUNT] = {
    0xE0E040FF, // bombs
    0x40E0FFFF, // high jump
    0xFFFFFFFF, // long beam
    0x40FF60FF, // screw attack
    0xFF8020FF, // maru mari
    0xFFA0FFFF, // varia
    0xC040FFFF, // wave beam
    0x80F0FFFF, // ice beam
    0xF02040FF, // energy tank
    0x30C030FF, // missiles
};

void SetVtx(Vtx& v, float x, float y, float z, int s, int t, uint32_t rgba, uint8_t shade) {
    v.v.ob[0] = (short)std::lround(x);
    v.v.ob[1] = (short)std::lround(y);
    v.v.ob[2] = (short)std::lround(z);
    v.v.flag = 0;
    v.v.tc[0] = (short)(s << 5);
    v.v.tc[1] = (short)(t << 5);
    v.v.cn[0] = (uint8_t)(((rgba >> 24) & 0xFF) * shade / 255);
    v.v.cn[1] = (uint8_t)(((rgba >> 16) & 0xFF) * shade / 255);
    v.v.cn[2] = (uint8_t)(((rgba >> 8) & 0xFF) * shade / 255);
    v.v.cn[3] = (uint8_t)(rgba & 0xFF);
}

} // namespace

void Renderer::Init(const Rom* rom) {
    mRom = rom;
    for (auto& s : mScreens) {
        s = ScreenMesh{};
    }
    mFrameDl.resize(kFrameGfxCapacity);
    mFrameVtx.reserve(kFrameVtxCapacity);
    mViewport.vp.vscale[0] = kNativeW * 2;
    mViewport.vp.vscale[1] = kNativeH * 2;
    mViewport.vp.vscale[2] = G_MAXZ / 2;
    mViewport.vp.vscale[3] = 0;
    mViewport.vp.vtrans[0] = kNativeW * 2;
    mViewport.vp.vtrans[1] = kNativeH * 2;
    mViewport.vp.vtrans[2] = G_MAXZ / 2;
    mViewport.vp.vtrans[3] = 0;
    mCameraInit = false;
}

const uint8_t* Renderer::TileTexture(Area area, bool alt, uint8_t chr, uint8_t pal, bool transparent) {
    uint64_t key =
        ((uint64_t)area << 16) | ((uint64_t)alt << 15) | ((uint64_t)transparent << 14) | ((uint64_t)pal << 8) | chr;
    auto it = mTextures.find(key);
    if (it != mTextures.end()) {
        return it->second.get();
    }
    uint32_t px[64];
    mRom->TilePixels(area, alt, chr, pal, px, transparent);
    auto tex = std::make_unique<uint8_t[]>(64 * 4);
    for (int i = 0; i < 64; i++) {
        tex[i * 4 + 0] = px[i] >> 24;
        tex[i * 4 + 1] = px[i] >> 16;
        tex[i * 4 + 2] = px[i] >> 8;
        tex[i * 4 + 3] = px[i];
    }
    const uint8_t* ptr = tex.get();
    mTextures.emplace(key, std::move(tex));
    return ptr;
}

const uint8_t* Renderer::SamusTexture(int frame, bool flip) {
    int key = frame * 2 + (flip ? 1 : 0);
    auto it = mSamusTextures.find(key);
    if (it != mSamusTextures.end()) {
        return it->second.get();
    }
    const SpriteFrame& f = mRom->SamusFrame(frame);
    auto tex = std::make_unique<uint8_t[]>(kSamusTexW * kSamusTexH * 4);
    int x0 = f.originX - kSamusTexW / 2;
    int y0 = f.originY - kSamusTexH / 2;
    for (int y = 0; y < kSamusTexH; y++) {
        for (int x = 0; x < kSamusTexW; x++) {
            int sx = x0 + (flip ? kSamusTexW - 1 - x : x), sy = y0 + y;
            uint32_t c = (sx >= 0 && sy >= 0 && sx < f.w && sy < f.h) ? f.rgba[sy * f.w + sx] : 0;
            uint8_t* p = &tex[(y * kSamusTexW + x) * 4];
            p[0] = c >> 24;
            p[1] = c >> 16;
            p[2] = c >> 8;
            p[3] = c;
        }
    }
    const uint8_t* ptr = tex.get();
    mSamusTextures.emplace(key, std::move(tex));
    return ptr;
}

void Renderer::AddTileBlock(std::vector<Quad>& out, int tx, int ty, bool cullNeighbours) {
    float x0 = tx * 8.0f, x1 = x0 + 8.0f;
    float yt = -(ty * 8.0f), yb = yt - 8.0f; // world y is up
    auto occluded = [&](int nx, int ny) {
        if (!cullNeighbours || nx < 0 || ny < 0 || nx >= kWorldTilesW || ny >= kWorldTilesH) {
            return false;
        }
        return mRom->Tile(nx, ny).kind == TILE_SOLID;
    };
    const uint32_t white = 0xFFFFFFFF;
    Quad q;
    // Front
    SetVtx(q.v[0], x0, yb, kBlockFront, 0, 8, white, kShadeFront);
    SetVtx(q.v[1], x1, yb, kBlockFront, 8, 8, white, kShadeFront);
    SetVtx(q.v[2], x1, yt, kBlockFront, 8, 0, white, kShadeFront);
    SetVtx(q.v[3], x0, yt, kBlockFront, 0, 0, white, kShadeFront);
    out.push_back(q);
    if (!occluded(tx, ty - 1)) { // top
        SetVtx(q.v[0], x0, yt, kBlockFront, 0, 8, white, kShadeTop);
        SetVtx(q.v[1], x1, yt, kBlockFront, 8, 8, white, kShadeTop);
        SetVtx(q.v[2], x1, yt, kBlockBack, 8, 0, white, kShadeTop);
        SetVtx(q.v[3], x0, yt, kBlockBack, 0, 0, white, kShadeTop);
        out.push_back(q);
    }
    if (!occluded(tx, ty + 1)) { // bottom
        SetVtx(q.v[0], x0, yb, kBlockBack, 0, 8, white, kShadeBottom);
        SetVtx(q.v[1], x1, yb, kBlockBack, 8, 8, white, kShadeBottom);
        SetVtx(q.v[2], x1, yb, kBlockFront, 8, 0, white, kShadeBottom);
        SetVtx(q.v[3], x0, yb, kBlockFront, 0, 0, white, kShadeBottom);
        out.push_back(q);
    }
    if (!occluded(tx - 1, ty)) { // left
        SetVtx(q.v[0], x0, yb, kBlockBack, 0, 8, white, kShadeSide);
        SetVtx(q.v[1], x0, yb, kBlockFront, 8, 8, white, kShadeSide);
        SetVtx(q.v[2], x0, yt, kBlockFront, 8, 0, white, kShadeSide);
        SetVtx(q.v[3], x0, yt, kBlockBack, 0, 0, white, kShadeSide);
        out.push_back(q);
    }
    if (!occluded(tx + 1, ty)) { // right
        SetVtx(q.v[0], x1, yb, kBlockFront, 0, 8, white, kShadeSide);
        SetVtx(q.v[1], x1, yb, kBlockBack, 8, 8, white, kShadeSide);
        SetVtx(q.v[2], x1, yt, kBlockBack, 8, 0, white, kShadeSide);
        SetVtx(q.v[3], x1, yt, kBlockFront, 0, 0, white, kShadeSide);
        out.push_back(q);
    }
}

void Renderer::AddBox(std::vector<Quad>& out, float x0, float y0, float z0, float x1, float y1, float z1,
                      uint32_t rgba) {
    // x/y in world pixels with NES y-down convention.
    float yt = -y0, yb = -y1;
    Quad q;
    SetVtx(q.v[0], x0, yb, z1, 0, 0, rgba, kShadeFront);
    SetVtx(q.v[1], x1, yb, z1, 0, 0, rgba, kShadeFront);
    SetVtx(q.v[2], x1, yt, z1, 0, 0, rgba, kShadeFront);
    SetVtx(q.v[3], x0, yt, z1, 0, 0, rgba, kShadeFront);
    out.push_back(q);
    SetVtx(q.v[0], x0, yt, z1, 0, 0, rgba, kShadeTop);
    SetVtx(q.v[1], x1, yt, z1, 0, 0, rgba, kShadeTop);
    SetVtx(q.v[2], x1, yt, z0, 0, 0, rgba, kShadeTop);
    SetVtx(q.v[3], x0, yt, z0, 0, 0, rgba, kShadeTop);
    out.push_back(q);
    SetVtx(q.v[0], x0, yb, z0, 0, 0, rgba, kShadeBottom);
    SetVtx(q.v[1], x1, yb, z0, 0, 0, rgba, kShadeBottom);
    SetVtx(q.v[2], x1, yb, z1, 0, 0, rgba, kShadeBottom);
    SetVtx(q.v[3], x0, yb, z1, 0, 0, rgba, kShadeBottom);
    out.push_back(q);
    SetVtx(q.v[0], x0, yb, z0, 0, 0, rgba, kShadeSide);
    SetVtx(q.v[1], x0, yb, z1, 0, 0, rgba, kShadeSide);
    SetVtx(q.v[2], x0, yt, z1, 0, 0, rgba, kShadeSide);
    SetVtx(q.v[3], x0, yt, z0, 0, 0, rgba, kShadeSide);
    out.push_back(q);
    SetVtx(q.v[0], x1, yb, z1, 0, 0, rgba, kShadeSide);
    SetVtx(q.v[1], x1, yb, z0, 0, 0, rgba, kShadeSide);
    SetVtx(q.v[2], x1, yt, z0, 0, 0, rgba, kShadeSide);
    SetVtx(q.v[3], x1, yt, z1, 0, 0, rgba, kShadeSide);
    out.push_back(q);
}

void Renderer::EmitTexturedQuads(Gfx*& g, const uint8_t* tex, int w, int h, const std::vector<Quad>& quads,
                                 std::vector<Vtx>& store) {
    if (quads.empty()) {
        return;
    }
    if (w == 8 && h == 8) {
        gDPLoadTextureBlock(g++, tex, G_IM_FMT_RGBA, G_IM_SIZ_32b, 8, 8, 0, G_TX_NOMIRROR | G_TX_CLAMP,
                            G_TX_NOMIRROR | G_TX_CLAMP, 3, 3, G_TX_NOLOD, G_TX_NOLOD);
    } else {
        gDPLoadTextureBlock(g++, tex, G_IM_FMT_RGBA, G_IM_SIZ_32b, kSamusTexW, kSamusTexH, 0,
                            G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK,
                            G_TX_NOLOD, G_TX_NOLOD);
    }
    for (size_t i = 0; i < quads.size(); i += 8) {
        size_t n = std::min<size_t>(8, quads.size() - i);
        if (store.size() + n * 4 > store.capacity()) {
            return; // out of per-frame vertex space; drop the rest rather than reallocate under the DL
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

void Renderer::EmitColorQuads(Gfx*& g, const std::vector<Quad>& quads) {
    for (size_t i = 0; i < quads.size(); i += 8) {
        size_t n = std::min<size_t>(8, quads.size() - i);
        if (mFrameVtx.size() + n * 4 > mFrameVtx.capacity()) {
            return;
        }
        Vtx* base = mFrameVtx.data() + mFrameVtx.size();
        for (size_t k = 0; k < n; k++) {
            mFrameVtx.insert(mFrameVtx.end(), quads[i + k].v, quads[i + k].v + 4);
        }
        gSPVertex(g++, base, (int)(n * 4), 0);
        for (size_t k = 0; k < n; k++) {
            int o = (int)k * 4;
            gSP2Triangles(g++, o, o + 1, o + 2, 0, o, o + 2, o + 3, 0);
        }
    }
}

void Renderer::BuildScreen(int sx, int sy) {
    ScreenMesh& mesh = mScreens[sy * kMapW + sx];
    mesh.built = true;
    Area area = mRom->ScreenArea(sx, sy);
    if (area == AREA_NONE) {
        mesh.dl.push_back(gsSPEndDisplayList());
        return;
    }
    bool alt = mRom->ScreenAltPalette(sx, sy);

    // Group faces by texture so each texture is loaded once per screen.
    std::map<const uint8_t*, std::vector<Quad>> groups;
    for (int ty = sy * kScreenTilesH; ty < (sy + 1) * kScreenTilesH; ty++) {
        for (int tx = sx * kScreenTilesW; tx < (sx + 1) * kScreenTilesW; tx++) {
            const WorldTile& t = mRom->Tile(tx, ty);
            if (t.kind == TILE_SOLID) {
                AddTileBlock(groups[TileTexture(area, alt, t.chr, t.palette, false)], tx, ty, true);
            } else if (t.kind == TILE_BG && !mRom->TileBlank(area, t.chr)) {
                Quad q;
                float x0 = tx * 8.0f, yt = -(ty * 8.0f);
                const uint32_t c = 0xFFFFFFFF;
                SetVtx(q.v[0], x0, yt - 8, kDecorZ, 0, 8, c, 200);
                SetVtx(q.v[1], x0 + 8, yt - 8, kDecorZ, 8, 8, c, 200);
                SetVtx(q.v[2], x0 + 8, yt, kDecorZ, 8, 0, c, 200);
                SetVtx(q.v[3], x0, yt, kDecorZ, 0, 0, c, 200);
                groups[TileTexture(area, alt, t.chr, t.palette, true)].push_back(q);
            }
        }
    }

    size_t quadCount = 0;
    for (auto& [tex, quads] : groups) {
        quadCount += quads.size();
    }
    mesh.vtx.reserve(quadCount * 4);
    // Worst case: 8 cmds per texture load + one vertex load and 8 tri pairs per 8 quads.
    mesh.dl.resize(groups.size() * 8 + (quadCount / 8 + 1) * 10 + groups.size() * 10 + 4);
    Gfx* g = mesh.dl.data();
    for (auto& [tex, quads] : groups) {
        EmitTexturedQuads(g, tex, 8, 8, quads, mesh.vtx);
    }
    gSPEndDisplayList(g++);
    mesh.dl.resize(g - mesh.dl.data());
}

Gfx* Renderer::BuildFrame(const Game& game) {
    const Samus& samus = game.GetSamus();
    mFrameVtx.clear();
    Gfx* g = mFrameDl.data();

    // Camera: follows Samus from slightly above, easing behind her movement.
    Vec3 target = { samus.x, -(samus.y - 20.0f), 0.0f };
    if (!mCameraInit) {
        mCamera = target;
        mCameraInit = true;
    }
    mCamera.x += (target.x - mCamera.x) * 0.15f;
    mCamera.y += (target.y - mCamera.y) * 0.10f;
    Vec3 eye = { mCamera.x, mCamera.y + 36.0f, 300.0f };
    Vec3 at = { mCamera.x, mCamera.y, 0.0f };
    ToMtx(Perspective(52.0f, (float)kNativeW / kNativeH, 16.0f, 2000.0f), &mProjection);
    ToMtx(LookAt(eye, at, { 0, 1, 0 }), &mView);

    // Frame setup. Fast3D clears colour and depth at the start of every frame, so no fill
    // rectangle is needed (one drawn with z-buffering still enabled would stamp the depth buffer).
    gDPPipeSync(g++);
    gDPSetScissor(g++, G_SC_NON_INTERLACE, 0, 0, kNativeW, kNativeH);

    gSPViewport(g++, &mViewport);
    gSPMatrix(g++, &mProjection, G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
    gSPMatrix(g++, &mView, G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH);
    gDPSetCycleType(g++, G_CYC_1CYCLE);
    gSPClearGeometryMode(g++, G_LIGHTING | G_FOG | G_TEXTURE_GEN | G_TEXTURE_GEN_LINEAR | G_CULL_BOTH);
    gSPSetGeometryMode(g++, G_ZBUFFER | G_SHADE | G_SHADING_SMOOTH);
    gDPSetTextureFilter(g++, G_TF_POINT);
    gDPSetTexturePersp(g++, G_TP_PERSP);
    gDPSetTextureLUT(g++, G_TT_NONE);
    gDPSetTextureLOD(g++, G_TL_TILE);
    gDPSetTextureDetail(g++, G_TD_CLAMP);
    gDPSetTextureConvert(g++, G_TC_FILT);
    gDPSetCombineKey(g++, G_CK_NONE);
    gDPSetAlphaCompare(g++, G_AC_NONE);
    gSPTexture(g++, 0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_ON);

    // Back walls (untextured)
    int csx = (int)samus.x / kScreenPxW, csy = (int)(samus.y - 8) / kScreenPxH;
    gDPSetCombineMode(g++, G_CC_SHADE, G_CC_SHADE);
    gDPSetRenderMode(g++, G_RM_AA_ZB_OPA_SURF, G_RM_AA_ZB_OPA_SURF2);
    {
        std::vector<Quad> walls;
        for (int sy = csy - 1; sy <= csy + 1; sy++) {
            for (int sx = csx - 2; sx <= csx + 2; sx++) {
                Area a = mRom->ScreenArea(sx, sy);
                if (a == AREA_NONE) {
                    continue;
                }
                Quad q;
                float x0 = sx * (float)kScreenPxW, y0 = -(sy * (float)kScreenPxH);
                uint32_t c = kAreaWallColor[a];
                SetVtx(q.v[0], x0, y0 - kScreenPxH, kBackWallZ, 0, 0, c, 255);
                SetVtx(q.v[1], x0 + kScreenPxW, y0 - kScreenPxH, kBackWallZ, 0, 0, c, 255);
                SetVtx(q.v[2], x0 + kScreenPxW, y0, kBackWallZ, 0, 0, c, 255);
                SetVtx(q.v[3], x0, y0, kBackWallZ, 0, 0, c, 255);
                walls.push_back(q);
            }
        }
        EmitColorQuads(g, walls);
    }

    // Static world geometry
    gDPPipeSync(g++);
    gDPSetCombineMode(g++, G_CC_MODULATERGBA, G_CC_MODULATERGBA);
    gDPSetRenderMode(g++, G_RM_AA_ZB_TEX_EDGE, G_RM_AA_ZB_TEX_EDGE2);
    for (int sy = csy - 1; sy <= csy + 1; sy++) {
        for (int sx = csx - 2; sx <= csx + 2; sx++) {
            if (sx < 0 || sy < 0 || sx >= kMapW || sy >= kMapH) {
                continue;
            }
            ScreenMesh& mesh = mScreens[sy * kMapW + sx];
            if (!mesh.built) {
                BuildScreen(sx, sy);
            }
            if (mesh.dl.size() > 1) {
                gSPDisplayList(g++, mesh.dl.data());
            }
        }
    }

    // Blastable blocks (dynamic: they disappear and regrow)
    {
        std::map<const uint8_t*, std::vector<Quad>> groups;
        for (int sy = csy - 1; sy <= csy + 1; sy++) {
            for (int sx = csx - 2; sx <= csx + 2; sx++) {
                Area area = mRom->ScreenArea(sx, sy);
                if (area == AREA_NONE) {
                    continue;
                }
                bool alt = mRom->ScreenAltPalette(sx, sy);
                for (int ty = sy * kScreenTilesH; ty < (sy + 1) * kScreenTilesH; ty++) {
                    for (int tx = sx * kScreenTilesW; tx < (sx + 1) * kScreenTilesW; tx++) {
                        const WorldTile& t = mRom->Tile(tx, ty);
                        if (t.kind == TILE_BLAST && game.TileIntact(tx, ty)) {
                            AddTileBlock(groups[TileTexture(area, alt, t.chr, t.palette, false)], tx, ty, true);
                        }
                    }
                }
            }
        }
        for (auto& [tex, quads] : groups) {
            EmitTexturedQuads(g, tex, 8, 8, quads, mFrameVtx);
        }
    }

    // Samus: the NES sprite extruded into a few layers so she has some thickness.
    if (game.Fade() < 0.99f) {
        int frame = game.SamusFrame();
        const uint8_t* tex = SamusTexture(frame, samus.facing < 0);
        const SpriteFrame& f = mRom->SamusFrame(frame);
        // Align the lowest opaque row of the frame with her feet.
        int bottom = 0;
        for (int y = 0; y < f.h; y++) {
            for (int x = 0; x < f.w; x++) {
                if (f.rgba[y * f.w + x] & 0xFF) {
                    bottom = y;
                }
            }
        }
        int texTop = f.originY - kSamusTexH / 2;
        float worldTop = samus.y - (bottom + 1 - texTop);
        float x0 = samus.x - kSamusTexW / 2.0f;
        std::vector<Quad> layers;
        for (int layer = 0; layer < 5; layer++) {
            float z = -4.0f + layer * 2.0f;
            uint8_t shade = layer == 4 ? 255 : 150;
            Quad q;
            SetVtx(q.v[0], x0, -(worldTop + kSamusTexH), z, 0, kSamusTexH, 0xFFFFFFFF, shade);
            SetVtx(q.v[1], x0 + kSamusTexW, -(worldTop + kSamusTexH), z, kSamusTexW, kSamusTexH, 0xFFFFFFFF, shade);
            SetVtx(q.v[2], x0 + kSamusTexW, -worldTop, z, kSamusTexW, 0, 0xFFFFFFFF, shade);
            SetVtx(q.v[3], x0, -worldTop, z, 0, 0, 0xFFFFFFFF, shade);
            layers.push_back(q);
        }
        EmitTexturedQuads(g, tex, kSamusTexW, kSamusTexH, layers, mFrameVtx);
    }

    // Untextured objects
    gDPPipeSync(g++);
    gDPSetCombineMode(g++, G_CC_SHADE, G_CC_SHADE);
    gDPSetRenderMode(g++, G_RM_AA_ZB_OPA_SURF, G_RM_AA_ZB_OPA_SURF2);
    {
        std::vector<Quad> boxes;
        float pulse = 0.5f + 0.5f * std::sin(game.FrameCount() * 0.15f);
        float viewL = samus.x - 420, viewR = samus.x + 420, viewT = samus.y - 300, viewB = samus.y + 300;
        auto visible = [&](float x, float y) { return x > viewL && x < viewR && y > viewT && y < viewB; };

        for (const DoorState& d : game.Doors()) {
            if (d.open || !visible((float)d.x0, (float)d.y0)) {
                continue;
            }
            uint32_t c = d.door.type == DOOR_BLUE     ? 0x3070F0FF
                         : d.door.type == DOOR_RED    ? 0xE02020FF
                         : d.door.type == DOOR_ORANGE ? 0xF09020FF
                                                      : 0x9090A0FF;
            float cx = (d.x0 + d.x1) * 0.5f;
            AddBox(boxes, cx - 5, (float)d.y0, -10, cx + 5, (float)d.y1, 10, c);
        }
        for (const ItemState& it : game.Items()) {
            float x = (float)it.item.worldPxX, y = (float)it.item.worldPxY;
            if (it.taken || !visible(x, y)) {
                continue;
            }
            float s = 5.0f + pulse * 1.5f;
            AddBox(boxes, x - s, y - s - 2, -s, x + s, y + s - 2, s, kItemColor[it.item.type]);
            AddBox(boxes, x - 8, y + 4, -8, x + 8, y + 8, 8, 0x606070FF); // pedestal
        }
        for (size_t i = 0; i < game.Elevators().size(); i++) {
            const ElevatorState& e = game.Elevators()[i];
            float y = (float)e.platY;
            float x0 = (float)e.platX0, x1 = (float)e.platX1;
            if (game.Riding() && std::fabs((x0 + x1) * 0.5f - samus.x) < 1 &&
                std::fabs(e.platY - samus.y) < kScreenPxH * 1.5f) {
                y = samus.y;
            }
            if (visible(x0, y)) {
                AddBox(boxes, x0, y, -14, x1, y + 6, 14, 0xD08030FF);
            }
        }
        for (const Projectile& p : game.Projectiles()) {
            if (p.missile) {
                bool vertical = p.vx == 0;
                AddBox(boxes, p.x - (vertical ? 2 : 6), p.y - (vertical ? 6 : 2), -2, p.x + (vertical ? 2 : 6),
                       p.y + (vertical ? 6 : 2), 2, 0xF0F0F0FF);
            } else {
                uint32_t c = samus.Has(PU_ICE_BEAM) ? 0x80F0FFFF : samus.Has(PU_WAVE_BEAM) ? 0xE060FFFF : 0xFFE060FF;
                AddBox(boxes, p.x - 3, p.y - 2, -2, p.x + 3, p.y + 2, 2, c);
            }
        }
        for (const Bomb& b : game.Bombs()) {
            float s = 3.0f + (b.fuse % 10 < 5 ? 1.0f : 0.0f);
            AddBox(boxes, b.x - s, b.y - s, -s, b.x + s, b.y + s, s, 0x303030FF);
        }
        for (const Effect& e : game.Effects()) {
            float t = (float)e.age / e.life;
            float s = 2.0f + 8.0f * t;
            AddBox(boxes, e.x - s, e.y - s, -s, e.x + s, e.y + s, s, e.color);
        }
        EmitColorQuads(g, boxes);
    }

    // Fade to black (elevators)
    if (game.Fade() > 0.0f) {
        gDPPipeSync(g++);
        gDPSetRenderMode(g++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
        gSPClearGeometryMode(g++, G_ZBUFFER);
        std::vector<Quad> fade;
        Quad q;
        uint32_t c = (uint32_t)(game.Fade() * 255.0f) & 0xFF;
        float z = 150.0f;
        SetVtx(q.v[0], eye.x - 400, eye.y - 400, z, 0, 0, c, 255);
        SetVtx(q.v[1], eye.x + 400, eye.y - 400, z, 0, 0, c, 255);
        SetVtx(q.v[2], eye.x + 400, eye.y + 400, z, 0, 0, c, 255);
        SetVtx(q.v[3], eye.x - 400, eye.y + 400, z, 0, 0, c, 255);
        fade.push_back(q);
        EmitColorQuads(g, fade);
    }

    gDPPipeSync(g++);
    gDPFullSync(g++);
    gSPEndDisplayList(g++);
    return mFrameDl.data();
}

} // namespace Metroid
