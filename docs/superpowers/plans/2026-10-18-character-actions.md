# Character Actions Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [x]`) syntax for tracking.

**Goal:** Implement the approved first-batch action contract while preserving delivery, vehicle and UI behavior.

**Architecture:** A deterministic action timeline owns attack instances, windows, combos and interruptions. A character component integrates collision, animation and input with CharacterMovement. Existing locomotion animation remains compatible; full-body action playback is layered through a named slot and never controls gameplay truth.

**Tech Stack:** UE 5.6 C++, Enhanced Input, Animation Blueprint/Montage, UE Automation, PowerShell.

**Spec:** `docs/art/character-actions-contract.md` (approved by user's “ok …继续完成功能部分”).

## Global Constraints

- Reuse `D:/eleme/.worktrees/city-courier`, branch `feat/city-courier`; no edits to the art worktree or arbitrary asset merge.
- Retain existing four animation paths; add ten actions and `FullBodyAction` slot. Root motion disabled.
- Light: active [0.14,0.22), combo [0.18,0.40), chain at 0.44, end 0.55; heavy: active [0.34,0.46), end 0.95.
- Light L → light R → heavy maximum; first light may chain directly to heavy. One pending input, most recent valid wins; simultaneous edges choose heavy; no held-button repeat.
- Ground attacks cap movement at 120 cm/s. No jump/interaction during attacks, airborne or first 0.12s of landing; land presentation 0.25s. Hit reaction 0.40s without timer stacking.
- Attack range ceilings 130/160 cm with 12 cm fist sweep; blockers occlude targets; unique attack/target deduplication.
- Menu transitions clear input and combos; nonpause UI cancels attacks. Pause preserves elapsed action and hit deduplication. Airborne UI retains vertical velocity.
- No health/death, NPC combat AI, crouch, vault or added economic penalties. Use a dedicated training target for visible hit feedback.
- Serial UE builds/tests/editor/package runs, isolated runtime saves; preserve local worktree and runnable artifacts.

## Review Focus

1. A large frame spans an entire active window or combo boundary: valid contacts occur once, ended windows never remain active (Tasks 1,3).
2. Pause → phone → gameplay and held mouse across interface transitions: no resumed ghost attack or lost airborne gravity (Task 2).
3. Camera rotates during active frames and a target is behind a wall: no through-wall or backward sweep damage (Task 3).
4. Asset missing, incompatible skeleton, actor removal, falling from ledge: no permanent action locks; honest asset readiness (Tasks 2,3).
5. Existing settlement, station charging and mounting during new states: no bypass of grounded/action gates and no save regression (Tasks 2,4).

## Task 1: Deterministic attack and reaction timeline

**Files:** Create `Source/CityCourier/Player/CourierActionTimeline.{h,cpp}`, `Tests/CharacterActionTimelineTests.cpp`.

**Interfaces:** `ECourierAction { None, LightL, LightR, Heavy, HitFront, HitBack }`; `FCourierActionTimeline::RequestAttack(bool bHeavy) -> bool`, `Advance(double Delta)`, `Cancel()`, `ReceiveHit(bool bFront)`, `ClearQueuedInput()`. Read-only `Action`, `Elapsed`, `InstanceId`; `FActionSlice { Action, InstanceId, Start, End }` returned by `Advance` records active time intersections including frame skips.

- [x] Write failing tests for active boundaries, whole-window frame skips, combo timing/three-hit limit, replacement, no late buffer, cancellation and nonstacking reaction.
- [x] Build Editor then run `./scripts/Test.ps1 -Filter CityCourier.CharacterActions.Timeline -EngineRoot D:/sdk/UE_5.6`; observe missing behavior.
- [x] Implement finite delta validation and timeline slicing across transitions without discarding elapsed remainder.
- [x] Rebuild and run timeline tests; all succeed. Commit `feat: add deterministic character action timeline`.

## Task 2: Input, movement and interface state integration

**Files:** Create `Player/CourierActionComponent.{h,cpp}`, `Tests/CharacterActionWorldTests.cpp`; modify `Player/CourierCharacter.{h,cpp}`, `Player/CourierController.cpp`, `Vehicles/RiderRecoveryComponent.cpp`, `Content/Python/build_player.py`; create focused `Content/Python/build_character_actions.py`.

**Interfaces:** Character exposes `GetActions()`, `TryJumpWithFeedback()`, `CanPerformGroundAction(FText&)`, `SetInputSuppressed(bool,bool bPreserveAction=false)`; component exposes `QueueAttack(bool)`, `Cancel()`, `SetSuppressed(bool,bool)`, `GetTimeline()`, `IsGroundActionBlocked()`, `ReceiveStrike(FGuid,FVector)` and `OnStrike` delegate. Tick consumes aggregated mouse edges, heavy wins. Character landing/movement-mode callbacks inform component.

- [x] Write failing world tests for grounded jump impulse/headroom/repeat, action speed limits, grounded interaction, pause/menu cancellation, airborne velocity retention, departure/landing gates and recovery cancellation. Test input mapping assets through actual Enhanced Input consumption when generated.
- [x] Build and run `CityCourier.CharacterActions.World`; observe expected failures.
- [x] Implement physics-backed jump (JumpZVelocity 420, AirControl 0.25), 0.12s landing gate, sprint/attack/reaction speed control. Maintain pause state through unpause, cancel on forced state transitions. Add persistent jump/light/heavy actions and update both fresh-build and focused editor scripts without rebuilding unrelated city assets.
- [x] Rebuild, generate input assets, run world tests and existing `CityCourier.InputModes`, `CityCourier.Scooter`, `CityCourier.Station`, `CityCourier.Recovery`. Commit `feat: integrate jumping and contextual melee controls`.

## Task 3: Animation playback, actual fist traces and training target

**Files:** Modify `Player/CourierAnimInstance.{h,cpp}`, `Player/CourierArtBuilder.cpp`, `Player/CourierActionComponent.{h,cpp}`, `Content/Python/build_character_actions.py`; create `Player/CourierActionAnimation.{h,cpp}`, `World/CourierTrainingTarget.{h,cpp}`, `Tests/CharacterActionContactTests.cpp`.

**Interfaces:** Animation helper loads and validates the ten Sequence paths, maps action/jump/land/run transitions, samples fist socket transforms from actual authored animation and skeleton. Component exposes `OnStrike(AActor*,FGuid,ECourierAction)`, reports asset availability and drives named full-body montage. Training target implements `ICourierStrikeReceiver::ReceiveCourierStrike(FGuid,FVector,ECourierAction)`; visible recoil and hit count, no economic effects or health system. It is placed near station only after collision/route clear validation.

- [x] Write failing tests for real authored fist contact, wall occlusion, range limits, unique hit count, frame skipping, actor deletion/missing assets cleanup and front/back receiver behavior. Keep test fixtures out of production APIs.
- [x] Build and run `CityCourier.CharacterActions.Contact`; observe missing behavior.
- [x] Add FullBodyAction slot above existing four-pose locomotion without expanding AnimationIndex for action combinations. Use in-place dynamic montages; align visual and gameplay time. Trace authored fist trajectory at <=1/120s samples clipped to valid windows; block by sorted visibility hits and enforce range; one receiver per attack instance. Validate missing/incompatible assets, close action safely. Add dedicated target without changing background pedestrian behavior.
- [x] Integrate the user's confirmed art delivery paths only when available; do not manufacture or overwrite their work. Rebuild affected generated ABP through updated builder.
- [x] Run contact, world, timeline and `CityCourier.ArtAssets` tests, inspect actual poses. Commit `feat: connect authored actions and occluded melee hit feedback`.

## Task 4: Full validation, packages and handoff

**Files:** Update `Validation/PackagedValidationDriver.{h,cpp}`, `docs/playing.txt`, `README.md`, `docs/art/character-actions-contract.md`; add `docs/testing/character-actions-validation.md` and focused evidence.

**Interfaces:** Development opt-in `CharacterActions` mode drives the same character/component interfaces, checks jump, combos, menu/air transitions and target strikes, captures actual poses in isolated UserDir. Shipping never spawns the driver.

- [x] Add failing integration checks before driver/fix implementation. Run the actual editor/packaged character, not only pure timeline tests.
- [x] Run Editor build and complete `CityCourier` suite; reject failed, zero or not-run report entries.
- [x] Build full Development and Shipping packages serially; run isolated Development character checks and existing real delivery route/reload validation; check physical jump/light/heavy and UI clicks in Shipping.
- [x] Record exact art provenance, tested input/state paths, screenshots, count and remaining limitations. Commit `test: verify playable character actions and delivery regression`.
- [x] Perform one independent final whole-change review, fix material findings with RED→GREEN regressions and passing suite, repackage if needed. Retain branch/artifacts with no push or merge.

## Execution

The user explicitly requested continuous implementation of the approved contract, and the session's Native method remains in force. Execute inline; do not ask again to authorize the implementation. Baseline 51/51 passed at `Automation-20261018-173121-c90090a9`. New art assets are not yet visible in the known worktrees; their delivery location has been requested asynchronously while independent functional work proceeds.

## Final delivery status

Functional code is complete at ae990f1; full suite63/63 and both final packages pass. Final runtime checks passed for jump, airborne UI, delivery and reload. User-confirmed art remains in production, so new-action visual acceptance is pending. Shipping physical mouse checks were attempted but desktop focus interference prevented reliable certification; this is explicitly recorded rather than counted as a pass. Joint art and mouse acceptance remain pending in the contract. See docs/testing/character-actions-validation.md and character-actions-review.md for exact evidence and deferred Minor. Local branch/worktree retained.
