# Native weapon finish comparison

Actual Unreal 5.8.4 meshes. Identical cameras, lighting and fixed exposure across each weapon's three variants. Pistol framing is closer than rifle framing; rows are not a common size scale. These are editor compositions, not concepts or gameplay captures.

| Charcoal / Crimson — selected | Midnight / Ice-blue | Charcoal / Copper-ember |
| --- | --- | --- |
| ![Rifle crimson](../rifle01/Previews/VS_Rifle01_Unreal_Studio.png) | ![Rifle ice](../rifle01/Previews/VS_Rifle01_Unreal_Studio_ice.png) | ![Rifle ember](../rifle01/Previews/VS_Rifle01_Unreal_Studio_ember.png) |
| ![Pistol crimson](Previews/VS_Pistol01_Unreal_Studio.png) | ![Pistol ice](Previews/VS_Pistol01_Unreal_Studio_ice.png) | ![Pistol ember](Previews/VS_Pistol01_Unreal_Studio_ember.png) |

Crimson provides the strongest restrained contrast against cool steel and makes both weapons feel cohesive. Ice-blue is colder and cleaner; copper-ember reads warmer. The Crimson material set is saved in source/FBX/native assets. The alternatives are transient native material studies generated from the same shared palette; they do not add duplicated production materials.

[Compact HTML layout](Previews/Weapon_Finish_Comparison.html) · [Capture settings and attachment evidence](unreal-inspection-report.json)

## Default first-person readability

| Weapon | Neutral — sun 3 / sky 1 | Dim — sun 0.75 / sky 0.3 |
| --- | --- | --- |
| R01 | ![Rifle neutral](../rifle01/Previews/VS_Rifle01_Unreal_FirstPerson.png) | ![Rifle dim](../rifle01/Previews/VS_Rifle01_Unreal_FirstPerson_Dim.png) |
| P01 | ![Pistol neutral](Previews/VS_Pistol01_Unreal_FirstPerson.png) | ![Pistol dim](Previews/VS_Pistol01_Unreal_FirstPerson_Dim.png) |

First-person compositions use 85° horizontal FOV with no arms, HUD, animation or runtime integration. The shared default is selected from rendered evidence; final camera/hand fit remains gameplay integration work.
