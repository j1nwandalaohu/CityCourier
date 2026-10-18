"""Freeze the existing courier as a helmet-free, clothed look comparison."""
import sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent))
import bpy,bmesh
from art_common import SOURCE,export

bpy.ops.wm.open_mainfile(filepath=str(SOURCE/'Blender/CourierRider.blend'))
source=bpy.data.objects['SK_Courier'];scene=bpy.context.scene;scene.frame_set(1)
mesh=bpy.data.meshes.new_from_object(source.evaluated_get(bpy.context.evaluated_depsgraph_get()))
obj=bpy.data.objects.new('SM_MakeHumanBust',mesh);scene.collection.objects.link(obj)
bm=bmesh.new();bm.from_mesh(mesh)
remove=[]
for face in bm.faces:
    material=mesh.materials[face.material_index].name
    if face.calc_center_median().z<1.22 or material in {'M_Helmet','M_Rubber','M_ReflectiveStrip'}:remove.append(face)
bmesh.ops.delete(bm,geom=remove,context='FACES');bm.to_mesh(mesh);bm.free();mesh.update()
export(obj.name,[obj],'CharacterComparison')
print('MAKEHUMAN BUST',len(mesh.vertices),len(mesh.polygons),flush=True)
