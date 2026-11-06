"""Compose labeled EN/FR contact sheets from real native brush preview readbacks.

Usage: python tools/build_manual_brush_atlas.py build/DocumentationNative/run-.../d3d12-brushes-en
The native scenario renders every shipped brush; this sheet selects twelve.
"""
import argparse
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
from manual_brushes import catalog, ROOT

IDS=['calligraphy','star_scatter','charcoal','airbrush','cy_bristle','cy_watercolor',
     'cy_grass','cy_chain','cy_texture','sx3d_fissures','sx3d_rivets','sx3d_coulures']


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('readbacks',type=Path)
    args=parser.parse_args()
    brushes={b['brush']:b for b in catalog()}
    images={name:Image.open(args.readbacks/('brush-'+name+'.png')).convert('RGBA') for name in IDS}
    if not all(im.size==(480,160) and im.getchannel('A').getextrema()[1]>0 for im in images.values()):
        raise ValueError('Expected nonempty native previews, 480x160')
    font=ImageFont.truetype('C:/Windows/Fonts/arial.ttf',24)
    heading=ImageFont.truetype('C:/Windows/Fonts/arialbd.ttf',34)
    small=ImageFont.truetype('C:/Windows/Fonts/arial.ttf',20)
    for language in ('en','fr'):
        french=language=='fr'
        image=Image.new('RGB',(1500,1050),'#191e2a');draw=ImageDraw.Draw(image)
        draw.text((35,24),'Aperçus réels de pinceaux' if french else 'Rendered brush previews',font=heading,fill='#eef1fa')
        draw.text((35,74),'Pression simulée · paramètres livrés · même couleur bleue' if french else
                  'Simulated pressure · shipped parameters · same blue painting color',font=small,fill='#c3cee4')
        for i,name in enumerate(IDS):
            x=15+(i%3)*495;y=125+(i//3)*220
            draw.rounded_rectangle((x,y,x+480,y+207),radius=10,fill='#252c3b',outline='#667693')
            draw.text((x+12,y+8),str(i+1)+'. '+brushes[name]['profile'][int(french)],font=font,fill='#eef1fa')
            image.paste(images[name],(x,y+42),images[name])
        draw.text((35,1020),'Voir les fiches pour les réglages et limites.' if french else
                  'See individual references for controls and limits.',font=small,fill='#c3cee4')
        image.save(ROOT/'.CyDocBuild/assets/manual'/('brush_atlas_'+language+'.png'))
    print('Composed two contact sheets from twelve native GPU previews.')


if __name__=='__main__': main()
