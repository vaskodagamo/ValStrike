# Meshy pilot playtest (Urban Entry character + Site01 props)

2026-11-04 (Europe/Berlin), INT-ART-01. This build shows the Meshy **Urban Entry** character as every *other* player's third-person body and four approved Meshy props in Site01. It is a development Editor playtest, not a packaged game.

**Tested revision (INT-ART-04, R02 rifle + P02 pistol):** `c2ba8522b22d3e46776c090c353d5356f4c5f6e2` on branch `claude/int-art-04-weapon-replacement`. It is main `caf7551` (knife #108–#110, ART-25 #112), plus the frozen KNIFE-01-M1 compile repair `adb7374` (PR #114), plus the new rifle and pistol in first and third person. Later commits on the branch only add documentation. Until that PR is merged, `main` has R01/P01 and does not compile because of the KNIFE-01-M1 duplicate. The Site01 facade and light fix are in the separate INT-ART-02B PR #116 and are not part of this revision. The earlier revisions `61fc972` (phase A pistol pose) and `34e6e78` (INT-ART-01 pilot) remain valid for what they tested.

Evidence for everything below: [INT-ART-01 evidence](../assets/meshy-pilot-integration/README.md), [INT-ART-02 evidence](../assets/meshy-site01-followups/README.md) and [INT-ART-04 weapon evidence](../assets/weapon-replacement02/README.md).

## 1. Prepare the other Mac

Requirements (as in [MAC_LAN.md](MAC_LAN.md)): Apple silicon, Git with Git LFS, Python 3.9+, **Unreal Engine 5.8.4** (this Mac: changelist 58902052), full **Xcode 26.1.1** selected (build 17B100), a working Metal compiler. This uses the installed engine; Unreal itself is not built from source.

```sh
VS_CHECKOUT="$HOME/Documents/ValStrike-meshy-pilot"
GIT_LFS_SKIP_SMUDGE=1 git clone https://github.com/vaskodagamo/ValStrike.git "$VS_CHECKOUT"
cd "$VS_CHECKOUT"
git lfs install --local
git fetch origin claude/int-art-04-weapon-replacement
GIT_LFS_SKIP_SMUDGE=1 git checkout --detach c2ba8522b22d3e46776c090c353d5356f4c5f6e2
git lfs pull origin
git lfs fsck
git rev-parse HEAD
git status --short
```

`HEAD` must print the commit above and `git status` must be empty. If `ls -la Content/Art/Characters/UrbanEntry01/SK_VS_UrbanEntry01.uasset` shows a file of only ~130 bytes, it is an LFS pointer: rerun `git lfs pull origin`.

Once the PR is merged you can instead check out the merge commit on `main`; use its full SHA and the same LFS steps.

## 2. Check and build

Set the other Mac's real engine path, then:

```sh
VS_ENGINE_ROOT='/actual/path/to/UE_5.8'
mkdir -p Saved/Playtest
python3 Scripts/ue.py --engine "$VS_ENGINE_ROOT" doctor --map site01
python3 Scripts/with_ue_slot.py -- \
  python3 Scripts/ue.py --engine "$VS_ENGINE_ROOT" build --target Editor --platform Mac \
  > Saved/Playtest/editor-build.log 2>&1
```

The build must end with `Result: Succeeded` and a `Verified Editor build receipt` line. Exit 75 means another Unreal job holds this Mac's slot: wait for it to finish, do not loop.

## 3a. Solo: see the character on one Mac

The local player never sees their own body (first-person view). To see the new character alone, start one Site01 server with **two rendered windows**:

```sh
python3 Scripts/with_ue_slot.py -- \
  python3 Tests/Playtest/run_session.py --engine "$VS_ENGINE_ROOT" \
  host --map site01 --mode practice --port 7777 --clients 2
```

This exact command (with `--duration-seconds 25` on port 7861) was run on this Mac on INT-ART-01 revision `34e6e78`: server and both rendered clients connected and the session completed. Phase A and INT-ART-04 (`c2ba852`) re-verified Site01 with server + 2 rendered clients through the weapons match suite. Click into one window, move with WASD, switch weapons with 1/2, crouch with Ctrl, then look at that player from the other window. Ctrl-C in the terminal stops only this session.

## 3b. Two Macs on the LAN

Host Mac (server + one local client):

```sh
python3 Scripts/with_ue_slot.py -- \
  python3 Tests/Playtest/run_session.py --engine "$VS_ENGINE_ROOT" \
  host --map site01 --mode practice --port 7777 --clients 1
```

Other Mac (set the host's actual LAN IPv4, never `127.0.0.1`):

```sh
VS_HOST_IP='PASTE_HOST_LAN_IPV4'
python3 Scripts/with_ue_slot.py -- \
  python3 Tests/Playtest/run_session.py --engine "$VS_ENGINE_ROOT" \
  join --map site01 --address "$VS_HOST_IP:7777" --clients 1
```

Both Macs need the same commit and their own build. **This two-Mac LAN run has not been performed for this revision**; only the single-Mac server/two-client paths were verified. Use `--mode match` on the host to try the full round flow (verified locally through the weapons match suite, see evidence).

## What to look at

- The other player: Urban Entry body holding the new compact **R02** rifle in both hands, walking when moving. With the pistol (key 2), the new **P02** in a two-handed ready pose. Fire (left mouse) and reload (R): the flash comes from the new muzzle, the pistol slide kicks back, and the magazine drops.
- The other player: Urban Entry body, rifle shouldered in both hands, walking when moving; with the pistol (key 2), a two-handed pistol ready pose.
- Site01 (INT-ART-02 phase B branch only, once published): a grey concrete wall/blind-recess/wall section on the north side of the service building along the north service route, with three roof parapets above it.
- Site01: four light concrete tiles in the Gallery (around X 100, Y 950), a dark panel on the service-building wall facing the Gallery, a long crate on the service roof, and Meshy cube crates replacing the north crate and the two-high site crate stack. Walk against the replaced crates: collision is the original crate collision.
- Your own view: the new R02 and P02 in first person, with no arms yet. R02 sits slightly nearer and more central than the old rifle. No body parts block the camera.

## Known temporary limitations (pilot only)

- **Animation:** only a static rifle hold and one forward rifle walk exist. Every movement direction plays the forward walk; idle↔walk switches without blending (visible pop). Above about 260 cm/s the walk is sped up to 1.8× and the feet then slide. No jump, fire, reload, death or crouch animation.
- **Crouch:** the body keeps the standing pose. The crouched hit zones (112 cm capsule) are lower than the visible head; aim by the gameplay rules, not the visual. No hit-zone or damage value was changed.
- **Extended arms/rifle:** in the hold pose the forearms and rifle extend forward beyond the 25 cm-deep body hit box; shots at them miss.
- **Death:** a dead player's body disappears (no death animation); it returns on respawn/round reset.
- **Pistol (remote):** held two-handed in a stationary ready pose (INT-ART-02). While moving with the pistol the body keeps that pose and glides; there is no pistol walk. Glove/finger contact is pilot quality up close.
- **New weapons (INT-ART-04):** R02 and P02 replace R01 and P01 at their source sizes. On the remote rifle, the stock reaches the cheek with no shoulder seat, and the fingers are not retuned for R02 (ART-17's open fit items). Each weapon is about 460K triangles with no LODs: pilot assets, not profiled. First person has no hands yet. In this revision, the development weapon checks `pistol_assets` and `attached_assets` still expect the old asset names and fail until their owners update them; gameplay is unaffected.
- **Character quality:** generated face/hair, cloth folds and finger contact are pilot quality, not close-up/hero quality.
- **Lighting message:** Site01 shows an orange on-screen "multiple directional lights" development warning on the INT-ART-01 and phase A revisions; INT-ART-02 phase B removes it (same sun, no visual change).
- Not verified: two-Mac LAN, packaged builds, Windows/Linux, four players, performance.
