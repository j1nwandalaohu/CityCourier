# Player and NPC library animation replacement

Initial replacement validation: **34 animation resources replaced, Editor built, version 9 animation graph generated, and 44 focused automation checks passed.** The user subsequently authorized rebuilding and running. The game was launched in a responding CityCourier window using the current project and `/Game/CityCourier/Maps/L_CourierCity`. No packaged build was requested. Automated graph/foot-motion/reaction checks passed; interactive visual acceptance remains outstanding. Evidence: `library-action-runtime-validation.json`.

The graph version 10 follow-up separated forward jog and sprint, matched playback cadence to movement speed and removed horizontal foot-IK dragging. See [the gait correction record](../locomotion-gait-fix.md) for its evidence. The latest version 11 work preserves running legs and speed during punches, replaces the broad heavy hook with a retimed library cross, and orders library flight phases by physical jump state; see [the combat/jump correction record](../combat-jump-continuity-fix.md) for its validation status. The version 9 JSON remains the historical replacement validation record.

Scope includes the player, current CharMorph NPCs and the legacy NPC walk asset. Existing references retain their original package addresses and now read library animation data. `all-character-animation-replacements.json` records every overwritten/imported package, source and saved hash. Character meshes and skeleton package hashes remained unchanged during the importer.

| Player motion | Library source | Change |
| --- | --- | --- |
| Idle | Mesh2Motion `Idle_A`, CC0 | Replace original idle; add explicit stationary branch in the Epic graph |
| Jog/sprint | Mesh2Motion `Jog` / `Sprint`, CC0; GASP while Epic graph is active | Retain existing library motion |
| Ride | Mesh2Motion `Driving`, CC0 | Replace original riding pose; this is a seated driving source, so scooter seat/grip alignment still needs visual validation |
| Jump start/flight/descent/landing fallback | Mesh2Motion `Jump_Start` / `Jump_air` / `Jump_Land`, CC0 | Replace original pose-edited clips; retain authoritative capsule trajectory |
| Epic flight and landing | Mesh2Motion flight phases; GASP landing | Explicit takeoff/ascent/descent; retain standing/moving landing selection |
| Light left/right, heavy punch | Mesh2Motion `Punch_Jab` / `Punch_Cross`, CC0 | Heavy uses the compact cross retimed to .95 s, with contact at .40 s; the previous hook is retained as source reference |
| Front/back hit | Content Examples `MM_HitReact_Front_Med_01` / `Back_Med_01` | Replace originally edited recoil poses with directional source motion, retimed to existing .4-second gameplay reaction |
| Fall / get-up | Mesh2Motion `Death_D` / `LayToIdle`, CC0 | Add complete fall, .6-second ground hold and automatic get-up |
| Run start/stop fallback | GASP sprint start/stop | Replace synthesized run transitions, resampling playback to the existing transition clock |
| Crouch/prone | Mesh2Motion crouch/crawl, CC0 | Retain existing library retargets; prone idle holds a library crawl frame |
| Vault/mantle | GASP | Retain existing library montages and motion warping |

| NPC motion | CC0 library source | Replacement |
| --- | --- | --- |
| Idle / walk / run | `Idle_A` / `Walk` / `Sprint` | Direct proportional retargets at existing paths |
| Guard / cower | `Fighting Idle` / `Shivering` | Direct library poses |
| Left / right punch | `Punch_Jab` / `Punch_Cross` | Remove original two-bone arm trajectory; align source extension to the existing .30-second contact time, mirroring only when the striking side requires it |
| Hit | `Hit_Chest` | Remove hand-shaped recoil envelopes |
| Fall / get-up | `Death_D` / `LayToIdle` | Matching complete supine clips; remove synthesized death/recovery key poses |

NPC duration contracts remain unchanged: hit 29/60 seconds, fall 1.05 seconds, get-up 2.7 seconds and punches .8 seconds. Existing NPC reaction/collision and hit sampling logic therefore consumes the replaced clips at its existing times. Root locking, proportional retargeting and grounding remain necessary adaptation, not original body motion authoring. No handwritten aiming curve is added back into the punches.

Normal hits retain a standing front/back reaction. Three distinct accepted strikes with at most 1.2 seconds between strikes trigger knockdown. Duplicate strike GUIDs do not count. Attacks and movement are blocked during recovery. Cancellation, UI interruption, loss of possession or airborne interruption clears the sequence and hit streak. Missing replacements do not fall back to original authored clips. The count and timing are implementation defaults; the user specified continuous hits followed by automatic recovery.

The primary GASP graph retains its ground/air matching. The added idle branch selects the library idle when grounded speed is at most 10 cm/s; this prevents a search result from displaying running while stationary. The idle branch was evaluated in the version 9 and version 10 automation runs.

Stationary-running follow-up: the saved generation receipt is graph version 8, and the old generator reused version 8 without rebuilding the newly added idle branch. The required graph version is now 9 in both native defaults and generation scripts. A loaded old graph is rejected by player configuration validation so it cannot use the new posture index against an old blend list. The fallback player clips still select the library idle. The idle threshold includes exactly 10 cm/s, matching the legacy movement threshold. Automation assertions now check evaluated foot motion, stop/restart, the threshold boundary and rejection of old graph defaults; these assertions subsequently passed after explicit build/test authorization. Version 10 retains this protection.

Player editable sources: `AssetsSource/CityCourier/Blender/CourierLibraryActions.blend`, exports under `AssetsSource/CityCourier/Exports/LibraryActions`, and source hashes in `AssetsSource/CityCourier/library_actions.json`. Final exports have been reimported at both `/Game/CityCourier/Animation/Library` and legacy player addresses. NPC editable sources are `AssetsSource/CityCourier/Ambient/CharMorph/CitizenLibrary.blend`, with all 10 FBXs and source manifests under `Reactions` and `Behavior`. `library-action-replacements.json` records reference targets and remaining native/graph work.

`tools/art/build_citizen_reactions.py` now delegates to the library-only NPC generator. The base rider geometry generator reuses library actions rather than recreating sine-wave walks or hand-positioned idle/riding poses. Old hand-edited idle/jump/pending-action entry points explicitly stop with the replacement command, preventing accidental restoration. Historical helper functions remain for existing posture authoring utilities.

The rebuild/connection sequence was completed after the user authorized it. For future authorized iterations, the order is:

1. Build the current Editor target.
2. Resource import is complete. If authoring changes again, run `Content/Python/replace_all_character_actions.py`; it resumes completed imports using source/package hashes.
3. Run `Content/Python/connect_library_actions.py` to regenerate the Epic graph, update legacy defaults, revalidate the player and record the changed output hashes.
4. Run focused CharacterActions and EpicAnimation automation and inspect rendered idle, air, hit, fall/get-up, and scooter contacts. The focused automation runs completed; full interactive contact acceptance remains outstanding.

Keep UE operations sequential. Do not package or launch a game without a new explicit user request.

The previously downloaded CMU ASF/AMC files and Epic death examples remain reference material. Epic death exports do not contain a complete grounded fall, so they were not used as the final fall/get-up pair. CMU clips were parsed but are not claimed as converted or integrated. Content Examples motion retains its acquired Epic/Fab license and is not relabeled CC0.
