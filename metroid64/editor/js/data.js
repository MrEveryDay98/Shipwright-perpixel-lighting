// The game's data as the editor sees it: metroid.o2r (the base, made by Metroid64 from your ROM) with the mod
// being edited on top (its files win, as in the game). Everything is read from the archive's own files.
'use strict';
var M64 = window.M64 || (window.M64 = {});

M64.AREAS = ['', 'brinstar', 'norfair', 'tourian', 'kraid', 'ridley'];
M64.AREA_TITLES = ['', 'Brinstar', 'Norfair', 'Tourian', "Kraid's Lair", "Ridley's Lair"];
M64.POWER_UPS = ['Bombs', 'High jump', 'Long beam', 'Screw attack', 'Maru Mari', 'Varia', 'Wave beam',
  'Ice beam', 'Energy tank', 'Missiles'];

// NTSC 2C02 colours (the same table as the game's Picture.cpp).
M64.NES_COLORS = [
  0x666666, 0x002A88, 0x1412A7, 0x3B00A4, 0x5C007E, 0x6E0040, 0x6C0600, 0x561D00, 0x333500, 0x0B4800, 0x005200,
  0x004F08, 0x00404D, 0x000000, 0x000000, 0x000000, 0xADADAD, 0x155FD9, 0x4240FF, 0x7527FE, 0xA01ACC, 0xB71E7B,
  0xB53120, 0x994E00, 0x6B6D00, 0x388700, 0x0C9300, 0x008F32, 0x007C8D, 0x000000, 0x000000, 0x000000, 0xFFFEFF,
  0x64B0FF, 0x9290FF, 0xC676FF, 0xF36AFF, 0xFE6ECC, 0xFE8170, 0xEA9E22, 0xBCBE00, 0x88D800, 0x5CE430, 0x45E082,
  0x48CDDE, 0x4F4F4F, 0x000000, 0x000000, 0xFFFEFF, 0xC0DFFF, 0xD3D2FF, 0xE8C8FF, 0xFBC2FF, 0xFEC4EA, 0xFECCC5,
  0xF7D8A5, 0xE4E594, 0xCFEF96, 0xBDF4AB, 0xB3F3CC, 0xB5EBF2, 0xB8B8B8, 0x000000, 0x000000,
];

M64.hex = function (s) { return parseInt(s, 16); };
M64.hex2 = function (n) { return (n < 16 ? '0' : '') + n.toString(16).toUpperCase(); };

// A zip archive (.o2r) or a set of loose files, path -> content (string or Uint8Array).
M64.Files = class {
  constructor() { this.map = new Map(); }
  static async fromZip(blob) {
    const zip = await JSZip.loadAsync(blob);
    const f = new M64.Files();
    const jobs = [];
    zip.forEach(function (path, entry) {
      if (!entry.dir) {
        jobs.push(entry.async('uint8array').then(function (bytes) { f.map.set(path, bytes); }));
      }
    });
    await Promise.all(jobs);
    return f;
  }
  has(path) { return this.map.has(path); }
  bytes(path) {
    const v = this.map.get(path);
    if (v === undefined) return null;
    return typeof v === 'string' ? new TextEncoder().encode(v) : v;
  }
  text(path) {
    const v = this.map.get(path);
    if (v === undefined) return null;
    let s = typeof v === 'string' ? v : new TextDecoder().decode(v);
    const nul = s.indexOf('\0');
    return nul >= 0 ? s.slice(0, nul) : s;
  }
  set(path, content) { this.map.set(path, content); }
  delete(path) { this.map.delete(path); }
  paths(prefix) { return Array.from(this.map.keys()).filter(function (p) { return !prefix || p.startsWith(prefix); }); }
};

// The base archive with the mod's files over it.
M64.Project = class {
  constructor(base) {
    this.base = base;
    this.mod = new M64.Files();
  }
  has(path) { return this.mod.has(path) || this.base.has(path); }
  text(path) { return this.mod.has(path) ? this.mod.text(path) : this.base.text(path); }
  bytes(path) { return this.mod.has(path) ? this.mod.bytes(path) : this.base.bytes(path); }
  xml(path) {
    const t = this.text(path);
    if (t === null) return null;
    const doc = new DOMParser().parseFromString(t, 'text/xml');
    return doc.getElementsByTagName('parsererror').length ? null : doc;
  }
};

// metroid/meshes/<area>/<h|v>_XX_YY: a section's geometry (game/data/RoomMesh.cpp, "RMSH").
M64.decodeRoomMesh = function (bytes) {
  const dv = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  let p = 0;
  const u8 = function () { return dv.getUint8(p++); };
  const u16 = function () { const v = dv.getUint16(p, true); p += 2; return v; };
  const i16 = function () { const v = dv.getInt16(p, true); p += 2; return v; };
  if (String.fromCharCode(u8(), u8(), u8(), u8()) !== 'RMSH' || u16() !== 1) return null;
  const mesh = { bank: u8(), horizontal: u8() !== 0, x: u8(), y: u8(), length: u8(), chunks: [] };
  u8();
  for (let i = 0; i < 4; i++) u16();
  const chunks = u16();
  for (let c = 0; c < chunks; c++) {
    const chunk = { screen: u8(), cx: u8(), cy: u8(), groups: [] };
    const groups = u16();
    for (let g = 0; g < groups; g++) {
      const group = { tile: u8(), palette: u8(), quads: [] };
      const quads = u16();
      for (let q = 0; q < quads; q++) {
        const quad = { pos: [], uv: [], shade: 0 };
        for (let v = 0; v < 4; v++) {
          quad.pos.push([i16(), i16(), i16()]);
          quad.uv.push([u8(), u8()]);
        }
        quad.shade = u8();
        group.quads.push(quad);
      }
      chunk.groups.push(group);
    }
    mesh.chunks.push(chunk);
  }
  return mesh;
};

// metroid/editor/tiles_<area>.png: 512 tiles (sprites then background), 16 per row, pixel values 0-3 as
// greys. Returns the pixel values, 64 per tile.
M64.decodeTiles = function (pngBytes) {
  return new Promise(function (resolve, reject) {
    const img = new Image();
    img.onload = function () {
      const c = document.createElement('canvas');
      c.width = img.width;
      c.height = img.height;
      const g = c.getContext('2d');
      g.drawImage(img, 0, 0);
      const px = g.getImageData(0, 0, c.width, c.height).data;
      const tiles = new Uint8Array(512 * 64);
      for (let t = 0; t < 512; t++) {
        const tx = (t % 16) * 8, ty = Math.floor(t / 16) * 8;
        for (let y = 0; y < 8; y++) {
          for (let x = 0; x < 8; x++) {
            const i = ((ty + y) * c.width + tx + x) * 4;
            if (ty + y < c.height) tiles[t * 64 + y * 8 + x] = Math.round(px[i] / 85);
          }
        }
      }
      URL.revokeObjectURL(img.src);
      resolve(tiles);
    };
    img.onerror = function () { reject(new Error('tile image')); };
    img.src = URL.createObjectURL(new Blob([pngBytes], { type: 'image/png' }));
  });
};

// Everything the editor shows of one area.
M64.loadArea = async function (project, bank) {
  const name = M64.AREAS[bank];
  const area = { bank: bank, name: name, sections: [], palette: null, tiles: null };
  // The background palettes the area starts with (palette 00, video memory 3F00-3F0F).
  const pal = project.xml('metroid/palettes/' + name);
  area.palette = new Array(32).fill(0x0F);
  if (pal) {
    const p0 = Array.from(pal.getElementsByTagName('Palette')).find(function (e) { return M64.hex(e.getAttribute('Number')) === 0; });
    if (p0) {
      for (const w of p0.getElementsByTagName('Write')) {
        const at = M64.hex(w.getAttribute('Address'));
        w.textContent.trim().split(/\s+/).forEach(function (b, i) {
          const a = at + i - 0x3F00;
          if (a >= 0 && a < 32) area.palette[a] = M64.hex(b);
        });
      }
    }
  }
  const tilePng = project.bytes('metroid/editor/tiles_' + name + '.png');
  if (tilePng) area.tiles = await M64.decodeTiles(tilePng);
  const prefix = 'metroid/meshes/' + name + '/';
  const paths = new Set(project.base.paths(prefix).concat(project.mod.paths(prefix)));
  for (const path of paths) {
    const mesh = M64.decodeRoomMesh(project.bytes(path));
    if (mesh) area.sections.push(mesh);
  }
  return area;
};

M64.loadWorldMap = function (project) {
  const doc = project.xml('metroid/world/map');
  const rooms = [];
  for (let y = 0; y < 32; y++) rooms.push(new Array(32).fill(0xFF));
  if (doc) {
    for (const row of doc.getElementsByTagName('Row')) {
      const y = M64.hex(row.getAttribute('Y'));
      row.textContent.trim().split(/\s+/).forEach(function (b, x) { if (x < 32 && y < 32) rooms[y][x] = M64.hex(b); });
    }
  }
  return rooms;
};

M64.sectionScreens = function (s) {
  const out = [];
  for (let i = 0; i < s.length; i++) {
    out.push({ i: i, x: s.horizontal ? s.x + i : s.x, y: s.horizontal ? s.y : s.y + i });
  }
  return out;
};

M64.roomPath = function (bank, number) { return 'metroid/rooms/' + M64.AREAS[bank] + '/room_' + M64.hex2(number); };
M64.itemsPath = function (bank) { return 'metroid/items/' + M64.AREAS[bank]; };
