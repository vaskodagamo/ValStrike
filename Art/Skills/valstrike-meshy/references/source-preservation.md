# Preserve Meshy source quality

Use this for assigned local cleanup, correction, rigging and export. The goal is to retain the supplied model's visual quality while making the specific changes the game needs. Do not regenerate a good source to repair damage caused locally.

## Establish the baseline

- Keep the original download and textures unchanged, with their existing hashes/provenance. Work on a duplicate; retain the untouched imported mesh and material setup as a reference. Select one documented original if GLB and FBX differ. Carry the recoverable master through the assigned LFS/source handoff; a temporary service URL is not a backup.
- Inspect the textured original before editing. Record dimensions, triangle count, UV/material setup and whether authored custom normals exist in the existing asset manifest. Counts describe the asset; they do not prove quality or require reduction.
- Make the first candidate with only required unit/orientation/pivot and path corrections. Preserve vertex positions relative to the object, UVs, normals and PBR response except for the documented transform. Do not normalize by reconstructing the visible surface.

## Change only what the task needs

| Operation | Required handling |
| --- | --- |
| Scale, axis conversion, mirroring | Transform geometry and normals consistently; inspect handedness, winding and shading. Nonuniform fitting needs a separate comparison because it changes the design. |
| Part separation, caps, rigging | Preserve existing surface data on untouched regions. Limit new geometry to the required area. Inspect cuts in assembled and separated poses; keep visible cap/seam artifacts on hold. Do not rescale the character to hide a weapon-fit defect. |
| Normals and smoothing | Retain authored corner/split normals and hard edges. Topology edits can discard them even when UVs and counts look correct. Preserve or transfer them from the master where appropriate and inspect the result. Avoid global smoothing, normal clearing/recalculation or weighted-normal replacement as default fixes. |
| Materials | Retain original textures, UV mapping, factors and channel connections. Fix a demonstrated import error before changing the design. Do not flatten color, suppress normal detail, or globally remap roughness/metalness to hide damaged geometry. |
| Decimation, retopology, rebaking, texture reduction, reconstruction | Make a separate derived candidate only for an assigned concrete need, such as demonstrated deformation failure, a measured runtime constraint or an explicit delivery budget. Record the affected region and the reason. Never run these automatically on every asset. |

Where a required budget and appearance conflict, preserve the full-detail master and report the optimized candidate as held. The art lead resolves the tradeoff within the assignment; an existing authorization for necessary rigging or repair does not need another user approval. High-detail source readiness does not imply suitability for the shipped game. Optimization and runtime acceptance remain explicit work for the assigned owners.

For glTF materials, base-color textures use sRGB, while normal/roughness/metallic data use linear interpretation. Packed glTF metal/roughness uses green for roughness and blue for metalness; do not guess that every texture file has this packing. Preserve the imported material's actual factors and normal convention. Document any required destination conversion. See the [glTF material specification](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#materials) and [Blender glTF material mapping](https://docs.blender.org/manual/en/3.0/addons/import_export/scene_gltf2.html?highlight=gltf). Custom normals are part of surface appearance; see [Blender's custom split normals explanation](https://docs.blender.org/manual/en/3.1/modeling/meshes/structure.html).

## Compare before accepting

Use one reusable preview scene. Compare the untouched imported original, edited source and actual reimported delivery under the same camera/framing, scale, pose, lights, world, exposure and color management. Keep original material settings wherever materials are not the intentional change. A Meshy thumbnail versus a differently lit Blender render is not a controlled comparison. If a dimension or pose intentionally changes, record it and align the views so it does not hide surface loss.

Use a whole-asset view and close views of the changed areas. Check silhouette, bevel/rail/panel edges, texture detail, paint boundaries, seams and specular response. Hands and characters also need the affected bend/grip poses; modular assets need the affected mating edges. Reuse the same views after an export correction, rather than rerendering unrelated assets.

The asset owner inspects these images and records the result in the existing handoff. New dents, softened edges, muddy or flat textures, stretched UVs, shading seams or worse deformation are failures even if bounds, topology and triangle checks pass. Restore the last intact candidate or restart the affected operation from the original; do not layer more smoothing or recoloring over the defect.

The handoff needs only: source/job hash, brief list of necessary changes and their reasons, paths to the matched comparisons, known limitations, and `pass`, `held` or `pending` with the reviewer/date and source revision. Missing evidence means pending. A source pass still leaves native material/import/runtime verification to the reserved integration owner. Do not add an image-test framework, another build or a separate review agent for this check.
