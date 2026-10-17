"""Small Blender authoring helpers. Source units are meters, exports are centimeters."""
import bpy
import math
from pathlib import Path
from mathutils import Vector

ROOT=Path(__file__).resolve().parents[2]
SOURCE=ROOT/'AssetsSource/CityCourier'
EXPORT=SOURCE/'Exports'
EXPORT.mkdir(parents=True,exist_ok=True)
MATERIALS={}

def reset():
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete(use_global=False)
    bpy.context.scene.unit_settings.system='METRIC'
    bpy.context.scene.unit_settings.scale_length=1.0

def material(name,color,rough=.65,metal=0,scan=None,emission=0):
    if name in MATERIALS:return MATERIALS[name]
    m=bpy.data.materials.get(name) or bpy.data.materials.new(name)
    m.use_nodes=True; p=m.node_tree.nodes.get('Principled BSDF')
    p.inputs['Base Color'].default_value=(*color,1)
    p.inputs['Roughness'].default_value=rough;p.inputs['Metallic'].default_value=metal
    m.diffuse_color=(*color,1)
    if emission:
        p.inputs['Emission Color'].default_value=(*color,1);p.inputs['Emission Strength'].default_value=emission
    if scan:
        for suffix,input_name in [('diff','Base Color'),('rough','Roughness'),('nor_gl','Normal')]:
            path=SOURCE/'External/PolyHaven'/f'{scan}_{suffix}_2k.jpg'
            if not path.exists():raise RuntimeError(f'Missing scanned material: {path}')
            node=m.node_tree.nodes.new('ShaderNodeTexImage');node.image=bpy.data.images.load(str(path),check_existing=True)
            if suffix!='diff':node.image.colorspace_settings.name='Non-Color'
            if suffix=='nor_gl':
                normal=m.node_tree.nodes.new('ShaderNodeNormalMap');normal.inputs['Strength'].default_value=.65
                m.node_tree.links.new(node.outputs['Color'],normal.inputs['Color']);m.node_tree.links.new(normal.outputs['Normal'],p.inputs[input_name])
            else:m.node_tree.links.new(node.outputs['Color'],p.inputs[input_name])
    MATERIALS[name]=m
    return m

def uv_box(obj,scale=2):
    mesh=obj.data; uv=mesh.uv_layers.new(name='UVMap') if not mesh.uv_layers else mesh.uv_layers.active
    for face in mesh.polygons:
        axis=max(range(3),key=lambda i:abs(face.normal[i]))
        indices=([1,2],[0,2],[0,1])[axis]
        for li in face.loop_indices:
            co=mesh.vertices[mesh.loops[li].vertex_index].co
            uv.data[li].uv=(co[indices[0]]/scale,co[indices[1]]/scale)

def finish(obj,name,mat,bevel=0,smooth=False):
    obj.name=name
    if mat:obj.data.materials.append(mat)
    bpy.context.view_layer.objects.active=obj
    bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
    if obj.type=='MESH':
        uv_box(obj)
        if bevel:
            mod=obj.modifiers.new('Manufactured edge bevel','BEVEL');mod.width=bevel;mod.segments=2
            bpy.ops.object.modifier_apply(modifier=mod.name)
        for p in obj.data.polygons:p.use_smooth=smooth
        if bevel and not smooth:
            mod=obj.modifiers.new('Weighted normals','WEIGHTED_NORMAL');mod.keep_sharp=True
            bpy.ops.object.modifier_apply(modifier=mod.name)
    return obj

def box(name,loc,size,mat,bevel=.015):
    bpy.ops.mesh.primitive_cube_add(size=1,location=loc)
    o=bpy.context.object;o.scale=size
    return finish(o,name,mat,bevel)

def sphere(name,loc,size,mat):
    bpy.ops.mesh.primitive_uv_sphere_add(segments=24,ring_count=12,radius=1,location=loc)
    o=bpy.context.object;o.scale=size
    return finish(o,name,mat,smooth=True)

def cylinder(name,a,b,r,mat,vertices=20):
    a,b=Vector(a),Vector(b);d=b-a
    bpy.ops.mesh.primitive_cylinder_add(vertices=vertices,radius=r,depth=d.length,location=(a+b)/2)
    o=bpy.context.object;o.rotation_euler=d.to_track_quat('Z','Y').to_euler()
    return finish(o,name,mat,.003,True)

def torus(name,loc,major,minor,mat,rotation=(math.pi/2,0,0)):
    bpy.ops.mesh.primitive_torus_add(major_segments=40,minor_segments=12,location=loc,major_radius=major,minor_radius=minor,rotation=rotation)
    return finish(bpy.context.object,name,mat,smooth=True)

def text_mesh(name,body,loc,size,mat,rotation=(math.pi/2,0,0)):
    bpy.ops.object.select_all(action='DESELECT')
    c=bpy.data.curves.new(name,'FONT');c.body=body;c.align_x='CENTER';c.size=size;c.extrude=.0015;c.bevel_depth=.0004
    c.font=bpy.data.fonts.load(str(SOURCE/'Fonts/NotoSansCJKsc-Regular.otf'))
    o=bpy.data.objects.new(name,c);bpy.context.collection.objects.link(o);o.location=loc;o.rotation_euler=rotation
    bpy.context.view_layer.objects.active=o;o.select_set(True)
    bpy.ops.object.convert(target='MESH');o=bpy.context.object;o.data.materials.append(mat)
    if o.data.vertices:
        height=max(v.co.y for v in o.data.vertices)-min(v.co.y for v in o.data.vertices)
        if height>0:
            for v in o.data.vertices:v.co*=size/height
    o.select_set(False)
    return o

def join(objects,name):
    bpy.ops.object.select_all(action='DESELECT')
    for o in objects:o.select_set(True)
    bpy.context.view_layer.objects.active=objects[0];bpy.ops.object.join()
    o=bpy.context.object;o.name=name
    bpy.context.scene.cursor.location=(0,0,0);bpy.ops.object.origin_set(type='ORIGIN_CURSOR')
    return o

def export(name,objects,folder,animation=False):
    path=EXPORT/folder;path.mkdir(parents=True,exist_ok=True)
    bpy.ops.object.select_all(action='DESELECT')
    for o in objects:o.select_set(True)
    bpy.context.view_layer.objects.active=objects[0]
    bpy.ops.export_scene.fbx(filepath=str(path/(name+'.fbx')),use_selection=True,
        object_types={'MESH','ARMATURE'},global_scale=1,apply_unit_scale=True,
        axis_forward='-Y',axis_up='Z',use_mesh_modifiers=True,
        add_leaf_bones=False,bake_anim=animation,bake_anim_use_all_actions=False,
        bake_anim_use_nla_strips=False,bake_anim_simplify_factor=0,
        mesh_smooth_type='FACE',path_mode='STRIP')
    print('EXPORTED',folder,name,flush=True)

def save(name):
    path=SOURCE/'Blender';path.mkdir(exist_ok=True)
    bpy.ops.wm.save_as_mainfile(filepath=str(path/(name+'.blend')))
    bpy.ops.file.make_paths_relative()
    bpy.ops.wm.save_as_mainfile(filepath=str(path/(name+'.blend')))
