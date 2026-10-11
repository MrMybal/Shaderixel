"""Script plugin example: a new pixel layer filled with a color given on the command line (r g b, 0..1)."""
import sys

from shaderixel_client import call

rgb = [float(x) for x in sys.argv[1:4]] if len(sys.argv) >= 4 else [0.9, 0.2, 0.2]
layer = call("shaderixel.layer_add", {"kind": "pixel", "name": "Calque coloré (script)", "opacity": 0.5})
call("shaderixel.fill", {"color": rgb + [1.0], "layer": layer.get("id", 0)})
print("Calque coloré ajouté (%s)" % ", ".join("%.2f" % c for c in rgb))
