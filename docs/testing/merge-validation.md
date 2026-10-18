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
