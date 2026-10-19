# Pedestrian boxing implementation plan

> Execute in this session using the native workflow; preserve the existing uncommitted vehicle traversal work.

**Goal:** Player punches visibly stagger pedestrians; a heavy punch or two quick light punches knocks them down, followed by recovery and resumed patrol.

**Architecture:** Reuse CourierStrikeReceiver and the existing sampled fist sweeps. A pedestrian state machine owns reactions, collision and recovery; animations use an independent CharMorph citizen skeleton with short crossfades. Retain the CC0 source attribution and reproducible animation exports.

**Tech stack:** Unreal Engine 5.6 C++, Blender 4.5, Unreal Python.

**Constraints:** Existing range, wall occlusion and per-punch dedup remain authoritative. No economy penalties or lethal combat. Pool recycling clears reactions. Do not commit or push without a request.

**Review focus:** Duplicate contacts; wall obstruction; pooled actors; standing into an obstacle; skeleton/ground alignment during fall and recovery.

## 1. Strike receiver and reactions

- [x] Add `PedestrianCombatTests.cpp`; establish a failing test that a spawned pedestrian implements the existing receiver and accepts a real strike.
- [x] Implement light stagger, heavy/two-light knockdown, finite down/get-up states and patrol pause in `AmbientPedestrian.h/.cpp`.
- [x] Test duplicate strikes, invalid input, down-state contacts, reset on population disable and blocked recovery.

## 2. Authored animations

- [x] Export citizen-compatible hit, fall and get-up clips from retained source motions; preserve reference bones and mesh.
- [x] Import animation-only assets onto the citizen skeleton; retain source, FBX, manifest and license attribution.
- [x] Add smooth native animation sampling and test runtime clip compatibility and reaction durations.

## 3. End-to-end verification

- [x] Verify production punch contact, range and wall occlusion; run the CityCourier automation suite.
- [x] Capture real PIE light/heavy strikes, down pose and recovery; inspect screenshots and state trace.
- [x] Build Shipping, validate startup, update play instructions and launch the game.

## Accepted scope update

User selected CharMorph to upgrade the pedestrian model. Reuse retained CC0 portrait head/skin and casual clothing, remove helmet/reflective accessories, reduce geometry, import a separate NPC mesh/skeleton and regenerate all five clips for it. Preserve legacy assets and the player.

## Verification ledger

- Receiver baseline failed as expected: Automation-20261020-051812-d2aa520f.
- Initial five combat tests passed: Automation-20261020-052808-40a242af.
- Wall-adjacent fall test failed before adding static footprint clearance: Automation-20261020-053541-9400ba4f.
- CharMorph model import protected existing packages: EditorScript-20261020-054649-aa011657.
- CharMorph animation import validated poses and reference: EditorScript-20261020-054730-acf61f41.

- Final source verified by 114 passing tests: Automation-20261020-060119-bdbc7589.
- Final real-input proof: Saved/PedestrianBoxing/20261020T060145-00048abf/results.json; 342 frames, complete, no errors, matching source hashes.
- Screenshot review found and fixed down-body player lifting and the modern FBX forward-axis mismatch; regression assertions failed before each fix, then passed in the final suite.
