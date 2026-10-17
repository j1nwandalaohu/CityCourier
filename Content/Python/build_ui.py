"""Import the licensed Chinese font and create native-backed widget/input assets."""
import pathlib
import unreal

root = '/Game/CityCourier'
assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
tools = unreal.AssetToolsHelpers.get_asset_tools()
font_dir = root + '/UI/Fonts'
face_path = font_dir + '/FF_CourierChinese'
if not assets.does_asset_exist(face_path):
    task = unreal.AssetImportTask()
    task.set_editor_property('filename', str(pathlib.Path(unreal.Paths.project_dir()) / 'AssetsSource/CityCourier/Fonts/NotoSansCJKsc-Regular.otf'))
    task.set_editor_property('destination_path', font_dir)
    task.set_editor_property('destination_name', 'FF_CourierChinese')
    task.set_editor_property('automated', True)
    task.set_editor_property('save', True)
    task.set_editor_property('factory', unreal.FontFileImportFactory())
    tools.import_asset_tasks([task])
    unreal.log('Font imports: ' + str(task.get_editor_property('imported_object_paths')))
face = unreal.load_asset(face_path)
assert isinstance(face, unreal.FontFace), 'Font face import missing'
font_path = font_dir + '/F_CourierChinese'
font = unreal.load_asset(font_path) if assets.does_asset_exist(font_path) else tools.create_asset('F_CourierChinese', font_dir, unreal.Font, unreal.FontFactory())
font.set_editor_property('font_cache_type', unreal.FontCacheType.RUNTIME)
composite = font.get_editor_property('composite_font')
composite.import_text('(DefaultTypeface=(Fonts=((Name="Regular",Font=(FontFaceAsset="/Script/Engine.FontFace\'/Game/CityCourier/UI/Fonts/FF_CourierChinese.FF_CourierChinese\'")))))')
font.set_editor_property('composite_font', composite)
assets.save_loaded_asset(font)

for name, parent in [('CourierHUD',unreal.CourierHUDWidget), ('CourierPhone',unreal.CourierPhoneWidget),
                     ('CourierMap',unreal.CourierMapWidget), ('CourierMenu',unreal.CourierMenuWidget)]:
    path = root + '/UI/WBP_' + name
    if assets.does_asset_exist(path):
        bp = unreal.load_asset(path)
    else:
        factory = unreal.WidgetBlueprintFactory()
        factory.set_editor_property('parent_class', parent)
        bp = tools.create_asset('WBP_' + name, root + '/UI', unreal.WidgetBlueprint, factory)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    assets.save_loaded_asset(bp)

context = unreal.load_asset(root + '/Input/IMC_Courier')
names = ['IA_Phone','IA_Map','IA_Pause']
mappings = [m for m in context.get_editor_property('mappings') if m.action.get_name() not in names]
for name, key in [('Phone','Tab'),('Map','M'),('Pause','Escape')]:
    path = root + '/Input/IA_' + name
    if assets.does_asset_exist(path):
        action = unreal.load_asset(path)
    else:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property('data_asset_class', unreal.InputAction)
        action = tools.create_asset('IA_' + name, root+'/Input',unreal.InputAction,factory)
    action.set_editor_property('value_type', unreal.InputActionValueType.BOOLEAN)
    action.set_editor_property('trigger_when_paused', True)
    assets.save_loaded_asset(action)
    input_key = unreal.Key()
    input_key.import_text(key)
    mappings.append(unreal.EnhancedActionKeyMapping(action=action, key=input_key))
context.set_editor_property('mappings', mappings)
assets.save_loaded_asset(context)
unreal.log('CityCourier Chinese UI assets ready')
