# main integration validation - 2026-10-18

Merged feat/city-courier (a9c8073) and feat/city-courier-art (facea7b).

- Keep the gameplay city as GameDefaultMap and open the separate art preview in the editor. Cook both maps and retain the gameplay always-cook directory.
- Use the art branch versions of shared character, animation and material assets, plus its character and scooter blueprints. Preserve gameplay C++ behavior and both branches of asset tests.
- Unify the two UCourierAnimInstance definitions under Player, retaining gameplay speed/steering state and the art sampling proxy and clip defaults.
- Reuse inherited scooter anchors, hide the legacy body under the updated art, and compensate character mesh height for the gameplay seat attachment.
- Runtime pose testing advances the engine frame counter between simulated frames; otherwise the native animation proxy does not update between calls in the same frame. All original pose assertions remain enabled.
- Retain the original art generator/importer as *_original.py. Current importer and generator use the updated art pipeline. Keep both branches of provenance and acceptance records.

Validation:
- Editor build: Saved/Validation/Build-Editor-20261018-112124-71ea8bdc/build.log, succeeded.
- UE automation: Saved/Validation/Automation-20261018-112136-47576263/Report/index.json, 39 passed, 0 failed.
- Tooling.Tests.ps1: 13 passed, 0 failed.
- Performance.Tests.ps1: all calculation and invalid-evidence checks passed.
- verify_sources.py: 38 retained sources match SHA-256 records.

No new distribution package or rendered visual acceptance was produced. Existing packages and screenshots are historical evidence from their original branches. Feature branches and their worktrees remain available.

## Functional actions and latest art integration - 2026-10-18 18:51 CST

Integrated functional branch `feat/city-courier` at `faf8a9c` and preserved the latest art merge `7fe84d4`. Integration commits are `4451f3b` and `dda002a`; local `main` was advanced to the verified combined tree. Both source branches and their worktrees remain available.

- Preserve main's corrected map projection while adding offer route previews, independent station/bike navigation, order history and action feedback.
- Preserve the production skeleton, realistic art assets and native locomotion proxy. Rebuild the action slot and hand sockets against this skeleton without reimporting meshes or replacing artist offsets.
- Fix a reproduced native-proxy integration defect: montage weights could be positive while the final pose still contained locomotion. Evaluate `FullBodyAction` explicitly after native locomotion blending. The regression checks the resulting fist position against a distinct authored sequence, beyond checking slot weights.
- Fix a reproduced imported-rig scaling defect: the 100x armature scale enlarged a raw socket translation of five units to five meters. Convert the default five-centimeter offset through accumulated reference-bone scaling; apply the same centimeter interpretation to the sampler's missing-socket fallback. A regression measures the persisted socket five centimeters from the hand. Existing artist-adjusted sockets remain unchanged.
- Preserve the existing attack reach limits, occlusion, interrupted-hit traversal and missing-animation safeguards. Borrowed Walk/Ride sequences remain test fixtures; the ten new action sequences still require artist delivery and visual acceptance.

Validation on final local main (`dda002a`):

- Editor build: `Saved/Validation/Build-Editor-20261018-185002-a5107ccb/build.log`, succeeded.
- Complete UE automation with real RHI: `Saved/Validation/Automation-20261018-185026-d84523b8/Report/index.json`, **66 passed, 0 failed, 0 not run**. The tracked copy is [main-automation.json](evidence/main-integration/main-automation.json).
- Before-fix reproductions are retained as [native-slot-red.json](evidence/main-integration/native-slot-red.json) and [scaled-socket-red.json](evidence/main-integration/scaled-socket-red.json). The latter includes the measured five-meter socket and failed real contact checks.
- On the identical final integration tree: `tests/Tooling.Tests.ps1` passed 13 checks; `tests/Performance.Tests.ps1` passed its calculation and invalid-evidence checks; `tools/art/verify_sources.py` verified 116 retained sources against SHA-256 records.

No remote push, new distribution package, physical mouse-input certification or new-animation visual acceptance is claimed. The existing shortcut and packaged game remain at their prior locations.

## Subsequent art integration - 2026-10-18

Merged the remaining art commits `6f04431` (recessed shops, clothing and CC0 PBR props) and `c0a021c` (Mesh2Motion / CharMorph probes) into main based on `eacc48d`.

- Resolved README and asset-manifest conflicts by preserving gameplay instructions, ambient asset provenance and the new art provenance. The main importer retains its merged-project behavior, imports reference materials and excludes experimental FBX folders from normal static imports.
- Preserved main's gameplay, ambient citizen skeleton repair, map-turn projection fix and scalability settings. Incoming C++ changes only extend the art asset checks.
- The merged gameplay mode also starts ambient actors in the character lab. The capture script now suspends this PIE world's population and checks that it stays inactive; gameplay spawning code is unchanged.
- Keep the separate preview and character-lab maps. Experimental clips and CharMorph remain independent assets; production animation selection is unchanged.

Validation of the merged tree:

- Editor build succeeded: `Saved/Validation/Build-Editor-20261018-183810-df6ad0f3/build.log`.
- UE full suite: **40 passed, 0 failed**, `Saved/Validation/Automation-20261018-183913-f4a6a328/Report/index.json`.
- Tooling tests: **13 passed, 0 failed**; performance calculation and invalid-evidence tests passed.
- Source verification: **116** retained files match SHA-256 records; CharMorph CC0 legal text also matches. Merged Python scripts compile, and the manifest retains both ambient and new art entries.
- Actual merged-main PIE lab capture: `Saved/Validation/EditorScript-20261018-184227-524048a3`, eight 1280x720 stills, no script error, one ambient manager disabled, root drift 0 and maximum runtime/sequence checked-bone error approximately **0.000016 cm**. Results: `Saved/Validation/CharacterProbe/results.json`. Visually checked the run comparison after background suppression.

The videos and images under `docs/art/character-probe` retain their original art-branch capture version. No new distribution package or performance benchmark was produced. Both feature worktrees and the pre-existing untracked root shortcut remain available.
