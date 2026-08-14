# Builds the hunting stand prop. Run:
#   blender --background --python tools/hunting_stand.py
# Outputs:
#   tools/hunting_stand.blend   (open in Blender to inspect)
#   tools/hunting_stand.png     (preview render)
#   src/stand_mesh.h            (baked vertex data, Y-up game coords)
import bpy
import bmesh
import math
import os
from mathutils import Matrix, Vector

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

WOOD_DARK = (0.28, 0.19, 0.11, 1.0)
WOOD_MID = (0.38, 0.27, 0.16, 1.0)

PLAT_Z = 3.0
ROOF_Z = 5.0
FOOT = 1.0
LEG = 0.13


def clear_scene():
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete()


BOX_AABBS = []
COL_BOXES = []
COL_MERGE = {}


def aabb_of(m, ranges):
    pts = [m @ Vector((px, py, pz)) for px in ranges[0] for py in ranges[1] for pz in ranges[2]]
    lo = Vector((min(p[i] for p in pts) for i in range(3)))
    hi = Vector((max(p[i] for p in pts) for i in range(3)))
    return lo, hi


def add_box(bm, cx, cy, cz, sx, sy, sz, rx=0.0, ry=0.0, col="exact"):
    m = (
        Matrix.Translation((cx, cy, cz))
        @ Matrix.Rotation(rx, 4, "X")
        @ Matrix.Rotation(ry, 4, "Y")
        @ Matrix.Diagonal((sx / 2, sy / 2, sz / 2, 1.0))
    )
    res = bmesh.ops.create_cube(bm, size=2.0)
    bmesh.ops.transform(bm, matrix=m, verts=res["verts"])
    lo, hi = aabb_of(m, ((-1, 1), (-1, 1), (-1, 1)))
    BOX_AABBS.append((lo, hi))

    if col == "skip":
        return
    if col.startswith("merge:"):
        key = col[6:]
        if key in COL_MERGE:
            plo, phi = COL_MERGE[key]
            COL_MERGE[key] = (Vector(map(min, plo, lo)), Vector(map(max, phi, hi)))
        else:
            COL_MERGE[key] = (lo, hi)
        return
    if rx == 0.0 and ry == 0.0:
        COL_BOXES.append((lo, hi))
        return
    sizes = (sx, sy, sz)
    axis = max(range(3), key=lambda i: sizes[i])
    n = max(1, math.ceil(sizes[axis] / 0.45))
    for i in range(n):
        r = [(-1, 1), (-1, 1), (-1, 1)]
        r[axis] = (-1 + 2 * i / n, -1 + 2 * (i + 1) / n)
        COL_BOXES.append(aabb_of(m, r))


def check_connected():
    def overlaps(a, b, pad=0.005):
        return all(a[1][i] - b[0][i] > pad and b[1][i] - a[0][i] > pad for i in range(3))

    ok = True
    for i, a in enumerate(BOX_AABBS):
        if a[0].z <= 0.01:
            continue
        if not any(i != j and overlaps(a, b) for j, b in enumerate(BOX_AABBS)):
            print(f"FLOATING BOX {i}: min {tuple(a[0])} max {tuple(a[1])}")
            ok = False
    print("connectivity:", "OK" if ok else "FLOATING PARTS FOUND")


def build_stand():
    bm = bmesh.new()

    splay = 0.35
    for sx in (-1, 1):
        for sy in (-1, 1):
            top = Vector((sx * FOOT, sy * FOOT, PLAT_Z))
            bot = Vector((sx * (FOOT + splay), sy * (FOOT + splay), 0.0))
            mid = (top + bot) / 2
            d = top - bot
            ry = math.atan2(d.x, d.z)
            rx = -math.atan2(d.y, math.hypot(d.x, d.z))
            add_box(bm, mid.x, mid.y, mid.z, LEG, LEG, d.length + 0.1, rx=rx, ry=ry)

    for z in (1.45, 2.1):
        c = FOOT + splay * (1 - z / PLAT_Z)
        L = 2 * c + 0.12
        add_box(bm, 0, c, z, L, 0.09, 0.09)
        add_box(bm, 0, -c, z, L, 0.09, 0.09)
        add_box(bm, c, 0, z, 0.09, L, 0.09)
        add_box(bm, -c, 0, z, 0.09, L, 0.09)

    for s in (-1, 1):
        add_box(bm, s * 1.02, 0, 2.94, 0.12, 2.24, 0.14, col="merge:platform")
        add_box(bm, 0, s * 1.02, 2.94, 2.24, 0.12, 0.14, col="merge:platform")

    plank_n = 7
    plank_w = 2.5 / plank_n
    for i in range(plank_n):
        y = -1.25 + plank_w * (i + 0.5)
        add_box(bm, 0, y, PLAT_Z + 0.04, 2.5, plank_w - 0.02, 0.08, col="merge:platform")

    roof_tilt = math.radians(6)
    for sx in (-1, 1):
        for sy in (-1, 1):
            top = ROOF_Z + 0.10 + sy * 1.15 * math.sin(roof_tilt)
            add_box(bm, sx * 1.15, sy * 1.15, (PLAT_Z + top) / 2, 0.11, 0.11, top - PLAT_Z)

    for i, z in enumerate((PLAT_Z + 0.35, PLAT_Z + 0.62, PLAT_Z + 0.9)):
        t = 0.10 if i == 2 else 0.07
        add_box(bm, 0, 1.15, z, 2.3, 0.05, t)
        add_box(bm, 1.15, 0, z, 0.05, 2.3, t)
        add_box(bm, -1.15, 0, z, 0.05, 2.3, t)
        add_box(bm, 0.775, -1.15, z, 0.75, 0.05, t)
        add_box(bm, -0.775, -1.15, z, 0.75, 0.05, t)

    add_box(bm, 0, 0, ROOF_Z + 0.10, 2.9, 3.0, 0.09, rx=roof_tilt)

    lx = 0.35
    y0, y1 = -2.0, -1.30
    rail_rx = -math.atan2(y1 - y0, PLAT_Z)
    rail_len = math.hypot(y1 - y0, PLAT_Z) + 0.35
    for s in (-1, 1):
        add_box(bm, s * lx, (y0 + y1) / 2, PLAT_Z / 2 + 0.08, 0.08, 0.10, rail_len, rx=rail_rx)
    n_rungs = 8
    for i in range(n_rungs):
        f = (i + 1) / (n_rungs + 1)
        add_box(bm, 0, y0 + f * (y1 - y0), f * PLAT_Z, 2 * lx + 0.06, 0.07, 0.06)

    mesh = bpy.data.meshes.new("hunting_stand")
    bm.to_mesh(mesh)
    bm.free()
    obj = bpy.data.objects.new("hunting_stand", mesh)
    bpy.context.collection.objects.link(obj)

    mat = bpy.data.materials.new("wood")
    mat.diffuse_color = WOOD_MID
    mesh.materials.append(mat)

    bpy.context.view_layer.objects.active = obj
    obj.select_set(True)
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.mesh.normals_make_consistent(inside=False)
    bpy.ops.object.mode_set(mode="OBJECT")
    return obj


def setup_preview():
    sun = bpy.data.objects.new("sun", bpy.data.lights.new("sun", "SUN"))
    sun.rotation_euler = (math.radians(50), 0, math.radians(30))
    bpy.context.collection.objects.link(sun)

    cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
    cam.location = (7.5, -8.5, 4.5)
    look = Vector((0, 0, 2.4)) - cam.location
    cam.rotation_euler = look.to_track_quat("-Z", "Y").to_euler()
    bpy.context.collection.objects.link(cam)
    bpy.context.scene.camera = cam

    sc = bpy.context.scene
    sc.render.engine = "BLENDER_WORKBENCH"
    sc.display.shading.light = "STUDIO"
    sc.display.shading.show_cavity = True
    sc.render.resolution_x = 900
    sc.render.resolution_y = 900
    sc.render.filepath = os.path.join(ROOT, "tools", "hunting_stand.png")


def export_header(obj, path):
    mesh = obj.data
    mesh.calc_loop_triangles()
    verts = []
    index_of = {}
    indices = []
    for tri in mesh.loop_triangles:
        for vi in tri.vertices:
            co = mesh.vertices[vi].co
            key = (round(co.x, 4), round(co.y, 4), round(co.z, 4))
            if key not in index_of:
                index_of[key] = len(verts)
                verts.append((co.x, co.z, -co.y))
            indices.append(index_of[key])

    xs = [v[0] for v in verts]
    ys = [v[1] for v in verts]
    zs = [v[2] for v in verts]
    lines = []
    lines.append("#pragma once")
    lines.append("")
    lines.append(f"inline constexpr int STAND_VERT_COUNT = {len(verts)};")
    lines.append(f"inline constexpr int STAND_IDX_COUNT  = {len(indices)};")
    lines.append(
        f"inline constexpr float STAND_MIN[3] = {{{min(xs):.4f}f, {min(ys):.4f}f, {min(zs):.4f}f}};"
    )
    lines.append(
        f"inline constexpr float STAND_MAX[3] = {{{max(xs):.4f}f, {max(ys):.4f}f, {max(zs):.4f}f}};"
    )
    lines.append("")
    lines.append("inline const float STAND_VERTS[] = {")
    for v in verts:
        lines.append(f"    {v[0]:.4f}f, {v[1]:.4f}f, {v[2]:.4f}f,")
    lines.append("};")
    lines.append("")
    lines.append("inline const unsigned STAND_IDX[] = {")
    for i in range(0, len(indices), 12):
        chunk = indices[i : i + 12]
        lines.append("    " + ", ".join(str(x) for x in chunk) + ",")
    lines.append("};")
    lines.append("")
    with open(path, "w") as f:
        f.write("\n".join(lines))
    print(f"wrote {path}: {len(verts)} verts, {len(indices) // 3} tris")


def export_collision(path):
    boxes = COL_BOXES + list(COL_MERGE.values())
    lines = ["#pragma once", ""]
    lines.append(f"inline constexpr int STAND_COL_COUNT = {len(boxes)};")
    lines.append("")
    lines.append("inline const float STAND_COL[][6] = {")
    for lo, hi in boxes:
        cx, cy, cz = (lo.x + hi.x) / 2, (lo.z + hi.z) / 2, -(lo.y + hi.y) / 2
        hx, hy, hz = (hi.x - lo.x) / 2, (hi.z - lo.z) / 2, (hi.y - lo.y) / 2
        lines.append(f"    {{{cx:.4f}f, {cy:.4f}f, {cz:.4f}f, {hx:.4f}f, {hy:.4f}f, {hz:.4f}f}},")
    lines.append("};")
    lines.append("")
    with open(path, "w") as f:
        f.write("\n".join(lines))
    print(f"wrote {path}: {len(boxes)} collision boxes")


clear_scene()
stand = build_stand()
check_connected()
setup_preview()
export_header(stand, os.path.join(ROOT, "src", "stand_mesh.h"))
export_collision(os.path.join(ROOT, "src", "stand_col.h"))
bpy.ops.render.render(write_still=True)
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(ROOT, "tools", "hunting_stand.blend"))
print("done")
