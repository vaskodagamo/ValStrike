# ART-24 — Correct FPArms03 pistol contact

- Date/backlog: 2026-11-04 (Europe/Berlin), PLAY-02; concrete ART-22 art-review defect.
- Owner: existing Codex “Art — Meshy first-person hands”, `01a1a6cf-0f10-73d0-8c33-29cb863d7b33`, High. ART-22 is merged and its owner is idle/stopped.
- Checkout: reuse the owner's `/Users/webproduktion3/Documents/ValStrike/.parallel-worktrees/art22-fparms03`; after confirming clean, create `codex/art24-fparms03-pistol-contact` from current `origin/main`. Preserve the frozen ART-22 branch/head `b1795991bed5cf2abaaabe8fcef79bfa5f9e5a4a`. No second writer or checkout takeover.
- Exclusive writes: `Art/Source/Characters/FPArms03/`, `docs/assets/fparms03/`, `docs/workstreams/art24-fparms03-pistol-contact.md`. Producer owns this brief and central records. ART-22's historical handoff stays intact.
- Read-only dependencies: preserved P02 `ccb08f06e22d23573697a1494a34436f901fa006`, R02 correction #89, K01 #85. No weapon or third-person character edits.
- Result: one usable stationary first-person P02 hold on the existing bare-forearm pair, correcting support-hand penetration and visible finger/web collapse. Other weapon holds retain their explicit held state.
- Validation V1: actual affected source/export pose inspection and existing focused round-trip checks only. No new test framework, broad regeneration of evidence, Unreal operation/build or paid service call. **Zero credits.**
- Publication: one main-targeted PR, exact source/hand-off commits, honest pass/held states, then stop. User merges.

## Observed defect and correction boundary

Producer inspected ART-22's matched original/source/export glove images: detail, cuff, normals and PBR appearance are preserved. Its actual `Previews/10_pistol_reverse_contact.png` shows support fingers/thumb disappearing into the grip. The source handoff also identifies local MCP/web stretching. Preserve the successful source surface; work on the pose and localized skin weights first.

Adjust only the P02 hold, its attachment recipe and the minimum local finger/thumb/web weighting needed for a believable grip. Keep the 39-bone names, hierarchy and rest pose, original mesh topology/positions, UVs, custom normals and material maps. Do not scale fingers or weapons, decimate, smooth, recolor or regenerate the asset to conceal defects. If safe correction requires structural mesh/rig changes beyond this boundary, stop with a precise held finding and proposed repair instead of silently expanding scope.

Keep the trigger finger clearly indexed outside the guard for this stationary hold; no firing/reload/equip/knife animation is assigned. Check both hands against the real P02 from opposite sides and above. Support contact should be with the dominant hand/grip, without buried fingertips, thumb collapse or slide/muzzle obstruction. Use the existing 90-degree source camera to confirm the grip reads at first-person size; it is not native camera approval.

## Evidence and acceptance

- Save the editable source and affected skeletal/animation export; preserve originals and record changed weights/action/attachment data.
- Supply matched before/after close contact views from both sides and above, plus one 90-degree composition and the corresponding actual FBX reimport. Retain the established lighting/material binding. No contact pass based only on sampled numerical distances or a single flattering angle.
- Reuse ART-22's preservation/rig evidence. If shared weights change, inspect the affected open/curl and existing rifle/knife poses once for newly introduced damage; do not broaden those holds into new fit tasks or claim them accepted.
- Update readiness with the exact P02 source and export revision, contact/deformation result, any remaining limitation, and native/LOD/performance gates still pending. Existing full-detail source is not a runtime budget approval.
- Handoff clearly distinguishes an improved/accepted stationary source hold from pending native integration. No engine job or new Meshy reservation is part of ART-24.
