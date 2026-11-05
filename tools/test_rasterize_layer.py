"""Check native layer baking, paintability, undo/redo and save/reopen.

Uses minimized CLI demo fixtures and GPU rendering. Never uses OS input devices.
Run: python tools/test_rasterize_layer.py --backend both
"""
import argparse
import json
import os
from pathlib import Path
import re
import subprocess
from PIL import Image, ImageChops

ROOT = Path(__file__).resolve().parents[1]
CASES = ("fill", "shape", "text", "smart", "pixel", "custom_blend", "clipped",
         "clipped_base", "disabled_mask", "hidden", "tiled", "many_filters",
         "material", "macro", "invalid", "locked", "plain", "group", "adjustment")
REOPEN = {"fill", "shape", "text", "smart", "pixel", "material"}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--backend", choices=("d3d12", "vulkan", "both"), default="both")
    parser.add_argument("--case", choices=CASES)
    args = parser.parse_args()
    output = ROOT / "build/RasterizeValidation"
    output.mkdir(parents=True, exist_ok=True)
    startup = subprocess.STARTUPINFO()
    startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
    startup.wShowWindow = 7  # SW_SHOWMINNOACTIVE
    results = []
    backends = ("d3d12", "vulkan") if args.backend == "both" else (args.backend,)
    for backend in backends:
        exe = ROOT / "build/Release" / ("Shaderixel.exe" if backend == "d3d12" else "ShaderixelVk.exe")
        cases = [(c, False) for c in ((args.case,) if args.case else CASES)]
        if not args.case:
            cases += [("fill", True), ("smart", True), ("tiled", True)]
        for name, float32 in cases:
            work = output / (backend + "-" + name + ("-float32" if float32 else ""))
            work.mkdir(exist_ok=True)
            environment = dict(os.environ, SX_RASTERIZE_CASE=name, SX_LANG="en")
            environment.pop("SX_RASTERIZE_FLOAT32", None)
            if float32:
                environment["SX_RASTERIZE_FLOAT32"] = "1"
            command = [str(exe), "--demo", "rasterize", "--frames", "300",
                       "--capture", str(work / "app.png"), "--dump-image", str(work / "image.png"),
                       "--save", str(work / "result.sxl")]
            run = subprocess.run(command, cwd=work, env=environment, startupinfo=startup,
                                 capture_output=True, timeout=180)
            log = (work / "shaderixel.log").read_text(encoding="utf-8", errors="replace")
            checks = re.findall(r"\[rasterize\] PASS (.+)", log)
            assert run.returncode == 0 and "[rasterize] COMPLETE 0 failures" in log, (backend, name, log[-8000:])
            assert "[rasterize] FAIL" not in log and "[ui-error]" not in log, (backend, name, log[-8000:])
            assert (work / "image.png").is_file() and (work / "result.sxl").is_file()
            record = {"backend": backend, "case": name, "float32": float32, "checks": checks}
            if name in REOPEN:
                reopened = work / "reopened"
                reopened.mkdir(exist_ok=True)
                run = subprocess.run([str(exe), str(work / "result.sxl"), "--frames", "80",
                                      "--capture", str(reopened / "app.png"),
                                      "--dump-image", str(reopened / "image.png")],
                                     cwd=reopened, env=environment, startupinfo=startup,
                                     capture_output=True, timeout=180)
                assert run.returncode == 0 and (reopened / "image.png").exists()
                a = Image.open(work / "image.png").convert("RGBA")
                b = Image.open(reopened / "image.png").convert("RGBA")
                assert a.size == b.size
                assert max(high for low, high in ImageChops.difference(a, b).getextrema()) <= 1, (backend, name, "reopen differs")
                record["reopen"] = True
            results.append(record)
            print("PASS", backend, name, "float32" if float32 else "half16",
                  len(checks), "native checks", "and save/reopen" if record.get("reopen") else "", flush=True)
    (output / ("results-" + args.case + ".json" if args.case else "results.json")).write_text(json.dumps(results, indent=2) + "\n", encoding="utf-8")
    print("PASS", len(results), "GPU scenarios", sum(len(r["checks"]) for r in results), "checks", flush=True)


if __name__ == "__main__":
    main()
