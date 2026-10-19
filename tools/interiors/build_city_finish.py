"""Validate every canonical building and export compact, instanced runtime art."""
import hashlib
import json
from collections import Counter
from pathlib import Path
from tools.interiors.finish_kit import plan_city

ROOT = Path(__file__).resolve().parents[2]


def main():
    source = ROOT/'AssetsSource/CityCourier/InteriorArt'
    inventory_path = ROOT/'AssetsSource/CityCourier/Interiors/inventory.json'
    inventory = json.loads(inventory_path.read_text(encoding='utf-8'))
    manifest = json.loads((source/'asset-manifest.json').read_text(encoding='utf-8'))
    output = dict(schema_version=1, buildings=[])
    report = dict(passed=True, errors=[], scope='all_source_geometry_not_runtime_or_performance',
                  inventory_sha256=hashlib.sha256(inventory_path.read_bytes()).hexdigest(), buildings=[])
    for seed in inventory['buildings']:
        sample = plan_city(dict(buildings=[seed]), {a['name']: a for a in manifest['assets']})['samples'][0]
        groups = {}
        for p in sample['placements']:
            key = (p['asset'], p['collision'] == 'BlockAll')
            groups.setdefault(key, []).append(p['location_cm'] + p['rotation_pyr'] + p['scale3'])
        output['buildings'].append(dict(id=seed['id'], groups=[dict(mesh=k[0], solid=k[1], transforms=v)
                                                     for k, v in sorted(groups.items())]))
        b = sample['building']
        report['buildings'].append(dict(id=b['id'], rooms=len(b['rooms']), doors=len(b['doors']),
            placements=len(sample['placements']), categories=dict(Counter(p['category'] for p in sample['placements'])),
            canonical_sha256=hashlib.sha256(json.dumps(b, sort_keys=True).encode()).hexdigest()))
        print(seed['id'], len(sample['placements']), flush=True)
    for key in ('rooms', 'doors', 'placements'):
        report[key] = sum(b[key] for b in report['buildings'])
    (source/'city-layout.json').write_text(json.dumps(output, separators=(',', ':'))+'\n', encoding='utf-8')
    report['layout_sha256'] = hashlib.sha256((source/'city-layout.json').read_bytes()).hexdigest()
    (source/'city-plan-validation.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    print(json.dumps({k: v for k, v in report.items() if k != 'buildings'}, indent=2))


if __name__ == '__main__':
    main()
