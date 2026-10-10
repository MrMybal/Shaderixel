"""End-to-end test of the Shaderixel Link plugin, headless (no window, nothing to click).

    python tools/test_unreal_link.py 5.5          # one engine (the plugin built by build_unreal_plugin.py)
    python tools/test_unreal_link.py              # every built engine

For each engine: a blank project (build/ue/test/<ver>) with the plugin, then Unreal's Python
(commandlet, -nullrhi) duplicates the engine sphere and runs "Shaderixel.Paint" twice:
  1. the slot has no texture: Shaderixel (started minimized with --demo linksend: one stroke, then
     "Envoyer vers Unreal") returns new BaseColor / Normal / ORM, imported with a material instance;
  2. the slot now has textures: they are sent, painted again and reimported in place.
A texture session ("Shaderixel.EditTexture") checks the image round trip; a Paint 3D project (.sx3d)
is imported as a model (Shaderixel.Import), and its OBJ imported normally picks up the .sxmat.json next to it. Report: build/ue/test/<ver>/report.json.
"""
import json
import os
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ENGINES = r'C:\Program Files\Epic Games'
SHADERIXEL = os.path.join(ROOT, 'build', 'Release', 'Shaderixel.exe')

UE_SCRIPT = r'''
import json, os, time
import unreal

report = {"steps": []}
LINK = os.path.join(os.environ["LOCALAPPDATA"], "Shaderixel", "Link", "sessions")
eal = unreal.EditorAssetLibrary

def cmd(c):
    unreal.SystemLibrary.execute_console_command(None, c)

def wait_result(prefix, since, timeout=240):
    end = time.time() + timeout
    while time.time() < end:
        if os.path.isdir(LINK):
            for d in os.listdir(LINK):
                r = os.path.join(LINK, d, "result.json")
                if d.startswith(prefix) and os.path.exists(r) and os.path.getmtime(r) > since:
                    time.sleep(1.0)
                    return r
        time.sleep(0.5)
    return None

def step(name, ok, info=""):
    report["steps"].append({"name": name, "ok": bool(ok), "info": info})
    unreal.log("[sxtest] %s: %s %s" % (name, "OK" if ok else "FAIL", info))

if not eal.does_asset_exist("/Game/Test/SM_Sphere"):
    eal.duplicate_asset("/Engine/BasicShapes/Sphere", "/Game/Test/SM_Sphere")
mesh = eal.load_asset("/Game/Test/SM_Sphere")
step("mesh", mesh is not None)

for label in ("new_textures", "in_place"):
    t0 = time.time()
    cmd("Shaderixel.Paint /Game/Test/SM_Sphere.SM_Sphere 0")
    r = wait_result("SM_Sphere_", t0)
    if not r:
        step(label, False, "no result.json from Shaderixel")
        continue
    cmd("Shaderixel.Poll")
    mat = mesh.get_material(0)
    path = mat.get_path_name() if mat else ""
    texs = [p for p in eal.list_assets("/Game/Test/Shaderixel", recursive=False) if "/T_" in p]
    with open(os.path.join(os.path.dirname(r), "session.json")) as f:
        sent = sorted(t["role"] for t in json.load(f)["textures"])
    want = ["BaseColor", "Normal", "ORM"] if label == "in_place" else []
    step(label, path.endswith("MI_SM_Sphere") and len(texs) >= 3 and sent == want,
         "material=%s textures=%d sent=%s" % (path, len(texs), ",".join(sent)))

tex_path = "/Game/Test/Shaderixel/T_SM_Sphere_BC"
if eal.does_asset_exist(tex_path):
    t0 = time.time()
    cmd("Shaderixel.EditTexture %s.T_SM_Sphere_BC" % tex_path)
    r = wait_result("T_SM_Sphere_BC_", t0)
    if r:
        cmd("Shaderixel.Poll")
    step("texture", r is not None)

# Model + material exported by Shaderixel: the .sx3d project converted by Shaderixel, then imported.
SX3D = os.environ.get("SXTEST_SX3D", "")
if SX3D and os.path.exists(SX3D):
    cmd('Shaderixel.Import "%s" /Game/SxImport' % SX3D)
    names = ["SM_SxCube", "MI_SxCube", "T_SxCube_BaseColor", "T_SxCube_Normal", "T_SxCube_ORM"]
    have = [n for n in names if eal.does_asset_exist("/Game/SxImport/" + n)]
    sm = eal.load_asset("/Game/SxImport/SM_SxCube") if "SM_SxCube" in have else None
    mat = sm.get_material(0) if sm else None
    ext = sm.get_bounds().box_extent if sm else None
    step("import_sx3d", len(have) == len(names) and mat is not None and mat.get_name() == "MI_SxCube",
         "assets=%s material=%s extent=%s" % (",".join(have), mat.get_name() if mat else "-", ext))
    orm = eal.load_asset("/Game/SxImport/T_SxCube_ORM")
    step("texture_settings", orm is not None and not orm.get_editor_property("srgb"), "")
    # The exported OBJ imported the usual way: its .sxmat.json (same name, same folder) gives the material.
    exports = os.path.join(os.environ["LOCALAPPDATA"], "Shaderixel", "Link", "exports")
    dirs = sorted((d for d in os.listdir(exports) if d.startswith("SxCube_")), key=lambda d: os.path.getmtime(os.path.join(exports, d))) if os.path.isdir(exports) else []
    if os.environ.get("SXTEST_SIDECAR") == "0":
        # UE 5.7: any OBJ import through Interchange asserts in a -nullrhi commandlet (Slate), plugin or not.
        step("sidecar", True, "skipped on this engine (Interchange needs Slate in commandlets)")
    elif dirs:
        task = unreal.AssetImportTask()
        task.filename = os.path.join(exports, dirs[-1], "SxCube.obj")
        task.destination_path = "/Game/SideCar"
        task.automated = True
        task.replace_existing = True
        task.save = False
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        meshes = [eal.load_asset(p) for p in task.get_editor_property("imported_object_paths")]
        meshes = [m for m in meshes if isinstance(m, unreal.StaticMesh)]
        mm = meshes[0].get_material(0) if meshes else None
        step("sidecar", mm is not None and mm.get_name() == "MI_SxCube", "imported=%d material=%s" % (len(meshes), mm.get_name() if mm else "-"))
    else:
        step("sidecar", False, "no export folder")

eal.save_directory("/Game", only_if_is_dirty=True, recursive=True)
with open(os.environ["SXTEST_REPORT"], "w") as f:
    json.dump(report, f, indent=1)
'''


def test(ver):
    plugin = os.path.join(ROOT, 'build', 'ue', ver, 'ShaderixelLink')
    if not os.path.isdir(plugin):
        return 'plugin non compilé (build_unreal_plugin.py %s)' % ver
    proj_dir = os.path.join(ROOT, 'build', 'ue', 'test', ver)
    shutil.rmtree(proj_dir, ignore_errors=True)
    os.makedirs(os.path.join(proj_dir, 'Plugins'))
    shutil.copytree(plugin, os.path.join(proj_dir, 'Plugins', 'ShaderixelLink'), ignore=shutil.ignore_patterns('Intermediate'))
    uproject = os.path.join(proj_dir, 'LinkTest.uproject')
    json.dump({'FileVersion': 3, 'EngineAssociation': ver, 'Category': '', 'Description': 'Shaderixel Link test',
               'Plugins': [{'Name': 'PythonScriptPlugin', 'Enabled': True}, {'Name': 'EditorScriptingUtilities', 'Enabled': True},
                           {'Name': 'ShaderixelLink', 'Enabled': True}]}, open(uproject, 'w'), indent=1)
    script = os.path.join(proj_dir, 'sxtest.py')
    open(script, 'w').write(UE_SCRIPT)
    report = os.path.join(proj_dir, 'report.json')
    # Sessions of earlier runs for these assets start from scratch.
    sessions = os.path.join(os.environ['LOCALAPPDATA'], 'Shaderixel', 'Link', 'sessions')
    if os.path.isdir(sessions):
        for d in os.listdir(sessions):
            if d.startswith('SM_Sphere_') or d.startswith('T_SM_Sphere_BC_'):
                shutil.rmtree(os.path.join(sessions, d), ignore_errors=True)
    engine = os.path.join(ENGINES, 'UE_' + ver, 'Engine', 'Binaries', 'Win64')
    exe = os.path.join(engine, 'UnrealEditor-Cmd.exe')
    if not os.path.exists(exe):
        exe = os.path.join(engine, 'UE4Editor-Cmd.exe')
    env = {k: v for k, v in os.environ.items() if k.lower() != 'nodefaultcurrentdirectoryinexepath'}
    env['SXTEST_REPORT'] = report
    env['SHADERIXEL_LINK_EXE'] = SHADERIXEL
    env['SHADERIXEL_LINK_TEST_ARGS'] = '--demo linksend --frames 90 --capture "%s"' % os.path.join(proj_dir, 'shaderixel.png')
    # A Paint 3D project for the model import (cube + smart material), saved by Shaderixel.
    sx3d = os.path.join(proj_dir, 'SxCube.sx3d')
    senv = dict(env)
    senv['SX_SMART'] = '0'
    senv.pop('SX_MESHFILE', None)
    si = subprocess.STARTUPINFO()
    si.dwFlags |= subprocess.STARTF_USESHOWWINDOW
    si.wShowWindow = 7  # SW_SHOWMINNOACTIVE: never takes the focus
    subprocess.run([SHADERIXEL, '--demo', 'smartmat', '--frames', '120', '--save', sx3d], env=senv, cwd=proj_dir, timeout=600, startupinfo=si)
    env['SXTEST_SX3D'] = sx3d
    env['SXTEST_SIDECAR'] = '0' if ver == '5.7' else '1'
    log = os.path.join(proj_dir, 'editor.log')
    with open(log, 'w', encoding='utf-8', errors='replace') as f:
        subprocess.run([exe, uproject, '-run=pythonscript', '-script=' + script, '-unattended', '-nosplash', '-nullrhi', '-nopause', '-stdout',
                        '-FullStdOutLogOutput'], stdout=f, stderr=subprocess.STDOUT, env=env, timeout=3600)
    if not os.path.exists(report):
        return 'pas de rapport (voir %s)' % log
    steps = json.load(open(report))['steps']
    bad = [s for s in steps if not s['ok']]
    return 'OK (%d étapes)' % len(steps) if not bad else 'ÉCHEC : ' + '; '.join('%s %s' % (s['name'], s['info']) for s in bad)


def main():
    vers = sys.argv[1:] or sorted((d for d in os.listdir(os.path.join(ROOT, 'build', 'ue')) if d[0].isdigit() and os.path.isdir(os.path.join(ROOT, 'build', 'ue', d))),
                                  key=lambda v: tuple(map(int, v.split('.'))))
    res = []
    for v in vers:
        print('Test Unreal %s...' % v, flush=True)
        res.append((v, test(v)))
        print('  ' + res[-1][1], flush=True)
    print()
    for v, r in res:
        print('UE %-5s %s' % (v, r))


if __name__ == '__main__':
    main()
