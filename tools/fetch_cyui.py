"""Fetch the exact CyUI revision locked by Shaderixel; preserve local edits."""
import json
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
LOCK = json.loads((ROOT / "cmake/dependencies.json").read_text())["cyui"]
DEST = ROOT / "external/cyui"

def git(*args):
    return subprocess.check_output(["git", "-c", "safe.directory=" + DEST.as_posix(),
                                   "-C", str(DEST), *args], text=True).strip()

def main():
    if not DEST.exists():
        subprocess.run(["git", "clone", "--no-checkout", LOCK["repository"], str(DEST)], check=True)
    else:
        if git("status", "--porcelain"):
            raise SystemExit("CyUI has local changes; preserve them before updating the dependency.")
        if git("rev-parse", "HEAD") == LOCK["revision"]:
            print("CyUI already at " + LOCK["revision"])
            return
    git("fetch", "origin", LOCK["revision"], "--depth=1")
    git("checkout", "--detach", LOCK["revision"])
    print("CyUI pinned to " + LOCK["revision"])

if __name__ == "__main__":
    main()
