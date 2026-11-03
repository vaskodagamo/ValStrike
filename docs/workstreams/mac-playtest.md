# Mac playtest tooling workstream

Append-only; dates Europe/Berlin. Checkout `.parallel-worktrees/mac-playtest`, branch `codex/mac-playtest`, starting commit `a520ff20fba6bb9a94d0fc6ae2c59acff3d94606`. This supersedes the completed tactical-map assignment; that checkout is untouched.

## 2026-11-03 — Source/LAN preparation started

- **Scope:** SET-05 source-checkout Mac playtest tools, with pinned tool validation. Independent tooling deliverable; packaged/server-source and real second-Mac acceptance remain open. Read the required project documents and PLAYTEST_WORKSTREAMS ownership contract.
- **Implementation:** Adding fixed graybox/Site01 and practice/match choices to the existing helper. Practice explicitly uses VSGameMode; match requires Site01 and a native reflected-class probe before server launch. Lightweight preflight validates current Git/LFS content and exact local tools. A single foreground session runner owns server/client lifetimes under the shared engine slot.
- **Parallel work:** Scoped assistants own the independent preflight module/tests, session runner/tests and LAN/bug-report drafts. No map, asset, gameplay, shared-slot or central-document edits authorized or made.
- **Verification plan:** Unit/error-path checks, current-checkout native probe including expected missing-match failure, clean clone with real LFS retrieval and native build, then bounded local server/two-client rehearsal on reserved UDP 7861. Every heavy command must acquire `Scripts/with_ue_slot.py`; busy exit75 is not bypassed.
- **Limits/next action:** Match runtime is absent in this common base. Validate available/failure paths honestly, finish source checkout rehearsal, then hand a local commit to the coordinator for publication and integration.

## 2026-11-03 — Implementation checkpoint before clean-clone rehearsal

- **Changes:** Fixed map/mode launch choices, stricter host validation, HEAD/LFS package integrity checks, exact tools and revision reporting, native class/package preflight, and foreground host/join runner with owned-process cleanup. Successful Mac Editor builds bind source/config/project/engine fingerprints to the actual module/manifest; native/session checks reject stale receipts. LAN checklist and bug-report template added.
- **Evidence:** 28 helper tests and 35 preflight/session tests passed at this checkpoint; subsequent review added a selected-engine mismatch regression. Syntax and diff whitespace checks passed. All fixture package headers remain temporary test data and are never opened by Unreal.
- **Native evidence:** Shared-slot baseline Editor build succeeded in 41.15s on UE5.8.4/Xcode26.1.1/SDK26.1; a subsequent up-to-date helper build succeeded in 2.10s and recorded its build receipt. Read-only preflight verified all 31 committed Content packages against actual LFS hashes. Native Site01 practice preflight passed, loading the actual world, hard dependencies and compiled VSGameMode; logs are under Saved/Playtest/native/1d630960fa0146b08fee158cb207cf20/. This is native loading evidence, not gameplay acceptance.
- **Coordination:** Two missing-match attempts returned shared-slot busy exit75 while rifle presentation owned the slot; neither launched Unreal. No lock bypass or other stream edits.
- **State:** SET-05 remains in progress until independent source/LFS checkout + build + bounded server/two-client verification. Match integration and real second-Mac LAN acceptance remain separate open gates.
