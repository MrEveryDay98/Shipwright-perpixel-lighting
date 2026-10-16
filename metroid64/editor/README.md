# Metroid64 Editor

A standalone page for making Metroid64 mods. Open `index.html` in a browser (Chrome, Edge or Firefox); it
needs an internet connection the first time for three.js and JSZip (from cdnjs and jsDelivr). Nothing is
uploaded: everything happens in the page.

## Use

1. **Open metroid.o2r**: the game data Metroid64 makes from your ROM, next to the game. (Made by an older
   Metroid64? Run the newer game once and it remakes it.)
2. Pick an **area**, then a screen on its **map**: the 3D view shows the scrolling section it is in, as the
   game draws it. Clicking the same screen again goes through the sections it is in. Right-drag turns the view,
   shift- or middle-drag pans, the wheel zooms.
3. Edit:
   - **Enemies** (red) and **power-ups** (yellow): drag them, or change them in the *Selected* panel. Enemies
     belong to the room, and a room can be used by several screens: they all change (the panel says how many).
     *+ Enemy* adds one to the room (6 at most), *+ Power-up* one to the screen.
   - **Save points** (*+ Save point*): click the floor where it goes. Samus saves by standing on it and pressing Up.
   - **Props** (*+ Prop*): import a model first (below), then click where it goes; set its depth, scale and turn in
     the panel.
   - **Room model** (*Screen* panel): a model drawn instead of the screen's tiles (or as well, with *Keep the
     game's tiles*). It only changes what you see: Samus still collides with the game's tiles.
   - Doors and elevators are shown but stay as they are.
4. **Models**: *Import Fast64 export (folder)* (or *Import files*) takes a Fast64 export for Ship of Harkinian:
   display lists in Shipwright's XML with their vertex files and textures. The export keeps the paths its files use
   for each other, so the game finds them. Static display lists work best; a skeleton's limbs show without their pose.
5. **Export mod (.o2r)** saves everything changed as a mod. Put it in Metroid64's `mods` folder. **Open a mod to keep
   editing** opens one again (with the same metroid.o2r opened first).

## What the mod holds

- `metroid/rooms/<area>/room_NN`, `metroid/items/<area>`: the rooms and special items changed (docs/DATA.md).
- `metroid64/savepoints.json`, `metroid64/props.json`: save points and props (README, docs/MODELS.md).
- `rooms/<area>/<XX>_<YY>.json`: room models (docs/MODELS.md), and the models' files.

## Files

- `js/data.js`: reading metroid.o2r and mods (rooms, items, palettes, the room meshes, the area tiles).
- `js/dl.js`: Fast64 / Shipwright XML display lists and libultraship textures, for showing models.
- `js/view.js`: the 3D view (three.js), markers, picking and dragging.
- `js/app.js`: the editor itself, and export.
