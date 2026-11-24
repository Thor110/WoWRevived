# Unit models in common formats

MODERN (update 248; agreed with the owner at 247). A unit's model, a WOF, can be given new shapes from an OBJ, a
glTF or a GLB in a texture pack. Buildings can't: they aren't models but pictures drawn beforehand and cut into
patches (TEXTURES.md, MODERN.md "Files"), so they're reworked with their pictures. The players' version of this
page is in README-TEXTURE-PACKS.txt ("Models").

## What a WOF is (as the engine reads it: `core/WofView`, `core/Models`, `core/WofMesh`)

- A tree of up to 500 **pieces**, each named (16 characters), with a **pivot**, up to 255 **points** (three words
  each, relative to the piece's place) and up to 255 **triangles** (10 bytes: three point numbers, a material, and
  each corner's place in the atlas from the material's place). The root's pivot has its y turned round
  (`WofInstance::Transform`); each child sits at its parent's place plus its pivot (`Walk`).
- **Animation** turns pieces by their place in the tree, frame by frame (header 0x1C frames, keys at 0x20).
- **Materials** (0x24): a word each, an offset into the atlas (the top bit: colour 0 see-through).
- The **atlas** (0x28 to 0x2C): 256 wide, the model's texture, which a texture pack's PNG repaints.
- **Smooth pieces** (flag 0x20) average their triangles' light into their points through groups at the table 0x2C
  (a word a piece: a count of groups, each n triangles and then n points).
- The engine finds some points by number: a gun's muzzle (its piece's first point, `FirstPoint`), the walkers'
  feet, the bridges' joints (`Stitch`).

## Export: `wowdump --obj <Dat.wow> <folder>`

Every WOF as `<name>.obj` and `<name>.mtl`:
- one group (`o`) a piece, named as the piece, its points (`v`) under it in their order;
- the points in the model's space at rest (each plus its piece's place in the tree; the animation's turns aren't
  in it), y up;
- a texture place (`vt`) for each triangle corner, across the whole atlas (u across its 256 pixels, v up from its
  last row, as OBJ has it), and `usemtl material_N` for the triangles' material;
- the MTL's materials each `map_Kd <name>.png`: the atlas as `wowdump --textures` writes it (as a BMP; saved as PNG).

## Import: a model in a texture pack

`<name>.obj`, `<name>.gltf` or `<name>.glb` beside the model's PNG in the pack (`human/units/AIRSHIP.obj`), read with
Enhanced assets on as the battle map loads the model (`BattleMap::PackModel`, after the atlas). An OBJ is used before
a glTF before a GLB. WoWEngine.log names it ("Texture pack: model human/units/AIRSHIP.obj") or why it wasn't used.

- **Pieces by name**: each group (OBJ) or mesh node (glTF; its points through the node's place in the scene) gives
  the piece of its name new points and triangles. Pieces it doesn't name keep the game's. A name the model doesn't
  have is noted and left out. The tree, pivots, animation, materials and atlas are the game's, so a piece's points
  are in the model's space as the export writes them, and its animation still turns it about its pivot.
- **Limits**: 255 points and 255 triangles a piece (the file's counts are bytes); a piece past them is noted and
  the game's kept. Polygons are fanned into triangles.
- **Materials**: `usemtl` / the glTF material's name ending in a number (`material_3`, `Material.3`) picks the
  WOF's material; without one, the piece's commonest. A triangle whose corners the material's part of the atlas
  can't reach takes another material of the same kind (see-through or not) that can, else is kept to the edge
  (noted).
- **Point order**: OBJ keeps a piece's points as listed (keep an exported piece's points in order, adding new ones
  after, so guns still fire from their muzzles). glTF keeps a texture place a vertex, so the vertices at the same
  place (with the same normal, where the file has normals) are one point, the first one's.
- **Smooth pieces**: with the game's numbers of points and triangles the game's groups are kept (they still fit);
  otherwise each point's group is the triangles that have it. A hard edge needs a point for each side of it: in
  glTF, vertices with different normals.
- **Written** (`ApplyMeshes`): the new points and triangles (and groups) are added at the file's end and the piece's
  record pointed at them; nothing else in the file changes.

## Making one in a 3D tool (the owner, update 249)

**OBJ files can't be reordered: they must keep their original structure** (the groups, each group's points and
triangles in the export's order), or the animation and the points found by number go wrong. **Blender reorders OBJ
files unless an option is ticked to stop it** (the owner, from experience; which options and versions isn't
recorded): its settings likely need adjusting on import and export to keep the order. Not verified here (Blender
couldn't be installed in the build container). Also: no "Merge by Distance" (the models' coincident points are on
purpose, a hard edge's two sides), no validating or cleaning up the mesh, the same axes and scale both ways, the
groups' names kept, and materials kept as `material_N`. glTF is less fussy (coincident vertices with the same normal
are joined), but needs normals for hard edges, and embedded data or `.glb`.

## Checked

- `wowdump --objcheck <Dat.wow>`: all 145 of the game's WOFs exported, read back and put into the game's file give
  the same points and triangles, byte for byte. `--objcheck <Dat.wow> <folder>` does the same with models from a
  folder: the 145 as glTF (written from the OBJs, every corner its own vertex, no normals) give the same shapes
  (each triangle's corners at the same places, the same materials and atlas places).
- In a battle (the London parity battle with Enhanced assets on): the 93 Human and Martian unit models as the
  exported OBJs draw the same to the pixel as without them; made twice as tall, the units are drawn twice as tall.
  As the glTF without normals, 361 pixels of the units' shading differ (their points merged, so the smooth pieces'
  groups are rebuilt).
- Unit tests: a small WOF to OBJ and back, a quad, texture places, the limits.

*Not yet*: a model made in a 3D tool (Blender) tried in a battle; the animation's turns in the export (the rest pose
is the tree's places only, which for a model whose first frame turns its pieces isn't how it stands).
