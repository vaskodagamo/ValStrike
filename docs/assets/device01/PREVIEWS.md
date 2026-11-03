# D01 Relay — actual native status comparison

Unreal 5.8.4 editor compositions, not gameplay. One 30 × 18 × 10 cm mesh at scale 1; only Status slot 2 changes. The three columns use matched cameras/lighting and fixed exposure. The glyph changes along with its color.

| Idle — ring/bar | Armed — triangle/exclamation | Defused — square/check |
| --- | --- | --- |
| ![Idle neutral](Previews/D01_Unreal_Studio_Idle.png) | ![Armed neutral](Previews/D01_Unreal_Studio_Armed.png) | ![Defused neutral](Previews/D01_Unreal_Studio_Defused.png) |
| ![Idle dim](Previews/D01_Unreal_Dim_Idle.png) | ![Armed dim](Previews/D01_Unreal_Dim_Armed.png) | ![Defused dim](Previews/D01_Unreal_Dim_Defused.png) |

Neutral: sun 3 / sky 1. Dim: sun 0.75 / sky 0.3. The status symbol has constant 0.15 glyph-only emission; bloom is disabled. The dim shell loses fine recess detail but trim and glyphs retain identity. The asymmetric checkmark confirms UV orientation after FBX Y reflection.

| Carry composition — no hands/player | Player-eye ground inspection — 160 cm eye height |
| --- | --- |
| ![Carry composition](Previews/D01_Unreal_Carry_Idle.png) | ![Player-eye composition](Previews/D01_Unreal_PlayerEye_Armed.png) |

Carry places the socket at (42,8,148) cm beneath a camera at (0,0,160); body yaw is 90°, scale remains 1. Low feet are cropped by the frame. This is not verified runtime hand alignment. Ground inspection views the real-sized device from 160 cm up and 100 cm away horizontally. The warning triangle remains visible, while its inner mark is small. Runtime HUD/interaction cues must supplement the physical status surface.

[Capture settings, transforms and hashes](unreal-inspection-report.json) · [Fixed integration contract](MANIFEST.md) · [Reproduce](IMPORT.md)
