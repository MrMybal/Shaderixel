"""Public signatures for CyDoc's neutral API adapter; no implementation export."""
import ast
import json
from pathlib import Path
import re
ROOT=Path(__file__).resolve().parents[1]
WORK=ROOT/'.CyDocBuild'

# Descriptions are paired explicitly; signatures always come from the shipped SDK.
DESCRIPTIONS={
 'unload':('Optional cleanup callback called before the plugin DLL is unloaded.','Rappel facultatif de nettoyage appelé avant le déchargement de la DLL du plugin.'),
 'log':('Write a diagnostic message to the application log.','Écrire un message de diagnostic dans le journal de l’application.'),
 'status':('Show a message in the application status bar.','Afficher un message dans la barre d’état de l’application.'),
 'add_command':('Register a command below the Plugins menu. The callback runs on the main thread.','Enregistrer une commande dans le menu Plugins. Le rappel s’exécute sur le thread principal.'),
 'add_panel':('Register a dockable panel. Draw its contents with SxUi inside its callback.','Enregistrer un panneau ancrable. Dessinez son contenu avec SxUi dans son rappel.'),
 'add_tool':('Register a canvas tool and its input-event callback.','Enregistrer un outil du canevas et son rappel d’événements de saisie.'),
 'add_operation':('Register an operation with a JSON input schema. Its writes flag declares document changes.','Enregistrer une opération avec un schéma d’entrée JSON. Son indicateur writes déclare les modifications du document.'),
 'call':('Request an editor operation. The result callback can run immediately or a few frames later.','Demander une opération à l’éditeur. Le rappel de résultat peut arriver immédiatement ou quelques images plus tard.'),
 'operations_json':('Return the operation catalog with identifiers, descriptions and JSON input schemas.','Renvoyer le catalogue des opérations avec identifiants, descriptions et schémas d’entrée JSON.'),
 'document_size':('Read the active document dimensions. Returns zero when no document is active.','Lire les dimensions du document actif. Renvoie zéro si aucun document n’est actif.'),
 'workspace_dir':('Return the plugin workspace directory for temporary exchange files.','Renvoyer le dossier de travail du plugin pour les fichiers d’échange temporaires.'),
 'open_panel':('Show or hide a registered panel by its title.','Afficher ou masquer un panneau enregistré à partir de son titre.'),
 'select_tool':('Select a registered tool by name.','Sélectionner un outil enregistré à partir de son nom.'),
 'draw_line':('Draw an overlay line during SX_TOOL_DRAW, in document pixels.','Tracer une ligne d’aperçu pendant SX_TOOL_DRAW, en pixels du document.'),
 'draw_circle':('Draw an overlay circle during SX_TOOL_DRAW, in document pixels.','Tracer un cercle d’aperçu pendant SX_TOOL_DRAW, en pixels du document.'),
 'draw_rect':('Draw an overlay rectangle during SX_TOOL_DRAW, in document pixels.','Tracer un rectangle d’aperçu pendant SX_TOOL_DRAW, en pixels du document.'),
 'draw_text':('Draw overlay text during SX_TOOL_DRAW, in document pixels.','Dessiner du texte d’aperçu pendant SX_TOOL_DRAW, en pixels du document.'),
 'imgui_context':('Return the optional host ImGui context. Direct ImGui use requires a matching ImGui version.','Renvoyer le contexte ImGui optionnel de l’hôte. Son utilisation directe exige une version ImGui compatible.'),
 'active_layer':('Return the active layer identifier, or zero when no layer is active. Requires API 2.','Renvoyer l’identifiant du calque actif, ou zéro si aucun calque n’est actif. Exige l’API 2.'),
 'pixels_read':('Request a pixel-layer rectangle. Layer zero selects the active paint target; nonpositive width or height selects the whole layer. The callback receives pixels on a later frame. Requires API 2.','Demander un rectangle d’un calque de pixels. Le calque zéro désigne la cible de peinture active ; une largeur ou hauteur non positive désigne le calque entier. Le rappel reçoit les pixels à une image ultérieure. Exige l’API 2.'),
 'pixels_write':('Write a pixel-layer rectangle as one named undo step. Returns one on success. Requires API 2.','Écrire un rectangle d’un calque de pixels en une étape d’annulation nommée. Renvoie un en cas de succès. Exige l’API 2.'),
 'get_color':('Read the foreground (0) or background (1) color as straight sRGB RGBA. Requires API 3.','Lire la couleur de premier plan (0) ou d’arrière-plan (1), en RVBA sRGB non prémultiplié. Exige l’API 3.'),
 'set_color':('Set the foreground (0) or background (1) color using straight sRGB RGBA. Requires API 3.','Définir la couleur de premier plan (0) ou d’arrière-plan (1), en RVBA sRGB non prémultiplié. Exige l’API 3.'),
 'setting_get':('Read a persistent plugin setting, using the supplied default when absent. Requires API 3.','Lire un réglage persistant du plugin, avec la valeur par défaut fournie s’il est absent. Exige l’API 3.'),
 'setting_set':('Store a plugin setting. A null or empty value removes the key. Requires API 3.','Enregistrer un réglage du plugin. Une valeur nulle ou vide supprime la clé. Exige l’API 3.'),
 'on_event':('Register a callback for document activation, edits and saves. Requires API 3.','Enregistrer un rappel pour l’activation, les modifications et l’enregistrement des documents. Exige l’API 3.'),
 'text':('Display a text label.','Afficher un texte.'),
 'text_disabled':('Display visually muted text.','Afficher un texte visuellement atténué.'),
 'text_wrapped':('Display text wrapped to the available width.','Afficher du texte avec retour à la ligne selon la largeur disponible.'),
 'button':('Display a button; return one when pressed.','Afficher un bouton ; renvoyer un lorsqu’il est activé.'),
 'checkbox':('Edit a Boolean value; return one when changed.','Modifier une valeur booléenne ; renvoyer un lorsqu’elle change.'),
 'slider_float':('Edit a floating-point value within the supplied bounds. Return one when changed.','Modifier une valeur réelle dans les limites fournies. Renvoyer un lorsqu’elle change.'),
 'slider_int':('Edit an integer within the supplied bounds. Return one when changed.','Modifier un entier dans les limites fournies. Renvoyer un lorsqu’il change.'),
 'input_text':('Edit a UTF-8 text buffer without exceeding its supplied capacity.','Modifier un tampon de texte UTF-8 sans dépasser la capacité fournie.'),
 'color_edit':('Edit an RGBA color; return one when changed.','Modifier une couleur RVBA ; renvoyer un lorsqu’elle change.'),
 'combo':('Choose an item from a zero-separated list and edit its index.','Choisir un élément d’une liste séparée par des zéros et modifier son indice.'),
 'same_line':('Place the next widget on the same row.','Placer le prochain contrôle sur la même ligne.'),
 'separator':('Insert a separator, optionally with a title.','Insérer un séparateur, avec un titre facultatif.'),
 'tooltip':('Attach explanatory text to the previous widget when hovered.','Associer un texte explicatif au contrôle précédent lors du survol.'),
 'push_id':('Push an integer identifier to distinguish repeated controls. Pair with pop_id.','Empiler un identifiant entier pour distinguer les contrôles répétés. Associer à pop_id.'),
 'pop_id':('Pop the most recently pushed control identifier.','Retirer le dernier identifiant de contrôle empilé.'),
 'collapsing_header':('Display a collapsible heading and return its open state.','Afficher un titre repliable et renvoyer son état ouvert.'),
 'content_width':('Return the available content width in pixels.','Renvoyer la largeur disponible du contenu en pixels.'),
 'set_next_width':('Set the next control width. A negative width is measured from the right edge.','Définir la largeur du prochain contrôle. Une valeur négative se mesure depuis le bord droit.'),
 'color_button':('Display a straight sRGB swatch. Return one for a left click or two for a right click. Requires API 3.','Afficher un échantillon sRGB non prémultiplié. Renvoyer un pour un clic gauche ou deux pour un clic droit. Exige l’API 3.'),
}
PARAMETERS={
 'buffer':('Writable UTF-8 character buffer.','Tampon de caractères UTF-8 modifiable.'),
 'buffer_size':('Buffer capacity in bytes, including the string terminator.','Capacité du tampon en octets, terminaison de chaîne comprise.'),
 'current':('Pointer to the selected item index.','Pointeur vers l’indice de l’élément sélectionné.'),
 'description':('Human-readable description of the registered operation.','Description de l’opération enregistrée destinée aux utilisateurs.'),
 'help':('Help text displayed for the tool.','Texte d’aide affiché pour l’outil.'),
 'id':('Stable identifier used to address the operation or widget.','Identifiant stable permettant de désigner l’opération ou le contrôle.'),
 'input_schema_json':('JSON schema describing accepted operation arguments.','Schéma JSON décrivant les arguments acceptés par l’opération.'),
 'integer':('Use integer values instead of floating-point values.','Utiliser des valeurs entières au lieu de valeurs réelles.'),
 'items':('Sequence of visible choices.','Séquence des choix visibles.'),
 'items_zero_separated':('Choice labels separated by null characters, ending with a second null.','Libellés séparés par des caractères nuls et terminés par un second caractère nul.'),
 'menu_path':('Menu path under Plugins; slash separates submenus.','Chemin du menu sous Plugins ; la barre oblique sépare les sous-menus.'),
 'msg':('Protocol message dictionary to serialize as a JSON line.','Dictionnaire du message du protocole à sérialiser en une ligne JSON.'),
 'name':('Registered tool name.','Nom de l’outil enregistré.'),
 'open':('One shows the panel; zero hides it.','Un affiche le panneau ; zéro le masque.'),
 'parameters':('Dictionary of arguments matching the operation schema.','Dictionnaire d’arguments correspondant au schéma de l’opération.'),
 'radius':('Circle radius in document pixels.','Rayon du cercle en pixels du document.'),
 'size':('Color swatch size in pixels.','Taille de l’échantillon de couleur en pixels.'),
 'thickness':('Overlay line thickness in pixels.','Épaisseur du trait d’aperçu en pixels.'),
 'tooltip':('Optional explanatory text shown on hover.','Texte explicatif facultatif affiché au survol.'),
 'ui':('Function returning the current list of widget descriptions.','Fonction renvoyant la liste actuelle des descriptions de contrôles.'),
 'widgets':('List of dictionaries produced by the Panel widget helpers.','Liste de dictionnaires produits par les fonctions de contrôles de Panel.'),
 'workspace':('Optional exchange directory; uses SHADERIXEL_WORKSPACE when omitted.','Dossier d’échange facultatif ; utilise SHADERIXEL_WORKSPACE s’il est omis.'),
 'writes':('Nonzero declares that the operation changes the document.','Une valeur non nulle déclare que l’opération modifie le document.'),
 'x':('Horizontal position in document pixels.','Position horizontale en pixels du document.'),
 'y':('Vertical position in document pixels.','Position verticale en pixels du document.'),
 'x0':('First horizontal coordinate in document pixels.','Première coordonnée horizontale en pixels du document.'),
 'x1':('Second horizontal coordinate in document pixels.','Seconde coordonnée horizontale en pixels du document.'),
 'y0':('First vertical coordinate in document pixels.','Première coordonnée verticale en pixels du document.'),
 'y1':('Second vertical coordinate in document pixels.','Seconde coordonnée verticale en pixels du document.'),
 'user':('Opaque caller data returned to callbacks.','Données opaques de l’appelant transmises aux rappels.'),
 'done':('Callback that receives the result.','Rappel recevant le résultat.'),
 'fn':('Callback to register.','Rappel à enregistrer.'),
 'format':('Formatting string for a widget, or SxPixelFormat for pixels.','Chaîne de formatage d’un contrôle, ou SxPixelFormat pour les pixels.'),
 'pixels':('Pixel buffer in the selected format.','Tampon de pixels au format choisi.'),
 'rgba':('Four color components.','Quatre composantes de couleur.'),
 'color':('Packed overlay color 0xAABBGGRR.','Couleur d’aperçu compacte 0xAABBGGRR.'),
 'width':('Output width in pixels.','Largeur de sortie en pixels.'),
 'height':('Output height in pixels.','Hauteur de sortie en pixels.'),
 'which':('Zero selects foreground; one selects background.','Zéro désigne le premier plan ; un désigne l’arrière-plan.'),
 'layer':('Layer identifier; zero selects the active paint target where supported.','Identifiant du calque ; zéro désigne la cible de peinture active si accepté.'),
 'w':('Rectangle width in document pixels.','Largeur du rectangle en pixels du document.'),
 'h':('Rectangle height in document pixels.','Hauteur du rectangle en pixels du document.'),
 'min':('Smallest accepted value.','Plus petite valeur acceptée.'),
 'max':('Largest accepted value.','Plus grande valeur acceptée.'),
 'undo_label':('Name shown for this undo step.','Nom affiché pour cette étape d’annulation.'),
 'value':('Value to edit or store.','Valeur à modifier ou à enregistrer.'),
 'text':('UTF-8 text.','Texte UTF-8.'),
 'message':('UTF-8 message.','Message UTF-8.'),
 'label':('Visible control label.','Libellé visible du contrôle.'),
 'title':('Panel or section title.','Titre du panneau ou de la section.'),
 'key':('Plugin setting key.','Clé de réglage du plugin.'),
 'default_value':('Value used when the setting is absent.','Valeur utilisée si le réglage est absent.'),
 'params_json':('Operation arguments encoded as JSON.','Arguments de l’opération encodés en JSON.'),
 'operation':('Registered operation identifier.','Identifiant de l’opération enregistrée.'),
}


def prepare_public_api():
    header=(ROOT/'sdk/include/shaderixel_plugin.h').read_text(encoding='utf-8')
    values={'en':[],'fr':[]}
    for typ in ('SxHost','SxUi','SxPlugin'):
        block=re.search(r'typedef struct '+typ+r' \{(.*?)\} '+typ,header,re.S)[1]
        for m in re.finditer(r'([^;\n]+?)\(\*(\w+)\)\(([^;]*?)\);',block):
            returns,name,params=m.groups();returns=returns.strip()
            if name not in DESCRIPTIONS:raise ValueError('Document the new SDK callback: '+name)
            signature=returns+' (*'+name+')('+params+');'
            for lang in values:
                fr=lang=='fr';description=DESCRIPTIONS[name][fr]
                if typ=='SxUi':description+=(' À utiliser dans le rappel de dessin d’un panneau.' if fr else ' Use inside a panel drawing callback.')
                parameters=[]
                for param in params.split(','):
                    param=param.strip()
                    if param=='void':continue
                    pm=re.search(r'(\w+)(\[[^]]*\])?$',param);pn=pm[1]
                    pt=param[:pm.start()].strip()+(pm[2] or '')
                    desc=PARAMETERS[pn][fr]
                    parameters.append({'name':pn,'type':pt,'description':desc})
                result=('Aucun résultat direct ; consultez la description du rappel.' if fr else 'No direct result; see the callback description.') if returns=='void' else ('Voir les conditions et la durée de validité décrites ci-dessus.' if fr else 'See the conditions and lifetime described above.')
                values[lang].append({'key':typ+'-'+name,'name':typ+'::'+name,'language':'c','kind':'function','signature':signature,'description':description,'parameters':parameters,'returnType':returns,'returns':result,'references':[{'key':'SxHost-call','name':'SxHost::call'}] if name=='operations_json' else []})
    panel_notes={
      'text':('Build a text widget description.','Construire la description d’un texte.'),
      'note':('Build a muted note widget description.','Construire la description d’une note atténuée.'),
      'separator':('Build a separator description.','Construire la description d’un séparateur.'),
      'same_line':('Place the next widget on the same row.','Placer le prochain contrôle sur la même ligne.'),
      'button':('Build a button with a stable event identifier.','Construire un bouton avec un identifiant d’événement stable.'),
      'slider':('Build a bounded slider; integer selects integer values.','Construire un curseur borné ; integer sélectionne des valeurs entières.'),
      'checkbox':('Build a checkbox.','Construire une case à cocher.'),
      'input':('Build an editable text field.','Construire un champ de texte modifiable.'),
      'color':('Build a color editor using straight sRGB RGBA values from zero to one.','Construire un éditeur de couleur en RVBA sRGB non prémultiplié, de zéro à un.'),
      'combo':('Build an item selector; value is its zero-based index.','Construire un sélecteur ; value est son indice à partir de zéro.'),
      'send':('Send one protocol message as a JSON line on standard output.','Envoyer un message du protocole sous forme d’une ligne JSON sur la sortie standard.'),
      'set_ui':('Replace the panel interface with a list of widget descriptions.','Remplacer l’interface du panneau par une liste de descriptions de contrôles.'),
      'status':('Send a status-bar message to Shaderixel.','Envoyer un message à la barre d’état de Shaderixel.'),
      'log':('Send a diagnostic message to Shaderixel.','Envoyer un message de diagnostic à Shaderixel.'),
      'on_click':('Register the click callback, which receives a widget identifier. Can be used as a decorator.','Enregistrer le rappel de clic qui reçoit l’identifiant du contrôle. Utilisable comme décorateur.'),
      'on_change':('Register the change callback, which receives a widget identifier and value. Can be used as a decorator.','Enregistrer le rappel de modification qui reçoit l’identifiant du contrôle et la valeur. Utilisable comme décorateur.'),
      'run':('Send the initial interface, handle input events and stop when the panel closes.','Envoyer l’interface initiale, traiter les événements reçus et s’arrêter à la fermeture du panneau.'),
    }
    for filename in ('shaderixel_client.py','shaderixel_panel.py'):
        tree=ast.parse((ROOT/'sdk/examples/script_plugin'/filename).read_text(encoding='utf-8'))
        functions=[n for n in tree.body if isinstance(n,ast.FunctionDef)] if filename=='shaderixel_client.py' else [n for c in tree.body if isinstance(c,ast.ClassDef) and c.name=='Panel' for n in c.body if isinstance(n,ast.FunctionDef)]
        for fn in functions:
            if fn.name.startswith('_'):continue
            name=('shaderixel_client.' if filename=='shaderixel_client.py' else 'Panel.')+fn.name
            notes={'describe':('Return the operation catalog and input schemas.','Renvoyer le catalogue des opérations et leurs schémas d’entrée.'),'call':('Invoke an operation on Shaderixel’s main thread and return the operation’s JSON result value. Raises ShaderixelError if the bridge is unavailable or the operation fails.','Appeler une opération sur le thread principal de Shaderixel et renvoyer sa valeur de résultat JSON. Lève ShaderixelError si le pont est indisponible ou l’opération échoue.')} if filename=='shaderixel_client.py' else panel_notes
            if fn.name not in notes:raise ValueError('Document the public Python function: '+name)
            args=[a for a in fn.args.args if a.arg!='self']
            defaults=[None]*(len(fn.args.args)-len(fn.args.defaults))+fn.args.defaults
            default_map={a.arg:ast.unparse(d) for a,d in zip(fn.args.args,defaults) if d is not None}
            signature=name+'('+', '.join(a.arg+('='+default_map[a.arg] if a.arg in default_map else '') for a in args)+')'
            for lang in values:
                fr=lang=='fr'
                params=[{'name':a.arg,'type':'object','default':default_map.get(a.arg,''),'description':PARAMETERS[a.arg][fr]} for a in args]
                values[lang].append({'key':name.replace('.','-'),'name':name,'language':'python','kind':'function','signature':signature,'description':notes[fn.name][fr],'parameters':params})
    for lang in values:
        path=WORK/'api'/('native-'+lang+'.json');path.parent.mkdir(exist_ok=True)
        path.write_text(json.dumps({'symbols':values[lang]},ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    return len(values['en'])
