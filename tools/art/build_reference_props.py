"""Adapt complete CC0 model sets, retaining their authored UVs and PBR channels."""
import json,re,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent))
import bpy
from mathutils import Vector
from art_common import SOURCE,reset,export,save

MODELS=[('tree_small_02','SM_ReferenceTree'),('plastic_crate_02','SM_ReferenceCrate'),('exterior_aircon_unit','SM_ReferenceAircon')]

def build():
    catalog=[]
    for source,prefix in MODELS:
        reset();folder=SOURCE/'External/PolyHavenModels'/source
        path=folder/(source+'_2k.gltf');gltf=json.loads(path.read_text())
        bpy.ops.import_scene.gltf(filepath=str(path))
        for definition in gltf['materials']:
            old=definition['name'];name='M_PH_'+re.sub('[^a-zA-Z0-9_]','_',old)
            material=bpy.data.materials.get(old)
            if material:material.name=name
            def image(info):
                if not info:return None
                uri=gltf['images'][gltf['textures'][info['index']]['source']]['uri']
                return (folder/uri).relative_to(SOURCE).as_posix()
            pbr=definition.get('pbrMetallicRoughness',{})
            candidates=list((folder/'textures').glob(old+'_opacity_2k.jpg'))
            if 'leaves' in old:candidates=list((folder/'textures').glob('*leaves_alpha_2k.jpg'))
            if source=='plastic_crate_02':candidates=list((folder/'textures').glob('*opacity_2k.jpg'))
            if source=='exterior_aircon_unit' and old.endswith('_02'):
                candidates=list((folder/'textures').glob(old.replace('_02','_01')+'_opacity_2k.jpg')) or candidates
            catalog.append(dict(name=name,color=pbr.get('baseColorFactor',[1,1,1,1])[:3],
                metallic=pbr.get('metallicFactor',1),roughness=pbr.get('roughnessFactor',1),
                color_map=image(pbr.get('baseColorTexture')),normal_map=image(definition.get('normalTexture')),
                arm_map=image(pbr.get('metallicRoughnessTexture')),ao_map=image(definition.get('occlusionTexture')),
                double_sided=definition.get('doubleSided',False),alpha=definition.get('alphaMode','OPAQUE'),
                foliage='leaves' in old.lower(),opacity_map=candidates[0].relative_to(SOURCE).as_posix() if candidates else None))
        objects=[o for o in bpy.context.scene.objects if o.type=='MESH']
        if not objects:raise RuntimeError('No source mesh: '+source)
        for index,obj in enumerate(objects):
            bpy.context.view_layer.objects.active=obj
            bpy.ops.object.select_all(action='DESELECT');obj.select_set(True)
            obj.matrix_world.translation=Vector((0,0,0))
            bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
            # glTF stores V downwards; Blender imported UVs have V upwards.
            # Bake source UV-set/texture transforms into UV0 for every surface.
            definitions={'M_PH_'+re.sub('[^a-zA-Z0-9_]','_',d['name']):d for d in gltf['materials']}
            uv_settings={}
            for material_index,material in enumerate(obj.data.materials):
                definition=definitions[material.name]
                info=definition.get('pbrMetallicRoughness',{}).get('baseColorTexture',{})
                transform=info.get('extensions',{}).get('KHR_texture_transform',{})
                uv_index=transform.get('texCoord',info.get('texCoord',0))
                uv_settings[material_index]=(uv_index,transform)
            for polygon in obj.data.polygons:
                uv_index,transform=uv_settings[polygon.material_index]
                if uv_index or transform:
                    sx,sy=transform.get('scale',[1,1]);ox,oy=transform.get('offset',[0,0])
                    for loop in polygon.loop_indices:
                        uv=obj.data.uv_layers[uv_index].data[loop].uv.copy()
                        obj.data.uv_layers[0].data[loop].uv=(uv.x*sx+ox,uv.y*sy+1-oy-sy)
            vertices=obj.data.vertices
            lo=Vector(tuple(min(v.co[i] for v in vertices) for i in range(3)))
            hi=Vector(tuple(max(v.co[i] for v in vertices) for i in range(3)))
            offset=Vector(((lo.x+hi.x)/2,(lo.y+hi.y)/2,lo.z))
            if source=='tree_small_02':
                roots=[v.co for v in vertices if v.co.z<lo.z+(hi.z-lo.z)*.025]
                offset.x=(min(v.x for v in roots)+max(v.x for v in roots))/2
                offset.y=(min(v.y for v in roots)+max(v.y for v in roots))/2
            scale=4.2/(hi.z-lo.z) if source=='tree_small_02' else .85/(hi.x-lo.x) if source=='exterior_aircon_unit' else 1
            for vertex in vertices:vertex.co=(vertex.co-offset)*scale
            triangles=sum(len(f.vertices)-2 for f in obj.data.polygons)
            if triangles>450000:
                decimate=obj.modifiers.new('Runtime foliage budget','DECIMATE');decimate.ratio=450000/triangles
                bpy.ops.object.modifier_apply(modifier=decimate.name)
            obj.name=prefix+('Rusted' if 'rusted' in obj.name.lower() else '')
            bpy.context.view_layer.update()
            export(obj.name,[obj],'ReferenceProps')
            print('REFERENCE',obj.name,tuple(obj.dimensions),len(obj.data.polygons),'polygons',flush=True)
        save(prefix)
    (SOURCE/'reference_materials.json').write_text(json.dumps(catalog,indent=2)+'\n',encoding='utf-8')

if __name__=='__main__':build()
