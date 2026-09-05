"""Small Blender authoring/export helpers. All authoring coordinates are game Y-up.
Generated vertices: position.xyz, normal.xyz, albedo.rgb, specular strength.
No external assets or add-ons. Geometry stays editable in the .blend file.
"""
import bpy, bmesh, math, os
from mathutils import Vector, Matrix, noise
from collections import defaultdict

GROUPS = defaultdict(list)
MATS = {}
GROUP = 'head'
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

def bl(v): return (v[0], -v[2], v[1])
def game(v): return (v[0], v[2], -v[1])

def material(name, rgb, spec=.04, camo=False):
    m = bpy.data.materials.new(name)
    m.diffuse_color = (*rgb, 1)
    m['specular'] = -spec if camo else spec
    m['camo'] = camo
    m.use_nodes = True
    p = m.node_tree.nodes.get('Principled BSDF')
    p.inputs['Base Color'].default_value = (*rgb, 1)
    p.inputs['Roughness'].default_value = .85 if spec < .1 else .38
    p.inputs['Metallic'].default_value = .7 if spec > .15 else 0
    att = m.node_tree.nodes.new('ShaderNodeVertexColor')
    att.layer_name = 'Albedo'
    m.node_tree.links.new(att.outputs['Color'], p.inputs['Base Color'])
    MATS[name] = m
    return name

def mesh(name, vertices, faces, mat, smooth=False, bevel=0):
    me = bpy.data.meshes.new(name)
    me.from_pydata([bl(v) for v in vertices], [], faces)
    me.update()
    ob = bpy.data.objects.new(name, me)
    bpy.context.collection.objects.link(ob)
    me.materials.append(MATS[mat])
    # Consistent outward winding, including custom lofts.
    bm = bmesh.new(); bm.from_mesh(me)
    bmesh.ops.recalc_face_normals(bm, faces=list(bm.faces))
    bm.to_mesh(me); bm.free()
    for p in me.polygons: p.use_smooth = smooth
    if bevel:
        bpy.context.view_layer.objects.active = ob
        mod = ob.modifiers.new('Soft manufactured edges', 'BEVEL')
        mod.width = bevel; mod.segments = 2
        bpy.ops.object.modifier_apply(modifier=mod.name)
        # Flat major faces, smooth small bevel strips.
        for p in me.polygons: p.use_smooth = False
    ob['asset_group'] = GROUP
    GROUPS[GROUP].append(ob)
    return ob

def box(name, c, s, mat, bevel=.015):
    vs = [(c[0]+x*s[0]/2,c[1]+y*s[1]/2,c[2]+z*s[2]/2)
          for x,y,z in [(-1,-1,-1),(1,-1,-1),(1,1,-1),(-1,1,-1),
                        (-1,-1,1),(1,-1,1),(1,1,1),(-1,1,1)]]
    return mesh(name,vs,[(0,3,2,1),(4,5,6,7),(0,1,5,4),(3,7,6,2),
                         (0,4,7,3),(1,2,6,5)],mat,bevel=bevel)

def ellipsoid(name,c,s,mat,rings=12,segs=20):
    vs=[]
    for j in range(rings+1):
        a=math.pi*j/rings
        for i in range(segs):
            t=2*math.pi*i/segs
            vs.append((c[0]+s[0]*math.sin(a)*math.cos(t)/2,
                       c[1]+s[1]*math.cos(a)/2,
                       c[2]+s[2]*math.sin(a)*math.sin(t)/2))
    faces=[]
    for j in range(rings):
        for i in range(segs):
            a=j*segs+i; b=j*segs+(i+1)%segs
            if j==0: faces.append((a,b+segs,a+segs))
            elif j==rings-1: faces.append((a,b,b+segs))
            else: faces.append((a,b,b+segs,a+segs))
    return mesh(name,vs,faces,mat,True)

def tube(name,a,b,r,mat,segs=16,r2=None):
    a=Vector(a); b=Vector(b); d=(b-a).normalized()
    u=d.cross(Vector((0,1,0)) if abs(d.y)<.9 else Vector((1,0,0))).normalized()
    v=d.cross(u)
    vs=[]
    for p,rad in [(a,r),(b,r if r2 is None else r2)]:
        for i in range(segs):
            t=2*math.pi*i/segs; vs.append(p+rad*(u*math.cos(t)+v*math.sin(t)))
    faces=[tuple(range(segs-1,-1,-1)),tuple(range(segs,2*segs))]
    faces += [(i,(i+1)%segs,(i+1)%segs+segs,i+segs) for i in range(segs)]
    ob=mesh(name,vs,faces,mat,True)
    ob.data.polygons[0].use_smooth=False; ob.data.polygons[1].use_smooth=False
    return ob

def path(name, points,r,mat,segs=10):
    for i in range(len(points)-1): tube(name+str(i),points[i],points[i+1],r,mat,segs)

def ring(name,c,outer,inner,depth,mat,segs=32):
    # Open ring facing +Z; an actual hole, not a painted black disk.
    vs=[]
    for z,r in [(-depth/2,outer),(depth/2,outer),(-depth/2,inner),(depth/2,inner)]:
        for i in range(segs):
            t=i*2*math.pi/segs; vs.append((c[0]+r*math.cos(t),c[1]+r*math.sin(t),c[2]+z))
    fs=[]
    for a,b in [(0,1),(1,3),(3,2),(2,0)]:
        for i in range(segs):
            j=(i+1)%segs; fs.append((a*segs+i,a*segs+j,b*segs+j,b*segs+i))
    return mesh(name,vs,fs,mat,True)

def cloth(name, profiles, mat, segs=20):
    # Profiles: y, width-radius, depth-radius, forward offset. Small folds in silhouette.
    vs=[]
    for j,(y,rx,rz,z) in enumerate(profiles):
        for i in range(segs):
            a=2*math.pi*i/segs
            fold=1+.028*math.sin(i*3+j*2.1)+.019*math.sin(j*4-i)
            vs.append((rx*math.cos(a)*fold,y,z+rz*math.sin(a)*fold))
    fs=[tuple(range(segs-1,-1,-1))]
    for j in range(len(profiles)-1):
        for i in range(segs):
            a=j*segs+i;b=j*segs+(i+1)%segs;fs.append((a,b,b+segs,a+segs))
    fs.append(tuple(range((len(profiles)-1)*segs,len(profiles)*segs)))
    return mesh(name,vs,fs,mat,True)

def color_at(p,m):
    rgb=tuple(m.diffuse_color[:3])
    if m['camo']:
        q=Vector(p)*7.5
        n=noise.noise(q)+.3*noise.noise(q*2.7)
        rgb=(.095,.115,.071) if n<-.10 else ((.23,.215,.14) if n>.10 else rgb)
    # Subtle baked irregularity, stable in local coordinates through animation.
    v=.96+.06*noise.noise_vector(Vector(p)*53)[0]
    return tuple(max(0,min(1,c*v)) for c in rgb)

def prepare_colors(ob):
    me=ob.data; m=me.materials[0]
    att=me.color_attributes.new(name='Albedo',type='FLOAT_COLOR',domain='CORNER')
    for loop in me.loops:
        p=game(me.vertices[loop.vertex_index].co)
        att.data[loop.index].color=(*color_at(p,m),1)

def export(groups,path,prefix,parts=False):
    arrays=[]
    with open(path,'w') as f:
        f.write('// Generated by tools/soldier/build.py. Edit the Blender source, not this file.\n#pragma once\n')
        for group in groups:
            verts=[]; idx=[]; seen={}
            for ob in GROUPS[group]:
                if 'Albedo' not in ob.data.color_attributes: prepare_colors(ob)
                me=ob.data; me.calc_loop_triangles(); colors=me.color_attributes['Albedo']
                for tri in me.loop_triangles:
                    for li in tri.loops:
                        loop=me.loops[li]; co=game(ob.matrix_world @ me.vertices[loop.vertex_index].co)
                        normal=game((ob.matrix_world.to_3x3().inverted().transposed() @ me.corner_normals[li].vector).normalized())
                        c=colors.data[li].color[:3]
                        key=tuple(round(v,6) for v in (*co,*normal,*c,me.materials[0]['specular']))
                        if key not in seen: seen[key]=len(verts);verts.append(key)
                        idx.append(seen[key])
            ident=prefix+'_'+group.upper()
            f.write('inline const float '+ident+'_VERTS[] = {\n')
            for v in verts: f.write('    '+','.join(f'{x:.6f}f' for x in v)+',\n')
            f.write('};\ninline const unsigned '+ident+'_IDX[] = {\n')
            for i in range(0,len(idx),24): f.write('    '+','.join(map(str,idx[i:i+24]))+',\n')
            f.write('};\n')
            arrays.append((ident,len(verts)*10,len(idx)))
            print('ASSET',group,'vertices',len(verts),'triangles',len(idx)//3,flush=True)
        if parts:
            if parts is True:
                f.write('struct PMeshPart { const float* verts; int floatCount; const unsigned* idx; int idxCount; };\n')
            else:
                f.write('#include "player_mesh.h"\n')
            table = 'PMESH_PARTS' if parts is True else parts
            f.write('inline const PMeshPart '+table+'[] = {\n')
            for ident,nv,ni in arrays:f.write(f'    {{{ident}_VERTS,{nv},{ident}_IDX,{ni}}},\n')
            f.write('};\n')

def export_lods(parts):
    """Offline reduction only: keep full geometry for close inspection/first person."""
    originals={g:list(GROUPS[g]) for g in parts+['uzi']}
    try:
        for g,objects in originals.items():
            GROUPS[g]=[]
            for ob in objects:
                dup=ob.copy();dup.data=ob.data.copy()
                bpy.context.scene.collection.objects.link(dup)
                bpy.context.view_layer.objects.active=dup
                mod=dup.modifiers.new('Distance reduction','DECIMATE')
                mod.ratio=.25
                bpy.ops.object.modifier_apply(modifier=mod.name)
                GROUPS[g].append(dup)
        export(parts,os.path.join(ROOT,'src/player_lod_mesh.h'),'PLOD','PMESH_LOD_PARTS')
        export(['uzi'],os.path.join(ROOT,'src/uzi_lod_mesh.h'),'WLOD')
    finally:
        for g,objects in originals.items():
            for ob in GROUPS[g]:
                me=ob.data;bpy.data.objects.remove(ob,do_unlink=True);bpy.data.meshes.remove(me)
            GROUPS[g]=objects
