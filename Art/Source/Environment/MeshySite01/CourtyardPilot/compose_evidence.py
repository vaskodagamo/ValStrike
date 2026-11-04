"""Label actual unretouched Blender renders; no synthetic or repainted asset imagery."""
from pathlib import Path
from PIL import Image,ImageDraw,ImageFont
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[4];PREV=ROOT/'docs/assets/meshy-site01/courtyard-pilot/Previews'
font=ImageFont.truetype('/System/Library/Fonts/Helvetica.ttc',23)
small=ImageFont.truetype('/System/Library/Fonts/Helvetica.ttc',19)
for aid in ['MSH-W01','MSH-W08','MSH-W09']:
 sheet=Image.new('RGB',(1800,1160),(24,35,45));d=ImageDraw.Draw(sheet)
 d.text((20,13),aid+' | ORIGINAL / PRESERVED SOURCE / ACTUAL FBX REIMPORT',font=font,fill='white')
 d.text((20,47),'Matched Blender source renders. Uniform scale + pivot only; supplied maps, UVs and corner normals.',font=small,fill=(190,207,216))
 for row,suffix in enumerate(['','_detail']):
  for col,stage in enumerate(['original','source','reimport']):
   im=Image.open(PREV/(aid+'_'+stage+suffix+'.png')).convert('RGB');im=im.resize((600,533))
   x=col*600;y=90+row*535;sheet.paste(im,(x,y));d.text((x+12,y+8),stage.upper()+(' / DETAIL' if row else ''),font=small,fill=(15,21,29))
 sheet.save(PREV/(aid+'_Comparison.png'))
# Dimension captions sit outside the actual images; asset pixels remain unpainted.
import json
m=json.loads((PREV.parent/'assembly-measurements.json').read_text())
sheet=Image.new('RGB',(1400,1995),(24,35,45));d=ImageDraw.Draw(sheet)
d.text((24,16),'ART-25 | MEASURED COURTYARD / SERVICE FACADE',font=font,fill='white')
d.text((24,50),'BLENDER SOURCE ASSEMBLY — not an Unreal screenshot. Gray blocks are retained-map context.',font=small,fill=(190,207,216))
sheet.paste(Image.open(PREV/'Assembly_eye_152cm.png').convert('RGB'),(0,85))
d.text((24,995),'Eye height 152 cm | facade 645.70 W × 300 H | roof base 394, parapet top 457.59 cm',font=small,fill='white')
sheet.paste(Image.open(PREV/'Assembly_overhead.png').convert('RGB'),(0,1030))
d.text((24,1940),'Overhead | front Y=1510 | retained wall Y=1500 | dashed guide Y=1750: 240 cm to visual',font=small,fill='white')
d.text((24,1970),'Two 1 cm nominal facade joints; actual sampled profiles 1.88–4.85 cm. Roof modules 200 cm + 1 cm joints.',font=small,fill=(190,207,216))
sheet.save(PREV/'Assembly_Dimensions.png')
