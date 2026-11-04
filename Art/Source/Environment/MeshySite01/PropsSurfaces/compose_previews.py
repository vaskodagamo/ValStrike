"""Compose labeled sheets from actual Blender renders; no image repainting."""
from pathlib import Path
from PIL import Image,ImageDraw,ImageFont
root=Path(__file__).resolve().parent
p=root.parents[4]/'docs/assets/meshy-site01/props-surfaces/Previews'
items=[('MSH-W03_After.png','MSH-W03 / Paving tile','498 triangles / 200 x 200 cm'),('MSH-W04_After.png','MSH-W04 / Long cargo crate','6,000 triangles / 200 x 100 x 100 cm'),('MSH-W10_After.png','MSH-W10 / Cube cargo crate','6,000 triangles / 100 cm family'),('MSH-W07_After.png','MSH-W07 / Infill panel','3,774 triangles / Wall backing required'),('MSH-W02_DetailOnly.png','MSH-W02 / Stairs - DEFERRED','Detailed source only / No game FBX')]
im=Image.new('RGB',(1200,920),'#18222e');draw=ImageDraw.Draw(im)
head=ImageFont.load_default(size=25);font=ImageFont.load_default(size=17);small=ImageFont.load_default(size=13)
draw.text((24,20),'ART-12 / MESHY SOURCE HANDOFF',font=head,fill='white')
draw.text((24,55),'Actual Blender renders. Four game-mesh sources; stairs require controlled retopology. Native integration pending.',font=small,fill='#b8c2cd')
for i,(filename,title,note) in enumerate(items):
 x=(i%3)*400;y=92+(i//3)*415
 with Image.open(p/filename) as photo: im.paste(photo.convert('RGB').resize((400,350),Image.Resampling.LANCZOS),(x,y))
 draw.text((x+12,y+357),title,font=font,fill='#e9edf2')
 draw.text((x+12,y+380),note,font=small,fill='#b8c2cd')
draw.text((825,575),'SOURCE ONLY',font=head,fill='white')
for y,text in [(625,'Original maps retained'),(655,'Editable detail masters'),(685,'No map or collision changes')]:
 draw.text((825,y),text,font=font,fill='#b8c2cd')
im.save(p/'ART12_SourceOverview.png')
sheet=Image.new('RGB',(1200,1210),'#18222e');d=ImageDraw.Draw(sheet)
d.text((20,15),'REPRESENTATIVE SAMPLES / ACTUAL SOURCE RENDERS',font=head,fill='white')
for row,aid in enumerate(['MSH-W03','MSH-W04']):
 for col,state in enumerate(['Before','After']):
  x=col*600;y=60+row*570
  d.text((x+20,y),aid+' / '+('Uploaded geometry + maps' if state=='Before' else 'Cleaned game mesh + rebake'),font=font,fill='white')
  with Image.open(p/f'{aid}_{state}.png') as photo: sheet.paste(photo.convert('RGB').resize((600,525),Image.Resampling.LANCZOS),(x,y+28))
sheet.save(p/'ART12_RepresentativeComparison.png')
print('Composed actual-render overview and representative comparison.')
