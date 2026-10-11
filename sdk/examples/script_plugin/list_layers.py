"""Script plugin example: the layers of the active document (the last line goes to the status bar)."""
from shaderixel_client import call

info = call("shaderixel.document_info")
names = [l.get("name", "?") for l in info.get("layers", [])]
print("Document %s : %d x %d" % (info.get("name", "?"), info.get("width", 0), info.get("height", 0)))
for n in names:
    print(" - " + n)
print("%d calque(s) : %s" % (len(names), ", ".join(names[:6]) + (" ..." if len(names) > 6 else "")))
