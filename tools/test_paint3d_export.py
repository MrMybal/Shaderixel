"""Exercise project texture export with separate native tiles, masked folders and a custom RGBA preset."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile
import time
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
OBJ = """o TwoTiles
v -1 0 0
v 0 0 0
v -1 1 0
v 0 1 0
v 1 0 0
v 1 1 0
vt 0.05 0.05
vt 0.95 0.05
vt 0.05 0.95
vt 0.95 0.95
vt 1.05 0.05
vt 1.95 0.05
vt 1.05 0.95
vt 1.95 0.95
f 1/1 2/2 3/3
f 3/3 2/2 4/4
f 2/5 5/6 4/7
f 4/7 5/6 6/8
"""


def check(exe, hybrid):
    validation = ROOT / "build" / "Paint3DExportValidation"
    validation.mkdir(exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix=exe.stem + "-", dir=validation))
    model = work / "tiles.obj"
    model_text = OBJ
    if hybrid:
        # The second surface is folded away from the first; face fill also has a geometric boundary.
        folded = OBJ.replace("v 1 0 0", "v 1 0 1").replace("v 1 1 0", "v 1 1 1")
        model_text = "mtllib tiles.mtl\n" + folded.replace("f 1/1", "usemtl Body\nf 1/1")
        model_text += "usemtl Trim\nf 2/1 5/2 4/3\nf 4/3 5/2 6/4\n"
        (work / "tiles.mtl").write_text("newmtl Body\nnewmtl Trim\n", encoding="utf-8")
    model.write_text(model_text, encoding="utf-8")
    env = dict(os.environ, SX_TEST_TILE_EXPORT="1", SX_OUT=str(work), SX_MESHFILE=str(model))
    if hybrid:
        env["SX_TEST_HYBRID"] = "1"
    startup = subprocess.STARTUPINFO()
    startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
    startup.wShowWindow = 7
    subprocess.run([str(exe), "--demo", "multimat", "--frames", "55", "--capture", str(work / "view.png")],
                   env=env, cwd=work, startupinfo=startup, timeout=180, check=True)
    cases = [("Body", 1001, 128, "red"), ("Body", 1002, 32, "neutral"), ("Trim", 1001, 64, "green")] if hybrid else [
        ("", 1001, 128, "red"), ("", 1002, 64, "green")]
    for material, udim, size, expected in cases:
        suffix = "_" + material if material else ""
        for prefix, channel in (("sep","BaseColor"), ("ue","BaseColor"), ("ue","ORM"), ("Modele","Packed")):
            path = work / f"{prefix}{suffix}_{channel}.{udim}.png"
            with Image.open(path) as img:
                assert img.size == (size, size), (path, img.size)
        packed = work / f"Modele{suffix}_Packed.{udim}.png"
        assert packed.read_bytes()[24:26] == bytes((16, 6)), (packed, "expected 16-bit RGBA PNG")
        with Image.open(packed) as img:
            center = img.convert("RGBA").getpixel((size // 4, size // 2))
        if expected == "red":
            assert center[0] > 240 and center[1] < 16, (packed, center)
        elif expected == "green":
            assert max(abs(x-y) for x, y in zip(center, (0, 255, 128 if hybrid else 64, 191))) <= 2, (packed, center)
        else:
            assert abs(center[0] - center[1]) < 100, (packed, center)
    if hybrid:
        log = (work / "shaderixel.log").read_text(encoding="utf-8", errors="replace")
        assert "[setchannels] target filter 1, undo 1, persisted 1" in log, (work,log[-2000:])
        assert "[setchannels] global pixels 1, fill visibility 1" in log, (work,log[-2000:])
        for prefix in ("Set","sep"):
            assert not (work / f"{prefix}_Trim_Roughness.1001.png").exists(), work
            assert (work / f"{prefix}_Body_Roughness.1001.png").is_file(), work
            assert not (work / f"{prefix}_Body_User2.1001.png").exists(), work
            with Image.open(work / f"{prefix}_Trim_User2.1001.png") as private:
                assert abs(private.getpixel((16,32))[0] - 153) <= 2, private.getpixel((16,32))
        with Image.open(work / "Atlas_Packed.png") as atlas:
            assert atlas.size == (256,256), atlas.size
            for x,y,filename in [(32,64,"Modele_Body_Packed.1001.png"),(136,16,"Modele_Body_Packed.1002.png"),
                                 (16,160,"Modele_Trim_Packed.1001.png")]:
                with Image.open(work / filename) as tile:
                    assert atlas.getpixel((x,y)) == tile.getpixel((tile.width//4,tile.height//2)), (x,y,filename)
        with Image.open(work / "ue_Trim_ORM.1001.png") as packed:
            assert abs(packed.getpixel((16,32))[1]-128) <= 1, packed.getpixel((16,32))
        obj = (work / "udim_back.obj").read_text(encoding="utf-8")
        assert "usemtl Body\n" in obj and "usemtl Trim\n" in obj and any(
            float(line.split()[1]) > 1 for line in obj.splitlines() if line.startswith("vt ")), work
    print(f"PASS {exe.name}: {len(cases)} native texture sets, isolated layer, custom RGBA16, legacy exports ({work})")


def check_4k(exe):
    validation = ROOT / "build" / "Paint3DExportValidation"
    validation.mkdir(exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix=exe.stem + "-native4k-", dir=validation))
    model = work / "four-native.obj"
    lines = []
    for i in range(4):
        n = i * 3 + 1
        lines.extend([f"v {i} 0 0", f"v {i+1} 0 0", f"v {i} 1 0", f"vt {i+.01} .01",
                      f"vt {i+.99} .01", f"vt {i+.01} .99", f"f {n}/{n} {n+1}/{n+1} {n+2}/{n+2}"])
    model.write_text("\n".join(lines), encoding="utf-8")
    env = dict(os.environ, SX_TEST_4K_EXPORT="1", SX_OUT=str(work), SX_MESHFILE=str(model))
    startup = subprocess.STARTUPINFO()
    startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
    startup.wShowWindow = 7
    started = time.perf_counter()
    subprocess.run([str(exe), "--demo", "multimat", "--frames", "70", "--capture", str(work / "view.png")],
                   env=env, cwd=work, startupinfo=startup, timeout=360, check=True)
    elapsed = time.perf_counter() - started
    for i, color in enumerate(((255,0,0),(0,255,0),(0,0,255),(255,255,0))):
        path = work / f"Native_BaseColor.{1001+i}.png"
        with Image.open(path) as img:
            assert img.size == (4096,4096), (path,img.size)
            assert img.getpixel((101,173))[:3] == (255,255,255), path
            for x,y in [(100,173),(102,173),(101,172),(101,174),(2048,2048)]:
                assert img.getpixel((x,y))[:3] == color, (path,x,y,img.getpixel((x,y)))
    for line in (work / "shaderixel.log").read_text(encoding="utf-8", errors="replace").splitlines():
        if "[native4k]" in line:
            print(line)
    print(f"PASS {exe.name}: 4 x 4096 x 4096, distinct colors, single-pixel marks, wall time {elapsed:.2f} s ({work})")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--backend", choices=("d3d12", "vulkan", "both"), default="both")
    parser.add_argument("--build", type=Path, default=ROOT / "build" / "Release")
    parser.add_argument("--case", choices=("plain", "combined", "both", "native4k"), default="both")
    args = parser.parse_args()
    for backend in (("d3d12", "vulkan") if args.backend == "both" else (args.backend,)):
        exe = (args.build / ("ShaderixelVk.exe" if backend == "vulkan" else "Shaderixel.exe")).resolve()
        if args.case == "native4k":
            check_4k(exe)
        else:
            for hybrid in ([False, True] if args.case == "both" else [args.case == "combined"]):
                check(exe, hybrid)
