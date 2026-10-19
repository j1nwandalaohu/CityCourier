# Seven outstanding rider actions

Seven new in-place AnimSequences use the formal rider's existing mesh, shared
skeleton, reference pose and ABP. Root translation and rotation remain fixed;
CharacterMovement owns physical jumping and actor movement. Existing meshes,
animations, Blueprints and maps are preserved.

| Asset | Seconds | Loop | Authored basis |
| --- | ---: | --- | --- |
| `A_JumpStart` | 0.18 | No | Retained genuine `Jump_Start` crouch/takeoff, edited arms and anticipation |
| `A_JumpLoop` | 0.40 | Yes | Retained `Jump_air`, reduced tuck, closed pose/velocity seam |
| `A_Land` | 0.25 | No | Retained `Jump_Land` compression, planted shoes and recovery |
| `A_HitReact_Front` | 0.40 | No | Retained `Hit_Chest` backward recoil and recovery |
| `A_HitReact_Back` | 0.40 | No | Original forward torso reaction using retained hit arm poses |
| `A_RunStart` | 0.20 | No | Original transition into delivered `A_Sprint` phase zero |
| `A_RunStop` | 0.25 | No | Delivered `A_Sprint` phase zero through a final step into idle |

The retained free source is
`External/Mesh2MotionProbe/static/animations/human-base-animations.glb`, revision
`653a9698f5f315523072f1c3ff3496100401317b`, CC0 under the existing provenance.
Run poses come from `Mesh2MotionProbe.blend` action `A_M2M_Run`, the source already
promoted to `A_Sprint`. No paid asset or claimed motion capture was used.
[The source manifest](../../AssetsSource/CityCourier/pending_actions.json) records
source, generator and FBX hashes, provenance, validation and actual import results.

The older shared contract specifies nominal 30 fps. These seven deliveries use
100 Hz because **0.18 and 0.25 seconds cannot be represented by integer 30 Hz
intervals**. Source motion remains the nominal 30 Hz reference. This delivery
sampling difference preserves requested durations, runtime seconds and gameplay
timelines; the shared contract is unchanged.

Authoring opens `CourierPhotoreal.blend` and preserves its actual mesh and rig.
The editable result is `Blender/CourierPendingActions.blend`; FBXs are under
`Exports/PendingActions/`. New upper-body motion is calibrated in local bone
space against actual production `A_Idle`, rather than the differently posed
Mesh2Motion Idle. It retains motion deltas and torso lean while removing
incompatible chest/neck/clavicle roll. Recovery uses production Idle; run endpoints
match the untouched delivered Sprint within 0.028 cm and 0 degrees.

This calibration addressed a visible chest skin patch in the initial JumpLoop.
The existing rigid head/neck skinning makes the source's 31.49-degree chest and
9.54-degree neck baseline differences unsuitable for this mesh. Neither the rider
mesh nor existing animations were altered to conceal it. Actual production Idle
was first captured under the same UE lighting as a control. Final source
front/side previews and actual UE samples were inspected for full helmet, shoes,
distinct poses and the former chest patch.

[Final collar diagnostics](pending-actions/collar-final.json) compare all 215
baked frames and 26 existing Sprint samples using deformed skin/shirt BVHs.
JumpLoop's maximum newly visible baseline-covered candidates are 3/2/0
(front/left/right), versus 173 frontal candidates at the original peak.
RunStart's maximum is 68/40/4; untouched Sprint is 67/40/41. These are diagnostic
vertex candidates, including threshold-edge and underarm cases, **not proof of
closed-mesh containment or zero intersections**. Existing Sprint's collar
limitations remain inherited at the exact run endpoints. The
[original diagnostic](pending-actions/collar-before.json) is retained separately.

Source validation checks every frame: reference matrices, mesh signature,
finite deformed vertex samples, attached joint lengths, root,
shoe soles, grounded Land/Hit foot locks, front/back direction, compression,
recovery, loop closure and Sprint endpoints. Root movement is zero; grounded
Land/Hit shoe-center travel is below 0.00005 cm. JumpLoop's pose seam is below
0.00004 cm. Raised airborne feet are local tuck, not actor translation.
Fourteen actual Blender previews are retained under
[source/](pending-actions/source/); these are source evidence, not gameplay images.
The formal source blend has no embedded pre-existing actions, so its recorded
protected-action digest map is empty. Existing delivered animations are protected
by the actual UE package hashes, rather than claiming an empty source map proves
their preservation.

Strict final UE import ran as `EditorScript-20261019-094230-2d3bd21f`.
Each import must return exactly its nonempty expected AnimSequence object path
before any asset is loaded. Only seven sequences are saved. All 17 protected
Characters/Blueprint packages retain their hashes; shared reference pose is
unchanged. All seven use the same 51-bone UE skeleton (50 source bones plus scene
root), requested durations, fixed root and disabled Root Motion. The importer
waits for asynchronous compression and checks 21 RAW/COMPRESSED poses per clip;
maximum difference is 0.0713 cm, below the 0.1 cm requirement. The existing engine
recorder compression setting is used without frame stripping, because default
compression exceeded the requirement on initial import.

`capture_pending_actions.py` captures real PIE single-node **clip samples** in the
existing character lab, then separately exercises the possessed production
CourierCharacter/ABP through Enhanced Input and movement input. Hits remain
clip-sample evidence: no public native hit callback was invoked. It checks
rendered/COMPRESSED and RAW/COMPRESSED poses, root, skeleton, upright pose, PNG
dimensions/hashes and protected package hashes. EndPIE teardown timeout is an
error. No asset or map is saved.

The arbitrary-release test exposed a RunStop foot snap with the previous
0.03-second montage blend. The coordinated one-line native fix changes **only
RunStop blend-in to 0.15 seconds**; movement and action timings are unchanged.
[Isolated before/after evidence](pending-actions/run-stop-comparison.json) uses
identical animation package hashes and closely matched measured Sprint phases:
0.025/0.2583/0.5875/0.7458 before versus 0.0208/0.2542/0.5875/0.7458 after.
At the problematic second release, maximum left-foot displacement between ticks
falls from 69.95 to 33.06 cm in world space (65.74 to 29.08 cm in component space).
This is sampled trajectory evidence, **not a claim that all release phases are
visually perfect**. The private Sprint clock is inferred from nearest two-foot
pose; frame timing and actor travel contribute to world displacement.

Older comparison reports predate capture/runtime source hash metadata; their
limitations and git-reference source hash are explicit. Final reports additionally
record script/runtime SHA256, package hashes and observed montage blend time.
Native activation is separate from still-image review and does not claim full
motion visual acceptance.

Final real UE capture `20261019T014419Z-86b693ef` completed all 21 requested
1800×1200 front/side/rear clip samples and 380 native samples. Rendered versus
COMPRESSED error is at most 0.000042 cm; RAW versus COMPRESSED at the pictured
times is at most 0.0721 cm. Root drift is zero, all 25 protected packages including
the lab map retain their hashes, physical jump height is 89.97 cm, and actual
JumpStart/JumpLoop/Land/RunStart/RunStop sequences were observed. UE's actual
montage blend property reports RunStop 0.150000006 seconds; other actions remain
0.03 seconds. [The full report](pending-actions/ue-final.json) retains metrics
and all 21 screenshot hashes; [the retained-image manifest](pending-actions/images.json)
records bytes, dimensions and SHA256 for the selected images in this directory.

| Actual UE clip sample | Retained full-body side picture |
| --- | --- |
| JumpStart, 0.12 s | [UE PNG](pending-actions/ue/A_JumpStart-side.png) |
| JumpLoop, 0.20 s | [UE PNG](pending-actions/ue/A_JumpLoop-side.png) |
| Land, 0.07 s | [UE PNG](pending-actions/ue/A_Land-side.png) |
| Front hit, 0.09 s | [UE PNG](pending-actions/ue/A_HitReact_Front-side.png) |
| Back hit, 0.09 s | [UE PNG](pending-actions/ue/A_HitReact_Back-side.png) |
| RunStart, 0.15 s | [UE PNG](pending-actions/ue/A_RunStart-side.png) |
| RunStop, 0.12 s | [UE PNG](pending-actions/ue/A_RunStop-side.png) |
| Existing Idle control | [UE PNG](pending-actions/idle-baseline-side.png) |

The 21-picture run completed its data checks and EndPIE before a late process
shutdown access violation. The capture script had also called immediate quit
after disabling keep-alive, duplicating the editor executor's deferred quit.
That duplicate was removed. A separate **zero-picture** native-only run,
`20261019T014839Z-f9a49d0c`, repeated all four recorded release phases and exited
cleanly with code 0; it does not replace or claim additional pictures.
[Its report](pending-actions/ue-native-cleanup.json) records the current capture
code/runtime SHA256. This distinction is explicit in
[verification.json](pending-actions/verification.json).

Final native regression with the calibrated seven assets and 0.15-second RunStop
fix passed **92/92, zero warnings, zero failures**, process exit 0:
`Automation-20261019-094711-f894aad3`. The
[durable automation report](pending-actions/automation-final.json) and
[log](pending-actions/automation-final.log) preserve this result.

Reproduce sequentially while holding the coordinated UE slot:

```powershell
& D:/eleme/.dependencies/art-tools/blender-4.5.3-windows-x64/blender.exe --background --python-exit-code 1 --python tools/art/build_pending_actions.py
& D:/eleme/.dependencies/art-tools/blender-4.5.3-windows-x64/blender.exe --background --python-exit-code 1 --python tools/art/validate_pending_actions.py
& D:/eleme/.dependencies/art-tools/blender-4.5.3-windows-x64/blender.exe --background --python-exit-code 1 --python tools/art/validate_pending_actions.py -- --preview-angles
& D:/eleme/.dependencies/art-tools/blender-4.5.3-windows-x64/blender.exe --background --python-exit-code 1 --python tools/art/validate_pending_actions.py -- --collar-all
./scripts/Run-EditorScript.ps1 -Script Content/Python/import_pending_actions.py -EngineRoot D:/sdk/UE_5.6
./scripts/Run-EditorScript.ps1 -Script Content/Python/capture_pending_actions.py -EngineRoot D:/sdk/UE_5.6 -Gui
./scripts/Test.ps1 -Headless -EngineRoot D:/sdk/UE_5.6
```

Default capture is 21 front/side/rear samples. `CC_PENDING_ANGLES=front,side`
selects 14; `CC_PENDING_RUNTIME=0` selects stills only.
`CC_PENDING_SKIP_SAMPLES=1` explicitly requests zero pictures and native checks
only; `CC_PENDING_BASELINE_ONLY=1` records existing Idle controls. Every run has
an immutable `Saved/PendingActions/UE/runs/<id>/` output directory.
