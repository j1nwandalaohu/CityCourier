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
