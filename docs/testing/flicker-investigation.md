# Surface flicker investigation, 2026-10-18

The user reported local surface/model flicker both while stationary and while
moving the camera. Their screenshot was taken near a delivery gate.

Two issues were addressed:

- The merged runtime log reported missing calf bones in SK_Citizen. The old
  citizen mesh was bound to the updated courier skeleton, whose parent hierarchy
  also differs at the neck, shoulders and feet. The strengthened asset regression
  failed for all these mismatches in
  `Automation-20261018-115132-e6401f71/Report/index.json`. The citizen now uses its
  retained original skeleton and walk in `Art/Ambient/Citizen`; runtime and import
  scripts use that rig independently. The ambient Blender generator now refers
  to the retained original generator rather than the new art generator.
- Medium quality inherited disabled TSR flicker rejection while the project
  enabled stochastic Lumen interpolation. At a fixed gate camera, separate
  changes to interpolation and flicker rejection reduced sampled lower-frame
  inter-frame RGB difference from 0.396 to 0.314 and 0.338 respectively (0–255
  scale). These are indicative measurements from three frames per variant,
  with moving ambient actors present, not a comprehensive flicker benchmark.
  Medium now enables TSR flicker rejection and uses non-stochastic interpolation.
  Lumen remains enabled. Frame-rate acceptance has not been repeated.

Validation:

- Editor build succeeded: `Build-Editor-20261018-115833-6a0dc619`.
- Complete UE automation: 39 passed, zero failed,
  `Automation-20261018-115852-acc04a0d/Report/index.json`. Includes citizen bone
  hierarchy checks and a real spawned pedestrian's active animation skeleton.
- Rendered gate captures completed with no skeleton/Python errors:
  `EditorScript-20261018-115949-2b3c32e5` and
  `Saved/Validation/FlickerFixed/fixed_*.png`.
- Before/variant captures: `Saved/Validation/FlickerTemporal`.

Paths above are under `Saved/Validation` unless stated otherwise. The user's
exact original camera view and the full reported flashing pattern were not
reproduced; the original location still needs user confirmation in the updated
game. Passing headless automation alone does not establish visual stability.
