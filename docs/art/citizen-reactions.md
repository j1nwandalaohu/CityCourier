# Editing citizen reactions

The editable animation source is `AssetsSource/CityCourier/Ambient/CharMorph/Reactions/CitizenReactions.blend`.
It contains the CharMorph citizen mesh and independent reference skeleton plus five separate actions:
`A_CitizenIdle`, `A_CitizenWalk`, `A_CitizenHit`, `A_CitizenFall`, `A_CitizenGetUp`.
The original ambient blend and legacy citizen assets are preserved as source history.

The new mesh source is `AssetsSource/CityCourier/Ambient/CharMorph/CitizenCharMorph.blend`.
It combines the retained CC0 CharMorph-Vitruvian head/skin with the CC0 MakeHuman
body, casual suit and shoes. It removes the courier helmet, straps and reflective
accessories. Hair is reduced to one-sixth of whole strands and the mesh is
simplified to about 93,000 triangles / 57,000 vertices for the bounded crowd.
The independent NPC skeleton shares the portrait rig's reference layout but is
a separate UE asset. The player mesh, skeleton, clips and materials are protected
against changes during import. There is one adult male appearance in this version.

## Editing in Blender

Open the blend with Blender 4.5, select `CourierRig`, enter Pose Mode, then open
Dope Sheet > Action Editor and select the desired action. Its timeline is 60 fps.
Use Graph Editor to refine interpolation and timing. Do not rename or reparent
bones, apply a new armature scale, or change the reference pose in Edit Mode.
Keep the root stationary and the skinned body above the ground plane.

Export only the armature as FBX, with animation enabled, all-actions/NLA disabled,
no leaf bones, forward -Y and up Z. The editable source uses metres
(`scale_length=1`); FBX converts to UE centimetres. Import as **animation only** and choose the citizen
skeleton `/Game/CityCourier/Art/Ambient/CharMorph/SK_CitizenCharMorph_Skeleton`.
Changing clip duration also requires updating the corresponding state duration
in `AmbientPedestrian.cpp` (hit 29/60 s, fall 1.05 s, get-up 2.7 s).

## Reproducible generated version

`tools/art/build_citizen_reactions.py` generates the five actions and FBX files;
rerunning it replaces manual animation edits in this generated blend. Keep a
separate copy when authoring by hand. Run the importer using:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/Run-EditorScript.ps1 -Script Content/Python/import_citizen_reactions.py -EngineRoot D:/sdk/UE_5.6
```

The importer checks source hashes, duration, reference skeleton, compressed poses
and hashes of existing ambient packages. Update `manifest.json` FBX hash and sample
count if deliberately importing a manually edited export.

## Sources and license

Mesh2Motion provides code under MIT and art/animations under CC0-1.0:
https://github.com/Mesh2Motion/mesh2motion-app

Retained upstream revision: `653a9698f5f315523072f1c3ff3496100401317b`.
Retained GLBs are in `AssetsSource/CityCourier/External/Mesh2MotionProbe/static/animations`;
the retained art license is `AssetsSource/CityCourier/External/Mesh2MotionProbe/LICENSE-CC0.MD`.
The new actions derive from `Idle_A`, `Walk`, `Hit_Chest`, `Death_C`, and `Zombie_Rise`.
The fall is used as a temporary, nonfatal knockdown, with a separate recovery.
The manifest records source/export hashes, retarget mapping and UE pose checks.

Blender: https://www.blender.org/features/animation/
UE Control Rig (optional in-engine editing):
https://dev.epicgames.com/documentation/en-us/unreal-engine/animating-with-control-rig-in-unreal-engine

## Gameplay and verification

Left mouse produces a short stagger; right mouse or two light hits within 1.2 s
starts a fall. A 2 s down hold precedes recovery. Patrol stops throughout the
reaction and resumes afterward. Down/get-up cannot be repeatedly restarted by
hits. Walls still block fist sweeps; a blocked fall footprint uses a stagger,
and blocked standing clearance delays recovery. Pool disable resets all states.
The fallen body ignores player capsules to avoid lifting a nearby player.

Final automation: 114 passed, 0 failed, no test warnings, in
`Saved/Validation/Automation-20261020-060119-bdbc7589/Report/index.json`.
Six combat tests cover production fist contact/occlusion, duplicates, combination
timing, recovery, pool reset, fall/standing clearance, real evaluated bones,
independent skeleton and fall direction.

Real PIE Enhanced Input proof:
`Saved/PedestrianBoxing/20261020T060145-00048abf/results.json` — complete, no errors,
342 sampled frames, source hashes match. Player feet stayed at street +2 cm
throughout the down hold; the NPC resumed its original patrol.

Unedited editor-play screenshots (include editor chrome):
[down](citizen-reactions/knocked-down.png),
[getting up](citizen-reactions/getting-up.png),
[walking again](citizen-reactions/walking-again.png).

Final Shipping package succeeded:
`Saved/Validation/Package-Shipping-20261020-060235-f6298acc/package.log`.
The standalone 30 s startup/profile smoke check passed:
`Saved/Validation/Interiors-Shipping-20261020-061000-5a6e579f/shipping-startup.json`.
That check cleans up its own offscreen process; it does not claim a normal GUI exit.
Playable build: `Artifacts/Windows/Shipping/CityCourier.exe`.
