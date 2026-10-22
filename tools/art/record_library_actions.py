"""Record source attribution and pending activation; performs no UE/test work."""
import csv
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT/'AssetsSource/CityCourier'
LIBRARY = '/Game/CityCourier/Animation/Library/'
CHARACTERS = '/Game/CityCourier/Art/Characters/'
EPIC = '/Game/CityCourier/Animation/Epic/Clips/'


def main():
    generated = json.loads((SOURCE/'library_actions.json').read_text(encoding='utf-8'))
    replacement_path = ROOT/'Saved/Validation/LibraryActions/all-characters-replaced.json'
    replacements = json.loads(replacement_path.read_text(encoding='utf-8')) if replacement_path.exists() else {}
    resources_replaced = replacements.get('status') == 'animation_resources_replaced'
    proof_path = ROOT/'Saved/Validation/LibraryActions/runtime-verification.json'
    proof = json.loads(proof_path.read_text(encoding='utf-8')) if proof_path.exists() else {}
    connection_path = ROOT/'Saved/Validation/LibraryActions/connected.json'
    connection = json.loads(connection_path.read_text(encoding='utf-8')) if connection_path.exists() else {}
    connected = bool(proof.get('tests_passed') and proof.get('asset_fingerprint') == connection.get('asset_fingerprint'))
    clips = {}
    for name, definition in generated['clips'].items():
        package = LIBRARY+name
        runtime = ROOT/'Content'/(package.removeprefix('/Game/')+'.uasset')
        clips[name] = {**definition, 'asset': package, 'source_export': 'AssetsSource/CityCourier/Exports/LibraryActions/'+name+'.fbx',
            'runtime_package_sha256': hashlib.sha256(runtime.read_bytes()).hexdigest() if runtime.exists() else None,
            'activation': 'connected_automation_passed' if connected else 'resource_imported_native_graph_pending' if resources_replaced else 'pending_editor_build_reimport_and_graph_generation'}
    production = json.loads((SOURCE/'production_actions.json').read_text(encoding='utf-8'))
    heavy_source = production['clips']['A_HeavyPunch_R']['source_action']
    for name, source_action in {'A_Jog': 'Jog', 'A_Sprint': 'Sprint', 'A_LightPunch_L': 'Punch_Jab',
            'A_LightPunch_R': 'Punch_Cross', 'A_HeavyPunch_R': heavy_source, 'A_CrouchIdle': 'Crouch_Idle',
            'A_CrouchWalk': 'Crouch_Walk', 'A_ProneIdle': 'Crawl (held library frame)', 'A_ProneCrawl': 'Crawl'}.items():
        clips[name] = {'asset': CHARACTERS+name, 'source_action': source_action, 'license': 'CC0-1.0',
            'activation': 'existing_library_animation_retained'}
    for name, source_action in {'A_RunStart': 'M_Neutral_Sprint_Start_F_Lfoot', 'A_RunStop': 'M_Neutral_Sprint_Stop_F_Lfoot'}.items():
        clips[name] = {'asset': EPIC+'CC_'+source_action, 'source_action': source_action, 'source_library': 'Epic Game Animation Sample',
            'activation': 'pending_native_loader_change', 'license': 'Acquired Epic/Fab sample terms; not CC0'}
    npc_clips = {}
    for group in ('Reactions', 'Behavior'):
        manifest = json.loads((SOURCE/'Ambient/CharMorph'/group/'manifest.json').read_text(encoding='utf-8'))
        for name, definition in manifest['clips'].items():
            npc_clips[name] = {**definition, 'asset': '/Game/CityCourier/Art/Ambient/CharMorph/'+group+'/'+name,
                'license': 'CC0-1.0', 'pose_authoring': manifest.get('pose_authoring', 'legacy'),
                'activation': 'existing_runtime_asset_replaced' if resources_replaced else 'pending_import'}
    catalog = {'scope': 'player_and_npc', 'status': 'connected_automation_passed_game_launched' if connected else 'animation_resources_replaced_native_graph_pending' if resources_replaced else 'resources_and_code_prepared_not_activated',
        'clips': clips, 'epic_paths_retained': ['GASP ground locomotion database', 'GASP airborne database', 'GASP standing/moving landings',
            'GASP vault/mantle montages', 'Stack O Bot reference contact solver'],
        'npc_clips': npc_clips, 'replaced_resource_count': replacements.get('replaced_count', 0),
        'recovery': {'accepted_hits': 3, 'maximum_gap_seconds': 1.2, 'ground_hold_seconds': .6, 'automatic_getup': True,
            'note': 'Timing defaults chosen during implementation; only continuous-hit trigger and auto get-up were user-specified'},
        'unrun': ['PIE visual/contact validation'] if connected else ['Editor build', 'player graph regeneration', 'UE automation', 'PIE visual/contact validation'],
        'user_constraint': 'User subsequently authorized rebuild and game launch' if connected else 'Resources and source edits only; no build or tests for this task'}
    (ROOT/'docs/art/library-action-replacements.json').write_text(json.dumps(catalog, indent=2)+'\n', encoding='utf-8')
    if resources_replaced:
        (ROOT/'docs/art/all-character-animation-replacements.json').write_text(json.dumps(replacements, indent=2)+'\n', encoding='utf-8')
    path = ROOT/'docs/art/asset-manifest.csv'
    with path.open(newline='', encoding='utf-8') as stream:
        reader = csv.DictReader(stream)
        fields, rows = reader.fieldnames, list(reader)
    rows = [r for r in rows if not r['asset_id'].startswith('library_player_')]
    for name, definition in generated['clips'].items():
        epic = definition.get('source_library') == 'Epic Content Examples'
        rows.append({'asset_id': 'library_player_'+name.lower(), 'source_path': clips[name]['source_export'], 'ue_path': LIBRARY+name,
            'author': 'Epic Games; retargeted by CityCourier' if epic else 'Mesh2Motion contributors; retargeted by CityCourier',
            'source_url': 'https://dev.epicgames.com/documentation/en-us/unreal-engine/content-examples-sample-project-for-unreal-engine' if epic else generated['upstream']+'/tree/'+generated['source_revision'],
            'license': 'Acquired Epic/Fab Content Examples sample terms; not CC0' if epic else 'CC0-1.0', 'is_placeholder': 'false'})
    for package, definition in replacements.get('assets', {}).items():
        matching = [r for r in rows if r['ue_path'] == package]
        if not matching:
            row = dict.fromkeys(fields, '')
            row.update(asset_id='library_replacement_'+hashlib.sha256(package.encode()).hexdigest()[:12], ue_path=package)
            rows.append(row)
            matching = [row]
        epic = definition['source_action'].startswith(('MM_HitReact_', 'CC_M_Neutral_'))
        source_path = definition.get('source_fbx') or ('Content/'+definition['source_package'].split('.')[0].removeprefix('/Game/')+'.uasset' if 'source_package' in definition else clips[package.rsplit('/', 1)[-1]]['source_export'])
        for row in matching:
            row.update(source_path=source_path,
                author='Epic Games; retargeted by CityCourier' if epic else 'Mesh2Motion contributors; retargeted by CityCourier',
                source_url='https://dev.epicgames.com/documentation/en-us/unreal-engine/game-animation-sample-project-in-unreal-engine' if definition['source_action'].startswith('CC_') else
                    'https://dev.epicgames.com/documentation/en-us/unreal-engine/content-examples-sample-project-for-unreal-engine' if epic else generated['upstream']+'/tree/'+generated['source_revision'],
                license='Acquired Epic/Fab sample terms; not CC0' if epic else 'CC0-1.0 retargeted; no original pose authoring', is_placeholder='false')
    with path.open('w', newline='', encoding='utf-8') as stream:
        writer = csv.DictWriter(stream, fieldnames=fields)
        writer.writeheader()
        writer.writerows(rows)
    bundle_path = SOURCE/'External/CombatRecovery/manifest.json'
    if resources_replaced and bundle_path.exists():
        bundle = json.loads(bundle_path.read_text(encoding='utf-8'))
        bundle.update(integration_status='Content Examples front/back hits used; other collected clips remain reference',
            status='selected_hit_sources_retargeted_and_imported', runtime_changed=True,
            converted_source_actions=['MM_HitReact_Front_Med_01', 'MM_HitReact_Back_Med_01'],
            cmu_runtime_usage=False)
        bundle_path.write_text(json.dumps(bundle, indent=2)+'\n', encoding='utf-8')
    print('Recorded '+str(len(clips))+' player and '+str(len(npc_clips))+' NPC clip references; '+str(replacements.get('replaced_count', 0))+' resources replaced; '+('build/tests passed and game launched' if connected else 'build/graph validation pending')+'.')


if __name__ == '__main__':
    main()
