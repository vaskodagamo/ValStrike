# ART-15 — frozen character pilot handoff

2026-11-04 (Europe/Berlin). **Implementation ready; partial pilot art approval;
native/runtime verification pending.** Sole writer: assigned Codex character chat,
`codex/art15-character-pilot`, worktree
`/Users/webproduktion3/Documents/ValStrike/.parallel-worktrees/art15-character-pilot`.
Prepared HEAD verified as `51d19eee81cfe03e0637730c45e20d1236ce5bae`.

- **Publication:** [PR #65](https://github.com/vaskodagamo/ValStrike/pull/65), targeting
  main. Frozen asset commit `45deb700a96b2aad3084808b1c7809d4589fdbd7`;
  initial readiness commit `d727f6d6e2af088f5a5d114b7a2d52320980fc22`.
  **This handoff commit contains the final readiness record**, including subsequent
  user weapon-scale steering; consume this commit plus its asset ancestry, not the
  earlier gate alone. The original ART-13 source and historical handoff are included
  unchanged from the coordinator-prepared checkout. User merges; no merge performed.
- **Approved subset:** unchanged character and three textures, new static R01 hold
  and forward in-place rifle walk, optional original unarmed Walking fallback.
  [Exact allowlist/hashes and clip verdicts](../assets/urban-entry01/pilot-readiness.json).
  The hold uses the supplied armed pose with neutral standing legs and bounded arm/
  head pose adjustments; walk retains original pelvis/leg motion. No skeleton,
  character scale, weights, geometry, original exports or first-person changes.
- **Evidence:** [source review/socket instructions](../assets/urban-entry01/PILOT.md),
  [measured checks](../assets/urban-entry01/pilot-source-validation.json),
  [actual PBR fit](../assets/urban-entry01/Previews/Pilot/hold_pbr.png),
  [grip](../assets/urban-entry01/Previews/Pilot/hold_hands.png),
  [support hand](../assets/urban-entry01/Previews/Pilot/hold_support.png),
  [5 m](../assets/urban-entry01/Previews/Pilot/distance_5m.png) /
  [10 m](../assets/urban-entry01/Previews/Pilot/distance_10m.png) source views.
- **Checks actually run:** Blender 5.2.2 LTS, two threads; two changed FBXs reimported
  onto actual character export; skeleton/rest/FPS/ranges and start/middle/end
  deformation passed (<.00017 cm sampled joint/vertex error). Saved pilot reopened;
  original rig/vertices/weights/actions retained, textures reload, fixed weapon/hand
  relation checked. Original seven FBXs and three textures match prior SHA-256
  evidence. Changed pose/gait frames and distance views visually reviewed. Forty
  branch-delivery LFS pointers match local bytes; LFS upload completed; JSON/script
  parsing, local links, scope review and diff checks passed. Reused ART-13 full
  normalization/original clip evidence. Initial sandbox launch and temporary-scene
  unit-check failures are disclosed in the guide and resolved before acceptance.
- **Held / limitations:** R01 is temporary: user finds it too large. Measured
  assembled envelope is 86 × 9.04 × 32.2 cm beside this 170 cm character; bulky
  receiver/handguard and 4.8 cm grip width remain, with support contact behind the
  generic marker and no tight shoulder seat. No character/hand scaling compensation.
  ART-17/18 own separate Rifle02/Pistol02 replacements and socket recipes; later
  native replacement is separately scheduled. No fitted P01 pose, crouch/directional
  set, reload/aim/fire/death coverage. All original traveling clips and Vault stay
  out; Running held. Foot sliding, stiff torso, topology/cloth debt, team readability
  and 170 cm visual versus capsule/hit-zone alignment remain explicit native checks.
- **Next owner / exact action:** INT-ART-01 takes this frozen branch revision and
  verifies readiness hashes/ancestry. After its assigned UE slot release, import
  **only** the allowlisted FBXs/textures, root motion off, retained pelvis root;
  derive the native `hand_r` socket from the supplied frame-1 matrices after verifying
  FBX axes. Tune idle/walk from existing replicated movement, preserve authority,
  capsule/camera/hit zones and first-person presentation. Apply documented P01 and
  directional/crouch limitations, then verify actual native reopen/materials, weapon
  switching, death/respawn and server/two-client presentation. No Unreal launch,
  account check, paid Meshy stage, new suite or central-document edit occurred here.

Branch frozen after publication. Further source changes require a new revision and
invalidate only affected evidence. No further ART-15 implementation is active.
