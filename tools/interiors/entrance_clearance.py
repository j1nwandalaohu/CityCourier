"""Deterministic street-prop placement around genuine external doorways (cm)."""
import math


def overlaps(a,b):
    return all(a['minimum'][i]<b['maximum'][i] and a['maximum'][i]>b['minimum'][i] for i in range(3))


def entry_zones(buildings):
    zones=[]
    footprints=[]
    for b in buildings:
        angle=math.radians(b['yaw']); c,s=math.cos(angle),math.sin(angle)
        def world(p): return (b['location'][0]+c*p[0]-s*p[1],b['location'][1]+s*p[0]+c*p[1],b['location'][2]+p[2])
        corners=[world((x*b['width']/2,y*b['depth']/2,0)) for x in [-1,1] for y in [-1,1]]
        footprints.append((min(p[0] for p in corners),min(p[1] for p in corners),max(p[0] for p in corners),max(p[1] for p in corners)))
        nodes={n['id']:n for n in b['nodes']}
        for d in b['doors']:
            if not d['external']: continue
            yaw=math.radians(b['yaw']+d['closed_yaw']); along=(math.cos(yaw),math.sin(yaw))
            hinge=world(d['hinge']); center=(hinge[0]+along[0]*d['width']/2,hinge[1]+along[1]*d['width']/2,hinge[2])
            outside=next(nodes[n] for n in [d['node_a'],d['node_b']] if nodes[n]['room_id']=='Outside')
            p=world(outside['position']); delta=(p[0]-center[0],p[1]-center[1]); length=math.hypot(*delta)
            out=(delta[0]/length,delta[1]/length)
            points=[(center[0]+along[0]*x+out[0]*y,center[1]+along[1]*x+out[1]*y) for x in [-d['width']/2-45,d['width']/2+45] for y in [-25,250]]
            zones.append(dict(id=d['id'],minimum=(min(p[0] for p in points),min(p[1] for p in points),center[2]-10),
                              maximum=(max(p[0] for p in points),max(p[1] for p in points),center[2]+230),along=along,outward=out))
    return zones,footprints


def _distance(p,a,b):
    dx,dy=b[0]-a[0],b[1]-a[1]
    t=max(0,min(1,((p[0]-a[0])*dx+(p[1]-a[1])*dy)/(dx*dx+dy*dy)))
    return math.hypot(p[0]-a[0]-t*dx,p[1]-a[1]-t*dy)


def relocate_props(props,zones,footprints,roads):
    result=[dict(p) for p in props]; changes=[]
    for index,p in enumerate(result):
        blocked=[z for z in zones if overlaps(p,z)]
        if not blocked: continue
        zone=blocked[0]; found=None
        for outward in [100,150,200,250,300,350]:
            for sideways in [200,250,300,350,400,450,500,600,700,800]:
                for side in [1,-1]:
                    offset=(zone['outward'][0]*outward+zone['along'][0]*sideways*side,
                            zone['outward'][1]*outward+zone['along'][1]*sideways*side,0)
                    q=dict(p)
                    for key in ['position','minimum','maximum']: q[key]=tuple(p[key][i]+offset[i] for i in range(3))
                    if any(overlaps(q,z) for z in zones): continue
                    if any(q['minimum'][0]<f[2] and q['maximum'][0]>f[0] and q['minimum'][1]<f[3] and q['maximum'][1]>f[1] for f in footprints): continue
                    if any(overlaps(q,other) for j,other in enumerate(result) if j!=index): continue
                    # A tree's crown can overhang the road above vehicle height;
                    # keep its pot/trunk outside the 12m driving corridor.
                    radius=80 if q.get('tree') else min((q['maximum'][i]-q['minimum'][i])/2 for i in [0,1])
                    if any(_distance(q['position'],a,b)<600+radius+20 for a,b in roads): continue
                    found=q; break
                if found: break
            if found: break
        if not found: raise ValueError('No safe entrance-clear placement: '+p['id']+' at '+zone['id'])
        result[index]=found
        changes.append(dict(id=p['id'],from_position=p['position'],to_position=found['position'],blocked_doors=[z['id'] for z in blocked]))
    return result,changes
