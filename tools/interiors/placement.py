"""Resolve real infill footprints and suppress doors buried in party walls."""
import math


def bounds(b):
    a = math.radians(b['yaw'])
    hx = (abs(math.cos(a))*b['width'] + abs(math.sin(a))*b['depth'])/2
    hy = (abs(math.sin(a))*b['width'] + abs(math.cos(a))*b['depth'])/2
    x, y = b['location'][:2]
    return x-hx, x+hx, y-hy, y+hy


def overlaps(a, b, clearance=0):
    x, y = bounds(a), bounds(b)
    return min(x[1]+clearance,y[1])-max(x[0]-clearance,y[0]) > .01 and min(x[3]+clearance,y[3])-max(x[2]-clearance,y[2]) > .01


def apply_site_constraints(buildings):
    streets = [b for b in buildings if b['id'] != 'B_Station' and not b['id'].startswith('B_Merchant_')]
    infill = [b for b in buildings if b not in streets]
    occupied = list(streets)
    for b in infill:
        b['source_location'] = list(b['location'])
        b['source_width'] = b['width']
        if b['id'] == 'B_Station':
            # The established corner site has 590cm between the road and the
            # neighboring facade. A 550cm single-storey kit fits both boundaries.
            b['width'] = 550
            b['placement_reason'] = 'fit_single_storey_station_between_existing_road_and_neighbor'
        elif any(overlaps(b, other) for other in occupied):
            origin = list(b['location'])
            fitted = False
            # Same-block courtyards are behind the preserved frontage, with a
            # real walking connector from the parallel public street.
            for dy in [2700, 3200, 3700, 4200, -2700, -3200]:
                for dx in [600, 1200, 1800, 2400]:
                    b['location'] = [origin[0]+dx, origin[1]+dy, origin[2]]
                    if not any(overlaps(b, other, 150) for other in occupied):
                        fitted = True
                        break
                if fitted:
                    break
            if not fitted:
                raise ValueError('No non-overlapping infill site for '+b['id'])
            b['placement_reason'] = 'same_block_courtyard_replaces_overlapping_visual_annex_site'
        if any(overlaps(b, other) for other in occupied):
            raise ValueError('Infill still overlaps a real building: '+b['id'])
        occupied.append(b)
    for b in buildings:
        sides = []
        a = math.radians(b['yaw'])
        local_y = b['depth']/2-325
        for side in [-1, 1]:
            local_x = side*(b['width']/2+140)
            x = b['location'][0] + math.cos(a)*local_x - math.sin(a)*local_y
            y = b['location'][1] + math.sin(a)*local_x + math.cos(a)*local_y
            blocked = False
            for other in buildings:
                if b is other:
                    continue
                lo_x, hi_x, lo_y, hi_y = bounds(other)
                if lo_x-36 <= x <= hi_x+36 and lo_y-36 <= y <= hi_y+36:
                    blocked = True
                    break
            if not blocked:
                sides.append(side)
        b['external_sides'] = [] if b.get('storeys') == 1 else sides
    return buildings
