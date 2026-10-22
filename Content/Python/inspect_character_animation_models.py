"""Read-only diagnostics for UE animation model/controller reflection."""
import unreal
clip = unreal.load_asset('/Game/CityCourier/Art/Characters/A_RunStart')
model = clip.get_editor_property('data_model_interface')
unreal.log('MODEL_DIAGNOSTIC '+str(type(model))+' '+model.get_class().get_path_name())
for name in ('AnimDataController', 'AnimSequencerController', 'AnimationSequencerDataModel'):
    cls = getattr(unreal, name, None)
    unreal.log('MODEL_TYPE '+name+' '+str(cls)+' '+str(isinstance(model, cls)) if cls else 'MISSING '+name)
for path in ('/Script/AnimationData.AnimSequencerController', '/Script/AnimationDataController.AnimDataController'):
    cls = unreal.load_class(None, path)
    obj = unreal.new_object(cls)
    unreal.log('CONTROLLER_DIAGNOSTIC '+obj.get_class().get_path_name()+' '+str(type(obj)))
