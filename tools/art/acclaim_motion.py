"""Strict reader for original Acclaim ASF/AMC data; coordinate conversion is separate."""
import math
import re
from pathlib import Path


def load_asf(path):
    text = Path(path).read_text(encoding='ascii')
    sections = re.split(r'^:(\w+)\s*', text, flags=re.M)
    sections = dict(zip(sections[1::2], sections[2::2]))
    bones = {}
    for block in re.findall(r'\bbegin\b(.*?)\bend\b', sections['bonedata'], re.S):
        values = {}
        for line in block.splitlines():
            fields = line.split()
            if fields and fields[0] in {'name', 'direction', 'length', 'axis', 'dof'}:
                values[fields[0]] = fields[1:]
        name = values['name'][0]
        bones[name] = {'direction': list(map(float, values['direction'])), 'length': float(values['length'][0]),
                       'axis': list(map(float, values['axis'][:3])), 'axis_order': values['axis'][3],
                       'dof': values.get('dof', [])}
    parents = {}
    for line in sections['hierarchy'].splitlines():
        fields = line.split()
        if not fields or fields[0] in {'begin', 'end'}:
            continue
        for child in fields[1:]:
            if child in parents:
                raise ValueError('Repeated ASF parent: ' + child)
            parents[child] = fields[0]
    if set(parents) != set(bones):
        raise ValueError('ASF hierarchy is incomplete')
    ordered, remaining = [], set(bones)
    while remaining:
        ready = sorted(name for name in remaining if parents[name] == 'root' or parents[name] in ordered)
        if not ready:
            raise ValueError('ASF hierarchy contains a cycle')
        ordered.extend(ready)
        remaining.difference_update(ready)
    return {'bones': bones, 'parents': parents, 'order': ordered,
            'dofs': dict(root=6, **{name: len(bone['dof']) for name, bone in bones.items() if bone['dof']})}


def parse_amc(text, dofs):
    frames, current = [], None

    def finish():
        if current is not None:
            if current.keys() != dofs.keys():
                raise ValueError('AMC frame channels differ from ASF')
            frames.append(current)

    for raw in text.splitlines():
        line = raw.strip()
        if not line or line.startswith(('#', ':')):
            continue
        if line.isdigit():
            finish()
            if int(line) != len(frames) + 1:
                raise ValueError('AMC frame numbers are not contiguous')
            current = {}
            continue
        name, *values = line.split()
        if current is None or name not in dofs or name in current or len(values) != dofs[name]:
            raise ValueError('AMC channels are invalid')
        values = list(map(float, values))
        if not all(math.isfinite(value) for value in values):
            raise ValueError('AMC contains non-finite values')
        current[name] = values
    finish()
    if not frames:
        raise ValueError('AMC has no frames')
    return frames


def load_amc(path, skeleton):
    return parse_amc(Path(path).read_text(encoding='ascii'), skeleton['dofs'])
