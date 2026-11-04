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
