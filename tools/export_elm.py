"""Blender background export of the supplied seven-model elm collection."""
import array
import hashlib
import json
import re
import struct
from pathlib import Path
import bpy
from mathutils import Vector

ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/'_RESOURCES/elmtree/high_detail_elm_tree_collection.glb'
OUT=ROOT/'models/textures/elm'
OUT.mkdir(parents=True,exist_ok=True)
existing=set(bpy.data.objects)
bpy.ops.import_scene.gltf(filepath=str(SOURCE))
objects=[o for o in bpy.context.scene.objects if o.type=='MESH' and o not in existing]
groups={}
for o in objects:
    parent=o.parent
    while parent and not re.search(r'\.(\d{3})_',parent.name): parent=parent.parent
    if not parent: raise ValueError(f'Unidentified collection member: {o.name}')
    key=re.search(r'\.(\d{3})_',parent.name).group(1)
    groups.setdefault(key,[]).append(o)
assert len(groups)==7
materials=sorted({m for o in objects for m in o.data.materials},key=lambda m:m.name)
assert len(materials)==2 and materials[0].name.startswith('bark')
width,height=4096,2048
cell=width//2
pixels=array.array('f',[0])*(width*height*4)
for k,m in enumerate(materials):
    bsdf=next(n for n in m.node_tree.nodes if n.type=='BSDF_PRINCIPLED')
    image=bsdf.inputs['Base Color'].links[0].from_node.image.copy()
    image.scale(cell-8,height-8)
    data=array.array('f',[0])*len(image.pixels); image.pixels.foreach_get(data)
    for y in range(height):
        sy=max(0,min(height-9,y-4)); row=array.array('f')
        for x in range(cell):
            sx=max(0,min(cell-9,x-4)); at=(sy*(cell-8)+sx)*4
            row.extend(data[at:at+4])
        at=(y*width+k*cell)*4; pixels[at:at+cell*4]=row
atlas=bpy.data.images.new('elm_atlas',width=width,height=height,alpha=True)
atlas.pixels.foreach_set(pixels); atlas.filepath_raw=str(OUT/'atlas.png')
atlas.file_format='PNG'; atlas.save()
metadata=[]
for key,parts in sorted(groups.items()):
    directory=ROOT/f'models/textures/elm_{key}'; directory.mkdir(parents=True,exist_ok=True)
    points=[o.matrix_world@Vector(c) for o in parts for c in o.bound_box]
    bottom=min(p.z for p in points); treeHeight=max(p.z for p in points)-bottom
    bark=[o.matrix_world@v.co for o in parts for v in o.data.vertices
          if o.data.materials[0].name.startswith('bark')]
    roots=[p for p in bark if p.z<bottom+treeHeight*.03]
    assert roots and treeHeight>0
    cx=sum(p.x for p in roots)/len(roots); cy=sum(p.y for p in roots)/len(roots)
    treeWidth=2*max(max(abs(p.x-cx),abs(p.y-cy)) for p in points)/treeHeight
    counts=[]; crowns=[]
    for lod,ratio in enumerate((.45,.10)):
        vertices=array.array('f'); indices=array.array('I'); crown=hashlib.sha256()
        for original in parts:
            o=original.copy(); o.data=original.data.copy(); bpy.context.collection.objects.link(o)
            bpy.context.view_layer.objects.active=o
            foliage=o.data.materials[0].name.startswith('foliage')
            if not foliage:
                mod=o.modifiers.new('wood_lod','DECIMATE'); mod.ratio=ratio
                bpy.ops.object.modifier_apply(modifier=mod.name)
            mesh=o.data; mesh.calc_loop_triangles(); uv=mesh.uv_layers[0].data
            normal=o.matrix_world.to_3x3().inverted().transposed(); first=len(vertices)
            for tri in mesh.loop_triangles:
                material=materials.index(mesh.materials[tri.material_index])
                for li in tri.loops:
                    vert=mesh.vertices[mesh.loops[li].vertex_index]
                    p=o.matrix_world@vert.co; n=(normal@vert.normal).normalized(); t=uv[li].uv
                    u=(material*cell+4+max(0,min(1,t.x))*(cell-8))/width
                    v=(4+max(0,min(1,t.y))*(height-8))/height
                    indices.append(len(vertices)//12)
                    vertices.extend(((p.x-cx)/treeHeight,(p.z-bottom)/treeHeight,-(p.y-cy)/treeHeight,
                                     n.x,n.z,-n.y,1,1,1,.02,u,v))
            if foliage: crown.update(vertices[first:].tobytes())
            bpy.data.objects.remove(o,do_unlink=True)
        with (directory/f'lod{lod}.bin').open('wb') as f:
            f.write(struct.pack('<II',len(vertices),len(indices))); vertices.tofile(f); indices.tofile(f)
        counts.append(len(indices)//3); crowns.append(crown.hexdigest())
    assert crowns[0]==crowns[1] and crowns[0]!=hashlib.sha256().hexdigest()
    info=dict(variant=key,height=treeHeight,width=treeWidth,triangles=counts)
    (directory/'info.json').write_text(json.dumps(info,indent=2)); metadata.append(info)
    print('ELM',info,flush=True)
(OUT/'collection.json').write_text(json.dumps(metadata,indent=2))
