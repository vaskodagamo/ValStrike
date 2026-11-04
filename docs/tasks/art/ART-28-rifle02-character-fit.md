# ART-28 — Seat R02 against the shoulder and refit the hands

Owner: existing character artist `01a1a6b0-8aa8-7373-b8bb-bc5506f451d4`, High, after frozen ART-27. New isolated worktree `/Users/webproduktion3/Documents/ValStrike/.parallel-worktrees/art28-rifle02-character-fit`, branch `codex/art28-rifle02-character-fit`, base merged main `fa637c5d04b83069f56f6a4c75eeeebced83c4f2`. Zero paid jobs/credits, no Unreal/build or new tests. Native INT-ART-03 continues separately; this task does not write native assets, code, the existing rifle source or the base character.

## Observed defect and result

INT-ART-04's actual `docs/assets/weapon-replacement02/runtime/remote_before_after.png` and `site01_remote_rifle.png` show R02's stock at the cheek with no shoulder seat. The imported measured offset exactly reproduces ART-17's source fit; another offset guess alone is not a correction. Producer confirmed the runtime images. Refit the preserved UrbanEntry pose around the actual full-size R02: butt against shoulder/chest pocket, face/head clear of stock/receiver, dominant hand at grip with a natural trigger-finger pose and supporting palm on the fore-end. Keep a plausible shoulder/elbow/wrist alignment and readable aim silhouette. No character/weapon scaling or geometry change to hide the problem.

Exact writes: new `Art/Source/Characters/UrbanEntry01/Rifle02Pilot/`, `docs/assets/urban-entry01/rifle02-pilot/`, and `docs/workstreams/art28-rifle02-character-fit.md`. Producer prepares this brief before handover; artist leaves central files/other briefs alone. Existing ART-15/23/27, rifle #89, native #118 and base source remain read-only. Use the same preserved 66-bone pelvis-root hierarchy/rest, weights, geometry, UV/custom normals/PBR. No regeneration, smoothing/decimation or unrelated gait/crouch/character redesign.

Deliver a compatible corrected rifle hold plus walk/run using the existing lower-body cycles, root motion off; do not reauthor gait. Explicit R02 assembly/hand_r matrices in source and expected native basis, with the unchanged weapon dimensions. Pose source/export must match. If the existing rig cannot seat the shoulder and both hands without structural work, show the exact limitation and smallest proposed change before expanding scope.

## Evidence and stop

Use actual saved-source and animation-FBX reimport, matched original/candidate/export contact views, both sides of the stock/face/shoulder and both hands, hold and locomotion extreme views, and unzoomed 5 m game-distance framing. Preserve original PBR for comparisons. Reuse existing skeleton/surface evidence and inspect only changed poses/exports; no generic test suite or full old-animation rerun. Provide concise readiness, exact transforms/hashes/remaining limitations and one main-targeted source PR. Freeze and stop. Native import and runtime blend/aim/reload acceptance are a later bounded assignment after INT-ART-03, not part of this source handoff. User merges.
