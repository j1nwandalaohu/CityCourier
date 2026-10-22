# Forward locomotion correction

The reported symptoms are a crouching start, sideways-looking forward movement, sliding, and indistinct ordinary/sprint motion.

Source investigation found one unrestricted ground database, no gait selection, fixed motion-matching playback rate, and foot IK using the horizontal position of the previous evaluated foot. Read-only saved-asset inspection confirmed forward root travel is already along mesh +Y (actor +X after its -90-degree mesh yaw), so a blanket mesh rotation is not warranted. Run and sprint forward loops measure approximately 504.56 and 706.39 cm/s. The eligible run-start sample lowers the pelvis from about 91 to 76 cm; this is evidence for excluding start poses from steady forward playback, not proof that a particular live query selected it.

The prepared correction keeps GASP motion matching, with separate forward-only run and sprint databases. The capsule's actual speed chooses the gait and drives each clip's cadence from its measured root-track speed. A ground-only gait blend preserves airborne matching, postures, combat slots, traversal and contacts. Ground foot IK changes height and surface tilt without pulling the new horizontal stride toward a previous-frame trace.

Graph version 10 is required. The graph/config generator refreshes the banks and hashes every output, including the new sprint database. Tests cover eligible clip identity, actual sprint selection, playback speed agreement, idle/move transitions and stale horizontal foot goals.

Validation on 2026-10-22, with explicit current-task build/check/restart authorization:

- Editor build passed: `Saved/Validation/Build-Editor-20261022-213840-92fd94bb/build.log`. The first build caught a test-only TObjectPtr deduction error, corrected before this successful build.
- Version 10 databases and graph regenerated: `Saved/Validation/EditorScript-20261022-213856-2afc8e11/stdout.log`.
- EpicAnimation: 9 passed (`Automation-20261022-213938-5aa6b169`); EpicCharacterIntegration: 2 passed (`Automation-20261022-214016-11cff6b8`); CharacterActions: 29 passed (`Automation-20261022-214043-10ed62d7`). All reports are under `Saved/Validation/<run>/Report/index.json`; the rendered-only contact test is excluded by NullRHI.
- Production Enhanced Input PIE capture completed without saving maps or changing protected packages. `Saved/Validation/ForwardGait/PIE/runs/20261022T214226-01b84b76/results.json` records ordinary movement at 420 cm/s with jog bank and 0.832401 playback multiplier, sprint at 700 cm/s with sprint bank and 0.990954 multiplier, and the Epic graph enabled in both stages. Forward jog and sprint PNGs are in the same directory.
- The first capture checked graph version before cold database indexes were ready. The capture now waits for the existing production retry to enable the graph; the successful run used that normal path without overriding the animation class.

The captures show forward running poses and separate gait selection. Single screenshots do not establish subjective smoothness or measure every planted-foot sliding error. Interactive movement acceptance remains for the user. The rebuilt city game was restarted with `Saved/Logs/CityCourier-GaitFix-20261022.log`; no packaged build was requested.
