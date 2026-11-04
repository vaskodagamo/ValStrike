# Meshy AI asset inbox

Drop your downloaded files here. Keep the original ZIPs; extracted FBX files with their complete texture folders are also welcome. One named subfolder per asset helps, but you can use `unsorted/` if that is easier.

```text
meshy_ai_assets/
  world_objects/   # Walls, crates, doors, roofs, paving, stairs, other props
    ConcreteWall/  # Example: original ZIP or FBX + texture folder
  characters/     # Rigged character + textures; original source if available
    UrbanEntry/
  animations/     # Clip exports for that character, retaining original names
    UrbanEntry/
  unsorted/       # Downloads you have not categorized
```

The example asset subfolders are suggestions; the four category folders already exist. Include available generation/reference details and source/license information with the download. You do not need to fill out the register or rename texture files. Tell the coordinator **“assets uploaded”** when the batch is complete so partially copied downloads are not treated as ready.

**This is a local inbox, not cloud storage.** Original downloads are Git-ignored and will not appear on another Mac after pulling. Keep your original backups. Reviewed source assets and final Unreal content will be versioned separately through Git LFS. The scaffold, rules and [asset register](ASSET_REGISTER.csv) are tracked.

## Artist pickup

The coordinator inventories uploads, assigns an asset ID and records category, exact relative input path, SHA-256 (or inventory-file link), source/provenance, owner, task/worktree, status and handoff. No files have been reviewed merely because they exist here. Register states: Received → Assigned → Source ready → Art reviewed → Imported → Runtime verified; merge status is recorded separately.

Each artist receives a bounded task and its own worktree. Read the **canonical inbox at `/Users/webproduktion3/Documents/ValStrike/meshy_ai_assets`**; ignored uploads do not propagate into other worktrees. Copy assigned inputs into task-owned source space before extracting or modifying. Keep the untouched originals here. Do not let multiple artists edit the same mesh, skeleton, animation set, material or map.

Environment cleanup covers real dimensions, flat modular connection faces, pivots, shading/topology, UVs, texture/material consistency and collision intent. First review one wall/corner, a crate and ground tile before processing the full kit. Preserve the approved [industrial palette](../Art/References/Environment/IndustrialV2/README.md); [PR #51](https://github.com/vaskodagamo/ValStrike/pull/51) supplies isolated modular references and target envelopes. Existing map dimensions override reference proposals.

Character cleanup starts by checking the rig and matching animation skeletons, scale, materials and representative clip playback. Keep character and animation inputs together by name. Preserve original rigging; first-person arms and third-person animation are separate presentation tasks.

## Existing-map integration

Target: **Site01**, `/Game/Maps/Site01/VS_Site01` ([map handoff](../docs/maps/site01/HANDOFF.md)). One assigned native/map integration owner places the reviewed assets. Preserve routes, gameplay cover, player clearance, spawn/site/recovery actors and tags. This is an art replacement of the existing tactical map, not a new layout or a change to VS_Graybox/startup settings.

The integration owner schedules Unreal imports/save/reopen and representative in-map/collision checks through the shared engine slot. Art contributors hand off source/export/preview work without independently building the game. No automatic folder watcher is installed and no agent is dispatched by a file appearing here. See [ART-10](../docs/tasks/art/ART-10-meshy-intake.md) and [team workflow](../docs/TEAM_WORKFLOW.md).
