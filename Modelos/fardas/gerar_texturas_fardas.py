"""Pinta as texturas das fardas (cor, normal, ORM) a partir dos dados exportados pelo gerar_fardas.py.
Tudo procedural, feito do zero: cada texel sabe a sua posicao 3D (pose de repouso), normal, material e peca;
as cores, costuras, botoes, galoes e emblemas sao desenhados por regras nessas coordenadas.
Rodar com Python que tenha numpy + Pillow:
  python gerar_texturas_fardas.py [FR] [GB]
Saida: EmpiresAtWar/Texturas/Fardas/T_Farda_<NAC>_C.png (cor), _N.png (normal, DirectX), _ORM.png (AO, rugosidade, metal)
"""
import os, sys, math, time
import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

AQUI = os.path.dirname(os.path.abspath(__file__))
DADOS = os.path.join(AQUI, "dados")
SAIDA = os.path.normpath(os.path.join(AQUI, "..", "..", "EmpiresAtWar", "Texturas", "Fardas"))
N = 2048


# ---------------------------------------------------------------- rasterizacao no espaco UV
def rasterizar(d):
    uv = d["uv"].astype(np.float64)
    T = uv.shape[0]
    tid = np.full((N, N), -1, np.int32)
    b0 = np.zeros((N, N), np.float32)
    b1 = np.zeros((N, N), np.float32)
    px = uv[:, :, 0] * N - 0.5
    py = (1.0 - uv[:, :, 1]) * N - 0.5
    for t in range(T):
        x0, x1, x2 = px[t]
        y0, y1, y2 = py[t]
        xmin = max(int(math.floor(min(x0, x1, x2))), 0)
        xmax = min(int(math.ceil(max(x0, x1, x2))), N - 1)
        ymin = max(int(math.floor(min(y0, y1, y2))), 0)
        ymax = min(int(math.ceil(max(y0, y1, y2))), N - 1)
        if xmax < xmin or ymax < ymin:
            continue
        den = (y1 - y2) * (x0 - x2) + (x2 - x1) * (y0 - y2)
        if abs(den) < 1e-12:
            continue
        ys, xs = np.mgrid[ymin:ymax + 1, xmin:xmax + 1]
        l0 = ((y1 - y2) * (xs - x2) + (x2 - x1) * (ys - y2)) / den
        l1 = ((y2 - y0) * (xs - x2) + (x0 - x2) * (ys - y2)) / den
        l2 = 1 - l0 - l1
        e = -0.02
        m = (l0 >= e) & (l1 >= e) & (l2 >= e)
        if not m.any():
            continue
        tid[ys[m], xs[m]] = t
        b0[ys[m], xs[m]] = l0[m]
        b1[ys[m], xs[m]] = l1[m]
    return tid, b0, b1


def interp(attr, tid, b0, b1, valid):
    t = tid[valid]
    a = attr[t]
    w0 = b0[valid][:, None]
    w1 = b1[valid][:, None]
    return a[:, 0] * w0 + a[:, 1] * w1 + a[:, 2] * (1 - w0 - w1)


# ---------------------------------------------------------------- ruido 3D (sem emendas entre ilhas)
def _hash(ix, iy, iz, seed):
    h = (ix * 73856093) ^ (iy * 19349663) ^ (iz * 83492791) ^ (seed * 2654435761)
    h = (h ^ (h >> 13)) * 1274126177
    return ((h ^ (h >> 16)) & 0xFFFF).astype(np.float32) / 65535.0


def noise3(p, escala, seed=0):
    q = p / escala
    f = np.floor(q)
    i = f.astype(np.int64)
    t = q - f
    t = t * t * (3 - 2 * t)
    out = 0
    for dx in (0, 1):
        for dy in (0, 1):
            for dz in (0, 1):
                w = (t[:, 0] if dx else 1 - t[:, 0]) * (t[:, 1] if dy else 1 - t[:, 1]) * (t[:, 2] if dz else 1 - t[:, 2])
                out = out + w * _hash(i[:, 0] + dx, i[:, 1] + dy, i[:, 2] + dz, seed)
    return out


def fbm3(p, escala, oit=4, seed=0):
    acc = 0
    amp = 1.0
    tot = 0
    for o in range(oit):
        acc = acc + amp * noise3(p, escala / (2 ** o), seed + o * 17)
        tot += amp
        amp *= 0.5
    return acc / tot


# ---------------------------------------------------------------- utilidades de desenho
def smooth(x, a, b):
    t = np.clip((x - a) / (b - a), 0, 1)
    return t * t * (3 - 2 * t)


def faixa(x, c, meia, borda=0.0008):
    """1 dentro de |x-c|<meia, com borda suave."""
    return 1 - smooth(np.abs(x - c), meia - borda, meia + borda)


def disco(dist, r, borda=0.0006):
    return 1 - smooth(dist, r - borda, r + borda)


def mix(a, b, t):
    t = np.asarray(t)
    if t.ndim == 1:
        t = t[:, None]
    return a * (1 - t) + b * t


def cor(hexa):
    hexa = hexa.lstrip("#")
    return np.array([int(hexa[i:i + 2], 16) / 255.0 for i in (0, 2, 4)], np.float32)


def srgb_to_lin(c):
    return np.where(c <= 0.04045, c / 12.92, ((c + 0.055) / 1.055) ** 2.4)


def lin_to_srgb(c):
    c = np.clip(c, 0, 1)
    return np.where(c <= 0.0031308, c * 12.92, 1.055 * c ** (1 / 2.4) - 0.055)


def carimbo(texto, tam=256, fonte_px=180):
    """Mascara (0..1) de um texto desenhado do zero com a fonte do sistema, para emblemas."""
    img = Image.new("L", (tam, tam), 0)
    dr = ImageDraw.Draw(img)
    try:
        f = ImageFont.truetype("timesbd.ttf", fonte_px)
    except Exception:
        f = ImageFont.load_default()
    bb = dr.textbbox((0, 0), texto, font=f)
    dr.text(((tam - (bb[2] - bb[0])) / 2 - bb[0], (tam - (bb[3] - bb[1])) / 2 - bb[1]), texto, fill=255, font=f)
    return np.asarray(img, np.float32) / 255.0


def amostra_mascara(mask, u, v):
    """u, v em 0..1 (fora = 0)."""
    h, w = mask.shape
    dentro = (u >= 0) & (u < 1) & (v >= 0) & (v < 1)
    out = np.zeros(u.shape, np.float32)
    xi = np.clip((u * w).astype(int), 0, w - 1)
    yi = np.clip(((1 - v) * h).astype(int), 0, h - 1)
    out[dentro] = mask[yi[dentro], xi[dentro]]
    return out


# ---------------------------------------------------------------- paletas (sRGB)
PALETA = {
    "FR": {
        "casaca": "#1b2442", "vermelho": "#8e1a1c", "branco": "#d9d5c8", "calca": "#d3cfc2", "polaina": "#141414",
        "sapato": "#121110", "correia": "#dcd6c4", "couro": "#141210", "latao": "#b08a3e", "pele": "#c79a7e",
        "feltro": "#151517", "pelo": "#6e4a2c", "capote": "#8d8678", "pompom": "#1f5a32", "cocar": "#1c2d6e",
        "forro": "#d9d5c8",
    },
    "GB": {
        "casaca": "#8e2520", "vermelho": "#1c2650", "branco": "#e0ddd2", "calca": "#6e7077", "polaina": "#141414",
        "sapato": "#121110", "correia": "#e3dfd2", "couro": "#141210", "latao": "#b08a3e", "pele": "#c79a7e",
        "feltro": "#151517", "mochila": "#1a1a1c", "capote": "#6b6b6c", "cantil": "#33476b", "cordao": "#dedbd0",
        "pluma": "#e6e4dc", "pluma2": "#9a1c1c", "forro": "#e0ddd2", "sacola": "#cdc5ad",
    },
}
# rugosidade, metal, tipo de superficie (para o relevo)
SUPERFICIE = {
    "casaca": (0.88, 0, "la"), "vermelho": (0.86, 0, "la"), "branco": (0.85, 0, "la"), "forro": (0.86, 0, "la"),
    "calca": (0.86, 0, "la"), "polaina": (0.62, 0, "couro_fosco"), "sapato": (0.38, 0, "couro"),
    "correia": (0.62, 0, "couro_fosco"), "couro": (0.34, 0, "couro"), "latao": (0.32, 1, "metal"),
    "pele": (0.55, 0, "pele"), "feltro": (0.80, 0, "feltro"), "pelo": (0.90, 0, "pelo"), "capote": (0.92, 0, "la"),
    "pompom": (0.95, 0, "pelo"), "cocar": (0.80, 0, "la"), "mochila": (0.50, 0, "lona"), "cantil": (0.55, 0, "madeira"),
    "cordao": (0.90, 0, "cordao"), "pluma": (0.95, 0, "pelo"), "pluma2": (0.95, 0, "pelo"), "sacola": (0.85, 0, "lona"),
}


# ---------------------------------------------------------------- pintura
def pintar(nac):
    t0 = time.time()
    d = dict(np.load(os.path.join(DADOS, "farda_%s.npz" % nac)))
    mats = [str(m) for m in d["mats"]]
    partes = [str(p) for p in d["partes"]]
    tid, b0, b1 = rasterizar(d)
    valid = tid >= 0
    print(nac, "raster", round(time.time() - t0, 1), "s, cobertura %.1f%%" % (100 * valid.mean()))
    P = interp(d["co"], tid, b0, b1, valid)
    Nn = interp(d["n"], tid, b0, b1, valid)
    Nn /= np.linalg.norm(Nn, axis=1, keepdims=True) + 1e-9
    UL = interp(d["uvl"], tid, b0, b1, valid)
    AO = interp(d["ao"][:, :, None], tid, b0, b1, valid)[:, 0] if "ao" in d else np.ones(len(P), np.float32)
    t_ = tid[valid]
    mid = d["mat"][t_]
    pid = d["parte"][t_]
    M = np.array([mats[i] for i in range(len(mats))])
    papel = M[mid]
    parte = np.array(partes)[pid]
    x, y, z = P[:, 0], P[:, 1], P[:, 2]
    n = len(P)

    pal = PALETA[nac]
    C = np.zeros((n, 3), np.float32)
    rough = np.zeros(n, np.float32)
    metal = np.zeros(n, np.float32)
    H = np.zeros(n, np.float32)          # relevo (metros)
    for nome in set(papel.tolist()):
        m = papel == nome
        C[m] = cor(pal.get(nome, "#808080"))
        r, me, _ = SUPERFICIE.get(nome, (0.8, 0, "la"))
        rough[m] = r
        metal[m] = me
    sup = np.array([SUPERFICIE.get(p, (0.8, 0, "la"))[2] for p in papel])

    # ---------- materiais base: variacao e relevo
    g1 = fbm3(P, 0.12, 4, 1)               # manchas grandes
    g2 = noise3(P, 0.004, 7)               # fibra
    g3 = noise3(P * np.array([1, 1, 3.0]), 0.0025, 9)
    la = sup == "la"
    # sarja da la: linhas diagonais no UV local (metros)
    tw = 0.5 + 0.5 * np.sin((UL[:, 0] + UL[:, 1]) * 2 * math.pi / 0.0016)
    C[la] *= (0.90 + 0.14 * g1[la] + 0.06 * g2[la])[:, None]
    H[la] += 0.00012 * tw[la] + 0.00025 * g2[la]
    fel = sup == "feltro"
    C[fel] *= (0.85 + 0.25 * g2[fel])[:, None]
    H[fel] += 0.0002 * g2[fel]
    cou = (sup == "couro") | (sup == "couro_fosco")
    grao = noise3(P, 0.0012, 11)
    C[cou] *= (0.88 + 0.2 * g1[cou] + 0.08 * grao[cou])[:, None]
    H[cou] += 0.00012 * grao[cou]
    rough[cou] += 0.15 * (g1[cou] - 0.5)
    met = sup == "metal"
    C[met] *= (0.85 + 0.25 * g1[met])[:, None]
    rough[met] += 0.2 * (g1[met] - 0.5)
    pel = sup == "pelo"
    fio = noise3(P * np.array([6.0, 6.0, 1.0]), 0.004, 13)
    C[pel] *= (0.7 + 0.5 * fio[pel])[:, None]
    H[pel] += 0.0006 * fio[pel]
    lona = sup == "lona"
    tram = 0.5 + 0.25 * (np.sin(UL[:, 0] * 2 * math.pi / 0.0018) + np.sin(UL[:, 1] * 2 * math.pi / 0.0018))
    C[lona] *= (0.88 + 0.12 * g1[lona] + 0.05 * tram[lona])[:, None]
    H[lona] += 0.00015 * tram[lona]
    mad = sup == "madeira"
    veio = 0.5 + 0.5 * np.sin(P[:, 2] * 900 + 6 * fbm3(P, 0.03, 3, 21))
    C[mad] *= (0.85 + 0.15 * veio[mad])[:, None]
    cord = sup == "cordao"
    tor = 0.5 + 0.5 * np.sin((UL[:, 0] * 2 + UL[:, 1]) * 2 * math.pi / 0.006)
    C[cord] *= (0.8 + 0.2 * tor[cord])[:, None]
    H[cord] += 0.0005 * tor[cord]
    pele = sup == "pele"
    C[pele] *= (0.92 + 0.12 * g1[pele])[:, None]
    C[pele] = mix(C[pele], cor("#b5735c"), 0.25 * fbm3(P[pele], 0.05, 3, 31))

    def botao(cx, cy, cz, r=0.0085, eixo="y", sinal=-1, msk=None, cor_b=None, metal_b=1.0, alt=0.0022):
        """Botao: disco de latao com cupula no relevo. Medido no plano perpendicular ao eixo."""
        if eixo == "y":
            dist = np.sqrt((x - cx) ** 2 + (z - cz) ** 2)
            lado = (y - cy) * sinal > -0.04
        elif eixo == "x":
            dist = np.sqrt((y - cy) ** 2 + (z - cz) ** 2)
            lado = (x - cx) * sinal > -0.04
        else:
            dist = np.sqrt((x - cx) ** 2 + (y - cy) ** 2)
            lado = (z - cz) * sinal > -0.04
        k = disco(dist, r) * lado
        if msk is not None:
            k = k * msk
        sel = k > 0.001
        if not sel.any():
            return
        cb = cor(pal["latao"]) if cor_b is None else cor_b
        dome = np.sqrt(np.clip(1 - (dist[sel] / r) ** 2, 0, 1))
        C[sel] = mix(C[sel], cb * (0.75 + 0.35 * dome[:, None]), k[sel])
        metal[sel] = metal[sel] * (1 - k[sel]) + metal_b * k[sel]
        rough[sel] = rough[sel] * (1 - k[sel]) + 0.3 * k[sel]
        H[sel] = H[sel] * (1 - k[sel]) + k[sel] * (alt * dome + 0.0004)

    def pintar_mask(msk, c, rgh=None, alt=None, met=None):
        sel = msk > 0.001
        if not sel.any():
            return
        k = msk[sel]
        C[sel] = mix(C[sel], c if np.ndim(c) == 1 else c[sel], k)
        if rgh is not None:
            rough[sel] = rough[sel] * (1 - k) + rgh * k
        if alt is not None:
            H[sel] = H[sel] + alt * k
        if met is not None:
            metal[sel] = metal[sel] * (1 - k) + met * k

    def em(*nomes):
        return np.isin(parte, nomes)

    ang = np.arctan2(x, -(y - 0.03))      # 0 = frente, +PI/2 = esquerda

    # ---------- costuras gerais (lado do tronco e das mangas)
    cost = em("tronco") & (np.abs(np.abs(ang) - 1.75) < 0.012)
    H[cost] -= 0.0004
    C[cost] *= 0.8

    if nac == "FR":
        verm, bran, azul = cor(pal["vermelho"]), cor(pal["branco"]), cor(pal["casaca"])
        # lapelas: vivo vermelho nas bordas externas, abertura no meio, 7 botoes de cada lado
        lap = em("lapelas")
        borda_lap = lap & (np.abs(ang) > 0.465) & (papel == "branco")
        pintar_mask(borda_lap.astype(np.float32), verm)
        meio = lap & (np.abs(x) < 0.0015)
        H[meio] -= 0.0005
        C[meio] *= 0.6
        for s in (1, -1):
            for k in range(7):
                zz = 1.405 - k * 0.047
                botao(s * 0.072, 0, zz, msk=lap.astype(np.float32))
        # gola vermelha com vivo azul em cima; punhos vermelhos com pala branca e 3 botoes
        gl = em("gola")
        zg = z[gl]
        topo = smooth(zg, 1.536, 1.540)
        pintar_mask(np.where(gl, 0, 0).astype(np.float32), azul)
        tmp = np.zeros(n, np.float32)
        tmp[gl] = topo
        pintar_mask(tmp, azul)
        for lado, s in (("l", 1), ("r", -1)):
            pu = em("punho_" + lado) & (papel == "vermelho")
            ax_ = np.abs(x)
            pala = pu & (z > 1.445) & (ax_ > 0.625) & (ax_ < 0.700) & (np.abs(y - 0.07) < 0.028)
            tmp = np.zeros(n, np.float32)
            tmp[pala] = 1
            pintar_mask(tmp, bran, alt=0.0004)
            viv = pala & ((np.abs(np.abs(y - 0.07) - 0.026) < 0.002) | (ax_ < 0.627))
            tmp = np.zeros(n, np.float32)
            tmp[viv] = 1
            pintar_mask(tmp, verm)
            for k in range(3):
                botao(s * (0.640 + 0.022 * k), 0.07, 1.47, r=0.0065, eixo="z", sinal=1, msk=pu.astype(np.float32))
            # vivo branco na borda do punho perto do cotovelo
            tmp = np.zeros(n, np.float32)
            tmp[pu & (ax_ < 0.622)] = 1
            pintar_mask(tmp, bran)
            # ombreira azul com vivo vermelho
            om = em("ombreira_" + lado)
            ed = om & (np.abs(np.abs(y - 0.045) - 0.0205) < 0.0025)
            tmp = np.zeros(n, np.float32)
            tmp[ed] = 1
            pintar_mask(tmp, verm)
            botao(s * 0.082, 0.045, 1.5, r=0.0075, eixo="z", sinal=1, msk=om.astype(np.float32))
        # abas: palas dos bolsos (vivo vermelho) e botoes na cintura das costas
        ab = em("abas") & (papel == "casaca")
        for s in (1, -1):
            linha = ab & (np.abs(np.abs(x) - 0.085) < 0.0022) & (z < 0.95) & (z > 0.80)
            tmp = np.zeros(n, np.float32)
            tmp[linha] = 1
            pintar_mask(tmp, verm, alt=0.0003)
            botao(s * 0.05, 0.20, 0.99, r=0.008, eixo="y", sinal=1)
        # forro das abas: ornamento vermelho (losango) perto da ponta
        fo = em("abas") & (papel == "forro")
        orn = fo & (np.abs(z - 0.80) + np.abs(np.abs(x) - 0.07) * 1.4 < 0.025)
        tmp = np.zeros(n, np.float32)
        tmp[orn] = 1
        pintar_mask(tmp, verm)
        # calca: braguilha e botoes
        ca = em("calca")
        brag = ca & (np.abs(x) < 0.04) & (z > 0.90) & (y < -0.05) & (np.abs(np.abs(x) - 0.035) < 0.0012)
        H[brag] -= 0.0004
        for k in range(3):
            botao(0.03, 0, 1.05 - 0.04 * k, r=0.006, msk=ca.astype(np.float32), cor_b=cor("#c8c2b0"), metal_b=0)
        # polainas: fileira de botoes pretos do lado de fora
        po = em("polaina")
        for s in (1, -1):
            for k in range(11):
                zz = 0.12 + k * 0.037
                botao(s * 0.15, 0.03, zz, r=0.0055, eixo="x", sinal=s, msk=po.astype(np.float32),
                      cor_b=cor("#202020"), metal_b=0, alt=0.0015)
        # barretina: placa (losango com aguia e numero), cocar tricolor, chevrons de couro dos lados
        ba = em("barretina")
        pl = ba & (papel == "latao") & (y < -0.06) & (z > 1.74)
        if pl.any():
            uu = (x[pl] + 0.05) / 0.10
            vv = (z[pl] - (1.712 + 0.085 - 0.062)) / 0.13
            borda = (np.abs(uu - 0.5) * 2 + np.abs(vv - 0.53) * 1.9)
            relevo = np.where(borda > 0.86, 0.0006, 0.0)
            aguia = np.zeros(pl.sum(), np.float32)
            # aguia estilizada: corpo + cabeca + asas abertas (formas simples)
            ex, ez = (uu - 0.5) * 0.10, (vv - 0.58) * 0.13
            aguia += disco(np.sqrt((ex / 0.6) ** 2 + ez ** 2), 0.010)
            aguia += disco(np.sqrt(ex ** 2 + (ez - 0.016) ** 2), 0.0055)
            asa = (np.abs(ex) < 0.026) & (ez > -0.004) & (ez < 0.012 - np.abs(ex) * 0.25) & (ez > np.abs(ex) * 0.55 - 0.008)
            aguia += asa
            numero = amostra_mascara(carimbo("45", 256, 150), (uu - 0.30) / 0.40, (vv - 0.08) / 0.30)
            k = np.clip(aguia, 0, 1) + numero
            C[pl] *= (1 - 0.25 * np.clip(k, 0, 1))[:, None] * 1.0
            H[pl] += relevo + 0.0008 * np.clip(k, 0, 1)
        co_ = ba & (papel == "cocar")
        if co_.any():
            rr = np.sqrt(x[co_] ** 2 + (z[co_] - (1.712 + 0.195 - 0.040)) ** 2)
            cc = np.where(rr[:, None] < 0.012, cor("#1c2d6e"), np.where(rr[:, None] < 0.022, cor("#e3e0d6"), cor("#a01c1e")))
            C[co_] = cc
            H[co_] += 0.0003 * np.sin(np.arctan2(x[co_], z[co_] - 1.867) * 24)
        fe = ba & (papel == "feltro")
        chev = fe & (np.abs(y) < 0.11) & (np.abs(np.abs((z - 1.725) - np.abs(y) * 0.9) - 0.04) < 0.006) & (np.abs(x) > 0.06)
        tmp = np.zeros(n, np.float32)
        tmp[chev] = 1
        pintar_mask(tmp, cor("#0a0a0a"), rgh=0.35, alt=0.0005)
        # mochila: couro de vaca malhado (branco/marrom) com pelo
        mo = em("mochila") & (papel == "pelo")
        malha = fbm3(P, 0.09, 3, 41)
        tmp = np.zeros(n, np.float32)
        tmp[mo] = smooth(malha[mo], 0.52, 0.56)
        pintar_mask(tmp, cor("#d8cfbf"))
        # cartucheira: aguia de latao ja e geometria; risco da aba
    else:
        verm, bran, azul = cor(pal["casaca"]), cor(pal["branco"]), cor(pal["vermelho"])
        renda = cor("#e6e3d8")
        listra = cor("#2a3a7a")
        # renda branca no peito: 10 lacos (em pares) de cada lado, ponta de baluarte, botao na ponta
        tr = em("tronco") & (papel == "casaca") & (y < 0.0)
        for s in (1, -1):
            for k in range(10):
                zz = 1.415 - k * 0.037 - (k // 2) * 0.012
                if zz < 1.03:
                    continue
                xa = np.abs(x)
                dentro = tr & (np.sign(x) == s) & (xa > 0.004) & (xa < 0.085 - 0.012 * (k > 6))
                fx = xa - 0.004
                comp = 0.081 - 0.012 * (k > 6)
                # ponta em "baluarte": a renda fica mais larga na ponta de fora
                meia = 0.0065 + 0.003 * smooth(fx, comp - 0.02, comp - 0.012)
                lc = dentro & (np.abs(z - zz) < meia)
                tmp = np.zeros(n, np.float32)
                tmp[lc] = 1
                pintar_mask(tmp, renda, alt=0.0004)
                ls = lc & (np.abs(z - zz) < 0.0011)
                tmp = np.zeros(n, np.float32)
                tmp[ls] = 1
                pintar_mask(tmp, listra)
                botao(s * (comp - 0.012), 0, zz, r=0.0075, msk=tr.astype(np.float32), cor_b=cor("#d8d6cf"),
                      metal_b=0.0)
        # gola azul com renda na borda; punhos azuis com 4 lacos
        gl = em("gola")
        tmp = np.zeros(n, np.float32)
        tmp[gl & ((z > 1.536) | (z < 1.477))] = 1
        pintar_mask(tmp, renda, alt=0.0003)
        for lado, s in (("l", 1), ("r", -1)):
            pu = em("punho_" + lado) & (papel == "vermelho")
            for k in range(4):
                xx = 0.636 + 0.018 * k
                lc = pu & (np.abs(np.abs(x) - xx) < 0.0045) & (z > 1.44)
                tmp = np.zeros(n, np.float32)
                tmp[lc] = 1
                pintar_mask(tmp, renda, alt=0.0003)
                botao(s * xx, 0.07, 1.478, r=0.0055, eixo="z", sinal=1, msk=pu.astype(np.float32),
                      cor_b=cor("#d8d6cf"), metal_b=0.0)
            om = em("ombreira_" + lado)
            ed = om & (np.abs(np.abs(y - 0.045) - 0.019) < 0.004)
            tmp = np.zeros(n, np.float32)
            tmp[ed] = 1
            pintar_mask(tmp, renda)
            botao(s * 0.082, 0.045, 1.5, r=0.0075, eixo="z", sinal=1, msk=om.astype(np.float32),
                  cor_b=cor("#d8d6cf"), metal_b=0.0)
        ab = em("abas") & (papel == "casaca")
        for s in (1, -1):
            lc = ab & (np.abs(np.abs(x) - 0.07) < 0.005) & (z > 0.86) & (z < 0.96)
            tmp = np.zeros(n, np.float32)
            tmp[lc] = 1
            pintar_mask(tmp, renda, alt=0.0003)
            botao(s * 0.05, 0.20, 0.99, r=0.008, eixo="y", sinal=1, cor_b=cor("#d8d6cf"), metal_b=0.0)
        # calca cinza: costura lateral
        ca = em("calca")
        H[ca & (np.abs(np.abs(x) - 0.15) < 0.0015) & (z < 0.85)] -= 0.0004
        po = em("polaina")
        for s in (1, -1):
            for k in range(4):
                botao(s * 0.15, 0.03, 0.08 + k * 0.022, r=0.005, eixo="x", sinal=s, msk=po.astype(np.float32),
                      cor_b=cor("#202020"), metal_b=0, alt=0.0012)
        # barretina: placa com coroa e GR; roseta preta pregueada
        ba = em("barretina")
        pl = ba & (papel == "latao") & (y < -0.06)
        if pl.any():
            uu = (x[pl] + 0.045) / 0.09
            vv = (z[pl] - (1.712 + 0.112 - 0.062)) / 0.124
            gr = amostra_mascara(carimbo("GR", 256, 120), (uu - 0.12) / 0.76, (vv - 0.12) / 0.45)
            coroa = amostra_mascara(carimbo("♛", 256, 200), (uu - 0.2) / 0.6, (vv - 0.55) / 0.42)
            borda = np.minimum(np.minimum(uu, 1 - uu), np.minimum(vv, 1 - vv))
            k = np.clip(gr + coroa * 0.9, 0, 1)
            C[pl] *= (1 - 0.3 * k)[:, None]
            H[pl] += 0.0009 * k + np.where(borda < 0.05, 0.0006, 0)
        ro = ba & (papel == "couro") & (x > 0.04) & (z > 1.80)
        H[ro] += 0.0004 * np.sin(np.arctan2(y[ro] + 0.08, z[ro] - 1.837) * 20)
        # mochila: argola branca com o numero do regimento
        mo = em("mochila") & (papel == "mochila")
        ymax = y[mo].max() if mo.any() else 0
        face = mo & (y > ymax - 0.004)
        if face.any():
            cx, cz = 0.0, 1.21
            rr = np.sqrt((x[face] - cx) ** 2 + (z[face] - cz) ** 2)
            anel = (np.abs(rr - 0.075) < 0.006).astype(np.float32)
            num = amostra_mascara(carimbo("30", 256, 170), (x[face] + 0.05) / 0.10, (z[face] - cz + 0.05) / 0.10)
            k = np.clip(anel + num, 0, 1)
            C[face] = mix(C[face], cor("#d9d6cc"), k)
            rough[face] = rough[face] * (1 - k) + 0.7 * k

    # ---------- pele: cabelo curto e sobrancelhas pintados
    pe = em("pele")
    cab = pe & (z > 1.63) & (y > -0.02 + (1.72 - z) * 0.2)
    cabelo = cor("#3a2a1e")
    tmp = np.zeros(n, np.float32)
    tmp[cab] = smooth(z[cab], 1.63, 1.66) * (0.85 + 0.15 * g3[cab])
    pintar_mask(tmp, cabelo, rgh=0.7, alt=0.0003)
    costeleta = pe & (np.abs(np.abs(x) - 0.075) < 0.012) & (z > 1.63) & (z < 1.71) & (y < 0.02) & (y > -0.04)
    tmp = np.zeros(n, np.float32)
    tmp[costeleta] = 0.9
    pintar_mask(tmp, cabelo, rgh=0.7)
    for s in (1, -1):
        sob = pe & (y < -0.06) & (np.abs(z - 1.695 - 0.004 * np.abs(x * s - 0.035) / 0.03) < 0.0045) & \
            (np.abs(x - s * 0.035) < 0.022)
        tmp = np.zeros(n, np.float32)
        tmp[sob] = 0.85
        pintar_mask(tmp, cabelo)
        olho = pe & (y < -0.06) & (np.sqrt((x - s * 0.035) ** 2 + ((z - 1.672) * 1.6) ** 2) < 0.016)
        tmp = np.zeros(n, np.float32)
        tmp[olho] = 0.35
        pintar_mask(tmp, cor("#6d4a3c"))

    # ---------- sujeira e uso
    poeira = smooth(-z, -0.30, -0.02) * (0.5 + 0.5 * g1)
    terra = cor("#6b5a45")
    C = mix(C, terra, (0.35 * poeira)[:, None] * (~pe)[:, None] * (metal < 0.5)[:, None])
    joelho = smooth(-np.abs(z - 0.50), -0.07, 0.0) * (y < 0) * em("calca", "polaina")
    C = mix(C, terra, (0.25 * joelho * g1)[:, None])
    suor = smooth(-np.abs(z - 1.33), -0.05, 0) * smooth(np.abs(x), 0.14, 0.18) * em("tronco", "manga_l", "manga_r")
    C *= (1 - 0.12 * suor)[:, None]
    desbote = smooth(z, 1.40, 1.50) * smooth(Nn[:, 2], 0.3, 0.9) * la
    C = mix(C, C * 1.15 + 0.03, (0.5 * desbote)[:, None])
    gasto = (smooth(g1, 0.6, 0.8) * 0.4 * cou)
    C = mix(C, C * 1.6 + 0.05, gasto[:, None])
    rough = np.clip(rough + 0.05 * (g2 - 0.5), 0.05, 1.0)

    # ---------- AO (vertices) + cavidade leve
    C *= (0.55 + 0.45 * AO)[:, None]

    # ---------- montar imagens
    img = np.zeros((N, N, 3), np.float32)
    img[valid] = np.clip(C, 0, 1)
    orm = np.zeros((N, N, 3), np.float32)
    orm[valid] = np.stack([np.clip(AO, 0, 1), rough, metal], 1)
    alt = np.zeros((N, N), np.float32)
    alt[valid] = H
    # normal pelo relevo (diferencas no espaco do atlas; texel em metros)
    texel = metros_por_texel(d)
    gx = np.zeros_like(alt)
    gy = np.zeros_like(alt)
    vv = valid
    ok_x = vv[:, 1:-1] & vv[:, 2:] & vv[:, :-2]
    gx[:, 1:-1] = np.where(ok_x, (alt[:, 2:] - alt[:, :-2]) / (2 * texel), 0)
    ok_y = vv[1:-1] & vv[2:] & vv[:-2]
    gy[1:-1] = np.where(ok_y, (alt[2:] - alt[:-2]) / (2 * texel), 0)
    nrm = np.stack([-gx, -gy, np.ones_like(alt)], -1)   # DirectX (G para baixo = +linha)
    nrm /= np.linalg.norm(nrm, axis=-1, keepdims=True)
    # preencher fora das ilhas (evita costura escura nos mipmaps)
    img, orm, nrm = preencher(valid, img, orm, nrm)
    os.makedirs(SAIDA, exist_ok=True)
    Image.fromarray((img * 255 + 0.5).astype(np.uint8)).save(os.path.join(SAIDA, "T_Farda_%s_C.png" % nac))
    Image.fromarray((orm * 255 + 0.5).astype(np.uint8)).save(os.path.join(SAIDA, "T_Farda_%s_ORM.png" % nac))
    Image.fromarray(((nrm * 0.5 + 0.5) * 255 + 0.5).astype(np.uint8)).save(os.path.join(SAIDA, "T_Farda_%s_N.png" % nac))
    print(nac, "pronto em", round(time.time() - t0, 1), "s ; texel = %.2f mm" % (texel * 1000))


def metros_por_texel(d):
    uv = d["uv"]
    co = d["co"]
    a3 = 0.5 * np.linalg.norm(np.cross(co[:, 1] - co[:, 0], co[:, 2] - co[:, 0]), axis=1)
    e1 = uv[:, 1] - uv[:, 0]
    e2 = uv[:, 2] - uv[:, 0]
    a2 = 0.5 * np.abs(e1[:, 0] * e2[:, 1] - e1[:, 1] * e2[:, 0]) * N * N
    ok = a2 > 1e-6
    return float(np.sqrt(np.median(a3[ok] / a2[ok])))


def preencher(valid, *imgs, passos=12):
    imgs = [im.copy() for im in imgs]
    m = valid.copy()
    for _ in range(passos):
        acc = [np.zeros_like(im) for im in imgs]
        cnt = np.zeros(m.shape, np.float32)
        for dy, dx in ((0, 1), (0, -1), (1, 0), (-1, 0)):
            sm = np.roll(m, (dy, dx), (0, 1))
            for k, im in enumerate(imgs):
                acc[k] += np.roll(im, (dy, dx), (0, 1)) * sm[..., None]
            cnt += sm
        novo = (~m) & (cnt > 0)
        for k, im in enumerate(imgs):
            im[novo] = acc[k][novo] / cnt[novo][:, None]
        m = m | novo
    return imgs


if __name__ == "__main__":
    for nac in (sys.argv[1:] or ["FR", "GB"]):
        pintar(nac)
