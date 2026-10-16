// The 3D view: one scrolling section of an area as the game draws it (its room mesh, textured with the area's
// tiles and palettes), with markers for what can be edited. Coordinates are the section's pixels: x right,
// y down (three.js gets -y), z towards the camera; the blocks are z -8 to 8.
'use strict';
var M64 = window.M64 || (window.M64 = {});

M64.View = class {
  constructor(canvas) {
    this.canvas = canvas;
    this.renderer = new THREE.WebGLRenderer({ canvas: canvas, antialias: true });
    this.renderer.setPixelRatio(window.devicePixelRatio || 1);
    this.scene = new THREE.Scene();
    this.scene.background = new THREE.Color(0x0b0d18);
    this.camera = new THREE.PerspectiveCamera(50, 1, 1, 20000);
    this.target = new THREE.Vector3(128, -120, 0);
    this.orbit = { theta: 0, phi: Math.PI / 2, radius: 330 };
    this.scene.add(new THREE.AmbientLight(0xffffff, 0.75));
    const sun = new THREE.DirectionalLight(0xffffff, 0.6);
    sun.position.set(0.3, 1, 0.8);
    this.scene.add(sun);
    this.world = new THREE.Group();
    this.scene.add(this.world);
    this.markers = [];
    this.handlers = {};
    this.atlas = null;
    this.section = null;
    this.screenGroups = [];
    this.bindInput();
    const self = this;
    new ResizeObserver(function () { self.resize(); }).observe(canvas.parentElement);
    this.resize();
    const loop = function () { self.render(); requestAnimationFrame(loop); };
    requestAnimationFrame(loop);
  }

  on(name, fn) { this.handlers[name] = fn; }
  emit(name) { const fn = this.handlers[name]; if (fn) return fn.apply(null, Array.prototype.slice.call(arguments, 1)); }

  resize() {
    const p = this.canvas.parentElement;
    const w = p.clientWidth, h = p.clientHeight;
    this.renderer.setSize(w, h, false);
    this.camera.aspect = w / Math.max(1, h);
    this.camera.updateProjectionMatrix();
  }

  render() {
    const o = this.orbit;
    this.camera.position.set(
      this.target.x + o.radius * Math.sin(o.phi) * Math.sin(o.theta),
      this.target.y + o.radius * Math.cos(o.phi),
      this.target.z + o.radius * Math.sin(o.phi) * Math.cos(o.theta));
    this.camera.lookAt(this.target);
    const t = performance.now() / 1000;
    for (const m of this.markers) if (m.pulse) m.pulse(t);
    this.renderer.render(this.scene, this.camera);
  }

  // The area's background tiles in its four background palettes: 16x16 tiles each, stacked.
  buildAtlas(area) {
    const c = document.createElement('canvas');
    c.width = 128;
    c.height = 512;
    const g = c.getContext('2d');
    const img = g.createImageData(128, 512);
    for (let p = 0; p < 4; p++) {
      for (let t = 0; t < 256; t++) {
        for (let y = 0; y < 8; y++) {
          for (let x = 0; x < 8; x++) {
            const v = area.tiles ? area.tiles[(256 + t) * 64 + y * 8 + x] : (x ^ y) & 3;
            const col = M64.NES_COLORS[area.palette[p * 4 + v] & 0x3F];
            const i = ((p * 128 + (t >> 4) * 8 + y) * 128 + (t & 15) * 8 + x) * 4;
            img.data[i] = col >> 16; img.data[i + 1] = (col >> 8) & 255; img.data[i + 2] = col & 255; img.data[i + 3] = 255;
          }
        }
      }
    }
    g.putImageData(img, 0, 0);
    const tex = new THREE.CanvasTexture(c);
    tex.flipY = false;
    tex.magFilter = THREE.NearestFilter;
    tex.minFilter = THREE.NearestFilter;
    return tex;
  }

  clear() {
    for (const m of this.markers) this.world.remove(m.object);
    this.markers = [];
    while (this.world.children.length) this.world.remove(this.world.children[0]);
    this.screenGroups = [];
  }

  // Shows a section (from M64.decodeRoomMesh): one group per screen, so a screen can be hidden when a
  // model replaces it.
  showSection(area, section, keepCamera) {
    this.clear();
    this.section = section;
    if (!this.atlas || this.atlas.area !== area) {
      this.atlas = this.buildAtlas(area);
      this.atlas.area = area;
    }
    const mat = new THREE.MeshBasicMaterial({ map: this.atlas, vertexColors: true });
    const perScreen = [];
    for (let i = 0; i < section.length; i++) perScreen.push({ pos: [], uv: [], col: [] });
    for (const chunk of section.chunks) {
      const b = perScreen[chunk.screen];
      if (!b) continue;
      for (const grp of chunk.groups) {
        const u0 = (grp.tile & 15) * 8, v0 = (grp.palette & 3) * 128 + (grp.tile >> 4) * 8;
        for (const q of grp.quads) {
          const s = q.shade / 255;
          for (const k of [0, 1, 2, 0, 2, 3]) {
            b.pos.push(q.pos[k][0], q.pos[k][1], q.pos[k][2]);
            b.uv.push((u0 + q.uv[k][0]) / 128, (v0 + q.uv[k][1]) / 512); // uv y 0 is the tile's top
            b.col.push(s, s, s);
          }
        }
      }
    }
    for (const b of perScreen) {
      const geo = new THREE.BufferGeometry();
      geo.setAttribute('position', new THREE.Float32BufferAttribute(b.pos, 3));
      geo.setAttribute('uv', new THREE.Float32BufferAttribute(b.uv, 2));
      geo.setAttribute('color', new THREE.Float32BufferAttribute(b.col, 3));
      const mesh = new THREE.Mesh(geo, mat);
      this.world.add(mesh);
      this.screenGroups.push(mesh);
    }
    // Screen outlines.
    const lines = new THREE.Group();
    const lineMat = new THREE.LineBasicMaterial({ color: 0x3b4a80 });
    for (let i = 0; i < section.length; i++) {
      const o = this.screenOrigin(i);
      const pts = [[0, 0], [256, 0], [256, 240], [0, 240], [0, 0]].map(function (p) {
        return new THREE.Vector3(o.x + p[0], -(o.y + p[1]), -9);
      });
      lines.add(new THREE.Line(new THREE.BufferGeometry().setFromPoints(pts), lineMat));
    }
    this.world.add(lines);
    if (!keepCamera) {
      const w = section.horizontal ? section.length * 256 : 256, h = section.horizontal ? 240 : section.length * 240;
      this.target.set(Math.min(w, 256) / 2, -Math.min(h, 240) / 2, 0);
      this.orbit = { theta: 0, phi: Math.PI / 2, radius: 330 };
    }
  }

  screenOrigin(i) {
    const s = this.section;
    return { x: s && s.horizontal ? i * 256 : 0, y: s && !s.horizontal ? i * 240 : 0 };
  }

  setScreenVisible(i, visible) { if (this.screenGroups[i]) this.screenGroups[i].visible = visible; }

  focusScreen(i) {
    const o = this.screenOrigin(i);
    this.target.set(o.x + 128, -(o.y + 120), 0);
  }

  // A marker: `object` placed at section pixels (x, y, z); `info` is what the app knows it by.
  addMarker(object, x, y, z, info) {
    object.position.set(x, -y, z || 0);
    object.traverse(function (o) { o.userData.marker = info; });
    this.world.add(object);
    const m = { object: object, info: info, z: z || 0 };
    info.view = m;
    this.markers.push(m);
    return m;
  }
  removeMarker(m) {
    this.world.remove(m.object);
    this.markers = this.markers.filter(function (x) { return x !== m; });
  }
  moveMarker(m, x, y, z) { m.object.position.set(x, -y, z === undefined ? m.z : z); if (z !== undefined) m.z = z; }

  highlight(info) {
    for (const m of this.markers) {
      const on = m.info === info;
      m.object.traverse(function (o) {
        if (o.material && o.material.emissive) o.material.emissive.setHex(on ? 0x666666 : 0x000000);
      });
      if (m.outline) m.outline.visible = on;
    }
  }

  // Section pixels under the mouse on the plane z.
  pointOnPlane(ev, z) {
    const r = this.canvas.getBoundingClientRect();
    const ndc = new THREE.Vector2(((ev.clientX - r.left) / r.width) * 2 - 1, -((ev.clientY - r.top) / r.height) * 2 + 1);
    const ray = new THREE.Raycaster();
    ray.setFromCamera(ndc, this.camera);
    const p = new THREE.Vector3();
    if (!ray.ray.intersectPlane(new THREE.Plane(new THREE.Vector3(0, 0, 1), -(z || 0)), p)) return null;
    return { x: p.x, y: -p.y };
  }
  pick(ev) {
    const r = this.canvas.getBoundingClientRect();
    const ndc = new THREE.Vector2(((ev.clientX - r.left) / r.width) * 2 - 1, -((ev.clientY - r.top) / r.height) * 2 + 1);
    const ray = new THREE.Raycaster();
    ray.setFromCamera(ndc, this.camera);
    const hits = ray.intersectObjects(this.markers.map(function (m) { return m.object; }), true);
    for (const h of hits) if (h.object.userData.marker) return h.object.userData.marker;
    return null;
  }

  // Left: pick and drag markers (or place, with a tool); right: orbit; middle or shift-left: pan; wheel: zoom.
  bindInput() {
    const self = this;
    const c = this.canvas;
    let drag = null;
    c.addEventListener('contextmenu', function (e) { e.preventDefault(); });
    c.addEventListener('mousedown', function (e) {
      if (e.button === 2 || e.button === 1 || (e.button === 0 && e.shiftKey)) {
        drag = { mode: e.button === 2 ? 'orbit' : 'pan', x: e.clientX, y: e.clientY };
        return;
      }
      if (e.button !== 0) return;
      const info = self.pick(e);
      if (info) {
        self.emit('select', info);
        const m = info.view;
        const start = self.pointOnPlane(e, m.z);
        if (start && info.movable) {
          drag = { mode: 'marker', info: info, start: start, x0: info.px, y0: info.py, moved: false };
        }
      } else {
        const p = self.pointOnPlane(e, 0);
        if (p && self.emit('place', p.x, p.y) !== true) self.emit('select', null);
      }
    });
    window.addEventListener('mousemove', function (e) {
      if (!drag) return;
      if (drag.mode === 'orbit') {
        self.orbit.theta -= (e.clientX - drag.x) * 0.008;
        self.orbit.phi = Math.min(Math.PI - 0.05, Math.max(0.05, self.orbit.phi - (e.clientY - drag.y) * 0.008));
        drag.x = e.clientX; drag.y = e.clientY;
      } else if (drag.mode === 'pan') {
        const k = self.orbit.radius / 600;
        const right = new THREE.Vector3().setFromMatrixColumn(self.camera.matrix, 0);
        const up = new THREE.Vector3().setFromMatrixColumn(self.camera.matrix, 1);
        self.target.addScaledVector(right, -(e.clientX - drag.x) * k).addScaledVector(up, (e.clientY - drag.y) * k);
        drag.x = e.clientX; drag.y = e.clientY;
      } else if (drag.mode === 'marker') {
        const p = self.pointOnPlane(e, drag.info.view.z);
        if (!p) return;
        drag.moved = true;
        self.emit('drag', drag.info, drag.x0 + (p.x - drag.start.x), drag.y0 + (p.y - drag.start.y));
      }
    });
    window.addEventListener('mouseup', function () {
      if (drag && drag.mode === 'marker' && drag.moved) self.emit('dragEnd', drag.info);
      drag = null;
    });
    c.addEventListener('wheel', function (e) {
      e.preventDefault();
      self.orbit.radius = Math.min(4000, Math.max(40, self.orbit.radius * Math.pow(1.0015, e.deltaY)));
    }, { passive: false });
  }
};

// Marker shapes.
M64.labelSprite = function (text, color) {
  const c = document.createElement('canvas');
  const g = c.getContext('2d');
  g.font = 'bold 28px sans-serif';
  c.width = Math.ceil(g.measureText(text).width) + 16;
  c.height = 40;
  g.font = 'bold 28px sans-serif';
  g.fillStyle = 'rgba(0,0,0,0.65)';
  g.fillRect(0, 0, c.width, c.height);
  g.fillStyle = color;
  g.fillText(text, 8, 29);
  const tex = new THREE.CanvasTexture(c);
  const s = new THREE.Sprite(new THREE.SpriteMaterial({ map: tex, depthTest: false }));
  s.scale.set(c.width / 4, c.height / 4, 1);
  s.renderOrder = 10;
  return s;
};

M64.boxMarker = function (w, h, d, color, label, labelColor) {
  const g = new THREE.Group();
  const box = new THREE.Mesh(new THREE.BoxGeometry(w, h, d),
    new THREE.MeshLambertMaterial({ color: color, emissive: 0x000000, transparent: true, opacity: 0.85 }));
  g.add(box);
  if (label) {
    const s = M64.labelSprite(label, labelColor || '#fff');
    s.position.set(0, h / 2 + 7, d / 2);
    g.add(s);
  }
  return g;
};
