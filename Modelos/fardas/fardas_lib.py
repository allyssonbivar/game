"""Biblioteca para gerar as fardas do Empires at War no Blender (rodar em segundo plano: blender -b).
Tudo feito do zero: cada peca de roupa e uma malha em aneis em volta de um eixo (osso), com o raio
medido no corpo do soldado por raios (BVH) + folga. Pesos copiados do corpo (ponto mais proximo).
Frente do soldado = -Y, esquerda = +X, pes em Z=0, metros.
"""
import bpy, bmesh, math
import numpy as np
from mathutils import Vector, Matrix
from mathutils.bvhtree import BVHTree

BODY = ("Alpha_Surface", "Alpha_Joints")
ARM = "SK_Soldado"


# ---------------------------------------------------------------- corpo
class Corpo:
    """Corpo do soldado em pose de repouso (mundo), com BVH e pesos por vertice."""

    def __init__(self):
        self.verts = []      # Vector
        self.weights = []    # dict osso->peso
        tris = []
        for n in BODY:
            ob = bpy.data.objects[n]
            mw = ob.matrix_world
            gi = {g.index: g.name for g in ob.vertex_groups}
            base = len(self.verts)
            for v in ob.data.vertices:
                self.verts.append(mw @ v.co)
                self.weights.append({gi[g.group]: g.weight for g in v.groups if g.weight > 1e-4})
            ob.data.calc_loop_triangles()
            for t in ob.data.loop_triangles:
                tris.append(tuple(base + i for i in t.vertices))
        self.tris = tris
        self.bvh = BVHTree.FromPolygons(self.verts, tris)

    def ray_out_in(self, center, d, rmax):
        """Raio de fora (center + d*rmax) para dentro; devolve a distancia da 1a superficie de frente."""
        o = center + d * rmax
        dist = rmax
        for _ in range(6):
            hit, nrm, idx, dd = self.bvh.ray_cast(o, -d, dist)
            if hit is None:
                return None
            # face de tras (o raio comecou dentro de outra parte): continua depois dela
            if nrm.dot(d) < 0:
                o = hit - d * 1e-4
                dist = (o - center).dot(d)
                if dist <= 0:
                    return None
                continue
            return (hit - center).dot(d)
        return None

    def weights_at(self, p):
        co, nrm, idx, dist = self.bvh.find_nearest(p)
        a, b, c = (self.verts[i] for i in self.tris[idx])
        # baricentricas
        v0, v1, v2 = b - a, c - a, co - a
        d00, d01, d11 = v0.dot(v0), v0.dot(v1), v1.dot(v1)
        d20, d21 = v2.dot(v0), v2.dot(v1)
        den = d00 * d11 - d01 * d01 or 1e-12
        wv = (d11 * d20 - d01 * d21) / den
        ww = (d00 * d21 - d01 * d20) / den
        wu = 1 - wv - ww
        out = {}
        for k, f in zip(self.tris[idx], (wu, wv, ww)):
            f = max(0.0, f)
            for g, w in self.weights[k].items():
                out[g] = out.get(g, 0.0) + w * f
        return out


# ---------------------------------------------------------------- malha em construcao
class Peca:
    """Acumula vertices/faces/uv/material/pesos de uma peca (ou de varias)."""

    def __init__(self, nome):
        self.nome = nome
        self.v = []
        self.f = []       # (indices, mat, uvs)
        self.tipo = []    # 'tubo' (uv em metros, continuo) | 'rigido' | 'pele' (reprojetar)
        self.parte = []   # nome da peca (para pintar detalhes)
        self.parte_atual = ''
        self.w = []       # dict por vertice (None = calcular pelo corpo)
        self.lock = []    # True = nao suavizar peso

    def add_v(self, p, w=None):
        self.v.append(Vector(p))
        self.w.append(w)
        self.lock.append(w is not None)
        return len(self.v) - 1

    def add_f(self, idx, mat, uvs=None, tipo='tubo'):
        self.f.append((tuple(idx), mat, uvs))
        self.tipo.append(tipo)
        self.parte.append(self.parte_atual)


def smooth_grid(r, iters=2, wrap=True, along=True):
    r = r.copy()
    for _ in range(iters):
        if wrap:
            r = 0.5 * r + 0.25 * (np.roll(r, 1, 1) + np.roll(r, -1, 1))
        else:
            r[:, 1:-1] = 0.5 * r[:, 1:-1] + 0.25 * (r[:, :-2] + r[:, 2:])
        if along and r.shape[0] > 2:
            r[1:-1] = 0.5 * r[1:-1] + 0.25 * (r[:-2] + r[2:])
    return r


def frame_from(axis):
    """Dois vetores perpendiculares ao eixo (u ~ para frente -Y quando possivel)."""
    axis = axis.normalized()
    ref = Vector((0, -1, 0)) if abs(axis.y) < 0.9 else Vector((0, 0, 1))
    u = (ref - axis * ref.dot(axis)).normalized()
    v = axis.cross(u).normalized()
    return u, v


def tubo(corpo, peca, centros, eixos, nang, mat, folga, rmax=0.35, rmin=0.02,
         dil=2, suav=3, clamp=None, ang0=0.0, ang1=None, fechado=True, uv_v0=0.0, matfn=None,
         raio_fn=None, wfn=None, espessura=0.0, mat_dentro=None, bvh=None, bordas=(True, True), faixa=None):
    """Cria um tubo de aneis. centros/eixos: listas de Vector (um por anel).
    clamp(i, ang, r) -> r  permite limitar o raio. espessura > 0 cria a face de dentro e as bordas.
    bvh: mede numa outra superficie (ex.: a propria roupa) em vez do corpo.
    Devolve a grade de indices [anel][ang]."""
    if bvh is not None:
        alvo = _Alvo(bvh)
    else:
        alvo = corpo
    return _tubo(alvo, corpo, peca, centros, eixos, nang, mat, folga, rmax, rmin, dil, suav, clamp, ang0, ang1,
                 fechado, uv_v0, matfn, raio_fn, wfn, espessura, mat_dentro, bordas, faixa)


class _Alvo:
    def __init__(self, bvh):
        self.bvh = bvh

    ray_out_in = None


_Alvo.ray_out_in = lambda self, c, d, rm: Corpo.ray_out_in(self, c, d, rm)


def bvh_da_peca(peca):
    return BVHTree.FromPolygons([tuple(p) for p in peca.v], [f[0] for f in peca.f])


def _tubo(corpo, corpo_pesos, peca, centros, eixos, nang, mat, folga, rmax, rmin, dil, suav, clamp, ang0, ang1,
          fechado, uv_v0, matfn, raio_fn, wfn, espessura, mat_dentro, bordas, faixa=None):
    n = len(centros)
    if ang1 is None:
        ang1 = ang0 + 2 * math.pi
    na = nang if fechado else nang + 1
    angs_r = []
    for i in range(n):
        a0, a1 = faixa(i) if faixa else (ang0, ang1)
        angs_r.append([a0 + (a1 - a0) * j / nang for j in range(na)])
    R = np.zeros((n, na))
    frames = []
    for i in range(n):
        u, v = frame_from(eixos[i])
        frames.append((u, v))
        for j, a in enumerate(angs_r[i]):
            d = u * math.cos(a) + v * math.sin(a)
            rm = rmax(i, a) if callable(rmax) else rmax
            h = corpo.ray_out_in(centros[i], d, rm)
            R[i, j] = h if h is not None else np.nan
    # buracos -> vizinho valido
    for i in range(n):
        row = R[i]
        if np.all(np.isnan(row)):
            R[i] = rmin
            continue
        idx = np.where(~np.isnan(row))[0]
        for j in np.where(np.isnan(row))[0]:
            k = idx[np.argmin(np.minimum(abs(idx - j), na - abs(idx - j)))]
            row[j] = row[k]
    # dilatacao ao longo do eixo (fecha as frestas entre as pecas do manequim)
    if dil:
        D = R.copy()
        for k in range(1, dil + 1):
            D[k:] = np.maximum(D[k:], R[:-k] * 0.97)
            D[:-k] = np.maximum(D[:-k], R[k:] * 0.97)
        R = D
    R = smooth_grid(R, suav, wrap=fechado)
    R = R + (folga if not callable(folga) else 0)
    grid = []
    dirs = []
    for i in range(n):
        row = []
        drow = []
        u, v = frames[i]
        for j, a in enumerate(angs_r[i]):
            r = R[i, j] + (folga(i, a) if callable(folga) else 0)
            if clamp:
                r = clamp(i, a, r)
            r = max(rmin, r)
            if raio_fn:
                r = raio_fn(i, a, r)
            d = u * math.cos(a) + v * math.sin(a)
            p = centros[i] + d * r
            w = wfn(i, a, p) if wfn else None
            row.append(peca.add_v(p, w))
            drow.append(d)
        grid.append(row)
        dirs.append(drow)
    # UV com tamanho real: u = arco medio, v = comprimento ao longo do eixo
    circ = []
    for row in grid:
        s = [0.0]
        for j in range(1, len(row) + (1 if fechado else 0)):
            s.append(s[-1] + (peca.v[row[j % len(row)]] - peca.v[row[j - 1]]).length)
        circ.append(s)
    cm = np.mean(np.array(circ), axis=0)
    vs = [uv_v0]
    for i in range(1, n):
        vs.append(vs[-1] + (centros[i] - centros[i - 1]).length)
    for i in range(n - 1):
        for j in range(nang):
            j2 = (j + 1) % na if fechado else j + 1
            a, b = grid[i][j], grid[i][j2]
            c, d = grid[i + 1][j2], grid[i + 1][j]
            uvs = [(cm[j], vs[i]), (cm[j + 1], vs[i]), (cm[j + 1], vs[i + 1]), (cm[j], vs[i + 1])]
            m = matfn(i, j, peca.v[a]) if matfn else mat
            # ordem para a normal apontar para fora
            peca.add_f((a, b, c, d), m, uvs)
    if espessura > 0:
        md = mat_dentro or mat
        inner = []
        for i in range(n):
            inner.append([peca.add_v(peca.v[grid[i][j]] - dirs[i][j] * espessura, peca.w[grid[i][j]])
                          for j in range(na)])
        for i in range(n - 1):
            for j in range(nang):
                j2 = (j + 1) % na if fechado else j + 1
                a, b = inner[i][j], inner[i][j2]
                c, d = inner[i + 1][j2], inner[i + 1][j]
                uvs = [(cm[j], vs[i]), (cm[j + 1], vs[i]), (cm[j + 1], vs[i + 1]), (cm[j], vs[i + 1])]
                peca.add_f((a, d, c, b), md, [uvs[0], uvs[3], uvs[2], uvs[1]])
        e = espessura

        def borda(o0, o1, i0, i1, m, s0, s1):
            peca.add_f((o0, o1, i1, i0), m, [(s0, 0), (s1, 0), (s1, e), (s0, e)])

        # aneis da ponta (bordas)
        for k, i in ((0, 0), (1, n - 1)):
            if not bordas[k]:
                continue
            for j in range(nang):
                j2 = (j + 1) % na if fechado else j + 1
                o0, o1, i0, i1 = grid[i][j], grid[i][j2], inner[i][j], inner[i][j2]
                if k == 0:
                    borda(o1, o0, i1, i0, md, cm[j + 1], cm[j])
                else:
                    borda(o0, o1, i0, i1, md, cm[j], cm[j + 1])
        if not fechado:
            for i in range(n - 1):
                borda(grid[i][0], grid[i + 1][0], inner[i][0], inner[i + 1][0], md, vs[i], vs[i + 1])
                borda(grid[i + 1][na - 1], grid[i][na - 1], inner[i + 1][na - 1], inner[i][na - 1], md, vs[i + 1], vs[i])
    return grid


def linha(a, b, n):
    a, b = Vector(a), Vector(b)
    return [a.lerp(b, i / (n - 1)) for i in range(n)]


def polilinha(pts, n):
    """n pontos igualmente espacados numa polilinha."""
    pts = [Vector(p) for p in pts]
    L = [0.0]
    for i in range(1, len(pts)):
        L.append(L[-1] + (pts[i] - pts[i - 1]).length)
    out = []
    for k in range(n):
        s = L[-1] * k / (n - 1)
        for i in range(1, len(pts)):
            if s <= L[i] + 1e-9:
                t = (s - L[i - 1]) / max(1e-9, L[i] - L[i - 1])
                out.append(pts[i - 1].lerp(pts[i], t))
                break
    return out


def tangentes(pts):
    out = []
    for i in range(len(pts)):
        a = pts[max(0, i - 1)]
        b = pts[min(len(pts) - 1, i + 1)]
        out.append((b - a).normalized())
    return out


# ---------------------------------------------------------------- pecas rigidas (bmesh)
def caixa(peca, centro, tam, mat, w, rot=None, bevel=0.0, segs=1):
    """Caixa (com chanfro opcional) presa rigidamente (w = dict de pesos)."""
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    bmesh.ops.scale(bm, vec=Vector(tam), verts=bm.verts)
    if bevel > 0:
        bmesh.ops.bevel(bm, geom=list(bm.edges), offset=bevel, segments=segs, affect='EDGES', profile=0.5)
    M = Matrix.Translation(Vector(centro)) @ (rot.to_4x4() if rot else Matrix())
    bmesh.ops.transform(bm, matrix=M, verts=bm.verts)
    return bm_para_peca(bm, peca, mat, w)


def cilindro(peca, p0, p1, r0, r1, nseg, mat, w, tampa=True, tampa_mat=None, rings=1):
    bm = bmesh.new()
    p0, p1 = Vector(p0), Vector(p1)
    ax = (p1 - p0)
    u, v = frame_from(ax)
    loops = []
    for k in range(rings + 1):
        t = k / rings
        c = p0.lerp(p1, t)
        r = r0 + (r1 - r0) * t
        loops.append([bm.verts.new(c + (u * math.cos(2 * math.pi * j / nseg) + v * math.sin(2 * math.pi * j / nseg)) * r)
                      for j in range(nseg)])
    for k in range(rings):
        for j in range(nseg):
            j2 = (j + 1) % nseg
            bm.faces.new((loops[k][j], loops[k][j2], loops[k + 1][j2], loops[k + 1][j]))
    if tampa:
        bm.faces.new(list(reversed(loops[0])))
        bm.faces.new(loops[-1])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    mats = {}
    if tampa_mat:
        for f in bm.faces:
            if len(f.verts) > 4:
                mats[f.index] = tampa_mat
    return bm_para_peca(bm, peca, mat, w, mats)


def bm_para_peca(bm, peca, mat, w, mats=None, uv_box=True):
    bm.verts.index_update()
    bm.faces.index_update()
    base = len(peca.v)
    for vv in bm.verts:
        peca.add_v(vv.co.copy(), dict(w) if isinstance(w, dict) else None)
    for f in bm.faces:
        m = (mats or {}).get(f.index, mat)
        # UV por projecao de caixa (escala real)
        n = f.normal
        ax = max(range(3), key=lambda k: abs(n[k]))
        uvs = []
        for vv in f.verts:
            c = vv.co
            if ax == 0:
                uvs.append((c.y, c.z))
            elif ax == 1:
                uvs.append((c.x, c.z))
            else:
                uvs.append((c.x, c.y))
        peca.add_f([base + vv.index for vv in f.verts], m, uvs, tipo='rigido')
    bm.free()


# ---------------------------------------------------------------- objeto final
def criar_objeto(peca, corpo, mats, colecao, arm_ob, suav_pesos=2, max_inf=4):
    me = bpy.data.meshes.new(peca.nome)
    me.from_pydata([tuple(p) for p in peca.v], [], [f[0] for f in peca.f])
    me.update()
    for m in mats:
        me.materials.append(m)
    mi = {m.name: k for k, m in enumerate(mats)}
    uvl = me.uv_layers.new(name="UVMap")
    for p, (idx, mat, uvs) in zip(me.polygons, peca.f):
        p.material_index = mi[mat]
        if uvs:
            for li, uv in zip(p.loop_indices, uvs):
                uvl.data[li].uv = uv
    partes = sorted(set(peca.parte))
    at = me.attributes.new("eaw_tipo", 'INT', 'FACE')
    ap = me.attributes.new("eaw_parte", 'INT', 'FACE')
    tmap = {'tubo': 0, 'rigido': 1, 'pele': 2}
    at.data.foreach_set("value", [tmap[t] for t in peca.tipo])
    ap.data.foreach_set("value", [partes.index(x) for x in peca.parte])
    me["eaw_partes"] = ",".join(partes)
    ob = bpy.data.objects.new(peca.nome, me)
    colecao.objects.link(ob)
    # pesos
    W = []
    for k, p in enumerate(peca.v):
        W.append(peca.w[k] if peca.w[k] is not None else corpo.weights_at(p))
    # suavizar pelos vizinhos (so os nao travados)
    adj = [[] for _ in peca.v]
    for e in me.edges:
        a, b = e.vertices
        adj[a].append(b)
        adj[b].append(a)
    for _ in range(suav_pesos):
        N = []
        for k in range(len(W)):
            if peca.lock[k] or not adj[k]:
                N.append(W[k])
                continue
            acc = {g: w * 0.5 for g, w in W[k].items()}
            f = 0.5 / len(adj[k])
            for j in adj[k]:
                for g, w in W[j].items():
                    acc[g] = acc.get(g, 0.0) + w * f
            N.append(acc)
        W = N
    grupos = {}
    for k, wd in enumerate(W):
        items = sorted(wd.items(), key=lambda t: -t[1])[:max_inf]
        s = sum(w for _, w in items) or 1.0
        for g, w in items:
            if w / s < 0.01:
                continue
            if g not in grupos:
                grupos[g] = ob.vertex_groups.new(name=g)
            grupos[g].add([k], w / s, 'REPLACE')
    ob.parent = arm_ob
    mod = ob.modifiers.new("Armature", 'ARMATURE')
    mod.object = arm_ob
    return ob


def material(nome, cor):
    m = bpy.data.materials.get(nome) or bpy.data.materials.new(nome)
    m.diffuse_color = (*cor, 1.0)
    return m


# ---------------------------------------------------------------- correias e formas fechadas
def cinta(corpo, peca, bvh, centro, normal_plano, largura, n, mat, folga=0.004, alt=0.003,
          arco=(0.0, 2 * math.pi), rmax=0.6, suav=3, wfn=None, ref=None):
    """Correia que da a volta numa superficie (bvh) no plano (centro, normal_plano).
    Raios de fora para dentro no plano; a correia fica sobre a superficie com largura e altura (alt)."""
    nrm_p = Vector(normal_plano).normalized()
    if ref is None:
        u, v = frame_from(nrm_p)
    else:
        u = (Vector(ref) - nrm_p * Vector(ref).dot(nrm_p)).normalized()
        v = nrm_p.cross(u).normalized()
    fechado = abs((arco[1] - arco[0]) - 2 * math.pi) < 1e-6
    na = n if fechado else n + 1
    alvo = _Alvo(bvh)
    H, N = [], []
    for k in range(na):
        a = arco[0] + (arco[1] - arco[0]) * k / n
        d = u * math.cos(a) + v * math.sin(a)
        r = alvo.ray_out_in(Vector(centro), d, rmax)
        H.append(r if r is not None else np.nan)
        N.append(d)
    H = np.array(H)
    ok = ~np.isnan(H)
    if not ok.any():
        return None
    idx = np.where(ok)[0]
    for k in np.where(~ok)[0]:
        H[k] = H[idx[np.argmin(abs(idx - k))]]
    for _ in range(suav):
        if fechado:
            H = 0.5 * H + 0.25 * (np.roll(H, 1) + np.roll(H, -1))
        else:
            H[1:-1] = 0.5 * H[1:-1] + 0.25 * (H[:-2] + H[2:])
    pts = [Vector(centro) + N[k] * (H[k] + folga) for k in range(na)]
    # normal da superficie ~ direcao radial no plano; lado = normal do plano
    half = largura * 0.5
    rows = []
    s = 0.0
    for k in range(na):
        p = pts[k]
        w = wfn(p) if wfn else corpo.weights_at(p - N[k] * folga)
        o1 = peca.add_v(p + nrm_p * half, w)
        o2 = peca.add_v(p - nrm_p * half, w)
        b1 = peca.add_v(p + nrm_p * half - N[k] * alt, w)
        b2 = peca.add_v(p - nrm_p * half - N[k] * alt, w)
        if k:
            s += (pts[k] - pts[k - 1]).length
        rows.append((o1, o2, b1, b2, s))
    for k in range(n):
        a = rows[k]
        b = rows[(k + 1) % na] if fechado else rows[k + 1]
        s0, s1 = a[4], (b[4] if b[4] > a[4] else a[4] + (pts[0] - pts[-1]).length)
        peca.add_f((a[0], a[1], b[1], b[0]), mat, [(0, s0), (largura, s0), (largura, s1), (0, s1)])
        peca.add_f((a[2], a[0], b[0], b[2]), mat, [(-alt, s0), (0, s0), (0, s1), (-alt, s1)])
        peca.add_f((a[1], a[3], b[3], b[1]), mat, [(largura, s0), (largura + alt, s0), (largura + alt, s1), (largura, s1)])
    return pts


def bolha(corpo, peca, centro, nlat, nlon, mat, folga, rmax=0.25, escala=(1, 1, 1), zmin=None, suav=2,
          bvh=None, matfn=None, w=None):
    """Forma fechada em volta de um centro (ex.: sapato), raios de fora para dentro de todas as direcoes."""
    alvo = _Alvo(bvh) if bvh is not None else corpo
    c = Vector(centro)
    sx, sy, sz = escala
    R = np.zeros((nlat + 1, nlon))
    D = []
    for i in range(nlat + 1):
        th = math.pi * i / nlat
        row = []
        for j in range(nlon):
            ph = 2 * math.pi * j / nlon
            d = Vector((math.sin(th) * math.cos(ph) * sx, math.sin(th) * math.sin(ph) * sy, math.cos(th) * sz)).normalized()
            h = alvo.ray_out_in(c, d, rmax)
            R[i, j] = h if h is not None else np.nan
            row.append(d)
        D.append(row)
    for i in range(nlat + 1):
        rr = R[i]
        if np.all(np.isnan(rr)):
            rr[:] = np.nanmean(R) if not np.all(np.isnan(R)) else 0.05
        rr[np.isnan(rr)] = np.nanmean(rr)
    R = smooth_grid(R, suav, wrap=True)
    R = R + folga
    grid = []
    for i in range(nlat + 1):
        row = []
        for j in range(nlon):
            p = c + D[i][j] * R[i, j]
            if zmin is not None and p.z < zmin:
                p.z = zmin
            row.append(peca.add_v(p, (w(p) if callable(w) else dict(w)) if w else None))
        grid.append(row)
    for i in range(nlat):
        for j in range(nlon):
            j2 = (j + 1) % nlon
            a, b, cc, d = grid[i][j], grid[i][j2], grid[i + 1][j2], grid[i + 1][j]
            uvs = [(j / nlon, i / nlat), (j2 / nlon if j2 else 1.0, i / nlat), (j2 / nlon if j2 else 1.0, (i + 1) / nlat),
                   (j / nlon, (i + 1) / nlat)]
            m = matfn(peca.v[a]) if matfn else mat
            if i == 0:
                peca.add_f((a, d, cc), m, [uvs[0], uvs[3], uvs[2]])
            elif i == nlat - 1:
                peca.add_f((a, d, b), m, [uvs[0], uvs[3], uvs[1]])
            else:
                peca.add_f((a, d, cc, b), m, [uvs[0], uvs[3], uvs[2], uvs[1]])
    return grid


def esfera(peca, centro, raios, nlat, nlon, mat, w, rot=None):
    bm = bmesh.new()
    bmesh.ops.create_uvsphere(bm, u_segments=nlon, v_segments=nlat, radius=1.0)
    bmesh.ops.scale(bm, vec=Vector(raios), verts=bm.verts)
    M = Matrix.Translation(Vector(centro)) @ (rot.to_4x4() if rot else Matrix())
    bmesh.ops.transform(bm, matrix=M, verts=bm.verts)
    return bm_para_peca(bm, peca, mat, w)


def copiar_partes(peca, ob, ossos, mat, decimar=1.0, colecao=None):
    """Copia as faces do manequim cujo osso dominante esta em 'ossos' (cabeca, maos), com os pesos."""
    me = ob.data
    gi = {g.index: g.name for g in ob.vertex_groups}
    bm = bmesh.new()
    bm.from_mesh(me)
    dl = bm.verts.layers.deform.active
    keep = set()
    for f in bm.faces:
        v0 = f.verts[0]
        dv = v0[dl]
        best = max(dv.items(), key=lambda t: t[1], default=None)
        if best and gi[best[0]] in ossos:
            keep.add(f)
    bmesh.ops.delete(bm, geom=[f for f in bm.faces if f not in keep], context='FACES')
    bmesh.ops.transform(bm, matrix=ob.matrix_world, verts=bm.verts)
    if decimar < 1.0:
        tmp = bpy.data.meshes.new("tmp_dec")
        bm.to_mesh(tmp)
        bm.free()
        tob = bpy.data.objects.new("tmp_dec", tmp)
        for g in ob.vertex_groups:
            tob.vertex_groups.new(name=g.name)
        (colecao or bpy.context.scene.collection).objects.link(tob)
        m = tob.modifiers.new("dec", 'DECIMATE')
        m.ratio = decimar
        dg = bpy.context.evaluated_depsgraph_get()
        em = bpy.data.meshes.new_from_object(tob.evaluated_get(dg), preserve_all_data_layers=True, depsgraph=dg)
        bm = bmesh.new()
        bm.from_mesh(em)
        bpy.data.objects.remove(tob)
        dl = bm.verts.layers.deform.active
    bm.verts.index_update()
    base = len(peca.v)
    uvl = bm.loops.layers.uv.active
    for vv in bm.verts:
        w = {gi[g]: x for g, x in vv[dl].items() if x > 1e-4} if dl else {}
        peca.add_v(vv.co.copy(), w)
    for f in bm.faces:
        uvs = [tuple(l[uvl].uv) for l in f.loops] if uvl else None
        peca.add_f([base + vv.index for vv in f.verts], mat, uvs, tipo='pele')
    bm.free()
