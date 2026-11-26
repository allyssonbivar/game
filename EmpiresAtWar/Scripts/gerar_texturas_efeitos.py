"""Gera as texturas dos efeitos (fumaca de polvora, sangue em jato/gotas e manchas) por ruido procedural.
Feitas do zero (nada copiado). Saida: EmpiresAtWar/Texturas/*.png (atlas 2x2; R = alfa, G/B = sombreado, A = alfa).
Rodar com um Python que tenha numpy e Pillow:  python gerar_texturas_efeitos.py
"""
import os
import numpy as np
from PIL import Image, ImageFilter

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Texturas")
N = 512                      # cada quadro do atlas
rng = np.random.default_rng(1805)


def smooth_noise(n, cells, seed):
    """Ruido de valor suave (interpolacao bicubica de uma grade aleatoria), periodico."""
    r = np.random.default_rng(seed)
    g = r.random((cells, cells)).astype(np.float32)
    img = Image.fromarray((g * 255).astype(np.uint8)).resize((n, n), Image.BICUBIC)
    return np.asarray(img, np.float32) / 255.0


def fbm(n, seed, octaves=6, base=4):
    acc = np.zeros((n, n), np.float32); amp = 1.0; tot = 0.0
    for o in range(octaves):
        acc += amp * smooth_noise(n, base * 2 ** o, seed * 31 + o); tot += amp; amp *= 0.5
    return acc / tot


yy, xx = np.mgrid[0:N, 0:N].astype(np.float32)
u = (xx + 0.5) / N * 2 - 1     # -1..1
v = (yy + 0.5) / N * 2 - 1


def puff(seed):
    """Floco de fumaca: nuvem de bolhas gaussianas deformada por ruido; sombra propria com luz vinda de cima."""
    r = np.random.default_rng(seed)
    blobs = np.zeros((N, N), np.float32)
    for _ in range(28):
        cx, cy = r.normal(0, 0.26, 2); s = r.uniform(0.2, 0.36)
        blobs += r.uniform(0.5, 1.0) * np.exp(-((u - cx) ** 2 + (v - cy) ** 2) / (2 * s * s))
    blobs = blobs / blobs.max()
    # Deformacao do dominio: bordas com fiapos e "couve-flor".
    wx = (fbm(N, seed + 1, 5, 4) - 0.5) * 0.35
    wy = (fbm(N, seed + 2, 5, 4) - 0.5) * 0.35
    ix = np.clip(((u + wx + 1) / 2 * N).astype(int), 0, N - 1)
    iy = np.clip(((v + wy + 1) / 2 * N).astype(int), 0, N - 1)
    warped = blobs[iy, ix]
    detail = fbm(N, seed + 3, 6, 5)
    dens = np.clip(warped * (0.6 + 0.8 * detail) - 0.12, 0, 1) * 1.4
    rad = np.sqrt(u ** 2 + v ** 2)
    dens = np.clip(dens, 0, 1) * np.clip(1 - rad / 0.88, 0, 1) ** 1.2
    # Sem chuvisco de alfa baixo fora da nuvem (40 flocos sobrepostos somavam um "quadrado" pontilhado no jogo).
    dens = np.asarray(Image.fromarray((np.clip(dens, 0, 1) * 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(2)), np.float32) / 255
    dens = np.clip((dens - 0.10) / 0.9, 0, 1)
    # Sombra propria: densidade acumulada na direcao da luz (cima-esquerda) escurece o miolo e a base.
    blur = np.asarray(Image.fromarray((dens * 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(18)), np.float32) / 255
    shadow = np.zeros_like(blur)
    for k in range(1, 9):
        shadow += np.roll(np.roll(blur, k * 6, axis=0), k * 3, axis=1)
    shadow /= 8
    shade = np.clip(1.0 - 0.45 * shadow + 0.14 * (detail - 0.5), 0.45, 1.0)
    return np.dstack([shade, shade, shade, np.clip(dens, 0, 1)])


def streak(seed, tendrils):
    """Filete de sangue: corpo afinando para tras, contas (gotas) e fiapos laterais; eixo do jato = U (x)."""
    r = np.random.default_rng(seed)
    a = np.zeros((N, N), np.float32)
    t = (u + 1) / 2                                   # 0 = cauda, 1 = cabeca
    wob = (fbm(N, seed, 4, 3)[N // 2] - 0.5) * 0.10
    center = np.interp(xx[0], np.arange(N), wob)[None, :]
    width = 0.035 + 0.15 * t ** 1.4                   # fino atras, gordo na frente
    a = np.maximum(a, np.clip(1 - np.abs(v - center) / width, 0, 1) * (t > 0.03))
    head = np.sqrt(((u - 0.74) / 0.2) ** 2 + ((v - center) / 0.16) ** 2)
    a = np.maximum(a, np.clip(1.2 - head, 0, 1))
    for _ in range(r.integers(4, 9)):                 # contas ao longo do filete
        px = r.uniform(-0.8, 0.6); py = np.interp((px + 1) / 2 * N, np.arange(N), wob) + r.normal(0, 0.02)
        rr = r.uniform(0.04, 0.08)
        a = np.maximum(a, np.clip(1.3 - np.sqrt((u - px) ** 2 + (v - py) ** 2) / rr, 0, 1))
    for _ in range(tendrils):                          # fiapos que saem do corpo
        sx = r.uniform(-0.3, 0.7); ang = r.uniform(0.25, 0.7) * r.choice([-1, 1]); ln = r.uniform(0.15, 0.4)
        s = np.linspace(0, 1, 60)
        for k in s:
            px = sx - k * ln * np.cos(ang); py = np.interp((sx + 1) / 2 * N, np.arange(N), wob) + k * ln * np.sin(ang)
            rr = 0.026 * (1 - k) + 0.006
            a = np.maximum(a, np.clip(1.2 - np.sqrt((u - px) ** 2 + (v - py) ** 2) / rr, 0, 1))
    for _ in range(r.integers(10, 22)):                # respingos soltos
        px = r.uniform(-0.9, 0.95); py = r.normal(0, 0.18); rr = r.uniform(0.006, 0.02)
        a = np.maximum(a, np.clip(1.3 - np.sqrt((u - px) ** 2 + (v - py) ** 2) / rr, 0, 1))
    a = np.clip(a * 1.6, 0, 1)
    a *= np.clip((1 - np.abs(u)) * 12, 0, 1) * np.clip((1 - np.abs(v)) * 12, 0, 1)
    # Sombreado de liquido: miolo escuro, brilho na borda de cima.
    gy, gx = np.gradient(a)
    rgb = np.clip(0.7 + 0.3 * a - gy * 8, 0.45, 1.0)
    return np.dstack([rgb, rgb, rgb, a])


def drops(seed, count, big):
    r = np.random.default_rng(seed)
    a = np.zeros((N, N), np.float32)
    for _ in range(count):
        px, py = r.normal(0, 0.35, 2) if not big else r.normal(0, 0.12, 2)
        rr = r.uniform(0.04, 0.12) if not big else r.uniform(0.25, 0.4)
        el = r.uniform(1.0, 1.6)
        d = np.sqrt(((u - px) / (rr * el)) ** 2 + ((v - py) / rr) ** 2)
        a = np.maximum(a, np.clip((1 - d) * 6, 0, 1))
    a *= np.clip((1 - np.abs(u)) * 12, 0, 1) * np.clip((1 - np.abs(v)) * 12, 0, 1)
    gy, gx = np.gradient(a)
    rgb = np.clip(0.55 + 0.45 * a - gy * 12 - gx * 6, 0, 1)
    return np.dstack([rgb, rgb, rgb, a])


def splat(seed):
    """Mancha no chao: poca irregular + coroa de gotas satelites + riscos de impacto."""
    r = np.random.default_rng(seed)
    ang = np.arctan2(v, u); rad = np.sqrt(u ** 2 + v ** 2)
    k = np.arange(1, 9)
    edge = 0.34 + sum(r.uniform(0, 0.05) * np.cos(kk * ang + r.uniform(0, 6.28)) for kk in k)
    a = np.clip((edge - rad) * 30, 0, 1)
    for _ in range(r.integers(18, 34)):
        th = r.uniform(0, 6.28); d0 = r.uniform(0.38, 0.85); rr = r.uniform(0.01, 0.045)
        px, py = d0 * np.cos(th), d0 * np.sin(th)
        a = np.maximum(a, np.clip((rr - np.sqrt((u - px) ** 2 + (v - py) ** 2)) * 200, 0, 1))
        if r.random() < 0.35:   # risco ligando a gota a poca
            for s in np.linspace(0.3, d0, 25):
                w = rr * 0.5 * (s / d0)
                a = np.maximum(a, np.clip((w - np.sqrt((u - s * np.cos(th)) ** 2 + (v - s * np.sin(th)) ** 2)) * 300, 0, 1))
    a *= np.clip((0.97 - rad) * 20, 0, 1)
    n = fbm(N, seed, 5, 8)
    rgb = np.clip(0.6 + 0.5 * (n - 0.5) + 0.25 * (1 - a), 0, 1)   # borda mais clara (seca), centro escuro
    return np.dstack([rgb, rgb, rgb, a])


def atlas(tiles, name):
    big = np.zeros((N * 2, N * 2, 4), np.float32)
    for i, t in enumerate(tiles):
        y, x = divmod(i, 2)
        # Canais: R = transparencia (alfa), G e B = sombreado, A = alfa de novo.
        # O material le a transparencia do R pela saida padrao da textura (a ligacao por nome "A" no Python caia no RGB).
        big[y * N:(y + 1) * N, x * N:(x + 1) * N] = np.dstack([t[:, :, 3], t[:, :, 0], t[:, :, 1], t[:, :, 3]])
    Image.fromarray((np.clip(big, 0, 1) * 255).astype(np.uint8), "RGBA").save(os.path.join(OUT, name))
    print("gerado", name)


os.makedirs(OUT, exist_ok=True)
atlas([puff(11), puff(23), puff(37), puff(41)], "T_Fumaca.png")
atlas([streak(5, 3), streak(8, 6), streak(13, 1), drops(17, 22, False)], "T_Sangue_Jato.png")
atlas([splat(3), splat(9), splat(21), splat(29)], "T_Sangue_Mancha.png")
