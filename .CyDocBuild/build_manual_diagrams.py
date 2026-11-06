"""Author bilingual vector diagrams; rasterize with render_manual_diagrams.cjs."""
from pathlib import Path
import html
import json

ROOT = Path(__file__).resolve().parents[1]
MEDIA = ROOT / '.CyDocBuild/assets/manual'


def diagram(language, name, title, boxes, arrows, footer):
    def txt(x, y, text, size=22, color='#eef1fa', anchor='middle'):
        return f'<text x="{x}" y="{y}" text-anchor="{anchor}" fill="{color}" font-size="{size}" font-family="Arial,sans-serif">{html.escape(text)}</text>'
    svg = ['<svg xmlns="http://www.w3.org/2000/svg" width="1200" height="640" viewBox="0 0 1200 640">',
           '<defs><marker id="arrow" markerWidth="10" markerHeight="10" refX="8" refY="4" orient="auto"><path d="M0,0 L8,4 L0,8" fill="#bac5dc"/></marker><pattern id="tiles" width="36" height="36" patternUnits="userSpaceOnUse"><rect width="36" height="36" fill="#244d57"/><path d="M0 0 H36 V36 H0 Z" fill="none" stroke="#51818b" stroke-width="1"/></pattern></defs>',
           '<rect width="1200" height="640" rx="18" fill="#191e2a"/>', txt(600, 57, title, 32)]
    for x1, y1, x2, y2 in arrows:
        svg += [f'<path d="M{x1} {y1} L{x2} {y2}" stroke="#bac5dc" stroke-width="3" marker-end="url(#arrow)"/>']
    for x, y, w, h, lines, color in boxes:
        if name == 'nine_slice' and color == '#244d57':
            color = 'url(#tiles)'
        svg += [f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="12" fill="{color}" stroke="#8394b3" stroke-width="2"/>']
        for i, line in enumerate(lines):
            svg += [txt(x+w/2, y+h/2+(i-(len(lines)-1)/2)*31+8, line)]
    svg += [txt(600, 601, footer, 21, '#c3cee4'), '</svg>']
    (MEDIA / f'{name}_{language}.svg').write_text('\n'.join(svg), encoding='utf-8')


def main():
    MEDIA.mkdir(parents=True, exist_ok=True)
    for lang in ('en', 'fr'):
        fr = lang == 'fr'
        t = lambda en, french: french if fr else en
        diagram(lang,'node_types',t('Choose the scale of calculation','Choisir l’échelle du calcul'),[
            (50,140,300,110,[t('Typed Float / Color','Valeur Float / Color'),t('One value per node','Une valeur par nœud')],'#39485e'),
            (450,140,300,110,[t('Image / channel','Image / canal'),t('One value per pixel','Une valeur par pixel')],'#244d57'),
            (850,140,300,110,[t('Shader graph','Graphe de shader'),t('Pixel calculation','Calcul par pixel')],'#454178'),
            (50,380,300,110,[t('Parameter pin','Broche de paramètre'),t('Main image graph','Graphe image principal')],'#39485e'),
            (450,380,300,110,[t('Image effect','Effet image'),t('Sample + process','Lire + traiter')],'#244d57'),
            (850,380,300,110,[t('Exec event','Événement Exec'),t('Bake / AI / export','Bake / IA / export')],'#714d33')],
            [(200,250,200,380),(600,250,600,380),(850,195,750,195),(850,435,750,435)],
            t('Math angles: degrees in the image graph; radians in the shader graph.','Angles Math : degrés dans le graphe image ; radians dans le shader.'))
        diagram(lang,'delivery',t('Deliver pixels with the intended interpretation','Livrer les pixels avec la bonne interprétation'),[
            (50,140,300,110,[t('Editable native master','Original natif modifiable'),'.sxl / .sx3d'],'#39485e'),
            (450,140,300,110,[t('Rendered channels','Canaux calculés'),t('Color / numeric data','Couleur / données')],'#454178'),
            (850,140,300,110,[t('Size + packing','Taille + canaux groupés'),t('Alpha + normal Y','Alpha + normale Y')],'#454178'),
            (50,380,300,110,[t('Receiving application','Application destinataire'),t('Check the material','Vérifier le matériau')],'#244d57'),
            (450,380,300,110,[t('Reopen and inspect','Rouvrir et inspecter'),t('Edges / channels','Bords / canaux')],'#244d57'),
            (850,380,300,110,['PNG / TIFF / TGA',t('Delivered files','Fichiers livrés')],'#244d57')],
            [(350,195,450,195),(750,195,850,195),(1000,250,1000,380),(850,435,750,435),(450,435,350,435)],
            t('A flattened export does not carry the editable graph or every external asset.','Un export aplati ne transporte ni graphe modifiable ni toutes les ressources.'))
        diagram(lang,'recovery',t('Diagnose a blank result in order','Diagnostiquer un résultat vide dans l’ordre'),[
            (50,140,300,110,[t('View + channel','Vue + canal'),t('Final / normal view','Vue finale / normale')],'#39485e'),
            (450,140,300,110,[t('Layer + target','Calque + cible'),t('Lock / mask / opacity','Verrou / masque / opacité')],'#39485e'),
            (850,140,300,110,[t('Connections + output','Connexions + sortie'),t('Nonempty source','Source non vide')],'#454178'),
            (50,380,300,110,[t('Minimal reproduction','Reproduction minimale'),t('Version + backend + log','Version + moteur + journal')],'#244d57'),
            (450,380,300,110,[t('Shader / resource error','Erreur shader / ressource'),t('Fix before export','Corriger avant export')],'#714d33'),
            (850,380,300,110,[t('Live or retained?','Direct ou conservé ?'),t('Run / clear results','Lancer / effacer résultats')],'#454178')],
            [(350,195,450,195),(750,195,850,195),(1000,250,1000,380),(850,435,750,435),(450,435,350,435)],
            t('Save a native copy before destructive troubleshooting.','Enregistrez une copie native avant les essais destructifs.'))
        diagram(lang, 'brush_pipeline', t('From pen input to painted pixels','Du stylet aux pixels peints'), [
            (50,135,300,105,[t('Position + pressure','Position + pression'),t('Tilt / rotation / speed','Inclinaison / rotation / vitesse')],'#39485e'),
            (450,135,300,105,[t('Spacing / airbrush','Espacement / aérographe'),t('Dab size + opacity','Taille + opacité des touches')],'#454178'),
            (850,135,300,105,['Dynamics',t('Placement / variation','Placement / variation')],'#454178'),
            (850,365,300,110,['Tip',t('Color + coverage','Couleur + couverture')],'#244d57'),
            (450,365,300,110,[t('Flow + selection','Débit + sélection'),t('Global opacity','Opacité globale')],'#454178'),
            (50,365,300,110,[t('Image / mask / material','Image / masque / matériau'),t('Painted pixels','Pixels peints')],'#244d57')],
            [(350,188,450,188),(750,188,850,188),(1000,240,1000,365),(850,420,750,420),(450,420,350,420)],
            t('Each preset uses only the input channels read by its shader.','Chaque preset utilise uniquement les canaux lus par son shader.'))
        diagram(lang, 'layers', t('A layer keeps content and composition separate', 'Le calque sépare contenu et composition'), [
            (70,120,330,78,[t('Text / shape / pixels','Texte / forme / pixels')],'#454178'),
            (70,228,330,78,[t('Transform + filters','Transformation + filtres')],'#454178'),
            (70,336,330,78,[t('Rendered content','Contenu rendu')],'#454178'),
            (530,175,560,104,[t('Bake to Pixel Layer','Convertir en calque de pixels'),t('Independent, paintable pixels','Pixels indépendants et peignables')],'#244d57'),
            (530,350,560,122,[t('Mask · opacity · blend','Masque · opacité · fusion'),t('Remain editable after baking','Restent éditables après conversion')],'#39485e')],
            [(235,198,235,228),(235,306,235,336),(400,375,500,375),(500,375,500,226),(500,226,530,226),(810,279,810,350)],
            t('Undo restores the original layer. Merge and mesh baking are different commands.', 'Annuler restaure le calque original. Fusion et cuisson de maillage sont distinctes.'))
        diagram(lang, 'graphs', t('Two graphs, two scales of calculation','Deux graphes, deux échelles de calcul'), [
            (45,135,240,100,[t('Source image','Image source')],'#244d57'),(340,135,240,100,[t('Image effect','Effet image')],'#454178'),
            (635,135,240,100,[t('Blend / function','Fusion / fonction')],'#454178'),(930,135,220,100,[t('Output image','Image de sortie')],'#244d57'),
            (45,365,240,100,['UV / XY'],'#39485e'),(340,365,240,100,[t('Math + sampling','Maths + échantillonnage')],'#454178'),
            (635,365,240,100,[t('Color / alpha','Couleur / alpha')],'#454178'),(930,365,220,100,[t('One shader','Un shader')],'#244d57')],
            [(285,185,340,185),(580,185,635,185),(875,185,930,185),(285,415,340,415),(580,415,635,415),(875,415,930,415)],
            t('Top: main image graph. Bottom: per-pixel effect graph.', 'Haut : graphe principal d’images. Bas : graphe de calcul par pixel d’un effet.'))
        diagram(lang, 'pbr', t('Photo to estimated PBR maps','Photo vers cartes PBR estimées'), [
            (50,140,270,100,[t('Evenly lit photo','Photo bien éclairée')],'#39485e'),(465,140,270,100,[t('Cleanup + seams','Nettoyage + raccords')],'#454178'),
            (880,140,270,100,['PBR Photo to Maps'],'#454178'),
            (50,365,320,120,[t('Albedo','Albédo'),'sRGB'],'#244d57'),
            (440,365,320,120,[t('Height / normal / AO','Hauteur / normale / AO'),t('Non-sRGB data','Données hors sRGB')],'#244d57'),
            (830,365,320,120,[t('Roughness / metal / ORM','Rugosité / métal / ORM'),t('Non-sRGB data','Données hors sRGB')],'#244d57')],
            [(320,190,465,190),(735,190,880,190),(1015,240,1015,300),(1015,300,210,300),(210,300,210,365),(600,300,600,365),(990,300,990,365)],
            t('Metalness is manual. Photo shading is not measured surface relief.', 'La métallicité se choisit. L’ombrage photo n’est pas un relief mesuré.'))
        diagram(lang, 'execution', t('Image data does not trigger an action','Une image ne déclenche pas une action'), [
            (60,145,300,100,[t('Live image input','Image d’entrée directe')],'#244d57'),
            (450,145,300,100,[t('Processing graph','Graphe de traitement')],'#454178'),
            (840,145,300,100,[t('Preview','Aperçu')],'#244d57'),
            (60,380,300,100,[t('Event button','Bouton d’événement')],'#714d33'),
            (450,380,300,100,[t('Bake / AI / upscale','Bake / IA / agrandir')],'#454178'),
            (840,380,300,100,[t('Stored result / export','Résultat conservé / export')],'#244d57')],
            [(360,195,450,195),(750,195,840,195),(360,430,450,430),(750,430,840,430),(600,245,600,380)],
            t('Rerun to update a retained result. Clear Results returns to the live preview.', 'Réexécutez pour actualiser. Effacer les résultats rétablit l’aperçu direct.'))
        diagram(lang, 'uv_chain', t('UV mapping is data until the final sample','Les UV sont des données jusqu’à l’échantillonnage'), [
            (40,175,230,115,[t('Coordinates','Coordonnées'),'R=U · G=V'],'#39485e'),
            (335,175,230,115,[t('Slice / transform','Découpe / transformation')],'#454178'),
            (630,175,230,115,[t('Address + mask','Adressage + masque')],'#454178'),
            (925,175,230,115,[t('Sample image','Échantillonner')],'#244d57'),
            (630,400,230,100,[t('Coverage','Couverture'),'B'],'#39485e'),
            (925,400,230,100,[t('Visible skin','Skin visible'),'RGBA'],'#244d57')],
            [(270,232,335,232),(565,232,630,232),(860,232,925,232),(745,290,745,400),(1040,290,1040,400)],
            t('Use float32. Do not color-grade or export coordinate fields as ordinary artwork.', 'Utilisez du float32. Ne traitez pas un champ UV comme une image couleur.'))
        # Nine-slice geometry shows fixed corners and a repeated central region.
        title=t('Nine-slice: preserve corners, repeat the center','Nine-slice : préserver les coins, répéter le centre')
        boxes=[]
        for x0,y0,width,height in [(65,190,300,240),(515,150,620,350)]:
            corners=60; xs=[x0,x0+corners,x0+width-corners];ys=[y0,y0+corners,y0+height-corners]
            ws=[corners,width-2*corners,corners]; hs=[corners,height-2*corners,corners]
            for y in range(3):
                for x in range(3):
                    label= t('Tile','Répéter') if x==1 and y==1 else str(y*3+x+1)
                    color='#624d83' if x!=1 and y!=1 else '#244d57' if x==1 and y==1 else '#39485e'
                    boxes.append((xs[x],ys[y],ws[x]-4,hs[y]-4,[label],color))
        diagram(lang,'nine_slice',title,boxes,[(380,310,490,310)],
            t('Source margins define corners. Transparent center creates an eight-slice border.', 'Les marges source définissent les coins. Centre transparent = bordure eight-slice.'))
        diagram(lang,'matte',t('Inspect coverage before exporting the foreground','Examinez la couverture avant d’exporter le sujet'),[
            (45,150,310,100,[t('Opaque green source','Source opaque sur vert')],'#39485e'),
            (440,150,310,100,['Color Key + Despill'],'#454178'),
            (835,150,320,100,[t('Foreground → PNG','Foreground → PNG'),t('Alpha already applied','Alpha déjà appliqué')],'#244d57'),
            (440,370,310,115,['Coverage Mask',t('White keep · black remove','Blanc garde · noir retire')],'#39485e'),
            (835,370,320,115,['Checker Preview',t('Inspection only','Inspection seulement')],'#39485e')],
            [(355,200,440,200),(750,200,835,200),(595,250,595,370),(750,225,795,225),(795,225,795,425),(795,425,835,425)],
            t('Do not multiply the same matte into an already keyed alpha a second time.', 'Ne multipliez pas deux fois le même matte dans un alpha déjà détouré.'))
        diagram(lang,'upscale',t('Read the file dimensions, not the preview size','Vérifiez la taille du fichier, pas celle de l’aperçu'),[
            (45,155,300,100,[t('Input image','Image d’entrée'),'256 × 128'],'#39485e'),
            (450,155,300,100,[t('Event: upscale ×2','Événement : agrandir ×2'),t('Local bicubic / optional AI','Bicubique / IA facultative')],'#454178'),
            (855,155,300,100,[t('Retained full-size result','Résultat pleine taille'),'512 × 256'],'#244d57'),
            (450,380,300,110,[t('Graph preview: fitted','Aperçu du graphe : adapté'),t('Document stays 256 × 128','Document : 256 × 128')],'#39485e'),
            (855,380,300,110,[t('Tool PNG export','Export PNG de l’outil'),'512 × 256'],'#244d57')],
            [(345,205,450,205),(750,205,855,205),(1005,255,1005,380),(855,230,805,230),(805,230,805,435),(805,435,750,435)],
            t('Rerun after input edits. A graph connection alone does not execute the tool.', 'Relancez après modification. Une connexion d’image ne déclenche pas l’outil.'))
    print('Authored 24 bilingual SVG diagrams in .CyDocBuild/assets/manual.')


if __name__ == '__main__':
    main()
