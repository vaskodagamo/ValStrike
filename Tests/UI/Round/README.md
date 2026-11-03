# Round HUD presentation checks

Build and run from the assigned checkout. **Wrap each whole foreground Unreal command in the shared engine-slot helper.** Exit 75 means another workstream owns the slot; continue lightweight work and retry later. Do not launch a second editor or bypass the lock.

```sh
python3 Scripts/with_ue_slot.py -- python3 Scripts/ue.py --engine /Users/webproduktion3/UE_5.8/UE_5.8 build --target Editor --platform Mac
python3 Scripts/with_ue_slot.py -- python3 Tests/UI/Round/preview_round_hud.py --engine /Users/webproduktion3/UE_5.8/UE_5.8
```

Run the native presentation-model automation (server-time boundaries, availability,
stable team identity, result precedence and interaction cancellation):

```sh
python3 Scripts/with_ue_slot.py -- /Users/webproduktion3/UE_5.8/UE_5.8/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor "$PWD/ValStrike.uproject" -nullrhi -nosound -unattended '-ExecCmds=Automation RunTests ValStrike.UI.RoundPresentation' '-TestExit=Automation Test Queue Empty' "-abslog=$PWD/Saved/Tests/UI/Round/presentation-automation.log"
```

Require `Result={Success}` for `ValStrike.UI.RoundPresentation`, not just a process
exit or the engine's unrelated startup automation messages. Preserve the existing
combat/menu integration with this separate regression on the HUD-assigned ports:

```sh
python3 Scripts/with_ue_slot.py -- python3 Tests/UI/test_session.py --engine /Users/webproduktion3/UE_5.8/UE_5.8 --rendered --port 7841 --failure-port 7842
```

The runner renders 960×540 and 1280×720 sequentially, with one owned development client at a time. `--resolutions 960x540` selects a single size. `--map site01` uses the committed tactical map; the default graybox keeps the regression environment. No network ports are used. Outputs are ignored under `Saved/Tests/UI/Round/<run-id>/`: exact commands, engine version, module/source hashes, native log records, native PNG dimensions and screenshots. The runner closes only its own processes and requires a complete fixture sequence and clean client exit.

`-VSRoundHUDPreview` is an explicit local **presentation test**. Its implementation is excluded from Shipping/Test and it refuses non-standalone worlds. The HUD calls the real controller provider first, then the opt-in fixture helper may substitute a labeled snapshot. Normal launches never substitute data. Frozen fixture timestamps only test formatting; they do not simulate an authoritative clock.

Eighteen fixtures cover unavailable/waiting/preparation, carrier/planting, planted/defusing, dropped/retrieval, placement pending, resolved winner, role swap, round draw, complete winner/draw with rematch eligibility, long provider messages, spectator **with an actually unpossessed pawn**, unavailable state after pawn removal, the existing Escape menu without a pawn, and a defused/lost round after returning from that menu. The HUD must call the helper before menu/pawn early returns so capture and cleanup can progress.

Inspect the PNGs for readable unclipped text, stable team identity after role swap, timer/progress formatting, no fabricated match panel when unavailable, no stale pawn fields in spectator views and an intact session menu. The fixture label must remain prominent wherever fixture match state is drawn. Export completion and fixture logs are not evidence of a replicated match; real Match controller acceptance follows integration. Preserve the existing integrated session/combat regression as a separate check, using HUD-reserved ports 7841–7844.
