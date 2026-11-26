"""Renderiza as fardas texturizadas em poses das animacoes (para conferir pesos, texturas e o mosquete).
  blender -b Modelos/fardas/dados/fardas_gerado.blend --python Modelos/fardas/preview_poses.py -- <saida_prefixo> <FR|GB> [acao:quadro ...]
"""
import bpy, sys, os, math
from mathutils import Vector

AQUI = os.path.dirname(os.path.abspath(__file__))
TEX = os.path.normpath(os.path.join(AQUI, "..", "..", "EmpiresAtWar", "Texturas", "Fardas"))
ARGS = sys.argv[sys.argv.index("--") + 1:]
SAIDA, NAC = ARGS[0], ARGS[1]
POSES = ARGS[2:] or ["AS_Mosquete_GuardaBaioneta:1", "AS_Mosquete_Mira:20", "AS_Sabre_Correr:5", "AS_Agachado_Frente:8",
                     "AS_Chute:14", "AS_Mosquete_Especial:30"]
sc = bpy.context.scene
arm = bpy.data.objects["SK_Soldado"]
arm.data.pose_position = 'POSE'
for pb in arm.pose.bones:
    for c in pb.constraints:
        c.mute = True

ob = bpy.data.objects["SK_Soldado_%s_LOD0" % NAC]
mostrar = {ob.name, "SKM_Mosquete"}
for o in sc.objects:
    if o.type == 'MESH':
        o.hide_render = o.name not in mostrar


def img(nome, cor):
    im = bpy.data.images.load(os.path.join(TEX, "T_Farda_%s_%s.png" % (NAC, nome)), check_existing=True)
    im.colorspace_settings.name = 'sRGB' if cor else 'Non-Color'
    return im


m = ob.data.materials[0]
m.use_nodes = True
nt = m.node_tree
nt.nodes.clear()
out = nt.nodes.new("ShaderNodeOutputMaterial")
bsdf = nt.nodes.new("ShaderNodeBsdfPrincipled")
nt.links.new(bsdf.outputs[0], out.inputs[0])
tc = nt.nodes.new("ShaderNodeTexImage"); tc.image = img("C", True)
tn = nt.nodes.new("ShaderNodeTexImage"); tn.image = img("N", False)
to = nt.nodes.new("ShaderNodeTexImage"); to.image = img("ORM", False)
nt.links.new(tc.outputs[0], bsdf.inputs["Base Color"])
sep = nt.nodes.new("ShaderNodeSeparateColor")
nt.links.new(to.outputs[0], sep.inputs[0])
nt.links.new(sep.outputs[1], bsdf.inputs["Roughness"])
nt.links.new(sep.outputs[2], bsdf.inputs["Metallic"])
# normal DirectX -> OpenGL (inverte o verde)
sn = nt.nodes.new("ShaderNodeSeparateColor")
nt.links.new(tn.outputs[0], sn.inputs[0])
inv = nt.nodes.new("ShaderNodeMath"); inv.operation = 'SUBTRACT'; inv.inputs[0].default_value = 1.0
nt.links.new(sn.outputs[1], inv.inputs[1])
cn = nt.nodes.new("ShaderNodeCombineColor")
nt.links.new(sn.outputs[0], cn.inputs[0]); nt.links.new(inv.outputs[0], cn.inputs[1]); nt.links.new(sn.outputs[2], cn.inputs[2])
nm = nt.nodes.new("ShaderNodeNormalMap")
nt.links.new(cn.outputs[0], nm.inputs["Color"])
nt.links.new(nm.outputs[0], bsdf.inputs["Normal"])

sc.render.engine = 'BLENDER_EEVEE'
try:
    sc.eevee.taa_render_samples = 16
except Exception:
    pass
sc.render.resolution_x, sc.render.resolution_y = 520, 760
sc.render.film_transparent = False
w = sc.world or bpy.data.worlds.new("W")
sc.world = w
w.use_nodes = True
bg = w.node_tree.nodes.get("Background")
if bg:
    bg.inputs[0].default_value = (0.42, 0.45, 0.5, 1)
    bg.inputs[1].default_value = 0.9
sol = bpy.data.objects.get("SOL_PREV")
if not sol:
    ld = bpy.data.lights.new("SOL_PREV", 'SUN'); ld.energy = 3.5
    sol = bpy.data.objects.new("SOL_PREV", ld); sc.collection.objects.link(sol)
sol.rotation_euler = (math.radians(50), math.radians(10), math.radians(-35))
for o in sc.objects:
    if o.type == 'LIGHT' and o.name != "SOL_PREV":
        o.hide_render = True

cam = bpy.data.objects["CAM_TESTE"]
sc.camera = cam
cam.data.lens = 60
cam.data.clip_start = 0.05
arquivos = []
for k, pq in enumerate(POSES):
    acao, q = pq.split(":")
    if acao not in bpy.data.actions:
        print("SEM ACAO", acao)
        continue
    arm.animation_data.action = bpy.data.actions[acao]
    sc.frame_set(int(q))
    bpy.context.view_layer.update()
    pel = arm.matrix_world @ arm.pose.bones["pelvis"].head
    alvo = Vector((pel.x, pel.y, max(0.75, pel.z - 0.05)))
    for nome, d in (("a", Vector((0.55, -1, 0.12))), ("b", Vector((-0.75, 0.9, 0.15)))):
        cam.location = alvo + d.normalized() * 3.3
        cam.rotation_euler = (alvo - cam.location).to_track_quat('-Z', 'Y').to_euler()
        sc.render.filepath = "%s_%d%s.png" % (SAIDA, k, nome)
        bpy.ops.render.render(write_still=True)
        arquivos.append(sc.render.filepath)
print("PREVIEW", arquivos)
