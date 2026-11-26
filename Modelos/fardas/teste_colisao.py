"""Percorre TODAS as animacoes AS_* e mede, em cada quadro amostrado:
 1) mosquete (SKM_Mosquete) atravessando a farda (sem contar maos/pele);
 2) roupa atravessando roupa: abas, cartucheira, bainha, sacola/cantil e mochila contra calca/polainas/tronco.
  blender -b Modelos/fardas/dados/fardas_gerado.blend --python Modelos/fardas/teste_colisao.py -- <FR|GB> [passo] [saida.json]
Profundidade = quanto o ponto esta do lado de dentro da superficie (cm). Ate ~0,5 cm nao aparece.
"""
import bpy, sys, os, json, math
import bmesh
from mathutils import Vector
from mathutils.bvhtree import BVHTree

ARGS = sys.argv[sys.argv.index("--") + 1:]
NAC = ARGS[0]
PASSO = int(ARGS[1]) if len(ARGS) > 1 else 3
SAIDA = ARGS[2] if len(ARGS) > 2 else None
sc = bpy.context.scene
arm = bpy.data.objects["SK_Soldado"]
arm.data.pose_position = 'POSE'
for pb in arm.pose.bones:
    for c in pb.constraints:
        c.mute = True
ob = bpy.data.objects["SK_Soldado_%s_LOD0" % NAC]
mosq = bpy.data.objects["SKM_Mosquete"]
partes = ob.data["eaw_partes"].split(",") if "eaw_partes" in ob.data else None
pid = [0] * len(ob.data.polygons)
ob.data.attributes["eaw_parte"].data.foreach_get("value", pid)
nomes = [partes[i] for i in pid]

GRUPO_ALVO = {"tronco", "manga_l", "manga_r", "calca", "polaina", "abas", "lapelas", "gola", "punho_l", "punho_r",
              "correame", "alcas", "cartucheira", "bainha", "mochila", "sacola", "barretina", "sapatos", "ombreira_l",
              "ombreira_r"}
PERNAS = {"calca", "polaina", "sapatos"}
SOLTAS = {"abas": PERNAS, "cartucheira": PERNAS, "bainha": PERNAS, "sacola": PERNAS, "mochila": {"barretina"}}

# vertices de cada peca solta (indices do LOD0)
vert_partes = {}
for poly, n in zip(ob.data.polygons, nomes):
    if n in SOLTAS:
        vert_partes.setdefault(n, set()).update(poly.vertices)


def malha_avaliada(o):
    dg = bpy.context.evaluated_depsgraph_get()
    e = o.evaluated_get(dg)
    me = e.to_mesh()
    vs = [o.matrix_world @ v.co for v in me.vertices]
    fs = [tuple(p.vertices) for p in me.polygons]
    e.to_mesh_clear()
    return vs, fs


def bvh_de(vs, fs, filtro):
    sel = [f for f, n in zip(fs, nomes) if n in filtro]
    return BVHTree.FromPolygons(vs, sel), sel


def profundidade(bvh, p, lim=0.06):
    """>0 = ponto do lado de dentro da superficie mais proxima (m)."""
    co, nrm, idx, d = bvh.find_nearest(p, lim)
    if co is None:
        return 0.0, None
    s = (p - co).dot(nrm)
    return (-s if s < 0 else 0.0), idx


def pose_de(acao, q):
    arm.animation_data.action = bpy.data.actions[acao]
    sc.frame_set(q)
    bpy.context.view_layer.update()
    return ({b.name: b.matrix_basis.copy() for b in arm.pose.bones},
            {b.name: b.matrix.copy() for b in arm.pose.bones})


TRONCO = set()


def _sub(b):
    TRONCO.add(b.name)
    for c in b.children:
        _sub(c)


_sub(arm.data.bones["spine_01"])
ARMA = {b.name for b in arm.data.bones if b.name.startswith("arma") or b.name == "vareta"}


def pose_em_camadas(cima, q_cima, baixo, q_baixo):
    """Como no jogo: pernas/quadril da animacao de andar, tronco e bracos da arma, mosquete preso ao peito."""
    bc, mc = pose_de(cima, q_cima)
    bb, mb = pose_de(baixo, q_baixo)
    arm.animation_data.action = None
    for b in arm.pose.bones:
        b.matrix_basis = bc[b.name] if (b.name in TRONCO or b.name in ARMA) else bb[b.name]
    bpy.context.view_layer.update()
    rel = mc["spine_03"].inverted() @ mc["arma"]
    arm.pose.bones["arma"].matrix = arm.pose.bones["spine_03"].matrix @ rel
    bpy.context.view_layer.update()


def casos():
    for an in sorted(a.name for a in bpy.data.actions if a.name.startswith("AS_")):
        a = bpy.data.actions[an]
        f0, f1 = int(a.frame_range[0]), int(a.frame_range[1])
        yield an, [(lambda q=q, an=an: pose_de(an, q)) for q in range(f0, f1 + 1, PASSO)], "Mosquete" in an
    for cima in ("AS_Mosquete_Guarda", "AS_Mosquete_GuardaBaioneta", "AS_Mosquete_Mira"):
        if cima not in bpy.data.actions:
            continue
        for baixo in sorted(a.name for a in bpy.data.actions if a.name.startswith(("AS_Trote_", "AS_Agachado_"))):
            a = bpy.data.actions[baixo]
            f0, f1 = int(a.frame_range[0]), int(a.frame_range[1])
            yield cima + "+" + baixo, [(lambda q=q, c=cima, b=baixo: pose_em_camadas(c, 1, b, q))
                                      for q in range(f0, f1 + 1, 2)], True


# ---------- capsulas das pernas (raio real da calca/polaina por trecho do osso, medido na pose de repouso)
SEGS = [(lado, osso) for lado in "lr" for osso in ("thigh_", "calf_")]
arm.data.pose_position = 'REST'
bpy.context.view_layer.update()
vs0 = [v.co.copy() for v in ob.data.vertices]
perna_v = set()
for poly, n in zip(ob.data.polygons, nomes):
    if n in ("calca", "polaina"):
        perna_v.update(poly.vertices)
NB = 12
RAIO = {}
for lado, osso in SEGS:
    b = arm.data.bones[osso + lado]
    h, t = arm.matrix_world @ b.head_local, arm.matrix_world @ b.tail_local
    ax = t - h
    L2 = ax.length_squared
    bins = [[] for _ in range(NB)]
    gi = ob.vertex_groups[osso + lado].index if (osso + lado) in ob.vertex_groups else -1
    for vi in perna_v:
        p = vs0[vi]
        if (p.x > 0) != (lado == "l"):
            continue
        w = sum(g.weight for g in ob.data.vertices[vi].groups if g.group == gi)
        if w < 0.5:
            continue
        u = (p - h).dot(ax) / L2
        if 0 <= u <= 1:
            bins[min(NB - 1, int(u * NB))].append((p - (h + ax * u)).length)
    rr = []
    for bb in bins:
        bb.sort()
        rr.append(bb[int(len(bb) * 0.85)] if bb else 0.0)
    RAIO[osso + lado] = rr
# raio do corpo nu (manequim) no mesmo trecho: separa o que ja vinha das animacoes do que a roupa causa
RAIO_CORPO = {}
for lado, osso in SEGS:
    b = arm.data.bones[osso + lado]
    h, t = arm.matrix_world @ b.head_local, arm.matrix_world @ b.tail_local
    ax = t - h
    bins = [[] for _ in range(NB)]
    for nome_c in ("Alpha_Surface", "Alpha_Joints"):
        oc = bpy.data.objects[nome_c]
        gidx = oc.vertex_groups[osso + lado].index
        for v in oc.data.vertices:
            if not any(g.group == gidx and g.weight > 0.5 for g in v.groups):
                continue
            p = oc.matrix_world @ v.co
            u = (p - h).dot(ax) / ax.length_squared
            if 0 <= u <= 1:
                bins[min(NB - 1, int(u * NB))].append((p - (h + ax * u)).length)
    RAIO_CORPO[osso + lado] = [sorted(bb)[int(len(bb) * 0.85)] if bb else 0.0 for bb in bins]
arm.data.pose_position = 'POSE'
bpy.context.view_layer.update()
print("RAIOS_CORPO", {k: [round(x * 100, 1) for x in v] for k, v in RAIO_CORPO.items()})
print("RAIOS", {k: [round(x * 100, 1) for x in v] for k, v in RAIO.items()})


def penetracao_pernas(p, tabela=None):
    tabela = tabela or RAIO
    melhor, onde = 0.0, ""
    for lado, osso in SEGS:
        pb = arm.pose.bones[osso + lado]
        h = arm.matrix_world @ pb.head
        t = arm.matrix_world @ pb.tail
        ax = t - h
        u = (p - h).dot(ax) / ax.length_squared
        if u < (0.15 if osso == "thigh_" else 0.0) or u > 1:
            continue        # o topo da coxa fica dentro do quadril (nao conta)
        r = tabela[osso + lado][min(NB - 1, int(u * NB))]
        if r <= 0:
            r = max(tabela[osso + lado])
        d = r - (p - (h + ax * u)).length
        if d > melhor:
            melhor, onde = d, osso + lado
    return melhor, onde


vert_soltas = {}
for poly, n in zip(ob.data.polygons, nomes):
    if n in ("abas", "cartucheira", "bainha", "sacola"):
        vert_soltas.setdefault(n, set()).update(poly.vertices)
vert_soltas = {k: sorted(v)[::2] for k, v in vert_soltas.items()}

res = {}
for an, quadros, com_mosquete in casos():
    reg = {"mosquete": 0.0, "mosquete_onde": "", "roupa": 0.0, "roupa_onde": "", "quadro_m": 0, "quadro_r": 0,
           "mosquete_corpo": 0.0}
    for q, aplicar in enumerate(quadros):
        aplicar()
        if com_mosquete:
            mv, _ = malha_avaliada(mosq)
            for p in mv[::2]:
                d, onde = penetracao_pernas(p)
                if d > reg["mosquete"]:
                    reg["mosquete"], reg["mosquete_onde"], reg["quadro_m"] = d, onde, q
                d2, _ = penetracao_pernas(p, RAIO_CORPO)
                reg["mosquete_corpo"] = max(reg["mosquete_corpo"], d2)
        vs, fs = malha_avaliada(ob)
        for parte, idx in vert_soltas.items():
            for vi in idx:
                d, onde = penetracao_pernas(vs[vi])
                if d > reg["roupa"]:
                    reg["roupa"], reg["roupa_onde"], reg["quadro_r"] = d, parte + "->" + onde, q
    reg["mosquete"] = round(reg["mosquete"] * 100, 1)
    reg["roupa"] = round(reg["roupa"] * 100, 1)
    reg["mosquete_corpo"] = round(reg["mosquete_corpo"] * 100, 1)
    res[an] = reg
    print("ACAO", an, reg, flush=True)

ruins_m = {k: (v["mosquete"], v["mosquete_corpo"], v["mosquete_onde"], v["quadro_m"]) for k, v in res.items() if v["mosquete"] > 0.8}
so_roupa = {k: v for k, v in ruins_m.items() if v[1] <= 0.3}
print("MOSQUETE_SO_PELA_ROUPA", json.dumps(so_roupa))
ruins_r = {k: (v["roupa"], v["roupa_onde"], v["quadro_r"]) for k, v in res.items() if v["roupa"] > 0.8}
print("RESUMO acoes=%d mosquete>0.8cm=%d roupa>0.8cm=%d" % (len(res), len(ruins_m), len(ruins_r)))
print("RUINS_M", json.dumps(ruins_m))
print("RUINS_R", json.dumps(ruins_r))
if SAIDA:
    json.dump(res, open(SAIDA, "w"), indent=1)
