"""Hash the frozen source/export/texture/preview package; no service calls."""
import hashlib,json
from pathlib import Path
R=Path(__file__).resolve().parent;REPO=R.parents[3];D=REPO/'docs/assets/knife01'
def row(p):return {'path':str(p.relative_to(REPO)),'bytes':p.stat().st_size,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()}
files=[p for p in R.rglob('*') if p.is_file() and 'Raw' not in p.relative_to(R).parts and p.suffix not in {'.blend1','.pyc'}]
files.extend(p for p in (D/'Previews').rglob('*.png'))
m={'schema_version':1,'asset':'K01','task':'ART-20','date_europe_berlin':'2026-11-04','files':[row(p) for p in sorted(files)]}
(D/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
s=json.loads((D/'source-inspection.json').read_text());v=json.loads((D/'verification.json').read_text())
ready={'asset':'K01','task':'ART-20','state':'source-ready for art review; dedicated hand posing, native and runtime pending','source':row(R/'Knife01.blend'),'game_export':row(R/'Exports/SM_VS_Knife01.fbx'),'material_recipe':row(R/'materials.json'),'dimensions_cm':s['dimensions_cm'],'triangles':s['source_triangles'],'material_slots':s['material_slots'],'uv_layers':s['uv_layers'],'units':'centimeters','forward':'+X along blade','up':'+Z','Grip':s['Grip'],'Tip':s['Tip'],'frame_purpose':'visual presentation markers only; not authoritative melee volumes','suggested_hold':v['fit'],'character_not_modified':v['character_file_unchanged'],'checks':{'saved_source_reopen':v['source_reopened'],'actual_fbx_roundtrip':v['fbx_roundtrip'],'topology':s['topology'],'preview_review':'Both neutral sides and actual frozen-character reference views inspected in Blender output','unreal_operations':0},'limitations':['Inherited rifle hand pose intersects grip and has extended index finger; dedicated hand pose required','No first-person FPArms02 attachment or attack animation acceptance','Constant-material blade/trim UV charts overlap; pack before future custom bake','Generated grip topology and normal response may need further art polish at first-person camera distances','Separate closed visual shells overlap internally; not manufacturing geometry','No native asset, gameplay equip, damage, melee networking, movement benefit or runtime verification'],'next_owner':'Art lead reviews; hands/animation owner poses against frozen manifest; separately assigned integration owner imports and verifies native source in reserved UE slot','credits_consumed':30,'knife_combat_implemented':False}
(D/'readiness.json').write_text(json.dumps(ready,indent=2)+'\n')
print('K01 manifest:',len(files),'files')
