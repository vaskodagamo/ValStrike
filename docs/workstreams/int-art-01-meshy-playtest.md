# INT-ART-01 — Meshy character and Site01 first playtest

## 2026-11-04 (Europe/Berlin) — Preflight done; blocked on ART-15 freeze and engine slot

- Branch `claude/int-art-01-meshy-playtest`, prepared HEAD `e61bcaa`. State: **Blocked** (no native work possible yet).
- Gates (details in [preflight](../assets/meshy-pilot-integration/PREFLIGHT.md)):
  - ART-15: not frozen. Branch head still `51d19ee`; no committed `pilot-readiness.json` or `art15-character-pilot.md`. Uncommitted pilot exports in its checkout were not read as acceptance.
  - ART-16: readiness record committed at `757a56f` (W03/W04/W07/W10 ready), but the required `docs/workstreams/art16-props-pilot.md` handoff and push/PR are missing. Not merged here yet; will accept `757a56f` or its frozen successor.
  - Engine slot: INT-05 running (`Tests/Damage/run_runtime.py`); waiting for its release PR. No slot attempt made.
- Prepared: [props importer](../../Art/Integration/MeshyPilot01/import_props.py) (allowlist/hashes from the readiness record, no map access, no overwrite, reopen mode; syntax-checked only, never run), native plan for props/character/presentation code and verification.
- Finding for ART-16 / coordinator: W03 and W07 placement assumes native local Y bounds equal the source record. Unreal's FBX handedness conversion likely negates Y (Blender `+Y` → Unreal `−Y`); no Y-asymmetric precedent exists in this project. The importer reports this; a `y_negated` result holds W03/W07 placement for an ART-16 recipe correction. Pre-confirming the intended native frame now would avoid that round trip.
- Checks run: read-only inspection of briefs, branches, boards, Site01 actor inventory (all recipe anchors match), lighting mobility; `py_compile`. No Unreal, build, import or runtime test.
- Next: on ART-15 freeze + ART-16 handoff + INT-05 release notice, merge the frozen revisions and current main, record the candidate SHA, then import/place/wire and run one server/two-client Site01 acceptance.

## 2026-11-04 (Europe/Berlin) — Resumed under producer Direktor (2); art inputs accepted; waiting for engine slot

- Candidate base: main `4876acc` (fast-forward; my preflight merged via #78). Ancestors confirmed: ART-15 handoff `02f6bf2` (readiness `partial`, source `45deb70`), ART-16 handoff `587e6e7` and readiness `757a56f` (source `f7a15a1`). `git lfs pull` done; all 15 ART-15 and 24 ART-16 readiness files match their SHA-256.
- **Current gate: engine slot.** Live gameplay board lists INT-06 as "Done; then one D3 attribution run" with no release recorded; `integration-06.md` has staged, uncommitted D3 edits. The lock metadata names INT-06's `test_movement.py --net-profile typical` (PID 77809), which has exited, but a free lock alone is not a release. No slot attempt made.
- Prepared, unbuilt and unrun (one combined build later):
  - `Presentation/VSCharacterPresentationComponent.*` (new): when native assets exist and not on a dedicated server, puts `SK_VS_UrbanEntry01` on the existing `GetMesh()` (OwnerNoSee, NoCollision), plays the hold→walk blend space, or switches clips if the blend space is absent, from replicated 2D speed. Walk clip speed is 145 cm/s (stance-foot measurement from ART-13 motion samples), play rate capped at 1.8×. Prototype `BodyVisual`/`HeadVisual` stop rendering but keep their transforms, so the movement smoke test's bounds stay as before. Missing assets leave the prototype unchanged.
  - `VSWeaponPresentationComponent`: optional `SetWorldSocket()`; the world model attaches to the socket with no grip offset when the anchor has it, otherwise uses the existing `WorldGrip`. First-person, recoil, flash and authority unchanged.
  - `VSCharacter`: +10/+5 presentation-only lines (create the component, initialize it before the weapon views, rifle uses `R01_Pilot_Grip`). No GPT-15 movement, crouch or jump hunk touched.
  - `Art/Integration/MeshyPilot01/import_character.py`: allowlist and hash import; measures the source-to-native basis from the native `hand_r` pose before building the socket (`inverse(H)·C·W·C`), holds only the socket if neither basis matches; best-effort blend space.
- Temporary pilot fallbacks to record at QA: forward walk for every direction; crouch keeps the standing pose (the visual head stays above the crouched hit zones, recorded, not disguised); body hidden while dead (no death clip); remote pistol keeps its existing floating world offset (no fitted pistol pose; the weapons smoke test requires the remote model to stay visible); walk above 261 cm/s slides.
- Checks run: `py_compile`, `git diff --check`, readiness hash verification. Next: on a recorded slot release → one build, imports and reopen, Site01 placement, server plus two clients.
