# Builds the segmented player model (one mesh per body part). Run:
#   blender --background --python tools/player_model.py
# Outputs:
#   tools/player_model.blend   (open in Blender to inspect the assembled figure)
#   tools/player_model.png     (preview render)
#   src/player_mesh.h          (baked per-part vertex data, unit-box space)
#
# Each part is authored inside the unit cube [-0.5, 0.5]^3 and is drawn by the game
# with exactly the same transform as the old plain cube (M * scale(2*half)), so the
# hitboxes (playerpose.h) stay untouched. Axis conventions per part, in Blender
# coords (game x = bl.x, game y = bl.z, game z = -bl.y):
#   spine parts (head/torso/pelvis/neck): +Z up, -Y front (game +Z forward)
#   limb parts (arm/leg): +Z distal (elbow/knee/foot end)
#   foot: +Z toe, +Y sole (world down)
# Part order here MUST match enum PlayerPartId in renderer.h.
import bpy
import bmesh
import math
import os
from mathutils import Matrix, Vector

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

PARTS = []          # (name, verts, indices) in declaration order


def box(bm, cx, cy, cz, sx, sy, sz):
    m = Matrix.Translation((cx, cy, cz)) @ Matrix.Diagonal((sx / 2, sy / 2, sz / 2, 1.0))
    res = bmesh.ops.create_cube(bm, size=2.0)
    bmesh.ops.transform(bm, matrix=m, verts=res["verts"])
    return res["verts"]


def bevel(bm, offset, verts=None):
    edges = [e for e in bm.edges if verts is None or (e.verts[0] in verts and e.verts[1] in verts)]
    bmesh.ops.bevel(bm, geom=edges, offset=offset, offset_type="OFFSET",
                    segments=1, profile=0.7, affect="EDGES")


def octprism(bm, r_bot, r_top, z0, z1, cx=0.0, cy=0.0):
    res = bmesh.ops.create_cone(bm, cap_ends=True, segments=8,
                                radius1=r_bot, radius2=r_top, depth=z1 - z0)
    m = Matrix.Translation((cx, cy, (z0 + z1) / 2)) @ Matrix.Rotation(math.pi / 8, 4, "Z")
    bmesh.ops.transform(bm, matrix=m, verts=res["verts"])
    return res["verts"]


def finish(name, bm):
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-4)
    mesh = bpy.data.meshes.new(name)
    bm.to_mesh(mesh)
    bm.free()
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(obj)
    bpy.context.view_layer.objects.active = obj
    obj.select_set(True)
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.mesh.normals_make_consistent(inside=False)
    bpy.ops.object.mode_set(mode="OBJECT")
    obj.select_set(False)

    mesh.calc_loop_triangles()
    verts, index_of, indices = [], {}, []
    for tri in mesh.loop_triangles:
        for vi in tri.vertices:
            co = mesh.vertices[vi].co
            key = (round(co.x, 4), round(co.y, 4), round(co.z, 4))
            if key not in index_of:
                index_of[key] = len(verts)
                verts.append((co.x, co.z, -co.y))       # Blender Z-up -> game Y-up
            indices.append(index_of[key])
    PARTS.append((name, verts, indices))
    return obj


# --- part builders (unit-box space) -------------------------------------------

def build_head():
    bm = bmesh.new()
    skull = box(bm, 0, 0.03, -0.02, 0.92, 0.86, 0.92)   # front face at y=-0.40
    bevel(bm, 0.10, skull)
    box(bm, 0, -0.44, 0.16, 0.70, 0.16, 0.16)           # brow ridge
    box(bm, 0, -0.49, -0.10, 0.16, 0.18, 0.20)          # nose (pokes past the face)
    box(bm, 0, -0.42, -0.34, 0.44, 0.10, 0.16)          # jaw/chin step
    for s in (-1, 1):
        box(bm, s * 0.47, 0.05, -0.06, 0.10, 0.26, 0.24)  # ears
    box(bm, 0, 0.10, 0.42, 0.80, 0.72, 0.14)            # flat crown (hair line)
    return finish("head", bm)


def build_torso():
    bm = bmesh.new()
    main = box(bm, 0, 0.02, 0.02, 0.98, 0.78, 0.94)
    bevel(bm, 0.09, main)
    box(bm, 0, -0.46, 0.06, 0.74, 0.14, 0.72)           # front vest plate
    box(bm, 0, -0.52, 0.16, 0.30, 0.10, 0.22)           # chest pouch
    for s in (-1, 1):
        box(bm, s * 0.24, -0.52, -0.14, 0.22, 0.10, 0.24)  # mag pouches
    back = box(bm, 0, 0.52, 0.10, 0.62, 0.22, 0.62)     # small backpack
    bevel(bm, 0.06, back)
    for s in (-1, 1):
        box(bm, s * 0.40, 0.0, 0.42, 0.16, 0.70, 0.12)  # shoulder straps
    return finish("torso", bm)


def build_pelvis():
    bm = bmesh.new()
    main = box(bm, 0, 0, -0.06, 0.94, 0.90, 0.86)
    bevel(bm, 0.08, main)
    box(bm, 0, 0, 0.42, 1.00, 0.96, 0.16)               # belt band
    box(bm, 0, -0.50, 0.42, 0.20, 0.10, 0.14)           # buckle
    return finish("pelvis", bm)


def build_neck():
    bm = bmesh.new()
    octprism(bm, 0.52, 0.48, -0.5, 0.5)
    return finish("neck", bm)


def build_arm():
    bm = bmesh.new()
    octprism(bm, 0.53, 0.44, -0.5, 0.5)                 # tapers toward the distal end
    return finish("arm", bm)


def build_hand():
    bm = bmesh.new()
    fist = box(bm, 0, 0, 0, 0.92, 0.92, 0.92)
    bevel(bm, 0.14, fist)
    return finish("hand", bm)


def build_leg():
    bm = bmesh.new()
    octprism(bm, 0.53, 0.45, -0.5, 0.5)
    return finish("leg", bm)


def build_foot():
    bm = bmesh.new()
    boot = box(bm, 0, -0.05, 0.02, 0.86, 0.80, 0.90)    # +Z toe, +Y sole
    bevel(bm, 0.10, boot)
    box(bm, 0, 0.42, 0.0, 0.96, 0.16, 1.00)             # sole slab
    box(bm, 0, -0.30, -0.42, 0.70, 0.40, 0.14)          # toe cap
    return finish("foot", bm)


# --- preview: assemble the rest-pose figure from the exported parts -----------

def place(obj, gx, gy, gz, sx, sy, sz):
    inst = obj.copy()
    inst.hide_render = False
    inst.location = (gx, -gz, gy)                       # game -> Blender coords
    inst.scale = (sx, sz, sy)
    bpy.context.collection.objects.link(inst)
    return inst


def preview():
    mats = {}
    def mat(name, rgba):
        if name not in mats:
            m = bpy.data.materials.new(name)
            m.diffuse_color = rgba
            mats[name] = m
        return mats[name]

    OLIVE = (0.24, 0.26, 0.16, 1.0)
    LIMB  = (0.20, 0.22, 0.14, 1.0)
    SKIN  = (0.55, 0.40, 0.28, 1.0)
    BOOT  = (0.10, 0.09, 0.08, 1.0)
    objs = {name: bpy.data.objects[name] for name, _, _ in PARTS}
    for o in objs.values():
        o.hide_render = True
        o.location = (0, 3, 0.5)                        # park originals out of frame

    def put(part, matname, rgba, *args):
        inst = place(objs[part], *args)
        inst.data = inst.data.copy()
        inst.data.materials.clear()
        inst.data.materials.append(mat(matname, rgba))

    put("pelvis", "olive", OLIVE, 0, 0.95, 0, 0.36, 0.18, 0.22)
    put("torso",  "olive", OLIVE, 0, 1.26, 0, 0.34, 0.44, 0.22)
    put("neck",   "skin",  SKIN,  0, 1.53, 0, 0.12, 0.10, 0.12)
    put("head",   "skin",  SKIN,  0, 1.68, 0, 0.24, 0.24, 0.24)
    for s in (-1, 1):
        put("arm",  "limb", LIMB, s * 0.27, 1.30, 0, 0.12, 0.30, 0.12)
        put("arm",  "limb", LIMB, s * 0.27, 1.00, 0, 0.11, 0.30, 0.11)
        put("hand", "skin", SKIN, s * 0.27, 0.82, 0, 0.13, 0.12, 0.13)
        put("leg",  "limb", LIMB, s * 0.10, 0.645, 0, 0.20, 0.43, 0.20)
        put("leg",  "limb", LIMB, s * 0.10, 0.215, 0, 0.18, 0.43, 0.18)
        inst = place(objs["foot"], s * 0.10, 0.06, -0.08, 0.12, 0.12, 0.16)
        inst.data = inst.data.copy()
        inst.data.materials.clear()
        inst.data.materials.append(mat("boot", BOOT))
        inst.rotation_euler = (0, 0, math.pi)           # toe forward (-game Z is +Y here)

    sun = bpy.data.objects.new("sun", bpy.data.lights.new("sun", "SUN"))
    sun.rotation_euler = (math.radians(55), 0, math.radians(35))
    bpy.context.collection.objects.link(sun)
    cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
    cam.location = (2.2, -3.0, 1.55)
    look = Vector((0, 0, 0.95)) - cam.location
    cam.rotation_euler = look.to_track_quat("-Z", "Y").to_euler()
    bpy.context.collection.objects.link(cam)
    bpy.context.scene.camera = cam
    sc = bpy.context.scene
    sc.render.engine = "BLENDER_WORKBENCH"
    sc.display.shading.light = "STUDIO"
    sc.display.shading.color_type = "MATERIAL"
    sc.display.shading.show_cavity = True
    sc.render.resolution_x = 700
    sc.render.resolution_y = 900
    sc.render.filepath = os.path.join(ROOT, "tools", "player_model.png")


def export_header(path):
    lines = ["#pragma once", ""]
    lines.append("// Baked by tools/player_model.py — do not edit. Part order matches")
    lines.append("// enum PlayerPartId in renderer.h. Unit-box space: draw with the same")
    lines.append("// M * scale(2*half) transform as the plain cube.")
    lines.append("")
    total_tris = 0
    for name, verts, indices in PARTS:
        up = name.upper()
        total_tris += len(indices) // 3
        lines.append(f"inline const float PMESH_{up}_VERTS[] = {{")
        for v in verts:
            lines.append(f"    {v[0]:.4f}f, {v[1]:.4f}f, {v[2]:.4f}f,")
        lines.append("};")
        lines.append(f"inline const unsigned PMESH_{up}_IDX[] = {{")
        for i in range(0, len(indices), 12):
            lines.append("    " + ", ".join(str(x) for x in indices[i:i + 12]) + ",")
        lines.append("};")
        lines.append("")
    lines.append("struct PMeshPart { const float* verts; int floatCount; "
                 "const unsigned* idx; int idxCount; };")
    lines.append(f"inline const PMeshPart PMESH_PARTS[] = {{")
    for name, verts, indices in PARTS:
        up = name.upper()
        lines.append(f"    {{PMESH_{up}_VERTS, {len(verts) * 3}, "
                     f"PMESH_{up}_IDX, {len(indices)}}},")
    lines.append("};")
    lines.append("")
    with open(path, "w") as f:
        f.write("\n".join(lines))
    print(f"wrote {path}: {len(PARTS)} parts, {total_tris} tris total")


bpy.ops.object.select_all(action="SELECT")
bpy.ops.object.delete()
build_head()
build_torso()
build_pelvis()
build_neck()
build_arm()
build_hand()
build_leg()
build_foot()
export_header(os.path.join(ROOT, "src", "player_mesh.h"))
preview()
bpy.ops.render.render(write_still=True)
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(ROOT, "tools", "player_model.blend"))
print("done")
