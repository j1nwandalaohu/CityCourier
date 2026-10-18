"""Deterministic cloth tailoring on the fitted, already weighted CC0 garment.

All distances are meters. These are authored rest-pose folds, not a cloth
simulation. Source UVs, source files and the skeleton are left intact.
"""
import math
import bpy
from mathutils import Vector
from mathutils.kdtree import KDTree


def tag_garment_panels(obj):
    """Identify the disconnected shirt by shape, never by a vertex-index cutoff."""
    mesh=obj.data
    adjacent=[[] for _ in mesh.vertices]
    for edge in mesh.edges:
        a,b=edge.vertices;adjacent[a].append(b);adjacent[b].append(a)
    panel=mesh.attributes.new('cloth_shirt','FLOAT','POINT')
    remaining=set(range(len(mesh.vertices)))
    while remaining:
        seed=remaining.pop();component=[seed];pending=[seed]
        while pending:
            for i in adjacent[pending.pop()]:
                if i in remaining:
                    remaining.remove(i);component.append(i);pending.append(i)
        shirt=max(mesh.vertices[i].co.z for i in component)>1.3
        for i in component:panel.data[i].value=float(shirt)


def weighted_subdivision(obj,levels=1):
    """Interpolate existing deform weights and UVs before adding surface detail.

    Never re-run the spatial weight classifier on subdivided clothing: its new
    vertices do not retain the original OBJ ordering. No armature modifier may
    be applied here, so all operations happen in the unchanged rest space.
    """
    if any(mod.type=='ARMATURE' for mod in obj.modifiers):
        raise ValueError('Refine garment before the armature modifier is added')
    bpy.context.view_layer.objects.active=obj
    mod=obj.modifiers.new('Weighted garment surface','SUBSURF')
    mod.subdivision_type='CATMULL_CLARK';mod.levels=levels;mod.render_levels=levels
    mod.boundary_smooth='PRESERVE_CORNERS';mod.uv_smooth='PRESERVE_BOUNDARIES'
    bpy.ops.object.modifier_apply(modifier=mod.name)


def _bell(value,width):
    return math.exp(-(value/width)**2)


def _fold(p,start,end,height,width):
    """A tapered fold ridge with small adjacent troughs, along a sewn tension line."""
    a=Vector(start);b=Vector(end);v=b-a;delta=p-a
    t=max(0.,min(1.,delta.dot(v)/v.length_squared))
    distance=(delta-v*t).length
    taper=math.sin(math.pi*t)**.7
    return height*taper*(_bell(distance,width)-.22*_bell(distance,width*2.4))


def _boundary_loops(mesh):
    counts={tuple(sorted(edge.vertices)):0 for edge in mesh.edges}
    for poly in mesh.polygons:
        for edge in poly.edge_keys:counts[tuple(sorted(edge))]+=1
    adjacency={}
    for (a,b),count in counts.items():
        if count==1:
            adjacency.setdefault(a,[]).append(b);adjacency.setdefault(b,[]).append(a)
    unused=set(adjacency);loops=[]
    while unused:
        start=min(unused);loop=[start];unused.remove(start);previous=None;current=start
        while True:
            options=[i for i in adjacency[current] if i!=previous]
            following=next((i for i in options if i in unused),None)
            if following is None:break
            loop.append(following);unused.remove(following);previous,current=current,following
        if len(loop)>2:loops.append(loop)
    return loops


def _opening_seams(obj,loops):
    """Small stitched binding cords with the original UVs and interpolated skinning.

    Keeping each point's garment UV and deform weights avoids floating rigid
    rings at the sleeve and waist when the rider raises an arm or sits down.
    """
    selection_before=list(bpy.context.selected_objects)
    mesh=obj.data;uv_layer=mesh.uv_layers.active
    uv_by_vertex={}
    for loop in mesh.loops:
        uv_by_vertex.setdefault(loop.vertex_index,uv_layer.data[loop.index].uv.copy())
    verts=[];faces=[];uvs=[];weights=[]
    for loop in loops:
        center=sum((mesh.vertices[i].co for i in loop),Vector())/len(loop)
        # The trouser waistband sits beneath the shirt; an exposed ring there
        # would create an artificial bulge through the tee.
        if .96<center.z<1.05:continue
        radius=.00065 if center.z>1.2 else .0008
        base=len(verts)
        for k,index in enumerate(loop):
            source=mesh.vertices[index];normal=source.normal.normalized()
            tangent=(mesh.vertices[loop[(k+1)%len(loop)]].co-
                     mesh.vertices[loop[k-1]].co).normalized()
            across=tangent.cross(normal).normalized()
            for s in range(6):
                angle=s*math.tau/6
                verts.append(source.co+normal*.00035+
                             radius*(normal*math.cos(angle)+across*math.sin(angle)))
                uvs.append(uv_by_vertex[index]);weights.append([(g.group,g.weight) for g in source.groups])
        for k in range(len(loop)):
            for s in range(6):
                faces.append((base+k*6+s,base+((k+1)%len(loop))*6+s,
                              base+((k+1)%len(loop))*6+(s+1)%6,base+k*6+(s+1)%6))
    data=bpy.data.meshes.new('Weighted_opening_stitches');data.from_pydata(verts,[],faces);data.update()
    seams=bpy.data.objects.new('Weighted_opening_stitches',data);bpy.context.collection.objects.link(seams)
    for mat in mesh.materials:data.materials.append(mat)
    for group in obj.vertex_groups:seams.vertex_groups.new(name=group.name)
    for index,groups in enumerate(weights):
        for group,weight in groups:seams.vertex_groups[group].add([index],weight,'REPLACE')
    layer=data.uv_layers.new(name='UVMap')
    for poly in data.polygons:
        poly.use_smooth=True
        for li in poly.loop_indices:layer.data[li].uv=uvs[data.loops[li].vertex_index]
    bpy.ops.object.select_all(action='DESELECT');obj.select_set(True);seams.select_set(True)
    bpy.context.view_layer.objects.active=obj;bpy.ops.object.join()
    # The caller's existing transform-application order includes its selection.
    # Restore it so adding seams cannot even introduce repeated identity-matrix
    # rounding differences in the already authored skeleton's rest matrices.
    bpy.ops.object.select_all(action='DESELECT')
    for selected in selection_before:selected.select_set(True)
    bpy.context.view_layer.objects.active=obj


def refine_workwear(obj,joints):
    """Give a fitted jersey and denim their own cloth volume and localized folds."""
    weighted_subdivision(obj,2)
    mesh=obj.data;mesh.update()
    loops=_boundary_loops(mesh)
    boundary=[i for loop in loops for i in loop]
    tree=KDTree(len(boundary))
    for k,i in enumerate(boundary):tree.insert(mesh.vertices[i].co,k)
    tree.balance()
    shirt=mesh.attributes['cloth_shirt']
    changes=[]
    for vertex in mesh.vertices:
        p=vertex.co.copy();normal=vertex.normal.copy();x=abs(p.x)
        side='l' if p.x>=0 else 'r';sign=1 if p.x>=0 else -1
        edge_distance=tree.find(p)[2]
        # Openings retain their fit; the material gains a little sewn edge relief.
        free=1-math.exp(-edge_distance/.018)
        seam=.00115*_bell(edge_distance-.005,.0035)
        displacement=seam
        if shirt.data[vertex.index].value>.5:
            # A work tee hangs away from the lower torso, while neckline and
            # shoulder remain anchored. This removes the painted-on silhouette.
            ease=.0035+.0045*_bell(p.z-1.08,.16)
            ease*=1-.75*_bell(p.z-1.48,.05)
            displacement+=ease*free
            front=max(0.,-normal.y)**1.5;back=max(0.,normal.y)**1.5
            q=Vector((x,p.z))
            # Asymmetric side-seam and underarm tension; no whole-body noise.
            asym=1.0 if sign>0 else .84
            for a,b,h,w in [((.185,1.04),(.045,1.075),.0060,.011),
                            ((.186,1.085),(.090,1.13),.0050,.012),
                            ((.174,1.165),(.055,1.205),.0035,.014),
                            ((.175,1.325),(.078,1.255),.0055,.012),
                            ((.195,1.36),(.115,1.315),.0038,.010),
                            ((.040,1.33),(.065,1.16),.0022,.018)]:
                displacement+=_fold(q,a,b,h*asym,w)*(front+.64*back)*free
            # Soft gathers at the hem are spatially bounded and not periodic
            # corrugation over the chest or skin.
            for cx,cz,h in [(-.13,.986,.003),(-.055,.967,.004),(.05,.973,.0028),(.146,.992,.0042)]:
                displacement+=h*_bell(p.x-cx,.018)*_bell(p.z-cz,.035)*(front+.5*back)*free
            shoulder=joints['joint-'+side+'-shoulder'];elbow=joints['joint-'+side+'-elbow']
            axis=(elbow-shoulder).normalized();along=(p-shoulder).dot(axis)
            sleeve=max(0,min(1,(x-.16)/.055))
            displacement+=(.003*_bell(along-.080,.012)-.001*_bell(along-.060,.010))*sleeve*free
        else:
            knee=joints['joint-'+side+'-knee']
            # Denim has broader, heavier creases clustered around articulation
            # and the shoe, with a quiet thigh rather than embossed all-over noise.
            displacement+=.0014*free
            around=math.atan2(p.y-knee.y,(p.x-knee.x)*sign)
            knee_height=p.z-knee.z
            for height,amp,width,phase in [(-.043,.0042,.014,.0),(.012,.0035,.016,1.1),(.074,.0028,.019,2.0)]:
                center=height+.018*math.sin(around*1.3+phase+sign*.22)
                displacement+=amp*_bell(knee_height-center,width)*(.6+.4*math.sin(around+.5)**2)*free
            for height,amp in [(.137,.0040),(.189,.0030),(.236,.0020)]:
                displacement+=amp*_bell(p.z-height-.012*math.sin(around+height*8),.014)*free
            hip_front=max(0.,-normal.y)
            q=Vector((x,p.z))
            for a,b,h,w in [((.05,.87),(.19,.83),.004,.014),((.06,.82),(.18,.77),.003,.019)]:
                displacement+=_fold(q,a,b,h,w)*hip_front*free
        changes.append(p+normal*displacement)
    for vertex,position in zip(mesh.vertices,changes):vertex.co=position
    mesh.update()
    # Give cuffs/hem an actual thin inner wall. Solidify copies deform groups,
    # and stays inward so it cannot inflate the garment silhouette.
    bpy.context.view_layer.objects.active=obj
    wall=obj.modifiers.new('Fabric edge thickness 0.8mm','SOLIDIFY')
    wall.thickness=.0008;wall.offset=-1;wall.use_even_offset=True
    bpy.ops.object.modifier_apply(modifier=wall.name)
    # Boundary indices still refer to the original outer sheet after solidify.
    _opening_seams(obj,loops)
    obj['tailoring']='Rest-pose ease, bounded tension folds, weighted subdivision, 0.8mm fabric wall'
    return obj
