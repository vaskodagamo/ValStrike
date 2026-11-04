# Network emulation profiles (MOVE-03, GUN-04)

`Scripts/ue.py server` and `client` accept optional packet-simulation flags so impaired-network runs are repeatable. With no flag the launch argv is unchanged.

```sh
python3 Scripts/ue.py server --net-profile typical
python3 Scripts/ue.py client --net-profile typical
python3 Scripts/ue.py client --net-profile poor --pkt-loss 5   # explicit values override the profile
python3 Scripts/ue.py --dry-run client --net-profile poor      # inspect the composed argv
```

## Profiles

Values are per process and apply to **outgoing** packets only. Give the server and every client the same profile; the added round trip is then about twice the lag.

| Profile | `-PktLag` (ms, one way) | `-PktLagVariance` (ms, ±) | `-PktLoss` (% outgoing) | Expected added RTT |
| --- | --- | --- | --- | --- |
| `lan` | 0 | 0 | 0 | 0 (explicit baseline; overrides any ini settings) |
| `good` | 20 | 5 | 0 | ~40 ms |
| `typical` | 40 | 10 | 1 | ~80 ms, ~2% round-trip loss |
| `poor` | 90 | 30 | 3 | ~180 ms, ~6% round-trip loss |

Explicit flags: `--pkt-lag 0-1000`, `--pkt-lag-variance 0-500` (must not exceed the lag), `--pkt-loss 0-100`. Any of them alone emits all three settings, using the profile (or `lan` zeros) for the unspecified ones. Negative values, fractions and loss above 100 are rejected before launch.

## Engine semantics (UE 5.8.4, pinned source)

Paths are under `Engine/Source/Runtime/Engine/`.

- `FPacketSimulationSettings` fields `PktLoss`, `PktLag`, `PktLagVariance`: `Classes/Engine/NetDriver.h:471,519,528`.
- Whole feature is guarded by `DO_ENABLE_NET_TEST`, defined as `!(UE_BUILD_SHIPPING)`: `Classes/Engine/NetDriver.h:450`. **Packet simulation is compiled out of Shipping.** It works in these Development editor launches and Development/Test packaged builds, never in a Shipping build.
- Each net driver reads ini then command line, command line winning: `Private/NetDriver.cpp:786-788` (`UNetDriver::InitPacketSimulationSettings`). Parsing uses `FParse::Value(Cmd, "PktLag=")` etc. and logs `LogNet: PktLag set to N` for each value: `Private/Net/NetEmulationHelper.cpp:534-626`.
- `PktLoss` is clamped to 0-100: `Private/Net/NetEmulationHelper.cpp:472`.
- Lag is `PktLag + uniform[-PktLagVariance, +PktLagVariance]` per outgoing packet, and only when `PktLag` is non-zero: `Private/NetConnection.cpp:2666-2673`. Delayed packets are still released in order (`Private/NetConnection.cpp:5184-5188`), so variance adds jitter without reordering.
- Loss drops outgoing packets with probability `PktLoss`%: `Private/NetConnection.cpp:2709-2714`.
- The same values can be changed live from the console with `NetEmulation.PktLag`, `NetEmulation.PktLagVariance`, `NetEmulation.PktLoss` and `NetEmulation.Off`: `Private/Net/NetEmulationHelper.cpp:88,341-365`.

## MOVE-03 measurement procedure (integration-owned)

Run on one Mac first (loopback plus emulation), then repeat `typical` on the two-machine LAN when that run is scheduled. Use `Scripts/with_ue_slot.py` as for any runtime check.

1. Start the server and one or two clients with the same `--net-profile`. Run `lan` first as the baseline, then `good`, `typical` and `poor`.
2. Confirm the emulation is active: each process log (`Saved/Logs/VS_Server_<port>.log`, `VS_Client_<pid>.log`) contains `LogNet: PktLag set to …`, `PktLagVariance set to …` and `PktLoss set to …` with the profile values. A missing line means the run is invalid.
3. On a client, open the console and enable `stat net` and `p.NetShowCorrections 1` (Development-only cheat cvar, `Private/Components/CharacterMovementComponent.cpp:422`). With it enabled the client logs `LogNetPlayerMovement: Warning: *** Client: Error for …` per correction (`:11423`).
4. Per profile, run the existing movement scenario set (walk, sprint/slow walk, crouch, jump/land, wall slide, stairs, cover corners) for at least 60 s of continuous movement, plus 20 rapid direction reversals and 10 jumps onto/off cover.
5. Record, per profile and client:
   - **RTT**: `stat net` Ping min/avg/max over the run (expected ≈ added RTT in the table plus loopback).
   - **Loss**: `stat net` In/Out packet loss %.
   - **Corrections**: count of `*** Client: Error` lines and the largest error distance; note which scenario triggered them. Visible rubber-banding counts even if the log is quiet.
   - **Stuck**: any position the player cannot leave by normal input, or repeated corrections at the same spot.
   - **Divergence**: client and server (or second client's view) disagreeing on position, stance or bounds after movement stops for 2 s.
6. Acceptance is the MOVE-03 backlog condition: measured RTT/jitter/loss and corrections recorded; no repeatable stuck or divergent movement on supported conditions. Treat `lan`, `good` and `typical` as supported conditions; `poor` results are recorded as limits, not release blockers, unless the lead decides otherwise.
7. Write one evidence entry under `docs/playtest/evidence/` with the candidate SHA, exact commands, platform and the table of results. Note that this is Development-editor evidence only.

## Limits

- Emulation is per process and outgoing only; it does not model asymmetric links, bandwidth caps or burst loss. OS-level shaping (dummynet/pfctl) is out of scope.
- `-PktLag*` switches are also read by any other net driver in the process (for example a beacon driver); ValStrike currently has only the game net driver.
- No ValStrike runner uses these flags yet; adopting them in `Scripts/test_*.py` is a separate task.
