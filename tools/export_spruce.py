"""Blender: export the supplied spruce to instanced mesh buffers and an atlas."""
import array
import json
import hashlib
import struct
import sys
from pathlib import Path
import bpy
from mathutils import Vector

ROOT=Path(__file__).resolve().parents[1]
variant=sys.argv[sys.argv.index('--')+1] if '--' in sys.argv else '1843'
if variant not in ('1843','243'): raise ValueError('Expected spruce 1843 or 243')
OUT=ROOT/('models/textures/spruce243' if variant=='243' else 'models/textures/spruce')
OUT.mkdir(parents=True,exist_ok=True)
bpy.ops.import_scene.gltf(filepath=str(ROOT/f'_RESOURCES/spruce/realistic_hd_norway_spruce_{variant}.glb'))
objects=[o for o in bpy.context.scene.objects if o.type=='MESH' and o.name.startswith('Picea')]
materials=[]
for o in objects:
    for m in o.data.materials:
        if m not in materials: materials.append(m)
size=2048
cell=size//4
pixels=array.array('f',[0])*(size*size*4)
for k,m in enumerate(materials):
    bsdf=next(n for n in m.node_tree.nodes if n.type=='BSDF_PRINCIPLED')
    image=bsdf.inputs['Base Color'].links[0].from_node.image.copy()
    image.scale(cell-8,cell-8)
    data=array.array('f',[0])*len(image.pixels)
    image.pixels.foreach_get(data)
    # Extend each cell's border to protect its mip footprint.
    for y in range(cell):
        sy=max(0,min(cell-9,y-4))
        row=array.array('f')
        for x in range(cell):
            sx=max(0,min(cell-9,x-4)); off=(sy*(cell-8)+sx)*4
            row.extend(data[off:off+4])
        start=((k//4*cell+y)*size+k%4*cell)*4
        pixels[start:start+cell*4]=row
atlas=bpy.data.images.new('spruce_atlas',width=size,height=size,alpha=True)
atlas.pixels.foreach_set(pixels)
atlas.filepath_raw=str(OUT/'atlas.png'); atlas.file_format='PNG'; atlas.save()
points=[o.matrix_world@Vector(c) for o in objects for c in o.bound_box]
bottom=min(p.z for p in points); height=max(p.z for p in points)-bottom
cx=(min(p.x for p in points)+max(p.x for p in points))*.5
cy=(min(p.y for p in points)+max(p.y for p in points))*.5
counts=[]
crowns=[]
for lod,ratio in enumerate((.45,.10)):
    vertices=array.array('f'); indices=array.array('I')
    crown=hashlib.sha256()
    for original in objects:
        o=original.copy(); o.data=original.data.copy(); bpy.context.collection.objects.link(o)
        bpy.context.view_layer.objects.active=o
        foliage=any(m.name.startswith('Picea_fol') for m in o.data.materials)
        first=len(vertices)
        # Keep the near crown in the middle LOD; collapsing needle cards opens holes.
        mod=o.modifiers.new('game_lod','DECIMATE'); mod.ratio=.45 if foliage else ratio
        bpy.ops.object.modifier_apply(modifier=mod.name)
        mesh=o.data; mesh.calc_loop_triangles()
        uv=mesh.uv_layers.active.data
        normal=o.matrix_world.to_3x3().inverted().transposed()
        for tri in mesh.loop_triangles:
            material=materials.index(mesh.materials[tri.material_index])
            for li in tri.loops:
                vert=mesh.vertices[mesh.loops[li].vertex_index]
                p=o.matrix_world@vert.co; n=(normal@vert.normal).normalized(); t=uv[li].uv
                u=(material%4*cell+4+max(0,min(1,t.x))*(cell-8))/size
                v=(material//4*cell+4+max(0,min(1,t.y))*(cell-8))/size
                indices.append(len(vertices)//12)
                vertices.extend(((p.x-cx)/height,(p.z-bottom)/height,-(p.y-cy)/height,
                                 n.x,n.z,-n.y,1,1,1,.02,u,v))
        if foliage: crown.update(vertices[first:].tobytes())
        bpy.data.objects.remove(o,do_unlink=True)
    with (OUT/f'lod{lod}.bin').open('wb') as f:
        f.write(struct.pack('<II',len(vertices),len(indices))); vertices.tofile(f); indices.tofile(f)
    counts.append(len(indices)//3)
    crowns.append(crown.hexdigest())
assert crowns[0]==crowns[1], 'Middle LOD changed the needle-card geometry'
width=max(max(p.x for p in points)-min(p.x for p in points),max(p.y for p in points)-min(p.y for p in points))/height
(OUT/'info.json').write_text(json.dumps(dict(height=height,width=width,triangles=counts),indent=2))
print('SPRUCE',height,width,counts)
