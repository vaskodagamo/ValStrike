# P02 corrected source import and socket recipe — verification pending

Corrected full-detail authoring/reference source: 461,412 triangles. Runtime LOD budgeting/optimization is deferred. Only a later assigned integration owner imports these sources. P02 is a new contract; do not overwrite P01 or reuse its numeric attachment offsets.

## Units, assets and assembly

Source scene: centimeters (`scale_length=0.01`), +X forward, +Z up. FBX exporter: forward `-Y`, up `Z`, global scale 1, apply unit scale, `FBX_SCALE_UNITS`, no animation. Blender round-trip preserves the centimeter envelope. In Unreal use import scale **1**, unit/scene conversion, no additional compensating actor scale; inspect the imported bounds against **21.000 × 3.352 × 13.833 cm** before wiring. Axis/handedness conversion still needs native confirmation.

Import Body, Slide and Magazine FBXs separately as static visual meshes, Combine Meshes off, auto collision off. Set NoCollision and disable overlap generation. No physics/gameplay geometry is supplied. Import the authored normals and tangents; do not silently recompute them. Custom normals are essential to preserving the original surface. The Blender FBX round-trip sampled-normal check passed, but native shading must still be inspected. No Nanite or lightmap policy is prescribed by this source task.

**All three mesh origins are the shared Grip datum**, with identity translation/rotation/scale at rest. Parent slide and magazine directly to the weapon root at identity. Their mesh origins are not relocated to the named anchors. If attaching a part to its named socket instead, apply the negative socket translation to the mesh child: Slide `(0,0,-3.9)`, Magazine `(2.5,0,1.0)` cm, zero rotation and unit scale. Applying both socket position and identity mesh placement would double-offset it.

| Named anchor on Body | Location X,Y,Z cm | Rotation X,Y,Z degrees | Scale |
| --- | --- | --- | --- |
| Grip | 0, 0, 0 | 0, 0, 0 | 1, 1, 1 |
| Muzzle | 15.5, 0, 5.0 | 0, 0, 0 | 1, 1, 1 |
| Slide | 0, 0, 3.9 | 0, 0, 0 | 1, 1, 1 |
| Magazine | -2.5, 0, -1.0 | 0, 0, 0 | 1, 1, 1 |

Muzzle +X is the intended visual forward. Anchor coordinates are source art data, not gameplay firing rules. Later slide motion is along local X; later magazine motion is down the grip. No travel/timing or functional mechanism is specified.

## Actual character alignment used in evidence

Frozen ART-13 supplied action `A_UE01_Walk_Backward_with_Gun`, frame 1. Weapon world translation `(52,-34.3,112)` cm; XYZ rotation `(0,0,-15.4)` degrees; scale `(1,1,1)` in the Blender source frame.

Relative to the evaluated `hand_r` bone matrix, the weapon transform is translation **(2.143424, 8.605095, -1.968700) cm** and Blender XYZ Euler **(1.537049, 177.314771, -89.946597) degrees**, unit scale. The full matrices in [fit-measurements.json](fit-measurements.json) are authoritative; Euler conventions can differ after FBX/Unreal skeleton import. Derive the native socket transform from those matrices and verify, rather than copying XYZ into Unreal Pitch/Yaw/Roll blindly.

The frozen support wrist is 23.089 cm from Grip. Fingers intersect the lower guard; a later pistol hold pose must address both. Do not scale the character, shrink hands or distort the gun to hide this. The 90-degree horizontal-FOV render uses a separate weapon-only root `(35,9,-8)` cm and camera at origin looking +X; it is a composition study, not the third-person bone offset or a runtime camera prescription.

## Materials

Two slots: `P02_Original_Meshy_PBR` and `P02_CutInterior`. Original Meshy PBR is preserved. Use returned base-color as sRGB; roughness, metallic and normal as data. Use the original metallic/roughness maps directly and normal strength **1.0**. The former metallic 0.5 multiplier, roughness floor 0.52 and normal strength 0.18 are removed. Interior base color linear RGBA `(0.035,0.045,0.060,1)`, roughness 0.7. FBX cannot guarantee native material graph conversion; recreate the source channels and inspect under comparable lighting. Check normal-map green-channel convention in Unreal without changing preserved originals. Base color/normal are 4K; metallic/roughness are 2K. No new shared palette asset is included.

## Next acceptance

Art lead/user reviews the corrected source against the untouched Meshy comparison. Set a runtime LOD budget before any later topology reduction, and retain this full-detail source for visual comparison and rebaking if needed. Integration checks exact imported bounds, default and first-person socket/camera placement, correct material channels, slide separation/caps and magazine visibility through motion. Animation owner authors a suitable pistol-specific right-finger/support-hand pose. Native/runtime, packaged and network verification remain deferred.
