"""Machine-specific paths of the development tools, read from tools/local_paths.json (not versioned).

Example of tools/local_paths.json:

    {
        "unreal_engines": ["C:/Program Files/Epic Games"],
        "cytexture_preset_source": "path/to/CyTextureMaterialPresetGenerator.cpp"
    }

Environment variables of the same names in upper case (UNREAL_ENGINES separated by ';') take precedence.
"""
import json
import os

_FILE = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'local_paths.json')


def _load():
    try:
        with open(_FILE, encoding='utf-8') as f:
            return json.load(f)
    except (OSError, ValueError):
        return {}


def get(key, default=None):
    env = os.environ.get(key.upper())
    if env:
        return env.split(';') if isinstance(default, list) else env
    return _load().get(key, default)
