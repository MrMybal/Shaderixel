"""Builds the application icon and the embedded logo from shaderixel-logo.png.

Outputs (committed, rebuilt only when the logo changes):
  assets/shaderixel.ico   multi-size Windows icon (exe, window, taskbar)
  assets/logo.png         512 px logo embedded as a resource (welcome screen, About)
"""
import os
from PIL import Image

root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
src = Image.open(os.path.join(root, 'shaderixel-logo.png')).convert('RGBA')
# Crop to the visible content so the icon fills its square.
bbox = src.getchannel('A').point(lambda a: 255 if a > 8 else 0).getbbox()
x0, y0, x1, y1 = bbox
side = max(x1 - x0, y1 - y0)
cx, cy = (x0 + x1) // 2, (y0 + y1) // 2
sq = Image.new('RGBA', (side, side), (0, 0, 0, 0))
sq.paste(src.crop((cx - side // 2, cy - side // 2, cx - side // 2 + side, cy - side // 2 + side)), (0, 0))

out = os.path.join(root, 'assets')
os.makedirs(out, exist_ok=True)
sq.save(os.path.join(out, 'shaderixel.ico'), sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)])
sq.resize((512, 512), Image.LANCZOS).save(os.path.join(out, 'logo.png'), optimize=True)
print('ok', bbox)
