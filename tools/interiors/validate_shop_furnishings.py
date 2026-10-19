"""Check the serialized city placements against current exported shop bounds."""
import hashlib
import json
from collections import Counter
from pathlib import Path
from tools.interiors.finish_kit import (EPS, SHOP_PRIMARY, _furniture_errors,
    _overlap, placement_bounds)
from tools.interiors.layout import compile_building

ROOT = Path(__file__).resolve().parents[2]


def main():
    source = ROOT/'AssetsSource/CityCourier'
    inventory = json.loads((source/'Interiors/inventory.json').read_text(encoding='utf-8'))
    manifest_path = source/'InteriorArt/asset-manifest.json'
    bounds = {a['name']:a for a in json.loads(manifest_path.read_text())['assets']}
    layout_path = source/'InteriorArt/city-layout.json'
    records = {b['id']:b for b in json.loads(layout_path.read_text())['buildings']}
    errors, counts = [], Counter()
    shops = 0
    for seed in inventory['buildings']:
        b = compile_building(seed)
        rooms = [r for r in b['rooms'] if r['floor']==1 and r['kind']=='shop']
        shops += len(rooms)
        fitted = {r['id']:[] for r in rooms}
        for group in records[b['id']]['groups']:
            if not group['mesh'].startswith('SM_IF_Shop'):
                continue
            if not group['solid']:
                errors.append(b['id']+': shop furniture collision disabled')
            for values in group['transforms']:
                p = dict(asset=group['mesh'],location_cm=values[:3],rotation_pyr=values[3:6],scale3=values[6:9])
                if group['mesh'][6:] in SHOP_PRIMARY and abs(values[4]%360)>.001:
                    errors.append(b['id']+': primary display does not face street')
                lo,hi = placement_bounds(p,bounds[p['asset']])
                room = next((r for r in rooms if all(lo[i]>=r['minimum'][i]-EPS and hi[i]<=r['maximum'][i]+EPS for i in range(3))),None)
                if not room:
                    errors.append(b['id']+': shop assembly outside a first-floor shop')
                    continue
                errors += [room['id']+': '+e for e in _furniture_errors(p,room,b,bounds[p['asset']])]
                for other in fitted[room['id']]:
                    if _overlap(lo,hi,*placement_bounds(other,bounds[other['asset']])):
                        errors.append(room['id']+': overlapping shop assemblies')
                fitted[room['id']].append(p)
                counts[p['asset']] += 1
        for room in rooms:
            if not any(p['asset']=='SM_IF_'+SHOP_PRIMARY[b['variant']] for p in fitted[room['id']]):
                errors.append(room['id']+': variant-specific primary missing')
    report = dict(passed=not errors,errors=errors,buildings=len(records),shops=shops,
        assemblies=dict(counts),layout_sha256=hashlib.sha256(layout_path.read_bytes()).hexdigest(),
        manifest_sha256=hashlib.sha256(manifest_path.read_bytes()).hexdigest(),
        checks=['real exported bounds','room containment','120cm standing square',
                'all route capsule sweeps','full door sweep','blocking collision','furniture separation'])
    (source/'InteriorArt/shop-plan-validation.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2))
    assert not errors, str(errors[:10])


if __name__=='__main__':
    main()
