"""Gera as fardas (francesa e britanica, ~1805-1815) sobre o esqueleto do SK_Soldado. Tudo feito do zero.
Rodar em segundo plano (nao mexe no Soldado.blend; trabalha numa copia em memoria):
  blender -b Modelos/Soldado.blend --python Modelos/fardas/gerar_fardas.py -- <etapa> [args]
Etapas: preview <saida> <FR|GB>  |  tudo (gera as duas, exporta FBX/LODs e dados de textura)
"""
import bpy, bmesh, sys, os, math, importlib, json
from mathutils import Vector, Matrix, Euler, Quaternion

AQUI = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, AQUI)
import fardas_lib as L
importlib.reload(L)

ARGS = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
ETAPA = ARGS[0] if ARGS else "preview"

arm = bpy.data.objects[L.ARM]
arm.data.pose_position = 'REST'
bpy.context.view_layer.update()
corpo = L.Corpo()
B = {b.name: (arm.matrix_world @ b.head_local, arm.matrix_world @ b.tail_local) for b in arm.data.bones}
PI = math.pi
V = Vector

# Materiais por "papel" (o atlas de textura e pintado depois a partir deste id).
# Cores so para o preview no Blender.
PAPEIS = {
    "FR": {
        "casaca": (0.06, 0.08, 0.24), "vermelho": (0.60, 0.07, 0.07), "branco": (0.86, 0.86, 0.82),
        "calca": (0.84, 0.83, 0.78), "polaina": (0.04, 0.04, 0.045), "sapato": (0.03, 0.03, 0.03),
        "correia": (0.90, 0.89, 0.84), "couro": (0.05, 0.045, 0.04), "latao": (0.75, 0.55, 0.20),
        "pele": (0.80, 0.60, 0.48), "feltro": (0.035, 0.035, 0.04), "pelo": (0.45, 0.30, 0.18),
        "capote": (0.55, 0.53, 0.48), "pompom": (0.10, 0.45, 0.20), "cocar": (0.15, 0.20, 0.55),
        "forro": (0.86, 0.86, 0.82),
    },
    "GB": {
        "casaca": (0.62, 0.08, 0.07), "vermelho": (0.10, 0.13, 0.36), "branco": (0.90, 0.90, 0.86),
        "calca": (0.33, 0.34, 0.38), "polaina": (0.04, 0.04, 0.045), "sapato": (0.03, 0.03, 0.03),
        "correia": (0.93, 0.92, 0.88), "couro": (0.05, 0.045, 0.04), "latao": (0.75, 0.55, 0.20),
        "pele": (0.80, 0.60, 0.48), "feltro": (0.035, 0.035, 0.04), "mochila": (0.05, 0.05, 0.055),
        "capote": (0.42, 0.42, 0.43), "cantil": (0.20, 0.28, 0.45), "cordao": (0.92, 0.92, 0.88),
        "pluma": (0.92, 0.92, 0.90), "pluma2": (0.60, 0.06, 0.06), "forro": (0.90, 0.90, 0.86),
        "sacola": (0.80, 0.77, 0.68),
    },
}


def W(**k):
    return dict(k)


# ---------------------------------------------------------------- medidas da cabeca
def medir_cabeca():
    c = V((0, 0.0, 1.70))
    rs = []
    for k in range(24):
        a = 2 * PI * k / 24
        d = V((math.sin(a), -math.cos(a), 0))
        r = corpo.ray_out_in(c, d, 0.3)
        if r:
            rs.append((a, r))
    hit = corpo.bvh.ray_cast(V((0, 0.0, 2.3)), V((0, 0, -1)), 1.0)
    topo = hit[0].z if hit[0] else 1.80
    hit2 = corpo.bvh.ray_cast(V((0, -0.4, 1.62)), V((0, 1, 0)), 1.0)
    queixo_y = hit2[0].y if hit2[0] else -0.09
    lst = sorted(r for _, r in rs)
    rx = max(r * abs(math.sin(a)) for a, r in rs)
    ry = max(r * abs(math.cos(a)) for a, r in rs)
    yc = 0.5 * (max(r * -math.cos(a) for a, r in rs) - max(r * math.cos(a) for a, r in rs))
    return dict(rmax=lst[len(lst) * 3 // 4], rs=rs, topo=topo, frente_y=queixo_y, rx=rx, ry=ry)


def ovalizar(p, n0, cy, sx, sy):
    """Achata em X (e Y) tudo o que foi criado depois do vertice n0 (barretina oval)."""
    for k in range(n0, len(p.v)):
        q = p.v[k]
        p.v[k] = V((q.x * sx, cy + (q.y - cy) * sy, q.z))


CAB = medir_cabeca()


# ---------------------------------------------------------------- casaca
def tronco(p, nac, M, z0=0.97, z1=1.43):
    zs = [z0 + (z1 - z0) * i / 17 for i in range(18)]
    cs = [V((0, 0.03, z)) for z in zs]

    def clamp(i, a, r):
        z = zs[i]
        if z > 1.28 and abs(math.sin(a)) > 0.6:
            r = min(r, 0.175 + 0.02 * (1.43 - z))
        return r

    return L.tubo(corpo, p, cs, [V((0, 0, 1))] * 18, 32, M("casaca"), folga=0.018, rmax=0.26, clamp=clamp, dil=4, suav=5,
                  espessura=0.006, mat_dentro=M("forro"), bordas=(True, False))


def manga(p, lado, M, x_fim=0.70):
    s = 1 if lado == "l" else -1
    ua, ub = B["upperarm_" + lado]
    la, lb = B["lowerarm_" + lado]
    pts = [V((0.0, 0.035, 1.415)), V((s * 0.10, 0.045, 1.43)), ua, ub]
    pts.append(la.lerp(lb, (abs(x_fim) - abs(la.x)) / (abs(lb.x) - abs(la.x))))
    cs = L.polilinha(pts, 30)
    ax = L.tangentes(cs)

    def rmax(i, a):
        return 0.22 if abs(cs[i].x) > 0.16 else 0.16

    def clamp(i, a, r):
        x = abs(cs[i].x)
        u, v = L.frame_from(ax[i])
        d = u * math.cos(a) + v * math.sin(a)
        if x < 0.2:
            if d.z < -0.3:
                r = min(r, 0.06)
            if d.z > 0.3 and x < 0.07:
                r = min(r, 0.075)
        return r

    return L.tubo(corpo, p, cs, ax, 20, M("casaca"), folga=0.016, rmax=rmax, clamp=clamp, dil=2, suav=3), cs, ax


def punho(p, lado, M, x0, x1, folga=0.026):
    la, lb = B["lowerarm_" + lado]
    f = lambda x: la.lerp(lb, (abs(x) - abs(la.x)) / (abs(lb.x) - abs(la.x)))
    cs = L.polilinha([f(x0), f(x1)], 5)
    ax = L.tangentes(cs)
    L.tubo(corpo, p, cs, ax, 20, M("vermelho"), folga=folga, rmax=0.15, dil=1, suav=2,
           espessura=0.005, mat_dentro=M("vermelho"), bordas=(False, True))


def gola(p, M, z0=1.468, z1=1.545, folga=0.014, abre=0.0):
    zs = [z0 + (z1 - z0) * i / 4 for i in range(5)]
    cs = [V((0, 0.032 - 0.012 * i / 4, z)) for i, z in enumerate(zs)]

    def raio(i, a, r):
        if i == 0:
            r = max(r, 0.085)
        elif i == 1:
            r = max(r, 0.072)
        return r

    ang0, ang1, fech = 0.0, 2 * PI, True
    if abre:
        ang0, ang1, fech = abre, 2 * PI - abre, False
    L.tubo(corpo, p, cs, [V((0, 0, 1))] * 5, 28, M("vermelho"), folga=folga, rmax=0.13, dil=1, suav=2,
           raio_fn=raio, espessura=0.006, mat_dentro=M("vermelho"), bordas=(False, True),
           ang0=ang0, ang1=ang1, fechado=fech)


def abas(p, M, z_fim, largura=0.85, folga0=0.022, abre=0.12):
    """Abas (caudas) da casaca nas costas: dois paineis com espessura (forro branco por dentro)."""
    z0 = 1.00
    zs = [z0 - (z0 - z_fim) * i / 9 for i in range(10)]
    cs = [V((0, 0.03, z)) for z in zs]
    ax = [V((0, 0, -1))] * 10   # descendo: frame u = frente(-Y)? -> usar angulos medidos abaixo

    def fol(i, a):
        t = i / 9
        return folga0 + 0.080 * t * t

    def wfn_lado(sinal):
        def wfn(i, a, pnt):
            t = i / 9
            k = 0.4 * t
            lado = "l" if pnt.x > 0 else "r"
            return {"pelvis": 1 - k, "thigh_" + lado: k}
        return wfn

    # eixo para baixo inverte o frame: com ax=-Z, u=-Y(frente), v = -Z x -Y = -X. Costas = a=PI.
    for sg in (1, -1):
        def faixa(i, sg=sg):
            t = i / 9
            lg = largura * (1 - 0.42 * t)
            ab = abre + 0.05 * t
            return (PI + ab, PI + lg) if sg > 0 else (PI - lg, PI - ab)
        L.tubo(corpo, p, cs, ax, 7, M("casaca"), folga=fol, rmax=0.30, dil=2, suav=2, faixa=faixa,
               fechado=False, espessura=0.007, mat_dentro=M("forro"), wfn=wfn_lado(1), bordas=(False, True))


def lapelas(p, M, z0=1.0, z1=1.44, larg=0.50):
    zs = [z0 + (z1 - z0) * i / 9 for i in range(10)]
    cs = [V((0, 0.03, z)) for z in zs]

    def clamp(i, a, r):
        return r

    L.tubo(corpo, p, cs, [V((0, 0, 1))] * 10, 12, M("branco"), folga=0.024, rmax=0.26, dil=3, suav=3,
           ang0=-larg, ang1=larg, fechado=False, espessura=0.004, mat_dentro=M("branco"), clamp=clamp)


def ombreira(p, lado, M, mat, comprimento=0.12, largura=0.045):
    s = 1 if lado == "l" else -1
    x0, x1 = 0.07, 0.07 + comprimento
    bvh = L.bvh_da_peca(p)
    pts = []
    for k in range(6):
        x = s * (x0 + (x1 - x0) * k / 5)
        h = bvh.ray_cast(V((x, 0.045, 1.8)), V((0, 0, -1)), 1.0)
        pts.append(h[0] if h[0] else V((x, 0.045, 1.47)))
    base = len(p.v)
    rows = []
    for k, q in enumerate(pts):
        w = corpo.weights_at(q - V((0, 0, 0.02)))
        a = p.add_v(q + V((0, -largura / 2, 0.004)), w)
        b = p.add_v(q + V((0, largura / 2, 0.004)), w)
        c = p.add_v(q + V((0, -largura / 2, -0.002)), w)
        d = p.add_v(q + V((0, largura / 2, -0.002)), w)
        rows.append((a, b, c, d, abs(q.x)))
    for k in range(5):
        A, Bq = rows[k], rows[k + 1]
        f = (A[0], A[1], Bq[1], Bq[0]) if s > 0 else (A[0], Bq[0], Bq[1], A[1])
        p.add_f(f, mat, [(A[4], 0), (A[4], largura), (Bq[4], largura), (Bq[4], 0)])
        p.add_f((A[2], A[0], Bq[0], Bq[2]) if s > 0 else (A[2], Bq[2], Bq[0], A[0]), mat, None)
        p.add_f((A[1], A[3], Bq[3], Bq[1]) if s > 0 else (A[1], Bq[1], Bq[3], A[3]), mat, None)
    return pts


# ---------------------------------------------------------------- pernas
def calca(p, M, z_fim=0.30, folga_fim=0.012):
    zs = [0.86 + (1.10 - 0.86) * i / 7 for i in range(8)]
    cs = [V((0, 0.01, z)) for z in zs]
    L.tubo(corpo, p, cs, [V((0, 0, 1))] * 8, 32, M("calca"), folga=0.010, rmax=0.3, dil=2, suav=2)
    for lado in "lr":
        ta, tb = B["thigh_" + lado]
        ca, cb = B["calf_" + lado]
        top = ta + V((0, 0, 0.05))
        t = (ca.z - z_fim) / (ca.z - cb.z)
        cs = L.polilinha([top, tb, ca.lerp(cb, t)], 22)
        ax = [(-x) for x in L.tangentes(cs)]
        n = len(cs)
        L.tubo(corpo, p, cs, ax, 18, M("calca"), folga=lambda i, a: 0.012 + (folga_fim - 0.012) * max(0, (i - n * 0.6) / (n * 0.4)),
               rmax=0.13, dil=2, suav=2, espessura=0.004 if folga_fim > 0.015 else 0.0, mat_dentro=M("calca"),
               bordas=(True, False))


def polaina(p, M, z1, folga=0.020):
    """Polaina preta do tornozelo ate z1, com abotoamento do lado de fora (pintado)."""
    for lado in "lr":
        ca, cb = B["calf_" + lado]
        top = ca.lerp(cb, (ca.z - z1) / (ca.z - cb.z))
        cs = L.polilinha([V((cb.x, cb.y + 0.005, 0.075)), V((cb.x, cb.y, 0.15)), top], 12 if z1 > 0.3 else 6)
        ax = L.tangentes(cs)
        n = len(cs)
        L.tubo(corpo, p, cs, ax, 16, M("polaina"), folga=lambda i, a: folga + (0.012 if i == n - 1 else 0),
               rmax=0.13, dil=1, suav=2, espessura=0.004, mat_dentro=M("polaina"), bordas=(False, True))


def sapatos(p, M):
    for lado in "lr":
        fa, fb = B["foot_" + lado]
        ba, bb = B["ball_" + lado]
        c = V((fa.x, (fa.y + bb.y) * 0.5 + 0.01, 0.055))

        def wsapato(q, lado=lado, ba=ba, fa=fa):
            # sapato rigido: do calcanhar ate a dobra = pe; bico = osso da ponta; cano perto do tornozelo mistura a canela
            t = min(1.0, max(0.0, (ba.y + 0.015 - q.y) / 0.03))
            w = {"foot_" + lado: 1.0 - t, "ball_" + lado: t}
            if q.z > 0.095:
                k = min(0.5, (q.z - 0.095) / 0.04 * 0.5)
                w = {g: x * (1 - k) for g, x in w.items()}
                w["calf_" + lado] = k
            return w

        L.bolha(corpo, p, c, 10, 16, M("sapato"), folga=0.010, rmax=0.25, zmin=0.0, suav=2, escala=(1.0, 2.2, 1.0),
                w=wsapato)


# ---------------------------------------------------------------- correame (por cima da casaca)
def correame(p, M, nac):
    """Duas correias cruzadas: esquerda->cartucheira (quadril direito, atras) e direita->bainha (quadril esq.)."""
    bvh = L.bvh_da_peca(p)
    larg = 0.062 if nac == "FR" else 0.058
    pontos = {}
    for lado, s in (("l", 1), ("r", -1)):
        ombro = V((s * 0.115, 0.04, 1.48))
        quadril = V((-s * 0.170, 0.07, 1.03))
        meio = (ombro + quadril) * 0.5
        eixo = (ombro - quadril).normalized()
        frente = V((0, -1, 0))
        nplano = eixo.cross(frente).normalized()
        c = meio + V((0, 0.0, 0))
        pts = L.cinta(corpo, p, bvh, c, nplano, larg, 56, M("correia"), folga=0.006 if s > 0 else 0.010,
                      alt=0.003, rmax=0.5, ref=eixo)
        pontos[lado] = pts
    return pontos


def cinto_mochila(p, M, bvh):
    """Alcas da mochila: passam por cima do ombro e voltam por baixo do braco."""
    for lado, s in (("l", 1), ("r", -1)):
        c = V((s * 0.165, 0.045, 1.37))
        nplano = V((1, 0, 0))
        L.cinta(corpo, p, bvh, c, nplano, 0.038, 24, M("correia"), folga=0.013, alt=0.003, rmax=0.14,
                ref=V((0, 0, 1)), arco=(-1.3, 2.2))


# ---------------------------------------------------------------- equipamento rigido
WP = {"pelvis": 1.0}
WCART = {"pelvis": 0.8, "thigh_r": 0.2}
WS3 = {"spine_03": 0.75, "spine_02": 0.25}
WH = {"head": 1.0}


def cartucheira(p, M, nac):
    if nac == "FR":
        c, tam = V((-0.120, 0.185, 0.995)), (0.22, 0.075, 0.15)
    else:
        c, tam = V((-0.120, 0.185, 1.0)), (0.20, 0.07, 0.14)
    rot = Euler((math.radians(-8), 0, math.radians(-38)), 'XYZ').to_matrix()
    L.caixa(p, c, tam, M("couro"), WCART, rot=rot, bevel=0.008, segs=1)
    # tampa (aba) um pouco maior por cima, e emblema de latao
    aba = c + rot @ V((0, 0.040, 0.012))
    L.caixa(p, aba, (tam[0] + 0.012, 0.008, tam[2] + 0.012), M("couro"), WCART, rot=rot, bevel=0.003)
    em = c + rot @ V((0, 0.046, 0.015))
    if nac == "FR":
        L.cilindro(p, em, em + rot @ V((0, 0.004, 0)), 0.03, 0.03, 10, M("latao"), WCART)
    else:
        L.cilindro(p, em, em + rot @ V((0, 0.004, 0)), 0.035, 0.035, 10, M("latao"), WCART)


def bainha(p, M):
    """Bainha da baioneta (couro preto, ponteira de latao) no quadril esquerdo, inclinada para tras."""
    top = V((0.188, 0.095, 0.985))
    fundo = top + V((0.030, 0.20, -0.40)).normalized() * 0.44

    def wb(q):
        t = max(0.0, min(1.0, (top.z - q.z) / (top.z - fundo.z)))
        return {"pelvis": 1 - 0.75 * t, "thigh_l": 0.75 * t}

    n0 = len(p.v)
    L.cilindro(p, top, fundo, 0.018, 0.012, 8, M("couro"), WP, rings=3)
    L.cilindro(p, fundo.lerp(top, 0.12), fundo + (fundo - top).normalized() * 0.01, 0.0135, 0.008, 8, M("latao"), WP)
    for k in range(n0, len(p.v)):
        p.w[k] = wb(p.v[k])
    # porta-baioneta (pala de couro branco presa na correia)
    L.caixa(p, top + V((0.0, 0.0, 0.03)), (0.028, 0.055, 0.09), M("correia"), WP,
            rot=Euler((math.radians(-26), 0, 0)).to_matrix())


def costas_y(p, z=1.27):
    bvh = L.bvh_da_peca(p)
    h = bvh.ray_cast(V((0, 1.0, z)), V((0, -1, 0)), 2.0)
    return h[0].y if h[0] else 0.21


def mochila_fr(p, M):
    c = V((0, costas_y(p) + 0.012 + 0.055, 1.255))
    tam = (0.34, 0.11, 0.31)
    L.caixa(p, c, tam, M("pelo"), WS3, bevel=0.025, segs=2)
    # aba de couro de vaca na frente (de costas) - mesma cor: pelo; correias brancas
    for x in (-0.10, 0.10):
        L.caixa(p, c + V((x, 0.058, 0.0)), (0.028, 0.006, 0.30), M("correia"), WS3)
    # capote enrolado em cima
    L.cilindro(p, c + V((-0.21, 0.0, 0.21)), c + V((0.21, 0.0, 0.21)), 0.065, 0.065, 14, M("capote"), WS3, rings=4)
    for x in (-0.13, 0.13):
        L.cilindro(p, c + V((x - 0.014, 0, 0.21)), c + V((x + 0.014, 0, 0.21)), 0.069, 0.069, 14, M("correia"), WS3)


def mochila_gb(p, M):
    c = V((0, costas_y(p) + 0.012 + 0.060, 1.255))
    tam = (0.36, 0.12, 0.30)
    L.caixa(p, c, tam, M("mochila"), WS3, bevel=0.012, segs=1)
    for x in (-0.11, 0.11):
        L.caixa(p, c + V((x, 0.062, 0.0)), (0.026, 0.006, 0.29), M("correia"), WS3)
    # cobertor/capote cinza enrolado em cima e marmita azul presa atras
    L.cilindro(p, c + V((-0.20, 0.0, 0.205)), c + V((0.20, 0.0, 0.205)), 0.058, 0.058, 14, M("capote"), WS3, rings=4)
    for x in (-0.12, 0.12):
        L.cilindro(p, c + V((x - 0.013, 0, 0.205)), c + V((x + 0.013, 0, 0.205)), 0.062, 0.062, 14, M("correia"), WS3)
    L.cilindro(p, c + V((0, 0.06, 0.04)), c + V((0, 0.11, 0.04)), 0.080, 0.080, 16, M("cantil"), WS3)


def cantil_sacola_gb(p, M):
    """Sacola de lona branca e cantil azul de madeira, no lado esquerdo atras."""
    WPq = {"pelvis": 0.7, "thigh_l": 0.3}
    rot = Euler((0, 0, math.radians(42))).to_matrix()
    L.caixa(p, V((0.115, 0.200, 0.965)), (0.24, 0.045, 0.20), M("sacola"), WPq, rot=rot, bevel=0.02, segs=2)
    L.cilindro(p, V((0.060, 0.228, 1.045)), V((0.040, 0.268, 1.045)), 0.072, 0.072, 16, M("cantil"), WPq)


# ---------------------------------------------------------------- barretinas
def barretina_fr(p, M):
    n0 = len(p.v)
    _barretina_fr(p, M)
    r0 = CAB["ry"] + 0.012
    ovalizar(p, n0, 0.0, (CAB["rx"] + 0.012) / r0, 1.0)


def barretina_gb(p, M):
    n0 = len(p.v)
    _barretina_gb(p, M)
    r0 = CAB["ry"] + 0.012
    ovalizar(p, n0, 0.0, (CAB["rx"] + 0.012) / r0, 1.0)


def _barretina_fr(p, M):
    r0 = CAB["ry"] + 0.012
    zb = 1.712
    h = 0.195
    r1 = r0 + 0.022
    cy = 0.0
    eixo0, eixo1 = V((0, cy, zb)), V((0, cy, zb + h))
    L.cilindro(p, eixo0, eixo1, r0, r1, 24, M("feltro"), WH, tampa=True, rings=4)
    # faixa de couro em cima e embaixo
    L.cilindro(p, eixo1 - V((0, 0, 0.032)), eixo1 + V((0, 0, 0.002)), r1 - 0.0035 + 0.003, r1 + 0.003, 24, M("couro"), WH, tampa=False)
    L.cilindro(p, eixo0 - V((0, 0, 0.002)), eixo0 + V((0, 0, 0.020)), r0 + 0.003, r0 + 0.0055, 24, M("couro"), WH, tampa=False)
    # tampo de couro
    L.cilindro(p, eixo1 + V((0, 0, 0.0)), eixo1 + V((0, 0, 0.004)), r1 + 0.004, r1 + 0.001, 24, M("couro"), WH)
    # pala (visera) inclinada para baixo
    bm = bmesh.new()
    pts = []
    for k in range(13):
        a = -1.25 + 2.5 * k / 12
        d = V((math.sin(a), -math.cos(a), 0))
        pts.append((V((0, cy, zb + 0.004)) + d * (r0 + 0.002), d))
    vs_in = [bm.verts.new(q) for q, d in pts]
    ext = []
    for k, (q, d) in enumerate(pts):
        a = -1.25 + 2.5 * k / 12
        L_ = 0.055 * math.cos(a * 0.9) ** 0.7
        ext.append(bm.verts.new(q + d * L_ + V((0, 0, -0.026 * L_ / 0.055))))
    top_in = vs_in
    bot_in = [bm.verts.new(v.co - V((0, 0, 0.003))) for v in vs_in]
    bot_ex = [bm.verts.new(v.co - V((0, 0, 0.003))) for v in ext]
    for k in range(12):
        bm.faces.new((top_in[k], ext[k], ext[k + 1], top_in[k + 1]))
        bm.faces.new((bot_in[k + 1], bot_ex[k + 1], bot_ex[k], bot_in[k]))
        bm.faces.new((ext[k + 1], ext[k], bot_ex[k], bot_ex[k + 1]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    L.bm_para_peca(bm, p, M("couro"), WH)
    # placa de latao (losango com aguia, pintada) na frente
    rf = r0 + (r1 - r0) * 0.45
    zc = zb + 0.085
    pl = V((0, cy - rf - 0.003, zc))
    bm = bmesh.new()
    vs = [bm.verts.new(pl + V(q)) for q in ((0, 0, 0.068), (0.050, 0, 0.0), (0, 0, -0.062), (-0.050, 0, 0.0))]
    vs2 = [bm.verts.new(v.co + V((0, 0.004, 0))) for v in vs]
    bm.faces.new(vs)
    for k in range(4):
        bm.faces.new((vs[k], vs2[k], vs2[(k + 1) % 4], vs[(k + 1) % 4]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    # entortar para seguir o cilindro
    for v in bm.verts:
        x = v.co.x
        v.co.y = cy - math.sqrt(max(0.0, (rf + 0.003 + (0.004 if v in vs2 else 0)) ** 2 - x * x)) + (0.004 if v in vs2 else 0)
        v.co.y = cy - math.sqrt(max(0.0, (rf + 0.003) ** 2 - x * x)) + (0.004 if v in vs2 else 0.0)
    L.bm_para_peca(bm, p, M("latao"), WH)
    # cocar tricolor + pompom (lentilha) no alto da frente
    ctop = V((0, cy - r1 + 0.001, zb + h - 0.040))
    L.cilindro(p, ctop + V((0, 0.006, 0)), ctop - V((0, 0.004, 0)), 0.033, 0.033, 16, M("cocar"), WH)
    L.esfera(p, ctop + V((0, 0.006, 0.050)), (0.036, 0.020, 0.032), 6, 12, M("pompom"), WH)
    # rosetas de latao dos lados (jugular presa)
    for s in (1, -1):
        q = V((s * (r0 + 0.004), cy, zb + 0.018))
        L.cilindro(p, q, q + V((s * 0.004, 0, 0)), 0.016, 0.016, 10, M("latao"), WH)
        # escamas da jugular levantadas, presas atras da barretina
        a0 = q + V((0, 0.0, 0.01))
        for k in range(6):
            a = 0.25 + k * 0.32
            pa = V((s * (r0 + 0.006) * math.cos(a), cy + (r0 + 0.006) * math.sin(a), zb + 0.022 + 0.012 * k))
            L.caixa(p, pa, (0.016, 0.016, 0.006), M("latao"), WH, rot=Euler((0, 0, s * a)).to_matrix())


def _barretina_gb(p, M):
    """Barretina 'Belgic' (1812): cilindro com frente falsa mais alta, placa grande, cordoes e pluma do lado."""
    r0 = CAB["ry"] + 0.012
    zb = 1.712
    h = 0.170
    r1 = r0 + 0.004
    cy = 0.0
    e0, e1 = V((0, cy, zb)), V((0, cy, zb + h))
    L.cilindro(p, e0, e1, r0, r1, 24, M("feltro"), WH, tampa=True, rings=4)
    L.cilindro(p, e0 - V((0, 0, 0.002)), e0 + V((0, 0, 0.018)), r0 + 0.003, r0 + 0.004, 24, M("couro"), WH, tampa=False)
    # frente falsa
    bm = bmesh.new()
    na = 14
    rows = []
    for k in range(na + 1):
        a = -1.05 + 2.10 * k / na
        d = V((math.sin(a), -math.cos(a), 0))
        ztop = zb + h + 0.070 * (math.cos(a * 1.4) ** 0.8)
        col = []
        for i in range(5):
            z = zb + 0.06 + (ztop - zb - 0.06) * i / 4
            rr = r0 + (r1 - r0) * (z - zb) / h + 0.003
            col.append(bm.verts.new(V((0, cy, z)) + d * rr))
        rows.append(col)
    inner = [[bm.verts.new(v.co - V((0, 0, 0)) + (v.co - V((0, cy, v.co.z))).normalized() * -0.004) for v in col] for col in rows]
    for k in range(na):
        for i in range(4):
            bm.faces.new((rows[k][i], rows[k + 1][i], rows[k + 1][i + 1], rows[k][i + 1]))
            bm.faces.new((inner[k][i + 1], inner[k + 1][i + 1], inner[k + 1][i], inner[k][i]))
        bm.faces.new((rows[k][4], rows[k + 1][4], inner[k + 1][4], inner[k][4]))
    for k in (0, na):
        for i in range(4):
            bm.faces.new((rows[k][i], rows[k][i + 1], inner[k][i + 1], inner[k][i]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    L.bm_para_peca(bm, p, M("feltro"), WH)
    # pala
    bm = bmesh.new()
    top_in, ext = [], []
    for k in range(13):
        a = -1.25 + 2.5 * k / 12
        d = V((math.sin(a), -math.cos(a), 0))
        q = V((0, cy, zb + 0.004)) + d * (r0 + 0.002)
        L_ = 0.052 * math.cos(a * 0.9) ** 0.7
        top_in.append(bm.verts.new(q))
        ext.append(bm.verts.new(q + d * L_ + V((0, 0, -0.022 * L_ / 0.052))))
    bot_in = [bm.verts.new(v.co - V((0, 0, 0.003))) for v in top_in]
    bot_ex = [bm.verts.new(v.co - V((0, 0, 0.003))) for v in ext]
    for k in range(12):
        bm.faces.new((top_in[k], ext[k], ext[k + 1], top_in[k + 1]))
        bm.faces.new((bot_in[k + 1], bot_ex[k + 1], bot_ex[k], bot_in[k]))
        bm.faces.new((ext[k + 1], ext[k], bot_ex[k], bot_ex[k + 1]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    L.bm_para_peca(bm, p, M("couro"), WH)
    # placa de latao (escudo com coroa e GR, pintado)
    rf = r0 + 0.008
    pl_c = V((0, cy, zb + 0.112))
    bm = bmesh.new()
    nw, nh = 6, 6
    grid = []
    for i in range(nh + 1):
        row = []
        for j in range(nw + 1):
            x = -0.045 + 0.090 * j / nw
            z = -0.062 + 0.124 * i / nh
            y = cy - math.sqrt(max(0.0, rf * rf - x * x))
            row.append(bm.verts.new(V((x, y, pl_c.z + z))))
        grid.append(row)
    for i in range(nh):
        for j in range(nw):
            bm.faces.new((grid[i][j], grid[i][j + 1], grid[i + 1][j + 1], grid[i + 1][j]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    ext_ = bmesh.ops.extrude_face_region(bm, geom=list(bm.faces))
    for v in [g for g in ext_["geom"] if isinstance(g, bmesh.types.BMVert)]:
        v.co.y += 0.003
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    L.bm_para_peca(bm, p, M("latao"), WH)
    # cordoes brancos em festao na frente
    pts = []
    for k in range(17):
        t = k / 16
        a = -1.35 + 2.7 * t
        d = V((math.sin(a), -math.cos(a), 0))
        z = zb + 0.058 - 0.026 * math.sin(PI * t)
        pts.append(V((0, cy, z)) + d * (r0 + 0.012))
    for k in range(16):
        L.cilindro(p, pts[k], pts[k + 1], 0.0045, 0.0045, 6, M("cordao"), WH, tampa=False)
    # pluma branca sobre vermelho do lado esquerdo + roseta preta
    base = V((0.62 * r1, cy - 0.78 * r1, zb + h - 0.030))
    L.cilindro(p, base, base + V((0, 0, 0.055)), 0.020, 0.026, 8, M("pluma2"), WH, rings=1)
    L.cilindro(p, base + V((0, 0, 0.055)), base + V((0, 0, 0.150)), 0.026, 0.016, 8, M("pluma"), WH, rings=2)
    L.esfera(p, base + V((0, 0, 0.150)), (0.016, 0.016, 0.014), 4, 8, M("pluma"), WH)
    roseta = V((0.58 * r1, cy - 0.82 * r1 - 0.004, zb + h - 0.045))
    L.cilindro(p, roseta + V((0, 0.004, 0)), roseta - V((0, 0.004, 0)), 0.024, 0.024, 12, M("couro"), WH)


# ---------------------------------------------------------------- cabeca e maos (do manequim, pele)
OSSOS_CABECA = {"head", "neck_01"}
DEDOS = {"hand", "thumb_01", "thumb_02", "thumb_03", "index_01", "index_02", "index_03", "middle_01", "middle_02",
         "middle_03", "ring_01", "ring_02", "ring_03", "pinky_01", "pinky_02", "pinky_03"}


def pele(p, M):
    for n in L.BODY:
        ob = bpy.data.objects[n]
        L.copiar_partes(p, ob, OSSOS_CABECA, M("pele"), decimar=1.0 if n == "Alpha_Surface" else 0.5)
        L.copiar_partes(p, ob, {d + "_l" for d in DEDOS} | {d + "_r" for d in DEDOS}, M("pele"), decimar=0.38)


# ---------------------------------------------------------------- montagem
def construir(nac):
    cores = PAPEIS[nac]
    mats = {k: L.material("%s_%s" % (nac, k), c) for k, c in cores.items()}
    M = lambda k: "%s_%s" % (nac, k)
    col = bpy.data.collections.new("Farda_" + nac)
    bpy.context.scene.collection.children.link(col)
    p = L.Peca("SK_Farda_" + nac)

    def P(nome, fn, *a, **k):
        p.parte_atual = nome
        return fn(p, *a, **k)

    P("tronco", tronco, nac, M)
    for lado in "lr":
        P("manga_" + lado, manga, lado, M)
    if nac == "FR":
        P("calca", calca, M, z_fim=0.40)
        P("polaina", polaina, M, z1=0.53, folga=0.028)
        P("abas", abas, M, z_fim=0.76)
        P("lapelas", lapelas, M)
        P("gola", gola, M)
        for lado in "lr":
            P("punho_" + lado, punho, lado, M, 0.615, 0.705)
            P("ombreira_" + lado, ombreira, lado, M, M("casaca"))
    else:
        P("calca", calca, M, z_fim=0.085, folga_fim=0.024)
        P("polaina", polaina, M, z1=0.13)
        P("abas", abas, M, z_fim=0.84, largura=0.80)
        P("gola", gola, M, abre=0.0)
        for lado in "lr":
            P("punho_" + lado, punho, lado, M, 0.625, 0.705)
            P("ombreira_" + lado, ombreira, lado, M, M("casaca"))
    P("sapatos", sapatos, M)
    bvh_corpo = L.bvh_da_peca(p)
    P("correame", correame, M, nac)
    P("alcas", cinto_mochila, M, bvh_corpo)
    P("cartucheira", cartucheira, M, nac)
    P("bainha", bainha, M)
    if nac == "FR":
        P("mochila", mochila_fr, M)
        P("barretina", barretina_fr, M)
    else:
        P("mochila", mochila_gb, M)
        P("sacola", cantil_sacola_gb, M)
        P("barretina", barretina_gb, M)
    P("pele", pele, M)
    ob = L.criar_objeto(p, corpo, [mats[k] for k in cores], col, arm)
    return col, mats, ob


def preview(colecao, saida, vistas=None):
    sc = bpy.context.scene
    sc.render.engine = 'BLENDER_WORKBENCH'
    sc.display.shading.light = 'STUDIO'
    sc.display.shading.color_type = 'MATERIAL'
    sc.display.shading.show_cavity = True
    sc.display.shading.show_backface_culling = True
    sc.render.resolution_x, sc.render.resolution_y = 700, 1000
    for ob in sc.objects:
        if ob.type == 'MESH':
            ob.hide_render = ob.name not in colecao.objects
    cam = bpy.data.objects['CAM_TESTE']
    sc.camera = cam
    cam.data.lens = 85
    cam.data.clip_start = 0.05
    outs = []
    vistas = vistas or (("frente", (1.2, -4.6, 1.3), (0, 0, 0.95)), ("lado", (4.8, -0.2, 1.3), (0, 0, 0.95)),
                        ("costas", (-1.5, 4.5, 1.4), (0, 0, 0.95)))
    for nome, loc, tgt in vistas:
        cam.location = loc
        cam.rotation_euler = (V(tgt) - V(loc)).to_track_quat('-Z', 'Y').to_euler()
        sc.render.filepath = saida + "_" + nome + ".png"
        bpy.ops.render.render(write_still=True)
        outs.append(sc.render.filepath)
    return outs


if ETAPA == "preview":
    saida = ARGS[1]
    nac = ARGS[2] if len(ARGS) > 2 else "FR"
    col, mats, ob = construir(nac)
    print("RESULT tris", sum(len(pp.vertices) - 2 for pp in ob.data.polygons), "cabeca", CAB["rx"], CAB["ry"], CAB["topo"])
    print("PREVIEW", preview(col, saida))


# ---------------------------------------------------------------- exportacao
EXPORT = os.path.normpath(os.path.join(AQUI, "..", "Export"))
DADOS = os.path.join(AQUI, "dados")
LODS = (1.0, 0.45, 0.2, 0.08)
SUBARVORE = {"Cabeca": "neck_01", "BracoE": "upperarm_l", "BracoD": "upperarm_r", "PernaE": "calf_l", "PernaD": "calf_r"}


def ativar(ob):
    for o in bpy.context.view_layer.objects:
        o.select_set(False)
    bpy.context.view_layer.objects.active = ob
    ob.select_set(True)


def uv_atlas(ob):
    """Copia o UV em metros para 'UVLocal' (padroes da textura) e monta o atlas unico em 'UVMap'."""
    me = ob.data
    loc = me.uv_layers.new(name="UVLocal")
    src = me.uv_layers["UVMap"]
    for k in range(len(src.data)):
        loc.data[k].uv = src.data[k].uv
    me.uv_layers.active = src
    tipos = [0] * len(me.polygons)
    me.attributes["eaw_tipo"].data.foreach_get("value", tipos)
    ativar(ob)
    bpy.context.scene.tool_settings.use_uv_select_sync = True
    for poly, t in zip(me.polygons, tipos):
        poly.select = t != 0
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.uv.smart_project(angle_limit=math.radians(55), island_margin=0.0, scale_to_bounds=False)
    bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.uv.average_islands_scale()
    bpy.ops.object.mode_set(mode='OBJECT')
    # densidade por regiao: maos (cor lisa) bem menores; rosto e placas de latao maiores
    from bpy_extras import mesh_utils
    import numpy as np
    partes = me["eaw_partes"].split(",")
    pid = [0] * len(me.polygons)
    me.attributes["eaw_parte"].data.foreach_get("value", pid)
    uvd = me.uv_layers["UVMap"].data
    nomes_mat = [m.name.split("_", 1)[1] for m in me.materials]
    for ilha in mesh_utils.mesh_linked_uv_islands(me):
        p0 = me.polygons[ilha[0]]
        parte = partes[pid[ilha[0]]]
        papel = nomes_mat[p0.material_index]
        zc = sum(me.vertices[v].co.z for v in p0.vertices) / len(p0.vertices)
        if parte == "pele" and zc < 1.55:
            k = 0.30
        elif parte == "pele":
            k = 1.6
        elif papel in ("latao", "cocar"):
            k = 1.5
        else:
            continue
        loops = [li for pi in ilha for li in me.polygons[pi].loop_indices]
        c = sum((uvd[li].uv for li in loops), Vector((0, 0))) / len(loops)
        for li in loops:
            uvd[li].uv = c + (uvd[li].uv - c) * k
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.uv.pack_islands(rotate=True, margin=0.003, shape_method='CONCAVE')
    bpy.ops.object.mode_set(mode='OBJECT')


def dados_textura(ob, nac):
    import numpy as np
    me = ob.data
    me.calc_loop_triangles()
    T = len(me.loop_triangles)
    tri_loops = np.zeros(T * 3, dtype=np.int32)
    me.loop_triangles.foreach_get("loops", tri_loops)
    tri_poly = np.zeros(T, dtype=np.int32)
    me.loop_triangles.foreach_get("polygon_index", tri_poly)
    uv = np.zeros(len(me.loops) * 2, dtype=np.float32)
    me.uv_layers["UVMap"].data.foreach_get("uv", uv)
    uvl = np.zeros(len(me.loops) * 2, dtype=np.float32)
    me.uv_layers["UVLocal"].data.foreach_get("uv", uvl)
    lv = np.zeros(len(me.loops), dtype=np.int32)
    me.loops.foreach_get("vertex_index", lv)
    co = np.zeros(len(me.vertices) * 3, dtype=np.float32)
    me.vertices.foreach_get("co", co)
    cn = np.zeros(len(me.loops) * 3, dtype=np.float32)
    me.corner_normals.foreach_get("vector", cn)
    mi = np.zeros(len(me.polygons), dtype=np.int32)
    me.polygons.foreach_get("material_index", mi)
    parte = np.zeros(len(me.polygons), dtype=np.int32)
    me.attributes["eaw_parte"].data.foreach_get("value", parte)
    uv = uv.reshape(-1, 2); uvl = uvl.reshape(-1, 2); co = co.reshape(-1, 3); cn = cn.reshape(-1, 3)
    tl = tri_loops.reshape(-1, 3)
    # oclusao ambiente por vertice (raios no hemisferio da normal)
    from mathutils.bvhtree import BVHTree
    bvh = BVHTree.FromPolygons([tuple(c) for c in co], [tuple(p.vertices) for p in me.polygons])
    vn = np.zeros(len(me.vertices) * 3, dtype=np.float32)
    me.vertices.foreach_get("normal", vn)
    vn = vn.reshape(-1, 3)
    rng = np.random.default_rng(3)
    dirs = []
    for k in range(20):
        u1, u2 = (k + 0.5) / 20, rng.random()
        r = math.sqrt(u1); th = 2 * math.pi * ((k * 0.618034) % 1.0)
        dirs.append((r * math.cos(th), r * math.sin(th), math.sqrt(1 - u1)))
    ao = np.ones(len(co), dtype=np.float32)
    for i in range(len(co)):
        nv = Vector(vn[i])
        if nv.length < 0.5:
            continue
        t1, t2 = L.frame_from(nv)
        p0 = Vector(co[i]) + nv * 0.003
        hit = 0
        for dx, dy, dz in dirs:
            dd = t1 * dx + t2 * dy + nv * dz
            h = bvh.ray_cast(p0, dd, 0.22)
            if h[0] is not None:
                hit += 1.0 - min(1.0, h[3] / 0.22) * 0.6
        ao[i] = 1.0 - hit / len(dirs)
    os.makedirs(DADOS, exist_ok=True)
    np.savez_compressed(os.path.join(DADOS, "farda_%s.npz" % nac),
                        uv=uv[tl], uvl=uvl[tl], co=co[lv[tl]], n=cn[tl], mat=mi[tri_poly], parte=parte[tri_poly],
                        ao=ao[lv[tl]],
                        mats=np.array([m.name.split("_", 1)[1] for m in me.materials]),
                        partes=np.array(me["eaw_partes"].split(",")))


def material_unico(ob, nac):
    m = bpy.data.materials.get("M_Farda_" + nac) or bpy.data.materials.new("M_Farda_" + nac)
    me = ob.data
    me.materials.clear()
    me.materials.append(m)
    for poly in me.polygons:
        poly.material_index = 0


def limpar_pesos(ob, limite=4):
    for v in ob.data.vertices:
        gs = sorted(((g.group, g.weight) for g in v.groups), key=lambda t: -t[1])
        keep = [g for g in gs[:limite] if g[1] > 0.01]
        s = sum(w for _, w in keep) or 1.0
        for g, w in gs:
            ob.vertex_groups[g].remove([v.index])
        for g, w in keep:
            ob.vertex_groups[g].add([v.index], w / s, 'REPLACE')


def fazer_lod(ob, ratio, nome):
    if ratio >= 1.0:
        me = ob.data.copy()
    else:
        tmp = ob.copy()
        tmp.data = ob.data.copy()
        bpy.context.scene.collection.objects.link(tmp)
        tmp.modifiers.clear()
        m = tmp.modifiers.new("dec", 'DECIMATE')
        m.ratio = ratio
        m.use_collapse_triangulate = True
        dg = bpy.context.evaluated_depsgraph_get()
        me = bpy.data.meshes.new_from_object(tmp.evaluated_get(dg), preserve_all_data_layers=True, depsgraph=dg)
        bpy.data.objects.remove(tmp)
    bm = bmesh.new()
    bm.from_mesh(me)
    bmesh.ops.triangulate(bm, faces=[f for f in bm.faces if len(f.verts) > 4])
    bm.to_mesh(me)
    bm.free()
    lod = bpy.data.objects.new(nome, me)
    bpy.context.scene.collection.objects.link(lod)
    for g in ob.vertex_groups:
        lod.vertex_groups.new(name=g.name)
    lod.parent = arm
    mod = lod.modifiers.new("Armature", 'ARMATURE')
    mod.object = arm
    limpar_pesos(lod)
    return lod


def exportar_fbx(objs, caminho):
    for o in bpy.context.view_layer.objects:
        o.select_set(False)
    nome_antigo = arm.name
    arm.name = "Armature"
    arm.select_set(True)
    for o in objs:
        o.select_set(True)
    bpy.context.view_layer.objects.active = arm
    try:
        bpy.ops.export_scene.fbx(filepath=caminho, use_selection=True, object_types={'ARMATURE', 'MESH'},
                                 apply_scale_options='FBX_SCALE_ALL', global_scale=1.0, apply_unit_scale=True,
                                 use_armature_deform_only=True, add_leaf_bones=False, bake_anim=False,
                                 mesh_smooth_type='FACE', use_mesh_modifiers=False, use_tspace=True)
    finally:
        arm.name = nome_antigo


def peca_membro(ob, osso, nome):
    """Copia so as faces do membro (vertices pesados na subarvore do osso), como as SK_Peca_* do corpo."""
    sub = set()

    def junta(b):
        sub.add(b.name)
        for c in b.children:
            junta(c)
    junta(arm.data.bones[osso])
    gi = {g.index: g.name for g in ob.vertex_groups}
    me = ob.data
    dentro = []
    for v in me.vertices:
        w = sum(g.weight for g in v.groups if gi[g.group] in sub)
        dentro.append(w > 0.5)
    bm = bmesh.new()
    bm.from_mesh(me)
    bm.verts.ensure_lookup_table()
    fora = [f for f in bm.faces if not all(dentro[v.index] for v in f.verts)]
    bmesh.ops.delete(bm, geom=fora, context='FACES')
    nm = bpy.data.meshes.new(nome)
    bm.to_mesh(nm)
    bm.free()
    po = bpy.data.objects.new(nome, nm)
    bpy.context.scene.collection.objects.link(po)
    for g in ob.vertex_groups:
        po.vertex_groups.new(name=g.name)
    po.parent = arm
    po.modifiers.new("Armature", 'ARMATURE').object = arm
    return po


def exportar_nacao(nac):
    col, mats, ob = construir(nac)
    uv_atlas(ob)
    dados_textura(ob, nac)
    material_unico(ob, nac)
    limpar_pesos(ob)
    os.makedirs(EXPORT, exist_ok=True)
    info = {}
    base = None
    for k, r in enumerate(LODS):
        lod = fazer_lod(ob, r, "SK_Soldado_%s_LOD%d" % (nac, k))
        nome = "SK_Soldado_%s%s.fbx" % (nac, "" if k == 0 else "_LOD%d" % k)
        exportar_fbx([lod], os.path.join(EXPORT, nome))
        info["LOD%d" % k] = sum(len(pp.vertices) - 2 for pp in lod.data.polygons)
        if k == 0:
            base = lod
    for nome, osso in SUBARVORE.items():
        po = peca_membro(base, osso, "SK_Peca_%s_%s" % (nac, nome))
        exportar_fbx([po], os.path.join(EXPORT, "SK_Peca_%s_%s.fbx" % (nac, nome)))
        info["peca_" + nome] = len(po.data.polygons)
    return info


if ETAPA == "exportar":
    res = {}
    for nac in (ARGS[1:] or ["FR", "GB"]):
        res[nac] = exportar_nacao(nac)
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(DADOS, "fardas_gerado.blend"), copy=True)
    print("RESULT", json.dumps(res))
