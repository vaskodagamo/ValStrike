# ART-22 — Universal gloved hands with bare forearms

- Date/backlog: 2026-11-04 (Europe/Berlin), PLAY-02; direct user priority verified in Direktor's user message: “universal hands … naked but with gloves … hold knife, pistol and rifles … make with meshy what is necessary”.
- Status: reserved for the existing hands owner after its ART-19 checkpoint. Never work both assignments concurrently.
- Owner: Codex “Art — Meshy first-person hands”, thread `01a1a6cf-0f10-73d0-8c33-29cb863d7b33`, High; lead/producer Direktor (2).
- Branch/worktree: `codex/art22-fparms03`, `/Users/webproduktion3/Documents/ValStrike/.parallel-worktrees/art22-fparms03`.
- Base: merged main `159431d` plus this coordinator-prepared task brief; exact prepared HEAD supplied in dispatch. Read the current producer policy at `/Users/webproduktion3/.codex/worktrees/10a1/ValStrike/docs/PRODUCER.md` until #88 merges.
- Exclusive writes: `Art/Source/Characters/FPArms03/`, `docs/assets/fparms03/`, `docs/workstreams/art22-fparms03.md`. This task brief and central board/log remain producer-owned.
- Read-only: safely frozen ART-19 FPArms02 source/rig; UrbanEntry01 skin/scale; corrected R02/P02/K01 source revisions. Preserve FPArms01/02 and every other owner's checkout. Document exact source commits/hashes copied into FPArms03.
- Result: one reusable paired first-person rig with bare forearms and fitted full-finger tactical gloves, plus separate rifle, pistol and knife hold poses. No jacket sleeves. Source ready/held status must reflect actual appearance, deformation and weapon contact.
- Validation: V1, real Blender source/pose/export inspection and existing targeted metadata checks only. No new test framework, contributor Unreal operation, build or native Content/code change. One future integration owner imports/wires and verifies runtime.
- Publication: commit/push owned files, open PR to main, concise handoff and stop; user merges. No recursive workers, additional paid stages or independent next assignment.

## Checkpoint and reuse first

Finish only ART-19's current concise checkpoint/publication with honest held/provisional findings, then stop writes to its checkout. Confirm that frozen commit before starting here. Do not discard the sleeve version or keep polishing it in parallel.

Inspect the frozen original glove surface and compatible rig before any paid request. Reuse good glove geometry/UVs/custom normals/materials when the wrist seam and proportions permit a clean bare-arm variant; document a short reuse decision with a real preview. Exposed forearm anatomy cannot be obtained by recoloring a sleeve. A local anatomical replacement is acceptable only if it meets the requested quality and preserves the good glove; otherwise use the single reserved Meshy draft below. Do not force reuse through crude cylinders, global smoothing or distorted fitting.

## Direction and necessary authoring

Keep natural, clean stylized anatomy in the CS/Valorant visual direction: fitted charcoal gloves, restrained burgundy details, flexible finger joints, believable thumb web and a neat cuff against exposed skin. Use UrbanEntry's skin tone where its source provides a usable reference. No jacket cuff, bracelet bulk, block fingers or photorealistic grunge. Make skin/glove material regions distinct where possible without damaging the source UVs or material response.

If generation is needed, derive one isolated **right** gloved hand plus bare forearm image with built-in imagegen: relaxed pose, five clearly separated natural fingers, complete wrist/forearm silhouette and plain background. No weapon, left/right pair, labels or contact sheet in the Meshy input. Inspect it before submission. Store image, exact prompt and provenance in the owned reference folder.

Artist constructs the paired left hand locally, correcting handedness, normals and meaningful asymmetry. Reuse compatible ART-19 finger/thumb/wrist/forearm bone names and centimeter, +X-forward/Z-up export conventions; document any necessary changes. A shared rig supports different weapon poses; it does not make one universal grip or automatically supply reload/equip animations. Preserve the full-detail master under the ART-21 source-preservation rules; optimize only a separately justified candidate and hold visible degradation.

## Acceptance and frozen fit dependencies

Inspect relaxed/open hands, curled fingers, thumb opposition and wrist bends for collapsing joints, thumb-web spikes, stretched UVs or intersecting fingers. Then author separate poses against actual frozen corrected weapons:

- R02 rifle: use the ART-17 correction follow-up, not the older merged #76 model. Record exact commit/hash when published; until then mark rifle fit provisional and continue other independent work.
- P02 pistol: correction #81, source head `ccb08f06e22d23573697a1494a34436f901fa006` (merged).
- K01 knife: preserved-source #85, head `d774eafb710af68bf5fa23666e5d0ddacb651d3e` (merged).

Check dominant grip, thumb-web/trigger/guard clearance, and support-hand placement appropriate to each weapon; the knife's off hand need not grip the blade. Include 90-degree first-person framing and close contact views. Keep unproven contacts held; do not shrink fingers or rescale weapons/character to hide a mismatch. Recheck only changed contacts after a source dependency freezes. No full reload, melee, equip, locomotion or gameplay expansion.

Deliver editable blend, skeletal FBX, bounded pose/actions, textures/material mapping, rig/bone table, dimensions and attachment recipe. Export only FPArms03 and its rig, not copied reference weapons/character. Compare original/generated source, finished source and reimported export under matching camera/light/scale/material conditions. For a locally combined variant compare the unchanged glove against its original and inspect the new anatomical seam separately. Record all intentional edits. Source readiness remains separate from native/in-game verification.

## Centrally reserved optional Meshy job

Reservation **`ART-22-FPArms03-draft-1`**, **30 credits maximum**, from the producer's 300-new-credit Berlin-day pool; not another use of ART-19's earlier allowance. Producer initialized/reserved the ledger; only the producer writes it. Reservation is an upper bound, not an instruction to spend if clean reuse succeeds.

Exactly one standard image-to-3D request, explicit **`meshy-7.1`**, standard geometry/default texture model, textured/PBR **4K**, one right hand/bare forearm. Official [API pricing](https://docs.meshy.ai/en/api/pricing) checked 2026-11-04 lists 30 credits for matching 4K image-to-3D without Ultra geometry. Confirm matching CLI 0.8.2 options and authenticated balance before create. No paid retry, retexture, remesh, second hand, humanoid auto-rig, animation or conversion stage; no credit purchase. If a matching request exceeds 30, stop that request and report.

Apply the installed `valstrike-meshy` skill and linked preservation guide. Submit once asynchronously in a fresh job workspace; immediately record task/operation ID, resource, options, source hashes and reservation ID in `docs/assets/fparms03/meshy-job.json`. Resume the recorded task after timeouts, never repeat create because output is uncertain. Download and preserve originals promptly. Record actual charge when evidenced, otherwise unknown. Producer binds/settles the ledger from that manifest; worker never resets or edits the producer ledger. If no request is needed, explicitly record “not submitted, reuse succeeded” so the producer can release the reservation at zero.

## Completion handoff

One concise `docs/workstreams/art22-fparms03.md`: exact commit/PR, reuse/generation decision and spend, rig compatibility, frozen weapon dependencies, preview/export evidence, pass/held/pending findings, native checks deferred and next owner. No claim of universal runtime readiness until centralized integration proves it.
