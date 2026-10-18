"""Validate the actual authored rider against the shipped skeleton contract."""
import hashlib,json,math
from pathlib import Path
import bpy

ROOT=Path(__file__).resolve().parents[2]
SOURCE=ROOT/'AssetsSource/CityCourier'
def reference(rig):
    return [(b.name,b.parent.name if b.parent else None,[list(r) for r in b.matrix_local]) for b in rig.data.bones]

def validate():
    path=SOURCE/'Blender/CourierPhotoreal.blend'
    assert path.exists(),'Photoreal rider source has not been authored'
    bpy.ops.wm.open_mainfile(filepath=str(SOURCE/'Blender/CourierRider.blend'))
    expected=reference(bpy.data.objects['CourierRig'])
    bpy.ops.wm.open_mainfile(filepath=str(path))
    rig=bpy.data.objects['CourierRig'];mesh=bpy.data.objects['SK_Courier']
    assert reference(rig)==expected,'Photoreal upgrade changed shared skeleton rest transforms'
    assert len(expected)==50
    names=[m.name for m in mesh.data.materials]
    for name in ('M_PortraitSkinHead','M_PortraitSkinBody','M_PortraitHair','M_PortraitIris','M_PortraitCornea','M_PortraitTearline','M_WorkClothes'):
        assert name in names,('Missing functional surface',name)
    assert len(mesh.data.uv_layers)>=2,'Neck transition requires a separate body UV'
    assert 'RiderSurface' in mesh.data.color_attributes
    # UV coordinates are face corners, so a body UV chart discontinuity must
    # not get averaged across the replacement neck during subdivision.
    color=mesh.data.color_attributes['RiderSurface'].data
    body_uv=mesh.data.uv_layers[1].data;neck_span=0.
    for polygon in mesh.data.polygons:
        if names[polygon.material_index]!='M_PortraitSkinHead':continue
        if min(color[i].color[0] for i in polygon.vertices)<.5:continue
        values=[body_uv[loop].uv for loop in polygon.loop_indices]
        neck_span=max(neck_span,max(max(v[axis] for v in values)-min(v[axis] for v in values) for axis in (0,1)))
    assert neck_span<.10,('Neck UV crosses unrelated body charts',neck_span)
    invalid=[];error=0
    for vertex in mesh.data.vertices:
        assert all(math.isfinite(v) for v in vertex.co)
        total=sum(g.weight for g in vertex.groups)
        error=max(error,abs(total-1))
        if not vertex.groups:invalid.append(vertex.index)
    assert not invalid,('Unbound vertices',len(invalid))
    assert error<1e-5,('Unnormalized deformation weights',error)
    authoring=json.loads((SOURCE/'portrait_rider.json').read_text())
    assert authoring['lashes']>=100,'Upper and lower eyelids need actual lashes'
    assert authoring['scalp_strands']>=8000,'Incomplete scalp groom'
    assert authoring['scalp_root_bounds_m'][2][0]<1.620,'Groom misses the sides and nape'
    triangles=sum(len(p.vertices)-2 for p in mesh.data.polygons)
    assert triangles<1200000,('Hero mesh exceeded source budget',triangles)
    catalog=json.loads((SOURCE/'portrait_materials.json').read_text())
    texture_count=0
    for material in catalog['materials'].values():
        for channel,file in material.get('textures',{}).items():
            p=SOURCE/file
            assert p.exists(),file
            assert hashlib.sha256(p.read_bytes()).hexdigest()==material['texture_sha256'][channel],file
            texture_count+=1
    result={'skeleton_bones':len(expected),'reference_unchanged':True,'vertices':len(mesh.data.vertices),
            'triangles':triangles,'unbound_vertices':len(invalid),'max_weight_error':error,
            'max_neck_face_uv_span':neck_span,'texture_references_verified':texture_count}
    (SOURCE/'portrait_source_validation.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    print('PORTRAIT SOURCE VALIDATION',json.dumps(result),flush=True)

if __name__=='__main__':validate()
