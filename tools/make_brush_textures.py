"""Default brush textures (shaders/brushes/textures/*.png), used by brushes
declaring //@texture: white = paint, alpha = shape. Deterministic.

    python tools/make_brush_textures.py
"""
import math
import os
import random

from PIL import Image, ImageDraw, ImageFilter

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(os.path.dirname(HERE), 'shaders', 'brushes', 'textures')
N = 256


def save(img, name):
    os.makedirs(OUT, exist_ok=True)
    img.save(os.path.join(OUT, name))
    print('wrote', name)


def splatter():
    rnd = random.Random(7)
    a = Image.new('L', (N, N), 0)
    d = ImageDraw.Draw(a)
    for _ in range(90):
        r = rnd.uniform(2, 14) * (1 if rnd.random() < 0.8 else 2.2)
        ang = rnd.uniform(0, math.tau)
        dist = min(abs(rnd.gauss(0, 0.17)), 0.36) * N
        x, y = N / 2 + math.cos(ang) * dist, N / 2 + math.sin(ang) * dist
        d.ellipse([x - r, y - r, x + r, y + r], fill=255)
    a = a.filter(ImageFilter.GaussianBlur(1.2))
    save(Image.merge('RGBA', (Image.new('L', (N, N), 255),) * 3 + (a,)), 'splatter.png')


def grain():
    rnd = random.Random(3)
    a = Image.new('L', (N, N))
    a.putdata([int(255 * (0.35 + 0.65 * rnd.random() ** 0.7)) for _ in range(N * N)])
    a = a.filter(ImageFilter.GaussianBlur(0.8))
    # Round falloff.
    m = Image.new('L', (N, N), 0)
    ImageDraw.Draw(m).ellipse([8, 8, N - 8, N - 8], fill=255)
    m = m.filter(ImageFilter.GaussianBlur(14))
    a = Image.composite(a, Image.new('L', (N, N), 0), m)
    save(Image.merge('RGBA', (Image.new('L', (N, N), 255),) * 3 + (a,)), 'grain.png')


def bristles():
    rnd = random.Random(11)
    a = Image.new('L', (N, N), 0)
    d = ImageDraw.Draw(a)
    for i in range(48):
        x = N * 0.1 + i * (N * 0.8 / 47) + rnd.uniform(-2, 2)
        w = rnd.uniform(1.5, 4)
        top, bottom = N * rnd.uniform(0.05, 0.2), N * rnd.uniform(0.8, 0.97)
        d.rectangle([x - w / 2, top, x + w / 2, bottom], fill=int(rnd.uniform(150, 255)))
    a = a.filter(ImageFilter.GaussianBlur(0.9)).rotate(90)
    save(Image.merge('RGBA', (Image.new('L', (N, N), 255),) * 3 + (a,)), 'bristles.png')


def canvas():
    a = Image.new('L', (N, N))
    px = []
    for y in range(N):
        for x in range(N):
            wx = 0.5 + 0.5 * math.sin(x * math.tau / 8)
            wy = 0.5 + 0.5 * math.sin(y * math.tau / 8)
            px.append(int(255 * (0.45 + 0.55 * max(wx * (y // 8 % 2), wy * (1 - y // 8 % 2)))))
    a.putdata(px)
    save(Image.merge('RGBA', (Image.new('L', (N, N), 255),) * 3 + (a,)), 'canvas.png')


if __name__ == '__main__':
    splatter()
    grain()
    bristles()
    canvas()
