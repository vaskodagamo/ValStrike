# Operator V2 — Urban Entry reference pack

Created 2026-11-04 with the built-in image-generation tool. The user selected **A — Urban entry: cropped jacket, single shoulder plate** after comparing three concepts. This selects an art direction; it does not approve a production mesh, rig or in-game implementation. Earlier procedural FPArms01/Player01 models were rejected visually and remain prototypes.

## Files and intended use

| File | Purpose |
| --- | --- |
| [A — Urban Entry](Concepts/A_UrbanEntry.png) | Selected concept and primary design authority |
| [B — Field Operative](Concepts/B_FieldOperative.png), [C — Recon](Concepts/C_ReconOperator.png) | Unselected exploration, not inputs for the selected character |
| [Front](Turnaround/01_Front.png) | Primary character modeling/image-to-3D reference |
| [Back](Turnaround/02_Back.png) | Rear reference with corrected palm orientation |
| [Left](Turnaround/03_Left.png), [Right](Turnaround/04_Right.png) | Profile references; left shoulder is armored, right is fabric with crimson tab |
| [Glove detail](Gloves/01_DorsalPalm.png) | Artist reference for materials, articulated fingers, palm and dorsal construction; two-view sheet |
| [First-person composition](FirstPerson/01_RifleComposition.png) | Illustrative target for sleeve/glove quality and weapon handling; not an engine screenshot |

The character's **left shoulder** has the only metal shoulder plate. The right shoulder carries the crimson tab. Preserve the raised collar, cropped slate jacket, burgundy lining, lighter gray shirt, fitted black gloves, modest chest carrier, charcoal trousers and compact boots. Do not drift back to uniform cylindrical limbs, slab fingers, faceted skin/cloth or pasted-on armor. Style is a grounded/stylized blend, with clean readable major shapes; generated microtexture is a reference, not a requirement to reproduce photographic surface noise in-game.

## Use with an image-to-3D tool

Start with `Turnaround/01_Front.png`. If the chosen service supports multi-view input, add the corrected back and profile images as **separate images**, not a contact-sheet image containing several bodies. Do not mix concepts B/C into that generation. Keep the neutral pose. Use the glove sheet and first-person composition for artist communication, not as whole-character geometry inputs.

These are generated reference images, not calibrated orthographic scans. Major costume identity and shoulder asymmetry were inspected; fine seams, folds, fingers, perspective and apparent limb projection can still differ between views. The rear costume surface is an inferred design. An artist must reconcile these before treating the images as precise reconstruction constraints. The first rear attempt showed dorsal knuckles from both front and back; a targeted image edit replaced the rear glove surfaces with palms. `02_Back_Draft.png` is preserved for history but excluded from the delivery ZIP.

## Artist handoff and acceptance

1. Reconcile the selected concept and reference views into one coherent design; preserve the face and garment identity. The selected concept/front view take priority over inconsistent generated details. Confirm the final rear construction and left/right hand anatomy.
2. Use an image-to-3D result as a draft if it saves work. Correct anatomy, silhouette and clothing; rebuild unsuitable finger/joint geometry instead of retaining it because it was generated. No generated 3D mesh is included in this pack.
3. Finish topology suitable for deformation, UVs and the restrained fabric/leather/rubber/metal material groups. Budget against the game's actual camera needs; visual approval precedes final optimization.
4. Establish the skeleton and skin weights, then inspect shoulder/elbow/wrist/finger/knee bends and rifle/pistol grips. Coordinate bone, socket and unit conventions with the gameplay owner. Do not move authoritative hit regions to disguise a visual-model mismatch.
5. Author first-person arms as a deliberately fitted view asset using the same costume and glove language. The generated FPS image is an illustrative composition, not a verified camera transform, reload animation, weapon specification or promise that the current model looks this way.
6. Import and inspect in Unreal under the central integration owner's slot; verify camera fit, material readability, deformation, sockets and animations. No Unreal build, import, native asset, rig, animation or runtime acceptance is claimed by this image pack.

Provenance: all new references generated using the built-in `image_gen` tool; full prompts are in [concept-prompts.json](concept-prompts.json) and [reference-prompts.json](reference-prompts.json). The project's existing R01 Unreal studio preview supplied the rifle reference for the FPS illustration. No external 3D service was called, no model subscription/credits were purchased and no third-party 3D output was represented as finished art.
