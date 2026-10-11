"""Windows of script plugins (plugin.json "panels"): the program describes its window, Shaderixel draws it.

    from shaderixel_panel import Panel

    panel = Panel()
    state = {"size": 32}

    def ui():
        return [panel.slider("size", "Taille", state["size"], 4, 256, integer=True),
                panel.button("go", "Appliquer")]

    @panel.on_change
    def changed(id, value):
        state[id] = value          # the window keeps the value: no need to send the UI again

    @panel.on_click
    def clicked(id):
        if id == "go":
            panel.status("Appliqué : %d" % state["size"])

    panel.run(ui)                   # sends the UI, then waits for the user's actions

Widgets: text, note (gray text), separator, same_line, button, slider (float / integer), checkbox,
input (text), color ([r, g, b, a] sRGB 0..1), combo (items + index). Each takes an optional tooltip.
The program talks to Shaderixel's operations with shaderixel_client.call() (same folder).
Protocol (one JSON object per line): Shaderixel -> program on stdin: {"event": "open" | "click" | "change" | "close",
"id", "value"}; program -> Shaderixel on stdout: {"ui": [...]}, {"status": "..."}, {"log": "..."}.
"""
import json
import sys


class Panel:
    def __init__(self):
        self._click = None
        self._change = None

    # ---- widgets (dicts)
    @staticmethod
    def text(text, tooltip=None):
        return _w({"type": "text", "text": text}, tooltip)

    @staticmethod
    def note(text, tooltip=None):
        return _w({"type": "note", "text": text}, tooltip)

    @staticmethod
    def separator(label=""):
        return {"type": "separator", "label": label}

    @staticmethod
    def same_line():
        return {"type": "same_line"}

    @staticmethod
    def button(id, label, tooltip=None):
        return _w({"type": "button", "id": id, "label": label}, tooltip)

    @staticmethod
    def slider(id, label, value, min, max, integer=False, format="%.2f", tooltip=None):
        return _w({"type": "slider", "id": id, "label": label, "value": value, "min": min, "max": max, "int": integer, "format": format}, tooltip)

    @staticmethod
    def checkbox(id, label, value, tooltip=None):
        return _w({"type": "checkbox", "id": id, "label": label, "value": bool(value)}, tooltip)

    @staticmethod
    def input(id, label, value="", tooltip=None):
        return _w({"type": "input", "id": id, "label": label, "value": value}, tooltip)

    @staticmethod
    def color(id, label, value, tooltip=None):
        return _w({"type": "color", "id": id, "label": label, "value": list(value)}, tooltip)

    @staticmethod
    def combo(id, label, items, value=0, tooltip=None):
        return _w({"type": "combo", "id": id, "label": label, "items": list(items), "value": value}, tooltip)

    # ---- messages
    @staticmethod
    def send(msg):
        sys.stdout.write(json.dumps(msg) + "\n")
        sys.stdout.flush()

    def set_ui(self, widgets):
        self.send({"ui": widgets})

    def status(self, text):
        self.send({"status": text})

    def log(self, text):
        self.send({"log": text})

    # ---- events
    def on_click(self, fn):
        self._click = fn
        return fn

    def on_change(self, fn):
        self._change = fn
        return fn

    def run(self, ui):
        """ui: the widgets, or a function returning them (called again after each click)."""
        build = ui if callable(ui) else (lambda: ui)
        self.set_ui(build())
        for line in sys.stdin:
            try:
                ev = json.loads(line)
            except ValueError:
                continue
            kind = ev.get("event")
            if kind == "close":
                break
            try:
                if kind == "click" and self._click:
                    self._click(ev.get("id", ""))
                    self.set_ui(build())
                elif kind == "change" and self._change:
                    self._change(ev.get("id", ""), ev.get("value"))
            except Exception as e:  # shown in Shaderixel, the window keeps running
                self.status("Erreur du script : %s" % e)


def _w(d, tooltip):
    if tooltip:
        d["tooltip"] = tooltip
    return d
