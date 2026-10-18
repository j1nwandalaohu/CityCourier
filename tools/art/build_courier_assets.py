"""Run with Blender --background --python tools/art/build_courier_assets.py."""
import sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent))
import build_environment
import build_scooter_art
import build_rider
import bpy,json
from art_common import SOURCE,material

material('M_Pavers',(.32,.31,.28),scan='pavement_05')

build_environment.build()
build_scooter_art.build()
build_rider.build()
catalog=[]
for m in bpy.data.materials:
    if not m.use_nodes:continue
    p=m.node_tree.nodes.get('Principled BSDF')
    if not p:continue
    catalog.append(dict(name=m.name,color=list(p.inputs['Base Color'].default_value)[:3],roughness=p.inputs['Roughness'].default_value,metallic=p.inputs['Metallic'].default_value,emission=p.inputs['Emission Strength'].default_value,
        textures=[str(Path(n.image.filepath).name) for n in m.node_tree.nodes if n.type=='TEX_IMAGE' and n.image]))
(SOURCE/'materials.json').write_text(json.dumps(catalog,indent=2),encoding='utf-8')
print('CityCourier art source generation finished',flush=True)
