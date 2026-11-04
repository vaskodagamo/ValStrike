"""Encode actual Blender renders as compact GIF playbacks and frame contact sheets.
Python 3 + Pillow. No generated/synthesized character imagery or pixel retouching.
"""
import json, html, shutil
from pathlib import Path
from PIL import Image, ImageDraw
ROOT=Path(__file__).resolve().parent
DOC=ROOT.parents[3]/'docs/assets/urban-entry01'
PREV=DOC/'Previews'
manifest=json.loads((DOC/'preview_manifest.json').read_text())
cards=[]
for clip,record in manifest.items():
    files=sorted((PREV/'Frames'/clip).glob('*.png'))
    assert len(files)==len(record['frames'])
    frames=[Image.open(p).convert('RGB') for p in files]
    # GIF timing uses 10ms ticks; cumulative rounding avoids systematic speedup.
    ticks=[round((f-record['frames'][0])/60*100) for f in record['frames']]+[round(record['duration_seconds']*100)]
    durations=[10*(b-a) for a,b in zip(ticks,ticks[1:])]
    frames[0].save(PREV/(clip+'.gif'),save_all=True,append_images=frames[1:],duration=durations,loop=0,optimize=False,disposal=2)
    # Six actual frames in time order; annotations are outside the rendered image.
    indices=[round(i*(len(frames)-1)/5) for i in range(6)]
    sheet=Image.new('RGB',(1200,980),(28,32,37));draw=ImageDraw.Draw(sheet)
    for n,i in enumerate(indices):
        frame=frames[i].resize((400,467))
        x,y=(n%3)*400,(n//3)*490
        sheet.paste(frame,(x,y));draw.text((x+10,y+470),f'{clip}  |  source frame {record["frames"][i]}',fill=(230,235,240))
    sheet.save(PREV/(clip+'_frames.png'))
    label=html.escape(clip.replace('_',' '))
    cards.append(f'<article><h2>{label}</h2><img src="Previews/{clip}.gif" alt="{label}: actual Blender source playback"><p>{record["duration_seconds"]:.3f} s at source 60 FPS. Preview sampled at 15 FPS; floor grid 50 cm.</p><a href="Previews/{clip}_frames.png">Six-frame inspection sheet</a></article>')
(DOC/'preview.html').write_text('''<!doctype html><html lang="en"><meta charset="utf-8"><title>Urban Entry — ART-13 source inspection</title><style>body{margin:28px;background:#171d24;color:#e1e8ee;font:16px system-ui;max-width:1200px}h1{font-size:26px}h2{font-size:18px}p{line-height:1.5;color:#b9c7d3}main{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:24px}article{background:#242d36;padding:16px}img{width:100%;max-width:480px;display:block}a{color:#a5d6ee}.still{display:flex;gap:14px}.still img{max-width:340px}small{display:block}</style><h1>Urban Entry / ART-13 — actual source previews</h1><p>Blender source only; no Unreal or gameplay verification. These are the supplied Meshy character and motions, with normalized common transforms. Workbench motion previews emphasize deformation; use the Cycles stills for the supplied PBR material appearance. GIFs replay the source motion: the reset of a traveling or one-shot clip is not a seamless animation loop.</p><main>'''+''.join(cards)+'''</main><h2>Material / neutral source comparison</h2><p>Texture interpretation and character identity are preserved. Changes are portable texture paths, common units/orientation and compact shared-skeleton exports.</p><div class="still"><figure><img src="Previews/01_material_before.png"><figcaption>Imported material — before</figcaption></figure><figure><img src="Previews/02_material_after.png"><figcaption>Portable material — after</figcaption></figure><figure><img src="Previews/03_rear.png"><figcaption>After — rear</figcaption></figure></div><p><a href="README.md">Delivery and native-integration notes</a></p></html>''')
print('Encoded six GIFs and six frame sheets from actual source renders.')
