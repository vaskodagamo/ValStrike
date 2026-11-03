# Apple-silicon Mac source playtest

This checklist prepares a **development Editor server and two separate Editor clients**, including a client on another Mac. It builds the ValStrike project from source using the installed Unreal engine. It does not build Unreal from source, cook/package the game, or prove packaged-server, Windows or Linux support.

Use the exact **coordinator-published full commit** on every Mac. A branch named `main`, an old screenshot or another machine's compiled module is not a revision identifier. The coordinator must publish a reachable repository URL, full commit, selected map/mode and any remaining limitations before the test.

**Current gate:** match runtime is a parallel integration dependency. At this document's drafting checkpoint, `VSMatchGameMode` is unavailable in this base; no successful match launch is claimed. `practice` is the available baseline. The coordinator must complete the same-source local rehearsal and record its result below before asking the second Mac to validate LAN behavior.

## 1. Prepare each Mac and checkout

Required: Apple silicon, Git with Git LFS, Python 3.9 or newer, **Unreal Engine 5.8.4**, full **Xcode 26.1.1** selected, and a working Metal compiler. The existing development machine uses UE changelist 58902052 and Xcode build 17B100. Verify the other Mac instead of assuming its installed Unreal version matches. The project helper enforces the patch/toolchain pins; keep both machines aligned.

Install missing tools through their official distribution channels. Open Xcode and finish any first-launch/license/component setup it requests. Select the installed full Xcode in Xcode's Locations settings; Command Line Tools alone are insufficient. No engine/source account setup is required for this installed-engine Editor test. Coordinate a free engine slot; do not close another person's editor.

Replace the first three values with the coordinator's published URL/commit and an unused checkout directory. These commands intentionally clone without initial LFS smudging, select the exact revision, then retrieve its real binary assets:

```sh
VS_REPO_URL='PASTE_COORDINATOR_REPOSITORY_URL'
VS_PLAYTEST_REV='PASTE_FULL_40_CHARACTER_COMMIT'
VS_CHECKOUT="$HOME/Documents/ValStrike-playtest"

GIT_LFS_SKIP_SMUDGE=1 git clone "$VS_REPO_URL" "$VS_CHECKOUT"
cd "$VS_CHECKOUT"
git lfs install --local
git fetch origin
GIT_LFS_SKIP_SMUDGE=1 git checkout --detach "$VS_PLAYTEST_REV"
git lfs pull origin
git lfs fsck
git rev-parse HEAD
git status --short
```

`HEAD` must equal the published commit and the tracked working tree must be clean. A tiny text LFS pointer is not a usable `.umap`, `.uasset` or source-art file. If retrieval fails, retain its error and resolve it before building. Do not regenerate maps to hide a missing asset. Do not copy `Binaries`, `Intermediate`, `Saved` or live generated outputs from another checkout/Mac.

Set the engine root to the other Mac's actual installation directory; it need not match this machine's path. Run from the new checkout root:

```sh
VS_ENGINE_ROOT='/actual/path/to/UE_5.8'
mkdir -p Saved/Playtest

uname -m
sw_vers
xcode-select -p
xcodebuild -version
xcrun --sdk macosx --show-sdk-version
xcrun --sdk macosx metal --version
git lfs version

python3 Scripts/ue.py --engine "$VS_ENGINE_ROOT" doctor --map site01
python3 Scripts/check_playtest.py --engine "$VS_ENGINE_ROOT" --map site01 \
  > Saved/Playtest/preflight.txt 2>&1
```

Expect `arm64`, the pinned engine/Xcode versions, and an executing Metal compiler. Merely locating `metal` can find an unusable stub. The read-only playtest check records revision/tool/asset information and hashes; review its exit status and complete output. A clean source preflight is not a compiled build or connection test. Save separate outputs from both Macs for comparison.

## 2. Build locally

Build the native project Editor target on each Mac. The shared slot wraps the complete foreground build and the helper retains Unreal's existing `-WaitMutex` behavior:

```sh
python3 Scripts/with_ue_slot.py -- \
  python3 Scripts/ue.py --engine "$VS_ENGINE_ROOT" build --target Editor --platform Mac \
  > Saved/Playtest/editor-build.log 2>&1
```

Require a successful exit and inspect the log before launch. If the slot reports busy (exit 75), finish lightweight preparation and retry after the current run ends; do not bypass or rapidly poll the lock. The slot coordinates worktrees of one repository on one machine. Each Mac still builds its own outputs. A stale module left from an earlier revision is not a substitute for this build.

**The helper's build receipt is mandatory for sessions and native preflight.** A successful Mac Editor build through `Scripts/ue.py` writes `Saved/Playtest/editor-build.json`. It snapshots `Source/`, `Config/`, `ValStrike.uproject` and the selected engine identity, confirms those inputs did not change during the build, and binds them to the resulting Editor module and module-manifest hashes. The session runner and native preflight reject a missing, stale or mismatched receipt.

If you built in Xcode or another tool, run the helper command above afterward—even if Unreal reports the target up to date—to create the current receipt. Rerun it after source, configuration, project or engine changes. Do not edit/copy receipts or compiled outputs between checkouts/Macs; each Mac creates its own. The lightweight source preflight can pass before this first build and does not waive the receipt requirement.

An optional **native** preflight launches the engine and therefore also needs the slot. Run it after building; the default check above remains a lightweight read-only source/LFS/tool check:

```sh
python3 Scripts/with_ue_slot.py -- \
  python3 Scripts/check_playtest.py --engine "$VS_ENGINE_ROOT" \
  --map site01 --mode practice --native > Saved/Playtest/native-preflight.txt 2>&1
```

Use `--mode match --native` only when checking the coordinator's integrated match revision. An unavailable reflected class must fail clearly; a Python source file or class name alone is not proof that the native mode can load.

## 3. Select map and server mode explicitly

| Choice | Meaning |
| --- | --- |
| `--map graybox` | Existing regression arena `/Game/Maps/VS_Graybox`; preserved default |
| `--map site01` | Switchyard `/Game/Maps/Site01/VS_Site01` |
| `server --mode practice` | Fixed `/Script/ValStrike.VSGameMode` override, preserving practice even if map WorldSettings differ; no objective match claim |
| `server --mode match` | Fixed `/Script/ValStrike.VSMatchGameMode` override; requires the integrated, rebuilt match class |

Map selection does not rewrite map assets or project defaults. Clients join the server's selected map/mode using only a validated `host:port`. Do not append URL options or console commands to addresses. For a match test, the coordinator must publish an integrated revision that actually contains the match adapter; an unavailable-class error is a dependency failure, not a passing fallback to practice.

Example command previews, which do not launch a process:

```sh
python3 Scripts/ue.py --engine "$VS_ENGINE_ROOT" --dry-run \
  server --map site01 --mode practice --port 7777
python3 Scripts/ue.py --engine "$VS_ENGINE_ROOT" --dry-run \
  client --address 192.168.1.42:7777
```

`192.168.1.42` above is illustrative only. Replace it with the host's actual current LAN IPv4; do not copy it blindly.

## 4. One server, two clients

`Tests/Playtest/run_session.py` owns the server/local clients on the host, and the client process on the remote Mac. Both Macs need their own current helper build receipt. Wrap each complete foreground runner once with `Scripts/with_ue_slot.py`, including the remote join runner. Do not wrap each nested process separately: the first process would hold the slot and block its peers. Do not detach/background child processes outside the runner's lifetime. Match-host startup probes the fixed reflected class before launching the session; the current base is expected to reject unavailable match mode.

Use this topology for the first real LAN session:

| Machine | Processes | Address used |
| --- | --- | --- |
| Host Mac | One separate development server, one client | Host client may use `127.0.0.1:7777` |
| Other Mac | One separate client | **Host's actual LAN IPv4:7777**, never loopback |

On the **host Mac**, from its checked-out project root:

```sh
python3 Scripts/with_ue_slot.py -- \
  python3 Tests/Playtest/run_session.py --engine "$VS_ENGINE_ROOT" \
  host --map site01 --mode practice --port 7777 --clients 1
```

On the **other Mac**, set the host's actual LAN address first:

```sh
VS_HOST_IP='PASTE_HOST_LAN_IPV4'
python3 Scripts/with_ue_slot.py -- \
  python3 Tests/Playtest/run_session.py --engine "$VS_ENGINE_ROOT" \
  join --map site01 --address "$VS_HOST_IP:7777" --clients 1
```

For an integrated match, change only the host's `--mode practice` to `--mode match`, using the coordinator's approved revision. The join-side `--map` selects its asset preflight and expected travel; the server remains authoritative over travel. Clients do not inject game-mode URL options.

For the coordinator's prior **local rehearsal**, run the same server plus two distinct headless clients on one Mac through one bounded runner:

```sh
python3 Scripts/with_ue_slot.py -- \
  python3 Tests/Playtest/run_session.py --engine "$VS_ENGINE_ROOT" \
  host --map site01 --mode practice --port 7861 --clients 2 \
  --duration-seconds 25 --headless-clients
```

This can provide local process/join evidence, not rendered gameplay or traffic across the LAN. Shared development tooling reserves ports 7861–7864 for this stream; use a free reserved port for rehearsal and record it. The user LAN examples use UDP 7777. Only one server may occupy the selected port.

Require server map/mode/listening evidence before the remote client joins. Confirm both client joins in the server log, then verify each rendered client sees the other move. Record the actual server and both client log paths printed by the runner/helper. Keep the runner foreground throughout; stop it with Ctrl-C or let its configured duration finish, and confirm its own child processes exit. Never use a global Unreal kill command.

## 5. LAN and permission checklist

1. Put both Macs on the same trusted, non-isolated LAN. Guest Wi-Fi/client isolation, separate VLANs or an active VPN can prevent peer access even when both machines have internet. Record those conditions; ask the network owner/coordinator to resolve isolation rather than changing router/firewall configuration through a script.
2. Read the **host's active Ethernet/Wi-Fi IPv4** from System Settings → Network → the active service's details. Do not assume the interface is `en0`. Give the other Mac that address plus the selected port. `127.0.0.1`, `localhost` and `0.0.0.0` are not the host address for the remote Mac.
3. If macOS asks whether the deliberately launched Unreal Editor application may receive incoming connections, allow that trusted test application on this private LAN. Existing per-app decisions can be inspected under System Settings → Network → Firewall. Do not disable the firewall, open all incoming services or add port-forwarding for this LAN test. [Apple's firewall guide](https://support.apple.com/en-gb/guide/mac-help/mh34041/mac)
4. Local Network privacy is a separate permission. If prompted for the application involved in this launch, review/allow its local-network access; inspect the applicable entry under System Settings → Privacy & Security → Local Network if previously denied. Managed restrictions may require the device administrator. No permission is modified by these scripts. [Apple's Privacy & Security settings](https://support.apple.com/en-ng/guide/mac-help/mchl211c911f/mac)

## 6. If joining fails

Record the exact time and error before retrying. Check, in order:

1. Server still running, correct map/mode loaded, and correct port listening; no unavailable-match-class or map/LFS error.
2. Both Macs use the exact published commit, native build and pinned engine; compare their preflight output. For an absent/stale receipt error, run the slot-wrapped Editor build helper in the affected checkout rather than copying a receipt or bypassing validation.
3. Remote address is the host's active LAN IPv4 and matching port. Recheck after Wi-Fi/Ethernet changes.
4. Host-local client can join. If it cannot, investigate server/build/map startup first. A local success with a remote failure narrows the problem but does not alone prove a firewall cause.
5. Same LAN/isolation/VPN conditions and the two macOS permissions above. Optional `ping` can provide reachability evidence, but neither ping success nor failure establishes Unreal UDP connectivity. A TCP-only port probe does not test UDP 7777.

Collect `Saved/Playtest/preflight.txt`, `editor-build.json`, build output, native-preflight output if run, the runner report/commands, the host server log and both client logs from the same attempt. Individual helper logs normally use `Saved/Logs/VS_Server_<port>.log` and `VS_Client_<helper-pid>.log`; use the exact paths printed for this run. Capture the error screen and complete a [bug report](BUG_REPORT.md). Do not keep retrying an unchanged failure without preserving evidence.

## 7. Playtest checks and evidence boundary

First verify join, mouse direction, WASD/walk/crouch/jump, remote-player movement, rifle fire/reload/health/death, Escape release of held input, disconnect and rejoin. Mark each observed result; earlier automated results from another revision do not fill this checklist.

Only after the match adapter/HUD are integrated, test role-correct spawns, preparation, hold E plant/defuse, E pickup, G drop, cancellation on release/menu/death, each win condition, fresh round reset, side swap after three rounds, six-round result and Enter rematch. Record spectator/late-join/disconnect behavior. These are acceptance targets, not claims of available behavior at this checkpoint. A two-client session is 1v1 development evidence; the full 2v2 acceptance requires four clients and its own record.

| Rehearsal/publication record — coordinator fills | Result |
| --- | --- |
| Published repository URL and full commit | Pending |
| Fresh source/LFS checkout, preflight and build | Passed locally at `cb891e6`: 70 LFS files, 31 native Content hashes, clean independent checkout, Editor build 30.70s |
| One local server/two-client result, map/mode/port | Passed at `cb891e6`: Site01 / practice / UDP7861; two headless clients, 25s after connection. Connection evidence only |
| Exact session runner CLI and cleanup result | Local rehearsal CLI above; server + both clients exited0 after owned cleanup; their process groups were absent |
| Match class integrated and successful match launch | Pending parallel dependency; actual native unavailable-class rejection verified at `cb891e6`, with no fallback server |
| Other Mac model/macOS/toolchain/preflight/build | Pending real second-Mac evidence |
| Actual two-Mac LAN result and logs | Pending user session |
| Human gameplay findings and bug reports | Pending user session |

See [source rehearsal evidence](SOURCE_REHEARSAL.md) for exact scope and artifacts. The tested tooling commit is local; it is not the coordinator-published integrated playtest revision.

Keep packaged builds, performance, network impairment and cross-platform results separate. This checklist does not complete those gates.
