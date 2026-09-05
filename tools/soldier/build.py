"""blender --background --python tools/soldier/build.py
Exports the runtime headers and an editable assembled Blender scene + studio previews.
"""
import bpy,sys,os,math
sys.path.insert(0,os.path.dirname(os.path.abspath(__file__)))
import mesh_tools as t
import soldier,uzi
from mathutils import Matrix,Vector
bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
soldier.build();uzi.build()
parts=['head','torso','pelvis','neck','arm','hand','leg','foot','forearm','shin']
t.export(parts,os.path.join(t.ROOT,'src/player_mesh.h'),'PMESH',True)
t.export(['uzi'],os.path.join(t.ROOT,'src/uzi_mesh.h'),'WMESH')
t.export_lods(parts)
# Authored components are preserved separately; assembled duplicates instance their data.
source=bpy.data.collections.new('SOURCE - normalized parts and Uzi meters')
bpy.context.scene.collection.children.link(source)
for objects in t.GROUPS.values():
    for ob in objects:
        for c in list(ob.users_collection):c.objects.unlink(ob)
        source.objects.link(ob)
source.hide_render=True;source.hide_viewport=True
assembled=bpy.data.collections.new('Soldier - assembled preview')
bpy.context.scene.collection.children.link(assembled)
C=Matrix(((1,0,0,0),(0,0,-1,0),(0,1,0,0),(0,0,0,1)))

def instance(group,M,label):
    for ob in t.GROUPS[group]:
        dup=ob.copy();dup.data=ob.data;dup.name=label+' / '+ob.name
        assembled.objects.link(dup);dup.matrix_world=C@M@C.inverted()

def fixed(group,p,s):instance(group,Matrix.Translation(p)@Matrix.Diagonal((*s,1)),group)
def segment(group,a,b,w):
    a=Vector(a);b=Vector(b);d=(b-a).normalized()
    u=Vector((0,1,0)) if abs(d.y)<.99 else Vector((1,0,0))
    x=u.cross(d).normalized();z=x.cross(d)
    M=Matrix.Identity(4)
    M.col[0]=(* (x*w),0);M.col[1]=(*(b-a),0);M.col[2]=(*(z*w),0);M.col[3]=(*(a+b)*.5,1)
    instance(group,M,group)
fixed('pelvis',(0,.95,0),(.36,.18,.22));fixed('torso',(0,1.26,0),(.34,.44,.22))
fixed('neck',(0,1.53,0),(.12,.10,.12));fixed('head',(0,1.68,0),(.24,.24,.24))
# Ready-low pose for the Blender presentation (runtime uses shared procedural IK).
for x,elbow,hand in [(-.27,(-.30,1.18,.09),(-.05,1.25,.27)),(.27,(.30,1.14,.16),(.05,1.29,.43))]:
    segment('arm',(x,1.45,0),elbow,.12);segment('forearm',elbow,hand,.11)
    fixed('hand',hand,(.085,.095,.105))
for x in [-.1,.1]:
    segment('leg',(x,.86,0),(x,.43,0),.20);segment('shin',(x,.43,0),(x,.03,0),.18)
    segment('foot',(x,.04,-.01),(x,.04,.15),.12)
instance('uzi',Matrix.Translation((-.015,1.34,.29)),'Uzi held')
# Extra display Uzi for close-up inspection, in its own collection.
weapon=bpy.data.collections.new('Uzi - product view');bpy.context.scene.collection.children.link(weapon)
for ob in t.GROUPS['uzi']:
    dup=ob.copy();dup.data=ob.data;weapon.objects.link(dup)
weapon.hide_render=True;weapon.hide_viewport=True
# Neutral studio: no downloaded HDRI or textures.
scene=bpy.context.scene;scene.render.engine='CYCLES';scene.cycles.samples=32
scene.cycles.use_denoising=True
scene.world.color=(.07,.07,.07)
bpy.context.preferences.filepaths.save_version=0
bpy.ops.mesh.primitive_plane_add(size=200,location=(0,0,-.028));floor=bpy.context.object;floor.name='Studio floor'
m=bpy.data.materials.new('Studio charcoal');m.diffuse_color=(.065,.073,.079,1);floor.data.materials.append(m)
def area(name,p,power,size,color):
    bpy.ops.object.light_add(type='AREA',location=p);o=bpy.context.object;o.name=name
    o.data.energy=power;o.data.shape='DISK';o.data.size=size;o.data.color=color
    o.rotation_euler=(Vector((0,0,.9))-o.location).to_track_quat('-Z','Y').to_euler()
area('Key softbox',(2,-3,3.7),450,3,(1,.94,.85));area('Fill',(-2,-1,1.7),180,2,(.78,.88,1));area('Rim',(1,2,2.9),550,2,(.83,.91,1))
bpy.ops.object.camera_add();cam=bpy.context.object;scene.camera=cam
cam.location=(2.25,-3.5,1.95);cam.rotation_euler=(Vector((0,0,.94))-cam.location).to_track_quat('-Z','Y').to_euler()
cam.data.type='ORTHO';cam.data.ortho_scale=2.1
scene.render.resolution_x=1000;scene.render.resolution_y=1200;scene.render.resolution_percentage=100
scene.view_settings.view_transform='AgX'
scene.view_settings.exposure=-1.0
scene.render.image_settings.file_format='PNG'
scene.render.filepath=os.path.join(t.ROOT,'tools/soldier/soldier_preview.png')
for screen in bpy.data.screens:
    for area in screen.areas:
        if area.type == 'VIEW_3D':
            area.spaces.active.region_3d.view_perspective = 'CAMERA'
            area.spaces.active.shading.color_type = 'MATERIAL'
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(t.ROOT,'tools/soldier/soldier_uzi.blend'))
bpy.ops.render.render(write_still=True)
assembled.hide_render=True;weapon.hide_render=False
floor.hide_render=True
cam.location=(.67,-.83,.47);target=Vector((0,-.045,-.045))
cam.rotation_euler=(target-cam.location).to_track_quat('-Z','Y').to_euler();cam.data.ortho_scale=.69
scene.render.resolution_x=1500;scene.render.resolution_y=1000
scene.render.filepath=os.path.join(t.ROOT,'tools/soldier/uzi_preview.png')
bpy.ops.render.render(write_still=True)
