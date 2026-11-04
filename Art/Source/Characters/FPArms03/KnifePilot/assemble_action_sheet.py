"""Assemble existing actual-FBX renders; run with Python/Pillow after --sheet."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parent
DOC = ROOT.parents[4] / 'docs/assets/fparms03/knife-pilot'
FRAMES = {
    'equip': [1, 5, 9, 13, 19, 25],
    'idle': [1, 13, 25, 37, 49, 61],
    'locomotion': [1, 10, 20, 30, 40, 49],
    'slash': [1, 5, 10, 17, 25, 34],
    'stab': [1, 10, 19, 26, 40, 55],
}
canvas = Image.new('RGB', (2880, 1500), '#dce1e8')
draw = ImageDraw.Draw(canvas)
font = ImageFont.load_default(size=16)
for row, (kind, frames) in enumerate(FRAMES.items()):
    for column, frame in enumerate(frames):
        path = DOC / 'Previews' / f'c1-strip_{kind}_{frame:02}_90fov.png'
        with Image.open(path) as image:
            canvas.paste(image, (column * 480, row * 300 + 30))
        label = f'{kind.upper()}  f{frame}  {(frame - 1) / 60:.3f}s'
        if (kind, frame) in [('slash', 10), ('stab', 19)]:
            label += ' IMPACT'
        draw.text((column * 480 + 8, row * 300 + 7), label, fill='black', font=font)
canvas.save(DOC / 'Previews/c1_full_actions.png')
