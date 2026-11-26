import bpy, json, sys
from mathutils import Vector
from mathutils.bvhtree import BVHTree
out={}
arm=bpy.data.objects['SK_Soldado']
for n in ('Alpha_Surface','Alpha_Joints'):
    ob=bpy.data.objects[n]; me=ob.data
    gi={g.index:g.name for g in ob.vertex_groups}
    def dom(v):
        best=max(v.groups,key=lambda g:g.weight,default=None)
        return gi[best.group] if best else None
    cnt={}
    for p in me.polygons:
        b=dom(me.vertices[p.vertices[0]])
        cnt[b]=cnt.get(b,0)+len(p.vertices)-2
    out[n]=cnt
    out[n+'_mw']=[list(r) for r in ob.matrix_world]
# perfil do tronco por raios de fora para dentro
import bmesh
bm=bmesh.new()
for n in ('Alpha_Surface','Alpha_Joints'):
    t=bmesh.new(); t.from_mesh(bpy.data.objects[n].data); t.transform(bpy.data.objects[n].matrix_world)
    tm=bpy.data.meshes.new('t'); t.to_mesh(tm); bm.from_mesh(tm); t.free()
bvh=BVHTree.FromBMesh(bm)
import math
prof={}
for z in [0.85,0.95,1.0,1.05,1.1,1.2,1.3,1.38,1.45,1.5,1.55]:
    row=[]
    for k in range(16):
        a=2*math.pi*k/16; d=Vector((math.cos(a),math.sin(a),0))
        c=Vector((0,0.02,z)); o=c+d*0.4
        h=bvh.ray_cast(o,-d,0.4)
        row.append(round((h[0]-c).length,3) if h[0] else None)
    prof[z]=row
out['prof']=prof
open(sys.argv[-1],'w').write(json.dumps(out))
