#include "game/data/RoomMesh.h"

#include <cstring>
#include <map>

namespace Game::Data {

namespace {

const char* const kAreaFolders[] = { "", "brinstar", "norfair", "tourian", "kraid", "ridley" };

struct Grid {
    const Section& s;
    const std::vector<ScreenTiles>& screens;
    int Width() const {
        return s.horizontal ? 32 * s.length : 32;
    }
    int Height() const {
        return s.horizontal ? 30 : 30 * s.length;
    }
    const ScreenTiles& Screen(int gx, int gy) const {
        return screens[s.horizontal ? gx / 32 : gy / 30];
    }
    uint8_t Tile(int gx, int gy) const {
        return Screen(gx, gy).Tile(gx % 32, gy % 30);
    }
    uint8_t Palette(int gx, int gy) const {
        return Screen(gx, gy).Palette(gx % 32, gy % 30);
    }
    bool Solid(int gx, int gy) const {
        if (gx < 0 || gy < 0 || gx >= Width() || gy >= Height()) {
            return false;
        }
        return Tile(gx, gy) < kFirstPassableTile;
    }
};

MeshQuad Quad(const int (&p)[4][3], uint8_t shade) {
    static const uint8_t kUv[4][2] = { { 0, 8 }, { 8, 8 }, { 8, 0 }, { 0, 0 } };
    MeshQuad q;
    for (int i = 0; i < 4; i++) {
        for (int k = 0; k < 3; k++) {
            q.pos[i][k] = (int16_t)p[i][k];
        }
        q.uv[i][0] = kUv[i][0];
        q.uv[i][1] = kUv[i][1];
    }
    q.shade = shade;
    return q;
}

void Put8(std::string& out, uint8_t v) {
    out.push_back((char)v);
}
void Put16(std::string& out, uint16_t v) {
    out.push_back((char)(v & 0xFF));
    out.push_back((char)(v >> 8));
}

struct Reader {
    const std::string& d;
    size_t p = 0;
    bool ok = true;
    uint8_t U8() {
        if (p + 1 > d.size()) {
            ok = false;
            return 0;
        }
        return (uint8_t)d[p++];
    }
    uint16_t U16() {
        uint16_t lo = U8();
        return (uint16_t)(lo | (U8() << 8));
    }
};

} // namespace

bool SectionScreens(const Level& level, const Section& s, const EngineRead& engine, std::vector<ScreenTiles>& out) {
    out.assign(s.length, ScreenTiles{});
    for (int i = 0; i < s.length; i++) {
        uint8_t x = s.ScreenX(i), y = s.ScreenY(i);
        if (x > 31 || y > 31) {
            return false;
        }
        if (!DrawScreen(level, s.bank, level.world.rooms[y][x], engine, out[i])) {
            // A room number past the area's room table: the game reads the next table as room pointers. It
            // happens on elevator rides, where the screen is black (the other area's screens pass by), so
            // the screen is left empty.
            memset(out[i].ram, kBlankTile, sizeof(out[i].ram));
        }
    }
    return true;
}

uint64_t LayoutHash(const std::vector<ScreenTiles>& screens) {
    uint64_t h = 0xCBF29CE484222325ull;
    for (const ScreenTiles& s : screens) {
        for (uint8_t b : s.ram) {
            h = (h ^ b) * 0x100000001B3ull;
        }
    }
    return h;
}

MeshChunk BuildMeshChunk(const Section& s, const std::vector<ScreenTiles>& screens, int screen, int cx, int cy) {
    Grid grid{ s, screens };
    MeshChunk chunk;
    chunk.screen = (uint8_t)screen;
    chunk.cx = (uint8_t)cx;
    chunk.cy = (uint8_t)cy;
    int ox = s.horizontal ? screen * 32 : 0, oy = s.horizontal ? 0 : screen * 30;
    std::map<std::pair<uint8_t, uint8_t>, std::vector<MeshQuad>> groups;
    for (int ty = cy * 8; ty < std::min(cy * 8 + 8, 30); ty++) {
        for (int tx = cx * 8; tx < cx * 8 + 8; tx++) {
            int gx = ox + tx, gy = oy + ty;
            uint8_t tile = grid.Tile(gx, gy);
            if (tile == kBlankTile) {
                continue;
            }
            auto& quads = groups[{ tile, grid.Palette(gx, gy) }];
            int x0 = gx * 8, x1 = x0 + 8;
            int yt = -(gy * 8), yb = yt - 8;
            const int f = kBlockFront, b = kBlockBack;
            if (tile >= kFirstPassableTile) {
                const int d = kDecorZ;
                int p[4][3] = { { x0, yb, d }, { x1, yb, d }, { x1, yt, d }, { x0, yt, d } };
                quads.push_back(Quad(p, kShadeDecor));
                continue;
            }
            int front[4][3] = { { x0, yb, f }, { x1, yb, f }, { x1, yt, f }, { x0, yt, f } };
            quads.push_back(Quad(front, kShadeFront));
            if (!grid.Solid(gx, gy - 1)) {
                int p[4][3] = { { x0, yt, f }, { x1, yt, f }, { x1, yt, b }, { x0, yt, b } };
                quads.push_back(Quad(p, kShadeTop));
            }
            if (!grid.Solid(gx, gy + 1)) {
                int p[4][3] = { { x0, yb, b }, { x1, yb, b }, { x1, yb, f }, { x0, yb, f } };
                quads.push_back(Quad(p, kShadeBottom));
            }
            if (!grid.Solid(gx - 1, gy)) {
                int p[4][3] = { { x0, yb, b }, { x0, yb, f }, { x0, yt, f }, { x0, yt, b } };
                quads.push_back(Quad(p, kShadeSide));
            }
            if (!grid.Solid(gx + 1, gy)) {
                int p[4][3] = { { x1, yb, f }, { x1, yb, b }, { x1, yt, b }, { x1, yt, f } };
                quads.push_back(Quad(p, kShadeSide));
            }
        }
    }
    for (auto& [key, quads] : groups) {
        MeshGroup g;
        g.tile = key.first;
        g.palette = key.second;
        g.quads = std::move(quads);
        chunk.groups.push_back(std::move(g));
    }
    return chunk;
}

RoomMesh BuildRoomMesh(const Section& s, const std::vector<ScreenTiles>& screens) {
    RoomMesh mesh;
    mesh.section = s;
    mesh.layoutHash = LayoutHash(screens);
    for (int i = 0; i < s.length; i++) {
        for (int cy = 0; cy < 4; cy++) {
            for (int cx = 0; cx < 4; cx++) {
                mesh.chunks.push_back(BuildMeshChunk(s, screens, i, cx, cy));
            }
        }
    }
    return mesh;
}

std::string EncodeRoomMesh(const RoomMesh& mesh) {
    std::string out = "RMSH";
    Put16(out, 1);
    Put8(out, (uint8_t)mesh.section.bank);
    Put8(out, mesh.section.horizontal ? 1 : 0);
    Put8(out, mesh.section.x);
    Put8(out, mesh.section.y);
    Put8(out, mesh.section.length);
    Put8(out, 0);
    for (int i = 0; i < 4; i++) {
        Put16(out, (uint16_t)(mesh.layoutHash >> (16 * i)));
    }
    Put16(out, (uint16_t)mesh.chunks.size());
    for (const MeshChunk& c : mesh.chunks) {
        Put8(out, c.screen);
        Put8(out, c.cx);
        Put8(out, c.cy);
        Put16(out, (uint16_t)c.groups.size());
        for (const MeshGroup& g : c.groups) {
            Put8(out, g.tile);
            Put8(out, g.palette);
            Put16(out, (uint16_t)g.quads.size());
            for (const MeshQuad& q : g.quads) {
                for (int v = 0; v < 4; v++) {
                    for (int k = 0; k < 3; k++) {
                        Put16(out, (uint16_t)q.pos[v][k]);
                    }
                    Put8(out, q.uv[v][0]);
                    Put8(out, q.uv[v][1]);
                }
                Put8(out, q.shade);
            }
        }
    }
    return out;
}

bool DecodeRoomMesh(const std::string& data, RoomMesh& mesh) {
    if (data.size() < 4 || data.compare(0, 4, "RMSH") != 0) {
        return false;
    }
    Reader r{ data, 4 };
    if (r.U16() != 1) {
        return false;
    }
    mesh = RoomMesh{};
    mesh.section.bank = r.U8();
    mesh.section.horizontal = r.U8() != 0;
    mesh.section.x = r.U8();
    mesh.section.y = r.U8();
    mesh.section.length = r.U8();
    r.U8();
    for (int i = 0; i < 4; i++) {
        mesh.layoutHash |= (uint64_t)r.U16() << (16 * i);
    }
    int chunks = r.U16();
    for (int c = 0; c < chunks && r.ok; c++) {
        MeshChunk chunk;
        chunk.screen = r.U8();
        chunk.cx = r.U8();
        chunk.cy = r.U8();
        int groups = r.U16();
        for (int gi = 0; gi < groups && r.ok; gi++) {
            MeshGroup g;
            g.tile = r.U8();
            g.palette = r.U8();
            int quads = r.U16();
            g.quads.resize(quads);
            for (MeshQuad& q : g.quads) {
                for (int v = 0; v < 4; v++) {
                    for (int k = 0; k < 3; k++) {
                        q.pos[v][k] = (int16_t)r.U16();
                    }
                    q.uv[v][0] = r.U8();
                    q.uv[v][1] = r.U8();
                }
                q.shade = r.U8();
            }
            chunk.groups.push_back(std::move(g));
        }
        mesh.chunks.push_back(std::move(chunk));
    }
    return r.ok;
}

std::string RoomMeshPath(const Section& s) {
    char name[64];
    snprintf(name, sizeof(name), "metroid/meshes/%s/%c_%02X_%02X", kAreaFolders[s.bank % 6], s.horizontal ? 'h' : 'v',
             s.x, s.y);
    return name;
}

bool IsRoomMeshFile(const std::string& path) {
    return path.rfind("metroid/meshes/", 0) == 0;
}

void ExtractRoomMeshes(const Level& level, const EngineRead& engine, std::map<std::string, std::string>& files) {
    for (const Section& s : AllSections(level, engine)) {
        std::vector<ScreenTiles> screens;
        if (!SectionScreens(level, s, engine, screens)) {
            continue;
        }
        files[RoomMeshPath(s)] = EncodeRoomMesh(BuildRoomMesh(s, screens));
    }
}

} // namespace Game::Data
