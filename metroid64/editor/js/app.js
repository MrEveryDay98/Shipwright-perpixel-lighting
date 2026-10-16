// The editor: open metroid.o2r, pick an area and a section on its map, move enemies and power-ups, add save
// points and props (Fast64 display lists), give a screen a model of its own, and export it all as a mod.
'use strict';
var M64 = window.M64 || (window.M64 = {});

(function () {
  const $ = function (id) { return document.getElementById(id); };
  const S = {
    project: null, world: null, areas: {}, bank: 1, area: null, section: null,
    docs: new Map(), dirty: new Set(),
    savePoints: [], props: [], roomGeo: new Map(),
    models: [], modModel: null, tool: 'select', selected: null, modelIndex: -1,
    sectionCycle: {},
  };
  let view = null;

  const status = function (text) { $('status').textContent = text; };
  const esc = function (s) { return String(s).replace(/[&<>"]/g, function (c) { return { '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c]; }); };

  // --- Data ------------------------------------------------------------------------------------------------

  const doc = function (path) {
    if (!S.docs.has(path)) S.docs.set(path, S.project.xml(path));
    return S.docs.get(path);
  };
  const touch = function (path) { S.dirty.add(path); updateExportInfo(); };
  const geoKey = function (bank, x, y) { return bank + ',' + x + ',' + y; };

  // Model that has the files of display list `path` (an imported one, or the mod's own files).
  const modelFor = function (path) {
    for (const m of S.models) if (m.files.has(path)) return m;
    if (S.modModel && S.modModel.files.has(path)) return S.modModel;
    return null;
  };
  const dlCache = new Map();
  const buildDL = function (path) {
    const m = modelFor(path);
    if (!m) return null;
    const key = m.name + '|' + path;
    if (!dlCache.has(key)) dlCache.set(key, M64.buildDisplayList(m, path));
    return dlCache.get(key).clone();
  };

  async function openBase(file) {
    status('Reading ' + file.name + '...');
    const base = await M64.Files.fromZip(file);
    if (!base.has('metroid/world/map')) {
      status(file.name + " isn't Metroid64's game data (metroid.o2r)");
      return;
    }
    if (!base.has('metroid/editor/tiles_brinstar.png')) {
      status('This metroid.o2r is from an older Metroid64: run the newer game once so it remakes it.');
    }
    S.project = new M64.Project(base);
    S.world = M64.loadWorldMap(S.project);
    S.areas = {};
    S.docs.clear();
    S.dirty.clear();
    $('needBase').style.display = 'none';
    $('main').classList.remove('disabled');
    await selectArea(Number($('area').value));
    status('Opened ' + file.name + ' (' + base.map.size + ' files)');
  }

  async function openMod(file) {
    if (!S.project) return;
    const mod = await M64.Files.fromZip(file);
    S.project.mod = mod;
    S.docs.clear();
    S.dirty.clear();
    S.modModel = new M64.Model('mod');
    for (const [p, b] of mod.map) S.modModel.files.set(p, typeof b === 'string' ? new TextEncoder().encode(b) : b);
    dlCache.clear();
    S.savePoints = [];
    S.props = [];
    S.roomGeo = new Map();
    const sp = mod.text('metroid64/savepoints.json');
    if (sp) try { S.savePoints = JSON.parse(sp).save_points || []; } catch (e) { status('savepoints.json: ' + e.message); }
    const pr = mod.text('metroid64/props.json');
    if (pr) try { S.props = JSON.parse(pr).props || []; } catch (e) { status('props.json: ' + e.message); }
    for (const p of mod.paths('rooms/')) {
      const m = p.match(/^rooms\/([a-z]+)\/([0-9A-Fa-f]{2})_([0-9A-Fa-f]{2})\.json$/);
      const bank = m ? M64.AREAS.indexOf(m[1]) : -1;
      if (bank < 1) continue;
      try {
        const j = JSON.parse(mod.text(p));
        const anim = j.animations && (j.animations.Idle || Object.values(j.animations)[0]);
        const dl = anim && anim.frames && anim.frames[0];
        if (dl) S.roomGeo.set(geoKey(bank, M64.hex(m[2]), M64.hex(m[3])), { display_list: dl, keep_tiles: !!j.keep_tiles,
          offset: j.offset || [0, 0, 0], scale: j.scale || 1 });
      } catch (e) { status(p + ': ' + e.message); }
    }
    S.areas = {};
    await selectArea(S.bank);
    $('modName').value = file.name.replace(/\.(o2r|zip)$/i, '');
    status('Opened mod ' + file.name + ' (' + mod.map.size + ' files)');
    updateExportInfo();
  }

  // --- Area, map and section ----------------------------------------------------------------------------

  async function selectArea(bank) {
    S.bank = bank;
    if (!S.areas[bank]) {
      status('Loading ' + M64.AREA_TITLES[bank] + '...');
      S.areas[bank] = await M64.loadArea(S.project, bank);
    }
    S.area = S.areas[bank];
    S.section = null;
    drawMap();
    // Start where Samus starts in the area, else the first section.
    const info = S.project.xml('metroid/areas/' + M64.AREAS[bank]);
    const root = info && info.documentElement;
    const sx = root ? M64.hex(root.getAttribute('StartMapX') || '0') : 0, sy = root ? M64.hex(root.getAttribute('StartMapY') || '0') : 0;
    const start = sectionsAt(sx, sy)[0] || S.area.sections[0];
    if (start) {
      showSection(start);
      const c = M64.sectionScreens(start).find(function (c) { return c.x === sx && c.y === sy; });
      if (c) showScreenPanel(c.i);
    }
    status(M64.AREA_TITLES[bank] + ': ' + S.area.sections.length + ' sections');
  }

  const sectionsAt = function (x, y) {
    return S.area.sections.filter(function (s) {
      return M64.sectionScreens(s).some(function (c) { return c.x === x && c.y === y; });
    });
  };

  function drawMap() {
    const c = $('map');
    const g = c.getContext('2d');
    const cell = c.width / 32;
    g.fillStyle = '#05060c';
    g.fillRect(0, 0, c.width, c.height);
    for (let y = 0; y < 32; y++) {
      for (let x = 0; x < 32; x++) {
        if (S.world[y][x] !== 0xFF) {
          g.fillStyle = '#151a2c';
          g.fillRect(x * cell, y * cell, cell - 1, cell - 1);
        }
      }
    }
    if (!S.area) return;
    for (const s of S.area.sections) {
      for (const c2 of M64.sectionScreens(s)) {
        g.fillStyle = s === S.section ? '#e0409c' : (s.horizontal ? '#3355aa' : '#2f6f8a');
        g.fillRect(c2.x * cell, c2.y * cell, cell - 1, cell - 1);
      }
    }
    for (const sp of S.savePoints) {
      if (M64.AREAS.indexOf(String(sp.area).toLowerCase()) === S.bank || sp.area === S.bank) {
        g.fillStyle = '#7fe0ff';
        g.fillRect(sp.map_x * cell + cell / 4, sp.map_y * cell + cell / 4, cell / 2, cell / 2);
      }
    }
  }

  function showSection(section, keepCamera) {
    S.section = section;
    S.selected = null;
    view.showSection(S.area, section, keepCamera);
    rebuildMarkers();
    drawMap();
    const kind = section.horizontal ? 'horizontal' : 'vertical';
    $('sectionInfo').textContent = M64.AREA_TITLES[S.bank] + ' - ' + kind + ' section at ' + M64.hex2(section.x) + ', ' +
      M64.hex2(section.y) + ' (' + section.length + ' screen' + (section.length > 1 ? 's' : '') + ')';
    showScreenPanel(0);
    showProps();
  }

  // --- Markers ----------------------------------------------------------------------------------------

  const areaOf = function (v) { return typeof v === 'number' ? v : M64.AREAS.indexOf(String(v).toLowerCase()); };

  function roomUses(room) {
    let n = 0;
    for (const s of S.area.sections) {
      for (const c of M64.sectionScreens(s)) if (S.world[c.y][c.x] === room) n++;
    }
    return n;
  }

  function rebuildMarkers() {
    for (const m of view.markers.slice()) view.removeMarker(m);
    const s = S.section;
    if (!s) return;
    const items = doc(M64.itemsPath(S.bank));
    for (const c of M64.sectionScreens(s)) {
      const o = view.screenOrigin(c.i);
      const geo = S.roomGeo.get(geoKey(S.bank, c.x, c.y));
      view.setScreenVisible(c.i, !geo || geo.keep_tiles);
      if (geo) {
        const obj = buildDL(geo.display_list);
        if (obj) {
          obj.scale.setScalar(geo.scale);
          view.addMarker(obj, o.x + geo.offset[0], o.y - geo.offset[1], geo.offset[2], { kind: 'geometry', screen: c, movable: false });
        }
      }
      const room = S.world[c.y][c.x];
      const rpath = M64.roomPath(S.bank, room);
      const rdoc = doc(rpath);
      if (rdoc) {
        for (const e of rdoc.getElementsByTagName('Enemy')) {
          const X = M64.hex(e.getAttribute('X')), Y = M64.hex(e.getAttribute('Y'));
          const info = { kind: 'enemy', el: e, path: rpath, screen: c, room: room, movable: true, px: o.x + X * 16 + 12, py: o.y + Y * 16 + 8 };
          view.addMarker(M64.boxMarker(14, 14, 14, 0xd04040, 'E' + e.getAttribute('Type'), '#ff9a9a'), info.px, info.py, 0, info);
        }
        for (const e of rdoc.getElementsByTagName('Door')) {
          const left = e.getAttribute('Side') === 'Left';
          const pal = Number(e.getAttribute('Palette'));
          const info = { kind: 'door', el: e, screen: c, movable: false };
          view.addMarker(M64.boxMarker(6, 48, 18, pal === 1 || pal === 3 ? 0x4090ff : 0xff5040), o.x + (left ? 4 : 252), o.y + 0x70, 0, info);
        }
      }
      const scr = items && Array.from(items.getElementsByTagName('Screen')).find(function (e) {
        return M64.hex(e.getAttribute('X')) === c.x && M64.hex(e.getAttribute('Y')) === c.y;
      });
      if (scr) {
        for (const e of scr.children) {
          if (e.tagName === 'PowerUp') {
            const X = M64.hex(e.getAttribute('X')), Y = M64.hex(e.getAttribute('Y'));
            const t = M64.hex(e.getAttribute('Type'));
            const info = { kind: 'powerup', el: e, screen: c, movable: true, px: o.x + X * 16 + 8, py: o.y + Y * 16 + 8 };
            view.addMarker(M64.boxMarker(12, 12, 12, 0xf0c020, M64.POWER_UPS[t] || ('Item ' + t), '#ffe08a'), info.px, info.py, 0, info);
          } else if (e.tagName === 'Elevator') {
            view.addMarker(M64.boxMarker(40, 6, 20, 0xaaaaaa, 'Elevator'), o.x + 128, o.y + 0x80, 0, { kind: 'elevator', el: e, screen: c, movable: false });
          } else if (e.tagName === 'Door') {
            const left = (M64.hex(e.getAttribute('Info')) & 0x10) !== 0;
            view.addMarker(M64.boxMarker(6, 48, 18, 0x4090ff), o.x + (left ? 4 : 252), o.y + 0x70, 0, { kind: 'door', el: e, screen: c, movable: false });
          }
        }
      }
      for (const sp of S.savePoints) {
        if (areaOf(sp.area) !== S.bank || sp.map_x !== c.x || sp.map_y !== c.y) continue;
        const g = new THREE.Group();
        g.add(new THREE.Mesh(new THREE.BoxGeometry(32, 3, 20), new THREE.MeshLambertMaterial({ color: 0x78e6ff, emissive: 0 })));
        g.children[0].position.y = 1.5;
        const beam = new THREE.Mesh(new THREE.PlaneGeometry(22, 40), new THREE.MeshBasicMaterial({ color: 0x78dcff, transparent: true, opacity: 0.3, side: THREE.DoubleSide }));
        beam.position.y = 23;
        g.add(beam);
        const info = { kind: 'save', sp: sp, screen: c, movable: true, px: o.x + sp.x, py: o.y + sp.y };
        view.addMarker(g, info.px, info.py, 0, info);
      }
      for (const pr of S.props) {
        if (areaOf(pr.area) !== S.bank || pr.map_x !== c.x || pr.map_y !== c.y) continue;
        let obj = buildDL(pr.display_list);
        if (obj) {
          const holder = new THREE.Group();
          obj.scale.setScalar(pr.scale || 1);
          obj.rotation.y = (pr.yaw || 0) * Math.PI / 180;
          holder.add(obj);
          obj = holder;
        } else {
          obj = M64.boxMarker(16, 16, 16, 0x9a60ff, 'missing model');
        }
        const info = { kind: 'prop', prop: pr, screen: c, movable: true, px: o.x + pr.x, py: o.y + pr.y };
        view.addMarker(obj, info.px, info.py, pr.z || 0, info);
      }
    }
    view.highlight(S.selected);
  }

  const screenAt = function (px, py) {
    const s = S.section;
    const i = s.horizontal ? Math.floor(px / 256) : Math.floor(py / 240);
    if (i < 0 || i >= s.length || px < 0 || py < 0 || (s.horizontal ? py >= 240 : px >= 256)) return null;
    return M64.sectionScreens(s)[i];
  };
  const clamp = function (v, a, b) { return Math.max(a, Math.min(b, v)); };

  // --- Editing -----------------------------------------------------------------------------------------

  function onDrag(info, px, py) {
    const o = view.screenOrigin(info.screen.i);
    const lx = clamp(px - o.x, 0, 255), ly = clamp(py - o.y, 0, 239);
    if (info.kind === 'enemy') {
      const X = clamp(Math.round((lx - 12) / 16), 0, 15), Y = clamp(Math.round((ly - 8) / 16), 0, 14);
      info.el.setAttribute('X', X.toString(16).toUpperCase());
      info.el.setAttribute('Y', Y.toString(16).toUpperCase());
      view.moveMarker(info.view, o.x + X * 16 + 12, o.y + Y * 16 + 8);
      touch(info.path);
    } else if (info.kind === 'powerup') {
      const X = clamp(Math.round((lx - 8) / 16), 0, 15), Y = clamp(Math.round((ly - 8) / 16), 0, 14);
      info.el.setAttribute('X', X.toString(16).toUpperCase());
      info.el.setAttribute('Y', Y.toString(16).toUpperCase());
      view.moveMarker(info.view, o.x + X * 16 + 8, o.y + Y * 16 + 8);
      touch(M64.itemsPath(S.bank));
    } else if (info.kind === 'save') {
      info.sp.x = Math.round(lx);
      info.sp.y = Math.round(ly / 8) * 8; // floors are on the 8-pixel tile grid
      view.moveMarker(info.view, o.x + info.sp.x, o.y + info.sp.y);
      updateExportInfo();
    } else if (info.kind === 'prop') {
      info.prop.x = Math.round(lx);
      info.prop.y = Math.round(ly);
      view.moveMarker(info.view, o.x + info.prop.x, o.y + info.prop.y);
      updateExportInfo();
    }
    showProps();
  }

  function onPlace(px, py) {
    if (S.tool === 'select') return false;
    const c = screenAt(px, py);
    if (!c) return true;
    const o = view.screenOrigin(c.i);
    const lx = clamp(Math.round(px - o.x), 0, 255), ly = clamp(Math.round(py - o.y), 0, 239);
    const area = M64.AREAS[S.bank];
    let added = null;
    if (S.tool === 'save') {
      const sp = { area: area, map_x: c.x, map_y: c.y, x: lx, y: Math.round(ly / 8) * 8 };
      S.savePoints.push(sp);
      added = sp;
    } else if (S.tool === 'prop') {
      const m = S.models[S.modelIndex];
      if (!m) { status('Import a model first (Models)'); return true; }
      const pr = { area: area, map_x: c.x, map_y: c.y, x: lx, y: ly, z: 0, scale: 1, yaw: 0, display_list: m.root };
      S.props.push(pr);
      added = pr;
    } else if (S.tool === 'powerup') {
      const items = doc(M64.itemsPath(S.bank));
      let scr = Array.from(items.getElementsByTagName('Screen')).find(function (e) {
        return M64.hex(e.getAttribute('X')) === c.x && M64.hex(e.getAttribute('Y')) === c.y;
      });
      if (!scr) {
        scr = items.createElement('Screen');
        scr.setAttribute('X', M64.hex2(c.x));
        scr.setAttribute('Y', M64.hex2(c.y));
        items.documentElement.appendChild(scr);
      }
      const e = items.createElement('PowerUp');
      e.setAttribute('Type', '09');
      e.setAttribute('X', clamp(Math.round((lx - 8) / 16), 0, 15).toString(16).toUpperCase());
      e.setAttribute('Y', clamp(Math.round((ly - 8) / 16), 0, 14).toString(16).toUpperCase());
      scr.appendChild(e);
      touch(M64.itemsPath(S.bank));
      added = e;
    } else if (S.tool === 'enemy') {
      const room = S.world[c.y][c.x];
      const rpath = M64.roomPath(S.bank, room);
      const rdoc = doc(rpath);
      let items = rdoc.getElementsByTagName('Items')[0];
      if (!items) { items = rdoc.createElement('Items'); rdoc.documentElement.appendChild(items); }
      const used = new Set(Array.from(items.getElementsByTagName('Enemy')).map(function (e) { return Number(e.getAttribute('Slot')); }));
      let slot = 0;
      while (slot < 6 && used.has(slot)) slot++;
      if (slot >= 6) { status('Room ' + M64.hex2(room) + ' already has 6 enemies (the game has 6 slots)'); return true; }
      const e = rdoc.createElement('Enemy');
      e.setAttribute('Slot', String(slot));
      e.setAttribute('Type', '00');
      e.setAttribute('X', clamp(Math.round((lx - 12) / 16), 0, 15).toString(16).toUpperCase());
      e.setAttribute('Y', clamp(Math.round((ly - 8) / 16), 0, 14).toString(16).toUpperCase());
      items.insertBefore(e, items.firstChild);
      touch(rpath);
      added = e;
    }
    setTool('select');
    rebuildMarkers();
    if (added) {
      const m = view.markers.find(function (m) { return m.info.el === added || m.info.sp === added || m.info.prop === added; });
      select(m ? m.info : null);
    }
    updateExportInfo();
    drawMap();
    return true;
  }

  function setTool(t) {
    S.tool = t;
    for (const b of document.querySelectorAll('[data-tool]')) b.classList.toggle('on', b.dataset.tool === t);
    $('toolHint').textContent = {
      select: 'Click to select, drag to move. Right-drag: turn. Shift- or middle-drag: pan. Wheel: zoom.',
      save: 'Click where the save point goes (the floor it stands on).',
      prop: 'Click where the selected model goes.',
      powerup: 'Click where the power-up goes.',
      enemy: 'Click where the enemy goes (in its room: every screen using the room gets it).',
    }[t];
  }

  function select(info) {
    S.selected = info;
    view.highlight(info);
    showProps();
    if (info && info.screen) showScreenPanel(info.screen.i);
  }

  // --- Panels -----------------------------------------------------------------------------------------

  const field = function (label, id, value, attrs) {
    return '<label>' + esc(label) + '<input id="' + id + '" value="' + esc(value) + '" ' + (attrs || '') + '></label>';
  };

  function showProps() {
    const p = $('props');
    const info = S.selected;
    if (!info) { p.innerHTML = '<p class="muted">Nothing selected.</p>'; return; }
    let h = '';
    const del = '<button id="pDelete" class="danger">Delete</button>';
    if (info.kind === 'enemy') {
      const n = roomUses(info.room);
      h += '<h3>Enemy</h3><p class="muted">In room ' + M64.hex2(info.room) + (n > 1 ? ', used by ' + n + ' screens: they all change' : '') + '.</p>';
      h += field('Type (hex)', 'pType', info.el.getAttribute('Type'));
      h += field('Slot (0-5)', 'pSlot', info.el.getAttribute('Slot'));
      h += field('X (16-pixel blocks, 0-F)', 'pX', info.el.getAttribute('X'));
      h += field('Y (0-E)', 'pY', info.el.getAttribute('Y'));
      h += '<label class="check"><input type="checkbox" id="pTough"' + (info.el.getAttribute('Tough') === '1' ? ' checked' : '') + '> Tough</label>' + del;
    } else if (info.kind === 'powerup') {
      const t = M64.hex(info.el.getAttribute('Type'));
      h += '<h3>Power-up</h3><label>Type<select id="pType">' + M64.POWER_UPS.map(function (n, i) {
        return '<option value="' + i + '"' + (i === t ? ' selected' : '') + '>' + n + '</option>';
      }).join('') + '</select></label>';
      h += field('X (0-F)', 'pX', info.el.getAttribute('X')) + field('Y (0-E)', 'pY', info.el.getAttribute('Y')) + del;
    } else if (info.kind === 'save') {
      h += '<h3>Save point</h3><p class="muted">Samus saves by standing on it and pressing Up.</p>';
      h += field('x (middle, 0-255)', 'pX', info.sp.x, 'type="number"') + field('y (floor, 0-239)', 'pY', info.sp.y, 'type="number"') + del;
    } else if (info.kind === 'prop') {
      const pr = info.prop;
      h += '<h3>Prop</h3><label>Display list<select id="pDL">' + allDisplayLists(pr.display_list) + '</select></label>';
      h += field('x', 'pX', pr.x, 'type="number"') + field('y', 'pY', pr.y, 'type="number"') + field('z (towards the camera)', 'pZ', pr.z || 0, 'type="number"');
      h += field('Scale', 'pScale', pr.scale || 1, 'type="number" step="0.05"') + field('Yaw (degrees)', 'pYaw', pr.yaw || 0, 'type="number"') + del;
    } else if (info.kind === 'door') {
      h += '<h3>Door</h3><p class="muted">Doors stay where the original has them.</p>';
    } else if (info.kind === 'elevator') {
      h += '<h3>Elevator</h3><p class="muted">Info ' + esc(info.el.getAttribute('Info')) + '</p>';
    } else if (info.kind === 'geometry') {
      h += '<h3>Room model</h3><p class="muted">Set in the Screen panel.</p>';
    }
    p.innerHTML = h;
    const bind = function (id, fn) { const e = $(id); if (e) e.addEventListener('change', function () { fn(e); rebuildMarkers(); select(S.selected); }); };
    if (info.kind === 'enemy' || info.kind === 'powerup') {
      const path = info.kind === 'enemy' ? info.path : M64.itemsPath(S.bank);
      const hexAttr = function (name, max, width) {
        return function (e) {
          const v = clamp(parseInt(e.value, 16) || 0, 0, max);
          info.el.setAttribute(name, width === 2 ? M64.hex2(v) : v.toString(16).toUpperCase());
          touch(path);
        };
      };
      bind('pX', hexAttr('X', 15, 1));
      bind('pY', hexAttr('Y', 14, 1));
      if (info.kind === 'enemy') {
        bind('pType', hexAttr('Type', 255, 2));
        bind('pSlot', function (e) { info.el.setAttribute('Slot', String(clamp(Number(e.value) || 0, 0, 5))); touch(path); });
        bind('pTough', function (e) { if (e.checked) info.el.setAttribute('Tough', '1'); else info.el.removeAttribute('Tough'); touch(path); });
      } else {
        bind('pType', function (e) { info.el.setAttribute('Type', M64.hex2(Number(e.value))); touch(path); });
      }
    } else if (info.kind === 'save') {
      bind('pX', function (e) { info.sp.x = clamp(Number(e.value) || 0, 0, 255); });
      bind('pY', function (e) { info.sp.y = clamp(Number(e.value) || 0, 0, 239); });
    } else if (info.kind === 'prop') {
      bind('pDL', function (e) { info.prop.display_list = e.value; });
      bind('pX', function (e) { info.prop.x = Number(e.value) || 0; });
      bind('pY', function (e) { info.prop.y = Number(e.value) || 0; });
      bind('pZ', function (e) { info.prop.z = Number(e.value) || 0; });
      bind('pScale', function (e) { info.prop.scale = Number(e.value) || 1; });
      bind('pYaw', function (e) { info.prop.yaw = Number(e.value) || 0; });
    }
    const d = $('pDelete');
    if (d) d.addEventListener('click', function () {
      if (info.kind === 'save') S.savePoints.splice(S.savePoints.indexOf(info.sp), 1);
      else if (info.kind === 'prop') S.props.splice(S.props.indexOf(info.prop), 1);
      else {
        const parent = info.el.parentNode;
        parent.removeChild(info.el);
        touch(info.kind === 'enemy' ? info.path : M64.itemsPath(S.bank));
      }
      select(null);
      rebuildMarkers();
      drawMap();
      updateExportInfo();
    });
  }

  function allDisplayLists(current) {
    const opts = [];
    for (const m of S.models) for (const r of m.roots) opts.push(r);
    if (current && opts.indexOf(current) < 0) opts.unshift(current);
    return opts.map(function (r) { return '<option' + (r === current ? ' selected' : '') + '>' + esc(r) + '</option>'; }).join('');
  }

  function showScreenPanel(i) {
    const p = $('screen');
    if (!S.section) { p.innerHTML = ''; return; }
    const c = M64.sectionScreens(S.section)[i];
    if (!c) return;
    const room = S.world[c.y][c.x];
    const geo = S.roomGeo.get(geoKey(S.bank, c.x, c.y));
    let h = '<h3>Screen ' + M64.hex2(c.x) + ', ' + M64.hex2(c.y) + '</h3><p class="muted">Room ' + M64.hex2(room) +
      (roomUses(room) > 1 ? ' (also used by ' + (roomUses(room) - 1) + ' other screens)' : '') + '</p>';
    h += '<label>Room model<select id="gDL"><option value="">(the game\'s tiles)</option>' + allDisplayLists(geo ? geo.display_list : '') + '</select></label>';
    if (geo) {
      h += field('Offset x', 'gX', geo.offset[0], 'type="number"') + field('Offset y (up; the screen is 0 to -240)', 'gY', geo.offset[1], 'type="number"') +
        field('Offset z', 'gZ', geo.offset[2], 'type="number"') + field('Scale', 'gS', geo.scale, 'type="number" step="0.05"');
      h += '<label class="check"><input type="checkbox" id="gKeep"' + (geo.keep_tiles ? ' checked' : '') + '> Keep the game\'s tiles too</label>';
    }
    h += '<p class="muted">The model only changes what you see: Samus still collides with the game\'s tiles.</p>';
    p.innerHTML = h;
    const key = geoKey(S.bank, c.x, c.y);
    const changed = function () { rebuildMarkers(); showScreenPanel(i); updateExportInfo(); };
    $('gDL').addEventListener('change', function (e) {
      if (!e.target.value) S.roomGeo.delete(key);
      else S.roomGeo.set(key, Object.assign({ offset: [0, 0, 0], scale: 1, keep_tiles: false }, geo || {}, { display_list: e.target.value }));
      changed();
    });
    if (geo) {
      ['gX', 'gY', 'gZ'].forEach(function (id, k) { $(id).addEventListener('change', function (e) { geo.offset[k] = Number(e.target.value) || 0; changed(); }); });
      $('gS').addEventListener('change', function (e) { geo.scale = Number(e.target.value) || 1; changed(); });
      $('gKeep').addEventListener('change', function (e) { geo.keep_tiles = e.target.checked; changed(); });
    }
    view.focusScreen(i);
  }

  function showModels() {
    const l = $('models');
    if (!S.models.length) { l.innerHTML = '<p class="muted">No models yet.</p>'; return; }
    l.innerHTML = S.models.map(function (m, i) {
      return '<div class="model' + (i === S.modelIndex ? ' on' : '') + '" data-i="' + i + '"><b>' + esc(m.name) + '</b><br><select data-root="' + i + '">' +
        m.roots.map(function (r) { return '<option' + (r === m.root ? ' selected' : '') + '>' + esc(r) + '</option>'; }).join('') + '</select></div>';
    }).join('');
    for (const d of l.querySelectorAll('.model')) d.addEventListener('click', function () { S.modelIndex = Number(d.dataset.i); showModels(); });
    for (const s of l.querySelectorAll('select')) s.addEventListener('change', function () { S.models[Number(s.dataset.root)].root = s.value; });
  }

  async function importModel(files) {
    if (!files.length) return;
    const first = files[0];
    const name = (first.webkitRelativePath || first.name).split('/')[0] || 'model';
    const m = await M64.modelFromFiles(name, Array.from(files));
    if (!m.roots.length) { status('No display lists (<DisplayList> files) in what was picked'); return; }
    S.models.push(m);
    S.modelIndex = S.models.length - 1;
    showModels();
    status('Imported ' + name + ': ' + m.files.size + ' files, ' + m.roots.length + ' display list' + (m.roots.length > 1 ? 's' : '') + ' to place');
  }

  // --- Export --------------------------------------------------------------------------------------------

  function modFiles() {
    const files = new Map(S.project.mod.map);
    const ser = new XMLSerializer();
    for (const path of S.dirty) files.set(path, ser.serializeToString(S.docs.get(path)) + '\n');
    const json = function (o) { return JSON.stringify(o, null, 2) + '\n'; };
    if (S.savePoints.length || files.has('metroid64/savepoints.json')) files.set('metroid64/savepoints.json', json({ save_points: S.savePoints }));
    if (S.props.length || files.has('metroid64/props.json')) files.set('metroid64/props.json', json({ props: S.props }));
    // Room models: drop the old manifests, write the current ones.
    for (const p of Array.from(files.keys())) if (/^rooms\/[a-z]+\/[0-9A-F]{2}_[0-9A-F]{2}\.json$/.test(p)) files.delete(p);
    const used = new Set(S.props.map(function (p) { return p.display_list; }));
    for (const [key, g] of S.roomGeo) {
      const k = key.split(',').map(Number);
      files.set('rooms/' + M64.AREAS[k[0]] + '/' + M64.hex2(k[1]) + '_' + M64.hex2(k[2]) + '.json', json({
        scale: g.scale, offset: g.offset, keep_tiles: g.keep_tiles, fallback: 'Idle',
        animations: { Idle: { fps: 1, loop: true, frames: [g.display_list] } } }));
      used.add(g.display_list);
    }
    // The models' files, at the paths their display lists use.
    for (const m of S.models) {
      let needed = false;
      for (const r of m.roots) if (used.has(r)) needed = true;
      if (needed) for (const [p, b] of m.files) files.set(p, b);
    }
    return files;
  }

  function updateExportInfo() {
    if (!S.project) return;
    const parts = [];
    if (S.dirty.size) parts.push(S.dirty.size + ' room/item file' + (S.dirty.size > 1 ? 's' : ''));
    if (S.savePoints.length) parts.push(S.savePoints.length + ' save point' + (S.savePoints.length > 1 ? 's' : ''));
    if (S.props.length) parts.push(S.props.length + ' prop' + (S.props.length > 1 ? 's' : ''));
    if (S.roomGeo.size) parts.push(S.roomGeo.size + ' room model' + (S.roomGeo.size > 1 ? 's' : ''));
    $('exportInfo').textContent = parts.length ? 'In the mod: ' + parts.join(', ') + '.' : 'No changes yet.';
  }

  async function exportMod() {
    const files = modFiles();
    const zip = new JSZip();
    for (const [p, b] of files) zip.file(p, b);
    const blob = await zip.generateAsync({ type: 'blob', compression: 'DEFLATE' });
    const name = ($('modName').value.trim() || 'mymod').replace(/[^A-Za-z0-9_ .-]/g, '_') + '.o2r';
    const a = document.createElement('a');
    a.href = URL.createObjectURL(blob);
    a.download = name;
    document.body.appendChild(a);
    a.click();
    a.remove();
    status('Exported ' + name + ' (' + files.size + ' files). Put it in Metroid64\'s mods folder.');
  }

  // --- Start --------------------------------------------------------------------------------------------

  window.addEventListener('DOMContentLoaded', function () {
    view = new M64.View($('view'));
    view.on('select', select);
    view.on('drag', onDrag);
    view.on('dragEnd', function (info) { if (info.kind === 'enemy') rebuildMarkers(); drawMap(); });
    view.on('place', onPlace);
    $('openBase').addEventListener('change', function (e) { if (e.target.files[0]) openBase(e.target.files[0]); });
    $('openMod').addEventListener('change', function (e) { if (e.target.files[0]) openMod(e.target.files[0]); });
    $('importFolder').addEventListener('change', function (e) { importModel(e.target.files); e.target.value = ''; });
    $('importFiles').addEventListener('change', function (e) { importModel(e.target.files); e.target.value = ''; });
    $('area').addEventListener('change', function (e) { selectArea(Number(e.target.value)); });
    $('export').addEventListener('click', exportMod);
    for (const b of document.querySelectorAll('[data-tool]')) b.addEventListener('click', function () { setTool(b.dataset.tool); });
    $('map').addEventListener('click', function (e) {
      if (!S.area) return;
      const r = e.target.getBoundingClientRect();
      const x = Math.floor((e.clientX - r.left) / r.width * 32), y = Math.floor((e.clientY - r.top) / r.height * 32);
      const list = sectionsAt(x, y);
      if (!list.length) return;
      // Clicking the same screen again goes through the sections it is in.
      const k = x + ',' + y;
      const n = S.sectionCycle[k] = list.indexOf(S.section) >= 0 ? (list.indexOf(S.section) + 1) % list.length : 0;
      showSection(list[n]);
      const c = M64.sectionScreens(list[n]).find(function (c) { return c.x === x && c.y === y; });
      if (c) showScreenPanel(c.i);
    });
    document.addEventListener('keydown', function (e) {
      if (e.target.tagName === 'INPUT' || e.target.tagName === 'SELECT') return;
      if ((e.key === 'Delete' || e.key === 'Backspace') && $('pDelete')) $('pDelete').click();
      if (e.key === 'Escape') { setTool('select'); select(null); }
    });
    setTool('select');
    showModels();
    // For tests: M64.editor exposes the state.
    M64.editor = { S: S, openBase: openBase, openMod: openMod, importModel: importModel, modFiles: modFiles, onPlace: onPlace,
      setTool: setTool, select: select, onDrag: onDrag, view: function () { return view; }, selectArea: selectArea };
  });
})();
