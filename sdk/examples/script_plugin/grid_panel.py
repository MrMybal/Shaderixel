"""Script plugin window example: "Grille" draws a grid on a new layer of the active document."""
from shaderixel_client import call, ShaderixelError
from shaderixel_panel import Panel

panel = Panel()
state = {"spacing": 64, "width": 2.0, "color": [0.2, 0.8, 1.0, 1.0], "diagonal": False, "mode": 0}
last = {"text": "Réglez puis « Dessiner la grille »."}


def ui():
    return [
        panel.note("Une grille sur un nouveau calque (pinceau par le pont de Shaderixel)."),
        panel.slider("spacing", "Espacement", state["spacing"], 8, 512, integer=True, tooltip="Pixels entre deux lignes"),
        panel.slider("width", "Épaisseur", state["width"], 1, 24, format="%.0f px"),
        panel.color("color", "Couleur", state["color"]),
        panel.combo("mode", "Lignes", ["Horizontales et verticales", "Horizontales", "Verticales"], state["mode"]),
        panel.checkbox("diagonal", "Diagonales en plus", state["diagonal"]),
        panel.separator(),
        panel.button("draw", "Dessiner la grille"),
        panel.same_line(),
        panel.button("undo", "Annuler"),
        panel.note(last["text"]),
    ]


@panel.on_change
def changed(id, value):
    state[id] = value


@panel.on_click
def clicked(id):
    try:
        if id == "undo":
            call("shaderixel.undo")
            last["text"] = "Annulé."
            return
        if id != "draw":
            return
        info = call("shaderixel.document_info")
        w, h = info.get("width", 0), info.get("height", 0)
        lid = 0
        step, size = max(4, int(state["spacing"])), float(state["width"])
        lines = []
        if state["mode"] in (0, 2):
            lines += [[[x, 0], [x, h]] for x in range(0, w + 1, step)]
        if state["mode"] in (0, 1):
            lines += [[[0, y], [w, y]] for y in range(0, h + 1, step)]
        if state["diagonal"]:
            for k in range(-h, w + 1, step):
                lines.append([[k, 0], [k + h, h]])
        call("shaderixel.history_group", {"action": "begin"})  # the layer and its lines: one undo step
        try:
            lid = call("shaderixel.layer_add", {"kind": "pixel", "name": "Grille"}).get("id", 0)
            for pts in lines:
                call("shaderixel.paint_stroke", {"points": pts, "layer": lid, "size": size, "hardness": 1.0, "color": state["color"]})
        finally:
            call("shaderixel.history_group", {"action": "end"})
        last["text"] = "%d lignes sur %d x %d px." % (len(lines), w, h)
        panel.status("Grille : " + last["text"])
    except ShaderixelError as e:
        last["text"] = "Erreur : %s" % e


panel.run(ui)
