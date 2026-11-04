import bpy,json,numpy as np
from pathlib import Path
R=Path(__file__).resolve().parent
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=str(R/'Raw/download/model.glb'))
out=[]
for o in bpy.context.scene.objects:
 if o.type!='MESH':continue
 v=np.array([o.matrix_world@x.co for x in o.data.vertices]);w,a=np.linalg.eigh(np.cov(v.T));axis=a[:,np.argsort(w)[::-1]];p=v@axis
 out.append({'name':o.name,'vertices':len(v),'polygons':len(o.data.polygons),'bbox':[v.min(0).tolist(),v.max(0).tolist()],'pca_axes_columns':axis.tolist(),'pca_bounds':[p.min(0).tolist(),p.max(0).tolist()],'materials':[m.name for m in o.data.materials],'uvs':[u.name for u in o.data.uv_layers],'end_widths':[[float(t),np.ptp(p[(p[:,0]>np.quantile(p[:,0],t))&(p[:,0]<np.quantile(p[:,0],t+.1))],axis=0).tolist()] for t in [0,.1,.4,.7,.89]]})
(R.parents[3]/'docs/assets/knife01/raw-inspection.json').write_text(json.dumps(out,indent=2)+'\n');print(json.dumps(out))
