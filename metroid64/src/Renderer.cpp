#include "Renderer.h"

#include "Enemy.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <libultraship/bridge/consolevariablebridge.h>
#include <map>

namespace Metroid {

namespace {

constexpr int kNativeW = 320;
constexpr int kNativeH = 240;
constexpr float kBlockFront = 8.0f; // solid blocks span z = -8..+8 around the play plane
constexpr float kBlockBack = -8.0f;
constexpr float kDecorZ = -6.0f;     // passable background tiles sit just behind Samus
constexpr float kBackWallZ = -48.0f; // dim wall closing off each screen
constexpr float kSpriteZ = 0.0f;
constexpr float kSamusZ = 1.0f;
constexpr float kShotZ = 2.0f;
constexpr int kSpriteTex = 64; // sprite frames are 64x64 canvases (4096 texels: one load block)
constexpr size_t kFrameGfxCapacity = 1 << 16;
constexpr size_t kFrameVtxCapacity = 1 << 16;

// Face brightness gives the blocks depth without needing lights.
constexpr uint8_t kShadeFront = 255, kShadeTop = 210, kShadeSide = 160, kShadeBottom = 110;

const uint32_t kAreaWallColor[AREA_COUNT] = { 0x000000FF, 0x0C1426FF, 0x1E0A18FF, 0x101016FF, 0x0C1A10FF, 0x1C0810FF };

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

float Lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

Lights1 sModelLights = gdSPDefLights1(0x50, 0x50, 0x50, 0xFF, 0xFF, 0xFF, 0x28, 0x50, 0x50);

} // namespace

void Renderer::Init(const Rom* rom) {
    mRom = rom;
    for (auto& s : mScreens) {
        s = ScreenMesh{};
    }
    mTextures.clear();
    mSpriteTextures.clear();
    mVisuals.Init(rom);
    mFrameDl.resize(kFrameGfxCapacity);
    mFrameVtx.reserve(kFrameVtxCapacity);
    mFrameMtx.reserve(512);
    mViewport.vp.vscale[0] = kNativeW * 2;
    mViewport.vp.vscale[1] = kNativeH * 2;
    mViewport.vp.vscale[2] = G_MAXZ / 2;
    mViewport.vp.vscale[3] = 0;
    mViewport.vp.vtrans[0] = kNativeW * 2;
    mViewport.vp.vtrans[1] = kNativeH * 2;
    mViewport.vp.vtrans[2] = G_MAXZ / 2;
    mViewport.vp.vtrans[3] = 0;
    mCamTick = -1;
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

const uint8_t* Renderer::SpriteTexture(const SpriteFrame& f) {
    auto it = mSpriteTextures.find(&f);
    if (it != mSpriteTextures.end()) {
        return it->second.get();
    }
    auto tex = std::make_unique<uint8_t[]>(kSpriteTex * kSpriteTex * 4);
    for (int y = 0; y < kSpriteTex; y++) {
        for (int x = 0; x < kSpriteTex; x++) {
            uint32_t c = (x < f.w && y < f.h) ? f.rgba[y * f.w + x] : 0;
            uint8_t* p = &tex[(y * kSpriteTex + x) * 4];
            p[0] = c >> 24;
            p[1] = c >> 16;
            p[2] = c >> 8;
            p[3] = c;
        }
    }
    const uint8_t* ptr = tex.get();
    mSpriteTextures.emplace(&f, std::move(tex));
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

void Renderer::AddFlat(std::vector<Quad>& out, float cx, float cy, float half, float z, uint32_t rgba) {
    Quad q;
    float yc = -cy;
    SetVtx(q.v[0], cx - half, yc - half, z, 0, 0, rgba, 255);
    SetVtx(q.v[1], cx + half, yc - half, z, 0, 0, rgba, 255);
    SetVtx(q.v[2], cx + half, yc + half, z, 0, 0, rgba, 255);
    SetVtx(q.v[3], cx - half, yc + half, z, 0, 0, rgba, 255);
    out.push_back(q);
}

void Renderer::EmitTexturedQuads(Gfx*& g, const uint8_t* tex, int size, const std::vector<Quad>& quads,
                                 std::vector<Vtx>& store) {
    if (quads.empty()) {
        return;
    }
    if (size == 8) {
        gDPLoadTextureBlock(g++, tex, G_IM_FMT_RGBA, G_IM_SIZ_32b, 8, 8, 0, G_TX_NOMIRROR | G_TX_CLAMP,
                            G_TX_NOMIRROR | G_TX_CLAMP, 3, 3, G_TX_NOLOD, G_TX_NOLOD);
    } else {
        gDPLoadTextureBlock(g++, tex, G_IM_FMT_RGBA, G_IM_SIZ_32b, kSpriteTex, kSpriteTex, 0,
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

Mtx* Renderer::AllocMtx(const Mat4& m) {
    if (mFrameMtx.size() >= mFrameMtx.capacity()) {
        return nullptr;
    }
    mFrameMtx.emplace_back();
    ToMtx(m, &mFrameMtx.back());
    return &mFrameMtx.back();
}

void Renderer::SetSpriteState(Gfx*& g) {
    gDPPipeSync(g++);
    gSPClearGeometryMode(g++, G_LIGHTING | G_FOG | G_TEXTURE_GEN | G_TEXTURE_GEN_LINEAR | G_CULL_BOTH);
    gSPSetGeometryMode(g++, G_ZBUFFER | G_SHADE | G_SHADING_SMOOTH);
    gDPSetCycleType(g++, G_CYC_1CYCLE);
    gDPSetTextureFilter(g++, G_TF_POINT);
    gDPSetTexturePersp(g++, G_TP_PERSP);
    gDPSetTextureLUT(g++, G_TT_NONE);
    gSPTexture(g++, 0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_ON);
    gDPSetCombineMode(g++, G_CC_MODULATERGBA, G_CC_MODULATERGBA);
    gDPSetRenderMode(g++, G_RM_AA_ZB_TEX_EDGE, G_RM_AA_ZB_TEX_EDGE2);
}

void Renderer::SetColorState(Gfx*& g) {
    gDPPipeSync(g++);
    gSPClearGeometryMode(g++, G_LIGHTING);
    gDPSetCombineMode(g++, G_CC_SHADE, G_CC_SHADE);
    gDPSetRenderMode(g++, G_RM_AA_ZB_OPA_SURF, G_RM_AA_ZB_OPA_SURF2);
}

void Renderer::DrawSprite(Gfx*& g, const SpriteFrame& f, float originX, float originY, bool mirror, float angleDeg,
                          uint32_t tint, float z) {
    if (f.Empty() || g + 32 >= mFrameEnd) {
        return;
    }
    // Corners of the 64x64 canvas around the origin, rotated in the play plane.
    float c = std::cos(angleDeg * (float)M_PI / 180.0f), s = std::sin(angleDeg * (float)M_PI / 180.0f);
    float left = (float)-f.originX, right = (float)(kSpriteTex - f.originX);
    float top = (float)-f.originY, bottom = (float)(kSpriteTex - f.originY);
    auto corner = [&](Vtx& v, float lx, float ly, int sTex, int tTex) {
        // local (lx, ly) in NES orientation (y down) -> world 3D (y up); mirroring flips x
        float wx = mirror ? -lx : lx, wy = -ly;
        float rx = wx * c - wy * s, ry = wx * s + wy * c;
        SetVtx(v, originX + rx, -originY + ry, z, sTex, tTex, tint, 255);
    };
    Quad q;
    corner(q.v[0], left, bottom, 0, kSpriteTex);
    corner(q.v[1], right, bottom, kSpriteTex, kSpriteTex);
    corner(q.v[2], right, top, kSpriteTex, 0);
    corner(q.v[3], left, top, 0, 0);
    EmitTexturedQuads(g, SpriteTexture(f), kSpriteTex, { q }, mFrameVtx);
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
    SetSpriteState(g);
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
        EmitTexturedQuads(g, tex, 8, quads, mesh.vtx);
    }
    gSPEndDisplayList(g++);
    mesh.dl.resize(g - mesh.dl.data());
}

void Renderer::UpdateCamera(const Game& game) {
    // One easing step per game tick so the camera is independent of the render rate.
    const Samus& s = game.GetSamus();
    Vec3 target = { s.x, -(s.y - 20.0f), 0.0f };
    if (mCamTick < 0) {
        mCamPrev = mCamCur = target;
        mCamTick = game.FrameCount();
        return;
    }
    int steps = std::min(game.FrameCount() - mCamTick, 8);
    for (int i = 0; i < steps; i++) {
        mCamPrev = mCamCur;
        mCamCur.x += (target.x - mCamCur.x) * 0.15f;
        mCamCur.y += (target.y - mCamCur.y) * 0.10f;
    }
    if (std::fabs(target.x - mCamCur.x) > kScreenPxW || std::fabs(target.y - mCamCur.y) > kScreenPxH) {
        mCamPrev = mCamCur = target; // teleported (elevator, respawn)
    }
    mCamTick = game.FrameCount();
}

Gfx* Renderer::BuildFrame(const Game& game, float alpha) {
    const Samus& samus = game.GetSamus();
    mFrameVtx.clear();
    mFrameMtx.clear();
    Gfx* g = mFrameDl.data();
    mFrameEnd = mFrameDl.data() + mFrameDl.size() - 64;

    UpdateCamera(game);
    float camX = Lerp(mCamPrev.x, mCamCur.x, alpha), camY = Lerp(mCamPrev.y, mCamCur.y, alpha);
    float fov = CVarGetFloat("gMetroid64.Camera.Fov", 52.0f);
    float dist = CVarGetFloat("gMetroid64.Camera.Distance", 300.0f);
    float height = CVarGetFloat("gMetroid64.Camera.Height", 36.0f);
    Vec3 eye = { camX, camY + height, dist };
    Vec3 at = { camX, camY, 0.0f };
    ToMtx(Perspective(fov, (float)kNativeW / kNativeH, 16.0f, 4000.0f), &mProjection);
    ToMtx(LookAt(eye, at, { 0, 1, 0 }), &mView);

    // Frame setup. Fast3D clears colour and depth at the start of every frame, so no fill
    // rectangle is needed (one drawn with z-buffering still enabled would stamp the depth buffer).
    gDPPipeSync(g++);
    gDPSetScissor(g++, G_SC_NON_INTERLACE, 0, 0, kNativeW, kNativeH);
    gSPViewport(g++, &mViewport);
    gSPMatrix(g++, &mProjection, G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
    gSPMatrix(g++, &mView, G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH);
    gDPSetTextureLOD(g++, G_TL_TILE);
    gDPSetTextureDetail(g++, G_TD_CLAMP);
    gDPSetTextureConvert(g++, G_TC_FILT);
    gDPSetCombineKey(g++, G_CK_NONE);
    gDPSetAlphaCompare(g++, G_AC_NONE);
    SetSpriteState(g);

    int csx = (int)samus.x / kScreenPxW, csy = (int)(samus.y - 8) / kScreenPxH;
    // Wider views (bigger FOV / distance) need more screens around Samus.
    int rangeX = dist * std::tan(fov * (float)M_PI / 360.0f) * 2.4f > kScreenPxW * 1.5f ? 3 : 2;

    // Back walls (untextured)
    SetColorState(g);
    {
        std::vector<Quad> walls;
        for (int sy = csy - 1; sy <= csy + 1; sy++) {
            for (int sx = csx - rangeX; sx <= csx + rangeX; sx++) {
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
    SetSpriteState(g);
    for (int sy = csy - 1; sy <= csy + 1; sy++) {
        for (int sx = csx - rangeX; sx <= csx + rangeX; sx++) {
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
            for (int sx = csx - rangeX; sx <= csx + rangeX; sx++) {
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
            EmitTexturedQuads(g, tex, 8, quads, mFrameVtx);
        }
    }

    float viewL = samus.x - 480, viewR = samus.x + 480, viewT = samus.y - 320, viewB = samus.y + 320;
    auto visible = [&](float x, float y) { return x > viewL && x < viewR && y > viewT && y < viewB; };
    int tick = game.FrameCount();

    // Power-ups on their pedestals
    for (const ItemState& it : game.Items()) {
        float x = (float)it.item.worldPxX, y = (float)it.item.worldPxY;
        if (it.taken || !visible(x, y)) {
            continue;
        }
        VisualInstance v;
        v.pose = Pose::Stand;
        v.ticks = tick;
        v.x = x;
        v.y = y;
        // The original flashes power-ups; pulse the brightness instead.
        uint8_t b = (uint8_t)(200 + 55 * (0.5f + 0.5f * std::sin(tick * 0.2f)));
        v.tint = (b << 24) | (b << 16) | (b << 8) | 0xFF;
        mVisuals.PowerUp(it.item.type)->Draw(*this, g, v);
    }

    // Enemies
    for (const Enemy& e : game.Enemies()) {
        float x = Lerp(e.prevX, e.x, alpha), y = Lerp(e.prevY, e.y, alpha);
        if (!visible(x, y) || (e.hurt > 0 && (e.hurt / 2) % 2 == 1)) {
            continue;
        }
        VisualInstance v;
        v.pose = Pose::Move;
        v.ticks = e.ticks;
        v.x = x;
        v.y = y;
        v.facing = e.facing;
        v.angle = e.angle;
        v.tint = e.frozen > 0 ? 0x90E8FFFF : 0xFFFFFFFF;
        mVisuals.Enemy(e.area, e.type)->Draw(*this, g, v);
    }

    // Samus (flickers while invulnerable)
    if (game.Fade() < 0.99f && !(samus.invuln > 0 && (samus.invuln / 3) % 2 == 1)) {
        VisualInstance v;
        v.pose = samus.pose;
        v.ticks = samus.poseTicks;
        v.x = Lerp(samus.prevX, samus.x, alpha);
        v.y = Lerp(samus.prevY, samus.y, alpha);
        v.facing = samus.facing;
        mVisuals.Samus()->Draw(*this, g, v);
    }

    // Beams, missiles and bombs (ROM sprites)
    for (const Projectile& p : game.Projectiles()) {
        float x = Lerp(p.prevX, p.x, alpha), y = Lerp(p.prevY, p.y, alpha);
        uint8_t frame = kFrameBeam;
        bool mirror = p.vx < 0;
        if (p.missile) {
            frame = p.vx == 0 ? kFrameMissileUp : kFrameMissileH;
        } else if (samus.Has(PU_WAVE_BEAM)) {
            frame = kFrameWaveBeam;
        }
        uint32_t tint = (!p.missile && samus.Has(PU_ICE_BEAM)) ? 0x9AF0FFFF : 0xFFFFFFFF;
        DrawSprite(g, mRom->ObjectFrame(frame), x, y, mirror, 0, tint, kShotZ);
    }
    for (const Bomb& b : game.Bombs()) {
        DrawSprite(g, mRom->ObjectFrame((b.fuse / 4) % 2 ? kFrameBomb0 : kFrameBomb1), b.x, b.y, false, 0, 0xFFFFFFFF,
                   kShotZ);
    }

    // Flat colour sprites: effects, enemy shots, pickups
    SetColorState(g);
    {
        std::vector<Quad> flats;
        for (const Effect& e : game.Effects()) {
            float t = (float)e.age / e.life;
            AddFlat(flats, e.x, e.y, 2.0f + 8.0f * t, kShotZ, e.color);
        }
        for (const EnemyShot& p : game.EnemyShots()) {
            AddFlat(flats, Lerp(p.prevX, p.x, alpha), Lerp(p.prevY, p.y, alpha), 2.5f, kShotZ, 0xFF8030FF);
        }
        for (const Pickup& p : game.Pickups()) {
            if (p.life < 90 && (p.life / 4) % 2) {
                continue;
            }
            uint32_t c = p.missile ? 0x40D040FF : ((tick / 4) % 2 ? 0xF0D040FF : 0xF04020FF);
            AddFlat(flats, p.x, p.y, 3.0f, kShotZ, c);
        }
        EmitColorQuads(g, flats);
    }

    // Environment objects (3D): doors and elevator platforms
    {
        std::vector<Quad> boxes;
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
        for (const ElevatorState& e : game.Elevators()) {
            float y = (float)e.platY;
            float x0 = (float)e.platX0, x1 = (float)e.platX1;
            if (game.Riding() && std::fabs((x0 + x1) * 0.5f - samus.x) < 1 &&
                std::fabs(e.platY - samus.y) < kScreenPxH * 1.5f) {
                y = Lerp(samus.prevY, samus.y, alpha);
            }
            if (visible(x0, y)) {
                AddBox(boxes, x0, y, -14, x1, y + 6, 14, 0xD08030FF);
            }
        }
        EmitColorQuads(g, boxes);
    }

    // Fade to black (elevators, death)
    if (game.Fade() > 0.0f) {
        gDPPipeSync(g++);
        gDPSetRenderMode(g++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
        gSPClearGeometryMode(g++, G_ZBUFFER);
        std::vector<Quad> fade;
        Quad q;
        uint32_t c = (uint32_t)(game.Fade() * 255.0f) & 0xFF;
        float z = dist * 0.5f;
        SetVtx(q.v[0], eye.x - 2000, eye.y - 2000, z, 0, 0, c, 255);
        SetVtx(q.v[1], eye.x + 2000, eye.y - 2000, z, 0, 0, c, 255);
        SetVtx(q.v[2], eye.x + 2000, eye.y + 2000, z, 0, 0, c, 255);
        SetVtx(q.v[3], eye.x - 2000, eye.y + 2000, z, 0, 0, c, 255);
        fade.push_back(q);
        EmitColorQuads(g, fade);
    }

    gDPPipeSync(g++);
    gDPFullSync(g++);
    gSPEndDisplayList(g++);
    return mFrameDl.data();
}

} // namespace Metroid
