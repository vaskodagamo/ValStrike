# INT-ART-03 — K01 knife native integration

## 2026-11-04 (Europe/Berlin) — Assignment acknowledged; candidate verified; remote rendered view held

**Ownership.** The existing sole native integrator took this task under the producer's [brief](../tasks/integration/INT-ART-03-knife.md) after #118 merged.
- **Branch:** `claude/int-art-03-knife` from main **`4997215`**. That is the observed `fa637c5` plus the docs-only #119.
- **Prior branches:** preserved.
- **Inputs, all verified on main:** ART-26-C1 #115 `5b8b675`/`25fc75b`, ART-27 #110 `bca668a`, K01 #85 and M1 #114 `adb7374`.
- **Dependency:** I combined the frozen WPN-02-CHECKS head `22b71bf` (PR #121, owned by GPT-04) unchanged before the one build. No gameplay or test file was edited here.

**Candidate `1b572d0783ba97337dd1cce2d376976ba1456980`.**
- **Import:** allowlisted with SHA-256 checks, followed by a fresh-process reopen. Existing character and weapon assets are byte-identical.
- **Attachments:** measured on the native skeletons. First person: a constant K01 offset across all five clips (spread 0), with source and native socket axes equal. Third person: `hand_r` is exact.
- **Code:** a new knife presentation component and a graph-less layered knife-body animation instance, plus knife-only hooks in the character presentation and `VSCharacter`.
- **Build:** one Editor build, 0 warnings.
- **Runs, once each:**
  - practice: **39/37/37**;
  - rifle presentation: **39/39**;
  - a rendered capture of the same practice roles, giving the first-person lifecycle evidence.
- [Evidence](../assets/knife-integration/README.md).

**Result.**
- **First-person full feature, runtime-verified:**
  - gloved hands with K01, equip, idle and locomotion;
  - slash and stab with impacts at the cue times;
  - wall and miss;
  - switch, Q, phase-cancel and death visibility;
  - firearm UI gating.
- **Remote:** cue binding is runtime-verified by log. The rendered action is **held**, because the practice smoke never points the observer at the driver. Editor views of the native third-person assembly are provided instead.

**For the producer to route.**
1. **Gameplay owner, smoke:** aim the practice observer at the driver for `Time >= 13` (`VSWeaponSmoke.cpp:346`). Optionally capture at the impacts. The exact snippet is in the evidence.
2. **Gameplay owner, `VSKnifeComponent`:** remote viewers get 2 of 3 slash Start cues; the first slash after equip arrives as Impact only. This repeated in both runs.
3. **Artists:**
   - first-person forearm/knife screen size, and the forearm across the crosshair at slash impact;
   - 781K-triangle hands with no LODs;
   - general deformation and the runtime budget remain gates;
   - crouch keeps the standing pose.

**Not run:** movement, session, map and shot-effects reruns (reused), two-Mac, packaged, four players.

**Stop.** Handoff frozen with one main-targeted PR. The user merges.
