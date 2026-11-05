"""Validate the UI UV library, optionally render numerical GPU and graph regressions.

python tools/test_ui_uv_library.py
python tools/test_ui_uv_library.py --gpu --backend both
Requires Pillow only for --gpu. Runs minimized CLI fixtures; never synthesizes input.
"""
from __future__ import annotations
import argparse
import copy
import json
import math
import re
import shlex
import struct
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
EFFECTS = ROOT / "shaders/effects"
FUNCTIONS = ROOT / "library/functions"
IDS = """ui_uv_coordinates ui_uv_fit ui_uv_affine ui_uv_address ui_uv_nine_slice
ui_uv_three_slice ui_uv_atlas ui_sprite_flipbook ui_uv_polar ui_uv_unwrap
ui_uv_repeat_grid ui_uv_wave ui_uv_displace ui_uv_projective ui_uv_mask_rect
ui_uv_mask_progress ui_uv_sample ui_draw_panel ui_draw_slice_test ui_effect_state
ui_blend_clip ui_uv_field_view""".split()


def metadata(path):
    text = path.read_text(encoding="utf-8")
    params = {}
    inputs = []
    generator = "//@generator" in text
    for line in text.splitlines():
        if line.startswith("//@input "):
            inputs.append(line[9:])
        if line.startswith("//@param "):
            t = shlex.split(line[9:])
            size = {"float2": 2, "float4": 4, "color": 4}.get(t[1], 1)
            params[t[0]] = (t[1], [float(v) for v in t[2:2 + size]])
    return text, params, [] if generator else inputs or ["Image"]


def static_checks():
    for id in IDS:
        text, params, inputs = metadata(EFFECTS / (id + ".slang"))
        assert re.fullmatch(r"[a-z][a-z0-9_]*", id), id
        assert "//@name UI " in text and "//@category UI UV/" in text, id
        assert len(params) <= 32 and len(inputs) <= 4, id
        assert "float4 Effect(EffectContext ctx)" in text, id
        assert text.isascii(), id
        if "InputUV(" in text or "UIReadLinear(" in text:
            assert "//@halo full" in text, id
        for name, (typ, defaults) in params.items():
            assert all(math.isfinite(v) for v in defaults), (id, name)
    paths = sorted(FUNCTIONS.glob("UI UV *.sxfn"))
    assert len(paths) == 24, len(paths)
    numeric = {"Float", "Int", "Bool"}
    for path in paths:
        doc = json.loads(path.read_text(encoding="utf-8"))
        assert doc["format"] == "shaderixel-function" and doc["name"] == path.stem, path
        assert doc["kind"] in ("subgraph", "tool") and doc["description"], path
        g = doc["graph"]
        nodes = {n["id"]: n for n in g["nodes"]}
        assert len(nodes) == len(g["nodes"]) and 0 not in nodes, path
        assert nodes[g["output"]]["kind"] == "suboutput", path
        for kind in ("subinput", "suboutput"):
            ports = sorted(n["port"] for n in nodes.values() if n["kind"] == kind)
            assert ports == list(range(len(ports))), (path, kind, ports)
        variables = {v["name"]: v for v in g.get("variables", [])}
        assert len(variables) == len(g.get("variables", [])), path
        for v in variables.values():
            assert v["exposed"] and v["desc"], (path, v["name"])
        visiting, visited = set(), set()

        def walk(id):
            if id in visited or id == 0:
                return
            assert id not in visiting and id in nodes, (path, id, "cycle or missing source")
            visiting.add(id)
            for src in nodes[id].get("inputs", []):
                walk(src)
            visiting.remove(id)
            visited.add(id)

        def node_type(id):
            if not id:
                return "Image"
            n = nodes[id]
            if n["kind"] in ("value", "varget", "subinput"):
                return n["value"]["type"]
            if n["kind"] == "math":
                return "Vec2" if n["op"] == "vec2" else "Float"
            return "Image"

        for n in nodes.values():
            walk(n["id"])
            assert all(src == 0 or src in nodes for src in n.get("inputs", [])), (path, n)
            if "inputChannels" in n:
                assert len(n["inputChannels"]) == len(n["inputs"]), (path, n["id"])
            if n["kind"] == "varget":
                assert n["op"] in variables, (path, n["op"])
                assert variables[n["op"]]["value"]["type"] == n["value"]["type"], (path, n)
            if n["kind"] == "effect":
                id = n["effect"]["effect"]
                assert (EFFECTS / (id + ".slang")).is_file(), (path, id)
                _, params, inputs = metadata(EFFECTS / (id + ".slang"))
                pins = n.get("paramPinNames", [])
                assert len(pins) == len(set(pins)) and all(p in params for p in pins), (path, n)
                assert len(n["inputs"]) == len(inputs) + len(pins), (path, n)
                assert all(k in params for k in n["effect"].get("set", {})), (path, n)
                for src in n["inputs"][:len(inputs)]:
                    assert node_type(src) in ("Image", "Canal"), (path, n["id"], src)
                for k, src in zip(pins, n["inputs"][len(inputs):]):
                    typ = params[k][0]
                    expected = {"float2": "Vec2", "color": "Color"}.get(typ, "Float")
                    actual = node_type(src)
                    assert actual == expected or actual in numeric and expected in numeric, (path, k, actual, expected)
                for k, v in n["effect"].get("set", {}).items():
                    size = len(params[k][1])
                    assert (isinstance(v, list) and len(v) == size) if size > 1 else isinstance(v, (int, float)), (path, k)
        assert visited == nodes.keys(), path
    print(f"PASS static: {len(IDS)} effects, {len(paths)} graphs, references, types, ports, parameters and cycles", flush=True)
    return paths


def save_document(path, nodes, output, width=256, height=192):
    graph = {"output": len(nodes) + 1, "variables": [], "nodes": copy.deepcopy(nodes)}
    graph["nodes"].append({"id": graph["output"], "kind": "output", "inputs": [output], "pos": [1400, 0], "name": "Output"})
    doc = dict(format="shaderixel", version=2, name=path.stem, width=width, height=height,
               precision="float32", mode="nodes", blendInGamma=False, nextId=len(nodes) + 2,
               activeNode=output, layers=[], graph=graph)
    data = json.dumps(doc, separators=(",", ":")).encode("utf-8")
    path.write_bytes(b"SXL1" + struct.pack("<IQ", 2, len(data)) + data)


def gpu_checks(paths, backend):
    from PIL import Image
    out = ROOT / "build/ui_uv_tests" / backend
    out.mkdir(parents=True, exist_ok=True)
    exe = ROOT / "build/Release" / ("ShaderixelVk.exe" if backend == "vulkan" else "Shaderixel.exe")
    assert exe.exists(), exe
    startup = subprocess.STARTUPINFO()
    startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
    startup.wShowWindow = 7
    index = 0

    def render(name, nodes, output, width=256, height=192):
        nonlocal index
        index += 1
        prefix = out / f"{index:02}_{name}"
        doc, png, screen = prefix.with_suffix(".sxl"), prefix.with_suffix(".png"), prefix.with_suffix(".screen.png")
        save_document(doc, nodes, output, width, height)
        if png.exists():
            png.unlink()
        # Both capture and dump are intentional: capture guarantees the CLI run exits after export.
        run = subprocess.run([str(exe), str(doc), "--frames", "180", "--dump-image", str(png), "--capture", str(screen)],
                             cwd=out, startupinfo=startup, timeout=120, check=True)
        log = (out / "shaderixel.log").read_text(encoding="utf-8", errors="replace")
        prefix.with_suffix(".log").write_text(log, encoding="utf-8")
        assert "Dumped document image" in log and png.exists(), (name, log)
        assert not re.search(r"FAILED|error\([0-9]+\)|unknown effect|cycle", log, re.I), (name, log)
        image = Image.open(png).convert("RGBA")
        image.load()
        return image

    def fx(nodes, id, inputs=(), **sets):
        n = len(nodes) + 1
        nodes.append({"id": n, "kind": "effect", "inputs": list(inputs), "pos": [n * 250, 0],
                      "effect": {"effect": id, "set": sets}, "paramPinNames": []})
        return n

    def linear(pixel):
        def decode(v):
            v /= 255
            return v / 12.92 if v <= .04045 else ((v + .055) / 1.055) ** 2.4
        return tuple(decode(v) for v in pixel[:3]) + (pixel[3] / 255,)

    def near(actual, expected, tolerance=.009):
        assert all(abs(a - e) <= tolerance for a, e in zip(actual, expected)), (actual, expected)

    common = dict(target_size=[208, 128], source_size=[64, 64], margin_units=1,
                  left=8, top=8, right=8, bottom=8, softness=0)
    nodes = []
    s = fx(nodes, "ui_uv_nine_slice", **common)
    stretched = render("slice_stretch", nodes, s)
    near(linear(stretched.getpixel((27, 35))), (3.5 / 64, 3.5 / 64, 1, 1))
    near(linear(stretched.getpixel((65, 65))), (.125 + (41.5 - 8) / 192 * .75, .125 + (33.5 - 8) / 112 * .75, 1, 1))
    near(linear(stretched.getpixel((5, 5)))[2:], (0, 1))
    nodes = []
    s = fx(nodes, "ui_uv_nine_slice", **common, center_mode=1, edge_mode=1)
    tiled = render("slice_tile", nodes, s)
    near(linear(tiled.getpixel((60, 70))), linear(tiled.getpixel((108, 70))))
    near(linear(tiled.getpixel((60, 35))), linear(tiled.getpixel((108, 35))))
    near(linear(tiled.getpixel((27, 35))), linear(stretched.getpixel((27, 35))))
    nodes = []
    source = fx(nodes, "ui_draw_slice_test")
    s = fx(nodes, "ui_uv_nine_slice", **common, center_mode=3, edge_mode=1)
    result = fx(nodes, "ui_uv_sample", [source, s, 0])
    hollow = render("slice_hollow", nodes, result)
    assert hollow.getpixel((128, 96))[3] == 0
    assert hollow.getpixel((27, 35))[3] == 255
    nodes = []
    s = fx(nodes, "ui_uv_nine_slice", target_size=[9, 7], source_size=[64, 64], margin_units=1,
           left=50, right=50, top=50, bottom=50, softness=0)
    tiny = render("slice_tiny", nodes, s)
    assert tiny.getpixel((128, 96))[2:] == (255, 255)
    assert all(0 < c < 255 for c in tiny.getpixel((128, 96))[:2])
    nodes = []
    source = fx(nodes, "ui_draw_slice_test")
    s = fx(nodes, "ui_uv_coordinates", target_size=[100.5, 60.5], softness=1)
    result = fx(nodes, "ui_uv_sample", [source, s, 0])
    fractional = render("fractional_widget_edge", nodes, result)
    near((fractional.getpixel((77, 96))[3] / 255,), (.25,), .005)
    assert fractional.getpixel((78, 96))[3] == 255
    nodes = []
    s = fx(nodes, "ui_uv_three_slice", target_size=[208, 64], source_size=[64, 64], start_cap=8, end_cap=8, softness=0)
    bar = render("three_slice_caps", nodes, s)
    near(linear(bar.getpixel((27, 96))), (3.5 / 64, 32.5 / 64, 1, 1))
    near(linear(bar.getpixel((60, 96))), linear(bar.getpixel((108, 96))))
    nodes = []
    s = fx(nodes, "ui_uv_nine_slice", **common, center_mode=2, edge_mode=2, phase=[-.25, 0])
    mirror = render("mirror_negative_phase", nodes, s)
    near(linear(mirror.getpixel((60, 70)))[0:1], (.125 + ((36.5 - 8) / 48 - .25) * .75,))
    near(linear(mirror.getpixel((60, 70))), linear(mirror.getpixel((156, 70))))
    nodes = []
    rounded_common = dict(common, target_size=[221, 133])
    s = fx(nodes, "ui_uv_nine_slice", **rounded_common, center_mode=1, tile_fit=1)
    round_tiles = render("round_whole_tiles", nodes, s)
    near(linear(round_tiles.getpixel((42, 80)))[0:1], (.125 + (17 / 205 * 4) * .75,))
    nodes = []
    s = fx(nodes, "ui_uv_nine_slice", **common, source_origin=[.25, .25], source_extent=[.5, .5])
    region = render("source_region", nodes, s)
    near(linear(region.getpixel((27, 35))), (.25 + 3.5 / 64 * .5, .25 + 3.5 / 64 * .5, 1, 1))
    nodes = []
    s = fx(nodes, "ui_uv_nine_slice", **common, source_origin=[1, 0])
    invalid_region = render("empty_source_region", nodes, s)
    near(linear(invalid_region.getpixel((128, 96))), (0, 0, 0, 1))
    nodes = []
    s = fx(nodes, "ui_uv_coordinates", softness=0)
    s = fx(nodes, "ui_uv_affine", [s], offset=[-1.25, 2.25])
    s = fx(nodes, "ui_uv_address", [s], mode_x=1, mode_y=2)
    addressed = render("negative_address", nodes, s)
    x, y = 80, 90
    u, v = (x + .5) / 256 - 1.25, (y + .5) / 192 + 2.25
    near(linear(addressed.getpixel((x, y))), (u % 1, 1 - abs((v * .5 % 1) * 2 - 1), 1, 1))
    nodes = []
    s = fx(nodes, "ui_uv_atlas", [0], grid=[4, 4], frame=-11, gutter=[2, 2])
    atlas = render("atlas_negative_frame", nodes, s)
    for x, y in [(0, 0), (128, 96), (255, 191)]:
        expected = ((64 + 2.5 + (x + .5) / 256 * 59) / 256,
                    (48 + 2.5 + (y + .5) / 192 * 43) / 192, 1, 1)
        near(linear(atlas.getpixel((x, y))), expected)
    nodes = []
    s = fx(nodes, "ui_uv_projective", top_left=[.125, .125], top_right=[.875, .125],
           bottom_left=[.125, .875], bottom_right=[.875, .875], softness=0)
    quad = render("quad_rectangle", nodes, s)
    near(linear(quad.getpixel((100, 80))), (((100.5 / 256) - .125) / .75, ((80.5 / 192) - .125) / .75, 1, 1))
    near(linear(quad.getpixel((0, 0)))[2:], (0, 1))
    nodes = []
    s = fx(nodes, "ui_uv_projective", top_left=[.5, .5], top_right=[.5, .5], bottom_left=[.5, .5], bottom_right=[.5, .5])
    degenerate = render("quad_degenerate", nodes, s)
    near(linear(degenerate.getpixel((128, 96))), (0, 0, 0, 1))
    nodes = []
    s = fx(nodes, "ui_uv_fit", target_size=[160, 160], source_size=[200, 100], softness=0)
    fitted = render("fit_contain", nodes, s)
    assert fitted.getpixel((128, 30))[2] == 0 and fitted.getpixel((128, 96))[2] == 255
    for progress in [0, 1]:
        nodes = []
        source = fx(nodes, "ui_draw_slice_test")
        s = fx(nodes, "ui_uv_coordinates", target_size=[160, 80], softness=0)
        s = fx(nodes, "ui_uv_mask_rect", [s], space_size=[160, 80], radius_tl=20, radius_tr=20, radius_bl=20, radius_br=20, softness=0)
        s = fx(nodes, "ui_uv_mask_progress", [s], progress=progress, space_size=[160, 80], softness=0)
        result = fx(nodes, "ui_uv_sample", [source, s, 0])
        image = render(f"progress_{progress}", nodes, result)
        assert image.getpixel((128, 96))[3] == progress * 255
        assert image.getpixel((48, 56))[3] == 0
    nodes = []
    neutral = fx(nodes, "ui_draw_panel", size=[256, 192], radius=0, border=0, bevel=0, color_top=[.5, .5, .5, 1], color_bottom=[.5, .5, .5, 1])
    s = fx(nodes, "ui_uv_displace", [0, neutral, 0], strength=[32, 32])
    image = render("neutral_displacement", nodes, s)
    near(linear(image.getpixel((80, 90))), (80.5 / 256, 90.5 / 192, 1, 1))
    nodes = []
    def solid(color):
        return fx(nodes, "ui_draw_panel", size=[256, 192], radius=0, border=0, bevel=0,
                  shadow_color=[0, 0, 0, 0], color_top=color, color_bottom=color)
    a, b = solid([.2, .4, .6, .5]), solid([.8, .1, .2, .5])
    result = fx(nodes, "ui_blend_clip", [a, b, 0], clip_to_background=0)
    image = render("premultiplied_blend", nodes, result)
    near(linear(image.getpixel((128, 96))), (.6, .2, 1 / 3, .75))
    nodes = []
    source = solid([.2, .4, .6, .8])
    result = fx(nodes, "ui_effect_state", [source], disabled=1)
    image = render("disabled_state", nodes, result)
    gray = .2 * .2126 + .4 * .7152 + .6 * .0722
    near(linear(image.getpixel((128, 96))), (gray, gray, gray, .8 * .45))
    for filter_mode in [0, 1]:
        nodes = []
        source = fx(nodes, "ui_draw_slice_test", grid_width=0)
        uv = fx(nodes, "ui_uv_affine", [0], repeat=[0, 0], pivot=[0, 0], offset=[.5, .5])
        result = fx(nodes, "ui_uv_sample", [source, uv, 0], filter=filter_mode)
        image = render("bilinear" if filter_mode == 0 else "nearest", nodes, result)
        expected = (.12, .26, .325, 1) if filter_mode == 0 else (.04, .07, .1, 1)
        near(linear(image.getpixel((128, 96))), expected)
    frames = []
    for frame in [0, 1, .5]:
        nodes = []
        source = fx(nodes, "ui_draw_slice_test", atlas=1)
        result = fx(nodes, "ui_sprite_flipbook", [source, 0], frame=frame, interpolate=1)
        frames.append(render(f"flipbook_{frame}", nodes, result))
    for point in [(50, 50), (128, 96), (200, 150)]:
        aa, bb, cc = [linear(im.getpixel(point)) for im in frames]
        near(cc, tuple((a + b) / 2 for a, b in zip(aa, bb)))
    print(f"PASS {backend}: numerical checks for slices, tiling, transparency, atlas gutters, addressing, quad, fit, masks, displacement, alpha and flipbook", flush=True)
    for path in paths:
        doc = json.loads(path.read_text(encoding="utf-8"))
        nodes = []
        source = fx(nodes, "ui_draw_slice_test", atlas=int("Atlas" in path.stem or "Flipbook" in path.stem))
        ports = sorted((n for n in doc["graph"]["nodes"] if n["kind"] == "subinput"), key=lambda n: n["port"])
        connections = [source if p["value"]["type"] == "Image" and "UV Map" not in p["name"] else 0 for p in ports]
        n = len(nodes) + 1
        nodes.append(dict(id=n, kind="function", name=doc["name"], inputs=connections, sub=doc["graph"],
                          funcKind=doc["kind"], applyMode=False, pos=[500, 0]))
        image = render(path.stem.replace(" ", "_"), nodes, n, 512, 384)
        alpha = image.getchannel("A")
        assert alpha.getbbox() is not None, (path, "empty graph output")
        extrema = image.convert("RGB").getextrema()
        assert max(hi for lo, hi in extrema) > 10, (path, "black graph output")
        print(f"PASS {backend}: {path.stem}", flush=True)
    print(f"PASS {backend}: {index} minimized GPU fixtures", flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--gpu", action="store_true")
    parser.add_argument("--backend", choices=["d3d12", "vulkan", "both"], default="d3d12")
    args = parser.parse_args()
    paths = static_checks()
    if args.gpu:
        for backend in (["d3d12", "vulkan"] if args.backend == "both" else [args.backend]):
            gpu_checks(paths, backend)


if __name__ == "__main__":
    main()
