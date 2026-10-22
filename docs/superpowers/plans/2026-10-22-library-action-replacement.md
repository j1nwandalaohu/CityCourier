# Library action replacement implementation plan

**Goal:** Remove original authored player and NPC poses from active animation paths, using already downloaded licensed animation sources.

**Architecture:** Keep the existing courier skeleton, collision and authoritative attack clock. Retarget library poses; reuse already retargeted GASP locomotion/traversal and genuine CC0 punches/postures. Replace the source-edited jump, hit, idle, ride and transition assets. Add an authored fall/hold/get-up action sequence after three accepted hits within 1.2 seconds of each other.

**Tech Stack:** UE 5.6.1, Blender 4.0, Python, Unreal C++.

**Spec:** User instructions in the current conversation: replace all self-authored player actions; ordinary hits have reactions; continuous hits cause knockdown followed by automatic get-up; no proxy downloads.

## Constraints

- Work on the existing feature branch and preserve unrelated changes.
- No additional downloads needed. Record source hashes and licenses.
- Preserve courier reference skeleton and mesh.
- UE operations run sequentially. Do not compile/package/launch without current-task authorization.
- A file copied into Content is not considered connected until its actual playback reference is verified.

## Tasks

1. Inventory every player playback path and classify original / library / library with original pose edits. Produce a machine-readable replacement catalog and source provenance.
2. Retarget complete library idle, driving, jump, death and get-up clips. Use Content Examples directional hit clips and GASP start/stop clips. Preserve genuine library punches and crouch/crawl. Validate finite transforms, fixed root, skeleton, duration and recovery endpoints.
3. Import against the existing skeleton; update serialized legacy references as well as Epic graph references. Give stationary grounded characters an explicit library idle branch so motion matching cannot choose a running pose at rest. Preserve moving GASP matching and traversal.
4. Extend the action clock with knockdown/ground-hold/get-up phases; reset hit streak on cancellation and timeout, ignore duplicate hit IDs, block input during recovery. Add automation coverage for these observable transitions.
5. Verify source/catalog checks, review all active clip references, and record unrun UE build/runtime checks explicitly. Obtain build/test permission only once concrete edits and resources are ready if it is still needed.

## Review focus

- Old serialized Blueprint animation maps must not override replacement defaults.
- Duplicate strike IDs must not count toward knockdown.
- Large time deltas must cross all recovery phases without hanging.
- Mount, UI suppression, airborne interruption and missing clips must cancel safely.
- Lying and recovery endpoints must agree; root motion must not move the capsule twice.

## Progress

- Inventory started. Existing punches, jog/sprint and crouch/crawl already originate in Mesh2Motion; old idle, ride and several source-edited transitions remain.
- Source inspection: Epic death exports end before a complete ground fall. Use local CC0 Death_D + LayToIdle for complete recovery, retaining CMU raw files as unused reference rather than claiming integration.
- Prepared 10 retargeted FBX clips and an editable blend, plus source hashes and a 21-reference player catalog. Initial UE import completed with the original courier mesh/skeleton files unchanged according to the importer; final provenance reimport remains pending.
- Updated native clip loading, serialized-default overrides, deterministic idle graph branch, recovery timeline and cancellation. Added automation cases for distinct hit counting, duplicate IDs, expired streak, interruption, large frame deltas and idle/move selection.
- User explicitly selected resource/code edits only, no build or tests. No native compilation, final graph connection, automation or game launch was performed. Stop at this requested boundary; do not describe runtime activation as complete.
- The final FBX exports must be reimported using the updated receipt format before running the supplied connection script. The script rejects stale imports and native binaries missing the new action enum.
- User expanded scope to NPCs and requested actual replacement rather than preparation. Generated all 10 NPC clips without original arm/recovery pose editing; imported them into their existing Reactions/Behavior addresses. Also replaced legacy NPC walk and 13 legacy player assets. Refreshed 10 player library assets: 34 saved resources total. The final importer completed successfully and verified character mesh/skeleton package hashes unchanged.
- The final resource reimport is now complete. New native state logic and Epic graph version 9 are still pending because the user has not authorized a build or tests. Resource-only imports did not compile native code/Blueprints, generate maps, package or launch a game.
- Retired manual-pose generator/importer entry points, removed synthetic arm/recovery editing from NPC authoring, and changed the base rider geometry generator to reuse retained library actions. Existing math helpers remain for legitimate retargeting/posture workflows.
- User then explicitly requested rebuild and run. Editor build passed, version 9 graph connection passed, and 44 focused automation checks passed (9 EpicAnimation, 29 CharacterActions, 6 Ambient.Combat). Updated an obsolete receiver test to let the hit streak expire before independently asserting rear standing recoil; three-hit knockdown remains covered by its dedicated cases. Launched the actual city map in a responding game window. No packaging. Interactive visual acceptance remains outstanding.
