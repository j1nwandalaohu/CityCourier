# Combat and jump continuity correction

The inspected runtime loads library `Punch_Jab` and `Punch_Cross` for light punches, and `Melee_Hook` for the old heavy punch. These are retargeted Mesh2Motion CC0 animations, not handwritten poses. Source attribution is retained in `AssetsSource/CityCourier/production_actions.json` and the art manifest.

Confirmed code issues:

- Attacks immediately capped horizontal velocity and MaxWalkSpeed at 120 cm/s, including a character already sprinting at 700 cm/s.
- The action slot replaced the entire running pose, including legs and pelvis.
- The Epic path bypassed the phase-specific library jump clips. Its air database mixed jump and landing samples without an explicit physical phase clock. This establishes an ordering risk; no new live jump trace has yet identified a particular incorrect sample.

Prepared changes:

- Preserve normal walk/sprint movement during punches; reactions and recovery still lock movement.
- Graph version 11 caches locomotion and restores its root, pelvis and legs during moving/airborne punches, retaining the source punch's spine and arms. The native fallback follows the same mask. Moving strike sampling uses the evaluated pelvis so collision follows the composed pose.
- Replace the broad heavy hook with the retained right cross, retimed from 0.55 to 0.95 seconds with contact at 0.40 seconds. `build_compact_heavy.py` changes key times only. It exports the FBX, editable blend and `compact_heavy.json`; the focused `import_compact_heavy.py` checks the export hash and protects all other character packages.
- Both animation paths explicitly play takeoff, ascent and descent using physical jump time and vertical velocity. Production Epic playback uses owned GASP forward start clips selected by takeoff speed, `Jump_F_Off_Run_Lfoot` for ascent and `Jump_Loop_Fall` for descent. Read-only source inspection found 2.3–2.5 second start clips with deep early anticipation; only the last .45 seconds are mapped into the physical .18 second takeoff window. The legacy fallback retains Mesh2Motion phases. Air phases hold their last frame until the next physical phase. GASP standing/moving landings remain in the Epic path.
- Tests cover sprint momentum during light/heavy punches, compiled leg layering and explicit takeoff/ascent/descent playback. Existing fist/contact, jump landing and interruption tests must also run.

Status: the user explicitly authorized build, checks and restart. Compact heavy import completed (`Saved/Validation/CompactHeavy/import.json`), version 11 graph generation completed (`EditorScript-20261022-221045-ea14e43f`) and the final Editor build passed (`Build-Editor-20261022-222318-5c141abc`). The city game was restarted with log `Saved/Logs/CityCourier-CombatJump-20261022.log`. No packaging was performed.

The first production-input capture exposed a one-frame initial animation-state delay in the assertion and a report-writing error in the capture script; that run is not counted as validation. The next complete capture kept 700 cm/s during both punches and the jump, but visual review rejected the Mesh2Motion airborne arm posture. That prompted the explicit GASP flight selection above.

Final evidence under `Saved/Validation`:

- CharacterActions: 31 passed, zero failed, `Automation-20261022-222444-7ceacea6/Report/index.json`.
- EpicAnimation: 9 passed, zero failed, `Automation-20261022-221511-ccb62227/Report/index.json`. This graph/contact suite passed before the final action-component flight selection; the graph and rig were unchanged afterward.
- EpicCharacterIntegration: 2 passed, zero failed, `Automation-20261022-222538-a0231025/Report/index.json`.
- Production-input capture: `CombatJump/20261022T222701-08880613/results.json`, complete, with protected packages unchanged. Both punches and the full running jump retained 700 cm/s; the standing jump retained zero horizontal velocity. Actual playback included sprint/standing GASP takeoff, forward off-ground, fall, and the corresponding moving/standing landing clips.
- Ten PNGs in that capture directory cover both running punches and takeoff/ascent/descent/landing for running and standing jumps. Visual inspection confirmed the compact punch over running legs and the GASP airborne arm posture. Smoothness and subjective feel still require interactive user acceptance.

The original pose/contact test failures were caused by testing before the extended .10-second punch blend completed. Their animation-tick timing was adjusted while preserving the same contact sample time and strict fist-position tolerance; the final suites above passed.

When authorized, close the game and run sequentially: Editor build, `import_compact_heavy.py`, `connect_library_actions.py`, affected CharacterActions/EpicAnimation/EpicCharacterIntegration tests, production-input running punch and jump captures, then restart the city game. No packaging is needed.
