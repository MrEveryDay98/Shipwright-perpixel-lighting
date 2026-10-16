// Display lists in Shipwright's XML (as Fast64 exports them for Ship of Harkinian), read just enough to show
// them in the editor: vertices, triangles, textures (libultraship's binary "OTEX" textures), the primitive
// colour, and vertex colours or normals (with G_LIGHTING the vertex colour bytes are a normal).
'use strict';
var M64 = window.M64 || (window.M64 = {});

// A model: the files of an export, by the path they are referred to with.
M64.Model = class {
  constructor(name) {
    this.name = name;
    this.files = new Map(); // resource path -> Uint8Array
    this.root = '';         // the display list drawn
  }
  text(path) {
    const b = this.files.get(path);
    if (!b) return null;
    const s = new TextDecoder().decode(b);
    const nul = s.indexOf('\0');
    return nul >= 0 ? s.slice(0, nul) : s;
  }
};

// Builds a model from the files picked (an export folder): every file is given the path the display lists
// use for it (they refer to each other as "<folder>/<name>", whatever the folder is called on disk).
M64.modelFromFiles = async function (name, fileList) {
  const model = new M64.Model(name);
  const byName = new Map();
  for (const f of fileList) {
    byName.set(f.name, new Uint8Array(await f.arrayBuffer()));
  }
  const isText = function (b) { return b.length > 0 && b[0] === 0x3C; };
  // The folder the display lists refer to.
  let folder = null;
  const dls = [];
  for (const [n, b] of byName) {
    if (!isText(b)) continue;
    const t = new TextDecoder().decode(b);
    if (t.indexOf('<DisplayList') >= 0) dls.push(n);
    const m = t.match(/Path="([^"]*?)\/[^"\/]+"/);
    if (m && folder === null && m[1].indexOf('>') < 0) folder = m[1];
  }
  if (folder === null) folder = 'models/' + name.replace(/[^A-Za-z0-9_]/g, '_');
  for (const [n, b] of byName) model.files.set(folder + '/' + n, b);
  // The root: a display list no other display list calls; with several, the largest.
  const called = new Set();
  for (const n of dls) {
    const t = new TextDecoder().decode(byName.get(n));
    for (const m of t.matchAll(/<(?:CallDisplayList|JumpToDisplayList)[^>]*Path="([^"]+)"/g)) called.add(m[1].split('/').pop());
  }
  const roots = dls.filter(function (n) { return !called.has(n); });
  roots.sort(function (a, b) { return byName.get(b).length - byName.get(a).length; });
  model.roots = (roots.length ? roots : dls).map(function (n) { return folder + '/' + n; });
  model.root = model.roots[0] || '';
  return model;
};

// libultraship texture (resource header 0x40 bytes, then type, width, height[, flags, scales], size, data).
M64.decodeTexture = function (bytes, tlut) {
  if (!bytes || bytes.length < 0x50) return null;
  const dv = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  const version = dv.getUint32(8, true);
  let p = 0x40;
  const type = dv.getUint32(p, true), w = dv.getUint32(p + 4, true), h = dv.getUint32(p + 8, true);
  p += 12;
  if (version >= 1) p += 12; // flags, horizontal byte scale, vertical pixel scale
  const size = dv.getUint32(p, true);
  p += 4;
  if (w === 0 || h === 0 || w > 1024 || h > 1024) return null;
  const out = new Uint8ClampedArray(w * h * 4);
  const data = bytes.subarray(p, p + size);
  const put = function (i, r, g, b, a) { out[i * 4] = r; out[i * 4 + 1] = g; out[i * 4 + 2] = b; out[i * 4 + 3] = a; };
  const rgba16 = function (v) { return [((v >> 11) & 31) * 255 / 31, ((v >> 6) & 31) * 255 / 31, ((v >> 1) & 31) * 255 / 31, (v & 1) ? 255 : 0]; };
  for (let i = 0; i < w * h; i++) {
    switch (type) {
      case 1: put(i, data[i * 4], data[i * 4 + 1], data[i * 4 + 2], data[i * 4 + 3]); break; // RGBA32
      case 2: { const c = rgba16((data[i * 2] << 8) | data[i * 2 + 1]); put(i, c[0], c[1], c[2], c[3]); break; }
      case 3: case 4: { // CI4 / CI8, through the palette loaded before (else grey)
        const idx = type === 3 ? ((data[i >> 1] >> ((i & 1) ? 0 : 4)) & 15) : data[i];
        if (tlut && tlut.length >= (idx + 1) * 2) {
          const c = rgba16((tlut[idx * 2] << 8) | tlut[idx * 2 + 1]);
          put(i, c[0], c[1], c[2], c[3]);
        } else {
          const v = type === 3 ? idx * 17 : idx;
          put(i, v, v, v, 255);
        }
        break;
      }
      case 5: { const v = ((data[i >> 1] >> ((i & 1) ? 0 : 4)) & 15) * 17; put(i, v, v, v, 255); break; } // I4
      case 6: put(i, data[i], data[i], data[i], 255); break; // I8
      case 7: { const n = (data[i >> 1] >> ((i & 1) ? 0 : 4)) & 15; const v = (n >> 1) * 255 / 7; put(i, v, v, v, (n & 1) ? 255 : 0); break; }
      case 8: { const v = (data[i] >> 4) * 17; put(i, v, v, v, (data[i] & 15) * 17); break; } // IA8
      case 9: put(i, data[i * 2], data[i * 2], data[i * 2], data[i * 2 + 1]); break; // IA16
      default: put(i, 200, 0, 200, 255);
    }
  }
  return { width: w, height: h, rgba: out };
};

// A palette texture's raw entries (big-endian RGBA16), for CI textures.
M64.textureData = function (bytes) {
  if (!bytes || bytes.length < 0x50) return null;
  const dv = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  let p = 0x40 + 12 + (dv.getUint32(8, true) >= 1 ? 12 : 0);
  const size = dv.getUint32(p, true);
  return bytes.subarray(p + 4, p + 4 + size);
};

// The display list `path` of `model` as a THREE.Group (y up, one unit per pixel, as the game draws it).
M64.buildDisplayList = function (model, path) {
  const group = new THREE.Group();
  const textures = new Map();
  const batches = new Map(); // material key -> {positions, colors, uvs, tex, mirror}
  const state = { vtx: new Array(64).fill(null), lighting: false, tex: null, texOn: false, prim: [1, 1, 1, 1],
    usesPrim: false, usesTex: false, imagePath: null, tlut: null, cms: 0, cmt: 0 };
  const light = new THREE.Vector3(0.4, 0.8, 0.6).normalize();

  const loadVertices = function (vpath, count, index, offset) {
    const text = model.text(vpath);
    if (!text) return;
    const doc = new DOMParser().parseFromString(text, 'text/xml');
    const list = doc.getElementsByTagName('Vtx');
    for (let k = 0; k < count && index + k < 64; k++) {
      const e = list[offset + k];
      if (!e) continue;
      const g = function (a) { return Number(e.getAttribute(a)) || 0; };
      state.vtx[index + k] = { x: g('X'), y: g('Y'), z: g('Z'), s: g('S'), t: g('T'), r: g('R'), g: g('G'), b: g('B'), a: g('A') };
    }
  };
  const textureFor = function (tpath) {
    const key = tpath + (state.tlut ? '#tlut' : '');
    if (textures.has(key)) return textures.get(key);
    const img = M64.decodeTexture(model.files.get(tpath), state.tlut);
    let tex = null;
    if (img) {
      tex = new THREE.DataTexture(img.rgba, img.width, img.height, THREE.RGBAFormat);
      tex.flipY = false;
      tex.magFilter = THREE.LinearFilter;
      tex.needsUpdate = true;
      tex.userData = { width: img.width, height: img.height };
    }
    textures.set(key, tex);
    return tex;
  };
  const vertexColor = function (v) {
    if (state.lighting) {
      const n = new THREE.Vector3((v.r << 24) >> 24, (v.g << 24) >> 24, (v.b << 24) >> 24).normalize();
      const d = 0.45 + 0.55 * Math.max(0, n.dot(light));
      return [d, d, d, v.a / 255];
    }
    return [v.r / 255, v.g / 255, v.b / 255, v.a / 255];
  };
  const triangle = function (a, b, c) {
    const vs = [state.vtx[a], state.vtx[b], state.vtx[c]];
    if (vs.some(function (v) { return !v; })) return;
    const tex = state.texOn && state.usesTex && state.imagePath ? textureFor(state.imagePath) : null;
    const prim = state.usesPrim ? state.prim : [1, 1, 1, 1];
    const key = (tex ? tex.uuid : 'none') + '|' + prim.join(',') + '|' + state.cms + state.cmt;
    let batch = batches.get(key);
    if (!batch) {
      batch = { positions: [], colors: [], uvs: [], tex: tex, prim: prim, cms: state.cms, cmt: state.cmt };
      batches.set(key, batch);
    }
    for (const v of vs) {
      batch.positions.push(v.x, v.y, v.z);
      const col = vertexColor(v);
      batch.colors.push(col[0], col[1], col[2], col[3]);
      const w = tex ? tex.userData.width : 32, h = tex ? tex.userData.height : 32;
      batch.uvs.push(v.s / 32 / w, v.t / 32 / h);
    }
  };
  const run = function (dlPath, depth) {
    if (depth > 16) return;
    const text = model.text(dlPath);
    if (!text) return;
    const doc = new DOMParser().parseFromString(text, 'text/xml');
    const root = doc.documentElement;
    for (const e of root.children) {
      const n = e.tagName;
      const a = function (k) { return e.getAttribute(k); };
      if (n === 'LoadVertices') {
        loadVertices(a('Path'), Number(a('Count')), Number(a('VertexBufferIndex')), Number(a('VertexOffset')));
      } else if (n === 'Triangle1') {
        triangle(Number(a('V00')), Number(a('V01')), Number(a('V02')));
      } else if (n === 'Triangles2') {
        triangle(Number(a('V00')), Number(a('V01')), Number(a('V02')));
        triangle(Number(a('V10')), Number(a('V11')), Number(a('V12')));
      } else if (n === 'SetGeometryMode' && a('G_LIGHTING') === '1') {
        state.lighting = true;
      } else if (n === 'ClearGeometryMode' && a('G_LIGHTING') === '1') {
        state.lighting = false;
      } else if (n === 'Texture') {
        state.texOn = a('On') === '1';
      } else if (n === 'SetTextureImage') {
        state.imagePath = a('Path');
      } else if (n === 'LoadTLUTCmd') {
        state.tlut = M64.textureData(model.files.get(state.imagePath));
      } else if (n === 'SetTextureLUT') {
        if (a('Mode') === 'G_TT_NONE') state.tlut = null;
      } else if (n === 'SetTile' && a('Tile') === '0') {
        state.cms = a('Cms1') === 'G_TX_MIRROR' ? 1 : 0;
        state.cmt = a('Cmt1') === 'G_TX_MIRROR' ? 1 : 0;
      } else if (n === 'SetPrimColor') {
        state.prim = [Number(a('R')) / 255, Number(a('G')) / 255, Number(a('B')) / 255, Number(a('A')) / 255];
      } else if (n === 'SetCombineLERP') {
        const all = ['A0', 'B0', 'C0', 'D0', 'A1', 'B1', 'C1', 'D1'].map(a).join(' ');
        state.usesPrim = all.indexOf('PRIMITIVE') >= 0;
        state.usesTex = all.indexOf('TEXEL0') >= 0;
      } else if (n === 'CallDisplayList') {
        run(a('Path'), depth + 1);
      } else if (n === 'JumpToDisplayList') {
        run(a('Path'), depth + 1);
        return;
      } else if (n === 'EndDisplayList') {
        return;
      }
    }
  };
  run(path, 0);
  for (const b of batches.values()) {
    const geo = new THREE.BufferGeometry();
    geo.setAttribute('position', new THREE.Float32BufferAttribute(b.positions, 3));
    geo.setAttribute('color', new THREE.Float32BufferAttribute(b.colors, 4));
    geo.setAttribute('uv', new THREE.Float32BufferAttribute(b.uvs, 2));
    if (b.tex) {
      b.tex.wrapS = b.cms ? THREE.MirroredRepeatWrapping : THREE.RepeatWrapping;
      b.tex.wrapT = b.cmt ? THREE.MirroredRepeatWrapping : THREE.RepeatWrapping;
      b.tex.needsUpdate = true;
    }
    const mat = new THREE.MeshBasicMaterial({ vertexColors: true, map: b.tex || null, side: THREE.DoubleSide,
      color: new THREE.Color(b.prim[0], b.prim[1], b.prim[2]), transparent: false, alphaTest: 0.5 });
    group.add(new THREE.Mesh(geo, mat));
  }
  return group;
};
