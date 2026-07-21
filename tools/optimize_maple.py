"""Blender background pipeline: source Maple OBJ -> three game-ready GLB LOD sets.

Run:
  python3 tools/prepare_maple_textures.py
  blender --background --factory-startup --python tools/optimize_maple.py
"""

from pathlib import Path
import random

import bmesh
import bpy
from mathutils import Vector

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "_RESOURCES/trees/Maple/OBJ.obj"
OUT = ROOT / "models/trees/maple"
TEX = OUT / "textures"
OUT.mkdir(parents=True, exist_ok=True)

VARIANTS = (
    ("Acer_X_freemanii_Freeman_Maple_Sapindaceae_Tree", "maple_a"),
    ("Acer_X_freemanii_Freeman_Maple_Sapindaceae_Tree03", "maple_b"),
    ("Acer_X_freemanii_Freeman_Maple_Sapindaceae_Version3_2", "maple_c"),
)
TRUNK_TARGETS = (20_000, 6_000, 1_200)
# Individual atlas leaves, not whole branch clusters. These counts still keep the
# instanced crown cheap, while avoiding a see-through silhouette at gameplay range.
LEAF_COUNTS = (5_000, 1_800, 500)
VOXEL_SIZES = (0.025, 0.060, 0.140)


def image_node(nodes, path: Path, non_color: bool = False):
    node = nodes.new("ShaderNodeTexImage")
    node.image = bpy.data.images.load(str(path), check_existing=True)
    if non_color:
        node.image.colorspace_settings.name = "Non-Color"
    return node


def make_material(name: str, leaf: bool):
    material = bpy.data.materials.new(name)
    material.use_nodes = True
    nodes = material.node_tree.nodes
    links = material.node_tree.links
    nodes.clear()
    output = nodes.new("ShaderNodeOutputMaterial")
    shader = nodes.new("ShaderNodeBsdfPrincipled")
    color = image_node(nodes, TEX / ("leaf_rgba.png" if leaf else "trunk.jpg"))
    normal = image_node(nodes, TEX / ("leaf_normal.jpg" if leaf else "trunk_normal.jpg"), True)
    rough = image_node(nodes, TEX / ("leaf_roughness.jpg" if leaf else "trunk_roughness.jpg"), True)
    normal_map = nodes.new("ShaderNodeNormalMap")
    normal_map.inputs["Strength"].default_value = 0.65 if leaf else 0.85
    links.new(color.outputs["Color"], shader.inputs["Base Color"])
    links.new(rough.outputs["Color"], shader.inputs["Roughness"])
    links.new(normal.outputs["Color"], normal_map.inputs["Color"])
    links.new(normal_map.outputs["Normal"], shader.inputs["Normal"])
    if leaf:
        links.new(color.outputs["Alpha"], shader.inputs["Alpha"])
        material.surface_render_method = "DITHERED"
        material.use_transparency_overlap = False
    links.new(shader.outputs["BSDF"], output.inputs["Surface"])
    return material


def keep_material(source, material_index: int, name: str, material):
    mesh = source.data.copy()
    mesh.name = name + "_mesh"
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(obj)
    bm = bmesh.new()
    bm.from_mesh(mesh)
    remove = [face for face in bm.faces if face.material_index != material_index]
    bmesh.ops.delete(bm, geom=remove, context="FACES")
    loose = [vert for vert in bm.verts if not vert.link_faces]
    if loose:
        bmesh.ops.delete(bm, geom=loose, context="VERTS")
    bm.to_mesh(mesh)
    bm.free()
    mesh.materials.clear()
    mesh.materials.append(material)
    for polygon in mesh.polygons:
        polygon.material_index = 0
    return obj


def normalize_mesh(obj, center_x: float, center_z: float, base_y: float):
    # OBJ vertex data remains source Y-up after import. Convert explicitly to
    # Blender Z-up; the glTF exporter then writes standard Y-up mesh attributes.
    for vertex in obj.data.vertices:
        source = vertex.co.copy()
        vertex.co.x = (source.x - center_x) * 0.001
        vertex.co.y = (source.z - center_z) * 0.001
        vertex.co.z = (source.y - base_y) * 0.001
    obj.data.update()


def weld_mesh(obj):
    # The 3ds Max OBJ duplicates vertices along nearly every face boundary. Welding
    # positions restores continuous branch topology while loop UVs retain their seams,
    # allowing quadric decimation to reduce the trunk instead of stalling.
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=0.00005)
    bm.to_mesh(obj.data)
    bm.free()
    obj.data.update()


def triangulate(obj):
    bpy.context.view_layer.objects.active = obj
    obj.select_set(True)
    modifier = obj.modifiers.new("triangulate", "TRIANGULATE")
    bpy.ops.object.modifier_apply(modifier=modifier.name)
    obj.select_set(False)


def voxel_remesh(obj, voxel_size: float):
    # Source branches contain large amounts of disconnected scan/export topology.
    # Voxel remeshing fuses it into a watertight tree skeleton that can actually be
    # simplified. Runtime bark is triplanar, so losing the source UVs is acceptable.
    bpy.context.view_layer.objects.active = obj
    obj.select_set(True)
    modifier = obj.modifiers.new(f"voxel_{voxel_size}", "REMESH")
    modifier.mode = "VOXEL"
    modifier.voxel_size = voxel_size
    modifier.use_remove_disconnected = False
    modifier.use_smooth_shade = True
    bpy.ops.object.modifier_apply(modifier=modifier.name)
    obj.select_set(False)


def decimate_to(obj, target: int):
    current = len(obj.data.polygons)
    if current <= target:
        return
    bpy.context.view_layer.objects.active = obj
    obj.select_set(True)
    modifier = obj.modifiers.new(f"decimate_{target}", "DECIMATE")
    modifier.decimate_type = "COLLAPSE"
    modifier.ratio = max(0.001, target / current)
    modifier.use_collapse_triangulate = True
    bpy.ops.object.modifier_apply(modifier=modifier.name)
    obj.select_set(False)


def leaf_samples(source, center_x: float, center_z: float, base_y: float):
    samples = []
    mesh = source.data
    for polygon in mesh.polygons:
        if polygon.material_index != 1 or len(polygon.vertices) < 3:
            continue
        center = polygon.center.copy()
        normal = polygon.normal.normalized()
        first = mesh.vertices[polygon.vertices[0]].co
        tangent = mesh.vertices[polygon.vertices[1]].co - first
        tangent -= normal * tangent.dot(normal)
        if tangent.length_squared < 1e-8:
            continue
        tangent.normalize()
        bitangent = normal.cross(tangent).normalized()
        radius = max((mesh.vertices[i].co - center).length for i in polygon.vertices)
        center = Vector(((center.x - center_x) * 0.001,
                         (center.z - center_z) * 0.001,
                         (center.y - base_y) * 0.001))
        tangent = Vector((tangent.x, tangent.z, tangent.y)).normalized()
        bitangent = Vector((bitangent.x, bitangent.z, bitangent.y)).normalized()
        radius = min(0.25, max(0.085, radius * 0.001 * 1.08))
        samples.append((center, tangent, bitangent, radius))
    random.Random(0xAC3E).shuffle(samples)
    return samples


def make_leaf_cards(name: str, samples, count: int, material):
    vertices, faces, uvs = [], [], []
    for index, (center, tangent, bitangent, radius) in enumerate(samples[:count]):
        # Cycle through the source's 3x3 individual-leaf atlas.
        tile = index % 9
        col, row = tile % 3, tile // 3
        inset = 0.012
        u0, u1 = (col + inset) / 3, (col + 1 - inset) / 3
        v0, v1 = (2 - row + inset) / 3, (3 - row - inset) / 3
        base = len(vertices)
        vertices.extend((
            center - tangent * radius - bitangent * radius,
            center + tangent * radius - bitangent * radius,
            center + tangent * radius + bitangent * radius,
            center - tangent * radius + bitangent * radius,
        ))
        faces.extend(((base, base + 1, base + 2), (base, base + 2, base + 3)))
        uvs.extend(((u0, v0), (u1, v0), (u1, v1), (u0, v1)))
    mesh = bpy.data.meshes.new(name + "_mesh")
    mesh.from_pydata(vertices, [], faces)
    mesh.materials.append(material)
    uv_layer = mesh.uv_layers.new(name="UVMap")
    for polygon in mesh.polygons:
        for loop_index in polygon.loop_indices:
            vertex_index = mesh.loops[loop_index].vertex_index
            uv_layer.data[loop_index].uv = uvs[vertex_index]
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(obj)
    return obj


def export_objects(path: Path, objects):
    bpy.ops.object.select_all(action="DESELECT")
    for obj in objects:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = objects[0]
    bpy.ops.export_scene.gltf(
        filepath=str(path),
        export_format="GLB",
        use_selection=True,
        export_apply=True,
        export_texcoords=True,
        export_normals=True,
        export_materials="EXPORT",
    )


def main():
    if not (TEX / "leaf_rgba.png").exists():
        raise RuntimeError("Run tools/prepare_maple_textures.py first")
    bpy.ops.wm.obj_import(filepath=str(SOURCE))
    trunk_material = make_material("Maple_Bark", False)
    leaf_material = make_material("Maple_Leaves", True)

    objects = {obj.name: obj for obj in bpy.context.scene.objects if obj.type == "MESH"}
    for source_name, output_name in VARIANTS:
        source = objects[source_name]
        xs = [v.co.x for v in source.data.vertices]
        ys = [v.co.y for v in source.data.vertices]
        zs = [v.co.z for v in source.data.vertices]
        center_x = (min(xs) + max(xs)) * 0.5
        center_z = (min(zs) + max(zs)) * 0.5
        base_y = min(ys)
        samples = leaf_samples(source, center_x, center_z, base_y)

        export_set = []
        for lod, (trunk_target, leaf_count, voxel_size) in enumerate(
                zip(TRUNK_TARGETS, LEAF_COUNTS, VOXEL_SIZES)):
            trunk = keep_material(source, 0, f"{output_name}_trunk_lod{lod}",
                                  trunk_material)
            normalize_mesh(trunk, center_x, center_z, base_y)
            weld_mesh(trunk)
            voxel_remesh(trunk, voxel_size)
            triangulate(trunk)
            decimate_to(trunk, trunk_target)
            leaves = make_leaf_cards(f"{output_name}_leaves_lod{lod}",
                                     samples, leaf_count, leaf_material)
            export_set.extend((trunk, leaves))
            print(f"{output_name} lod{lod}: trunk={len(trunk.data.polygons)} "
                  f"leaves={leaf_count * 2}")
        output = OUT / f"{output_name}.glb"
        export_objects(output, export_set)
        for obj in export_set:
            bpy.data.objects.remove(obj, do_unlink=True)
        for stale in OUT.glob(f"{output_name}_lod*.glb"):
            stale.unlink()
        print(f"{output.name}: three LODs, shared embedded textures")

    print(f"Optimized Maple LODs written to {OUT}")


main()
