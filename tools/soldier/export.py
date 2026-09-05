"""Export edited .blend SOURCE components without regenerating their geometry.
blender tools/soldier/soldier_uzi.blend --background --python tools/soldier/export.py
Edit meshes/materials in the SOURCE collection; preview copies share mesh data.
"""
import bpy,os,sys
sys.path.insert(0,os.path.dirname(os.path.abspath(__file__)))
import mesh_tools as t
source=bpy.data.collections.get('SOURCE - normalized parts and Uzi meters')
if source is None: raise RuntimeError('Open soldier_uzi.blend before exporting')
for ob in source.objects:
    if ob.type=='MESH' and 'asset_group' in ob:
        if ob.matrix_world.to_3x3().determinant() <= 0:
            raise RuntimeError('Apply mirrored transforms and recalculate normals: '+ob.name)
        t.GROUPS[ob['asset_group']].append(ob)
parts=['head','torso','pelvis','neck','arm','hand','leg','foot','forearm','shin']
if any(not t.GROUPS[g] for g in parts+['uzi']): raise RuntimeError('Missing required asset part')
t.export(parts,os.path.join(t.ROOT,'src/player_mesh.h'),'PMESH',True)
t.export(['uzi'],os.path.join(t.ROOT,'src/uzi_mesh.h'),'WMESH')
t.export_lods(parts)
