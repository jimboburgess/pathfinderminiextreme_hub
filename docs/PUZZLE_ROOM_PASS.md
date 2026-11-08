# Puzzle feedback and Rogue bypass pass

This pass builds on `8f8d3e8` (the deployed multiplayer reliability release). It adds no puzzle types, networking endpoints, browser gameplay implementation, or character-save fields. The live reliability release is independent of this puzzle source checkpoint; committing/pushing this work does not deploy it or reset a running adventure.

## Changed sources

- `src/dungeon/bellpuzzle.{cpp,h}`: compact shared visual timing, sequence replay, and player strikes.
- `src/dungeon/roomdraw.cpp`: original C++ bell marks, replay-button border, owner-specific Number tile highlight.
- `src/main.cpp`, `apps/runtime/runtime.cpp`: update visual timing before original display rendering.
- `src/map/interaction.{cpp,h}`: authoritative physical Rogue exit interaction and reusable bypass validation.
- `src/dungeon/{dungeon.{cpp,h},puzzleroom.cpp,brazierpuzzle.cpp,riddlepuzzle.cpp}`: room-owned bypass persistence, initialization, and passage checks.
- `src/dungeon/numbertilepuzzle.cpp`, `src/data/entities.h`: cardinal path bridges, physical exit approach, independent native crossing progress, safe occupied landing.
- `src/dungeon/puzzleinstructions.cpp`: explain the physical Bell replay button.
- `apps/runtime/{CMakeLists.txt,puzzle_pass_tests.cpp}`: six production-runtime/render/generation test modes.
- `tests/native/portable_tests.cpp`: match the updated Bell instruction.
- `test/test_bell_puzzle/test_bell_puzzle.cpp`, `test/test_number_tile_puzzle/test_number_tile_puzzle.cpp`: minimal embedded fixture globals/stub needed by the production calls.
- This document records behavior and verification.

## Bell feedback

The existing sprites and original 240x240 C++ renderer are retained. Four short white drawLine primitives (two on each side) fit inside the bell's 16x16 tile. Only the ringing bell changes. Start/end dirty that tile; no new full-screen redraw loop is introduced. Native multiplayer retains its existing observer refresh at 20 Hz.

A room-owned timestamp selects the stored sequence note: **300 ms tone, 160 ms silence**, then the next note. Player strikes use the same 300 ms pulse. Mute does not suppress information. Expired pulses disappear, including for late/reconnecting observers. The sequence is not regenerated per observer. A later strike/replay replaces the current visual playback, matching the existing tone-sequence interruption behavior.

The original replay rune remains in its original position with a visible button border. Stepping on it still replays; A while facing it or standing on it replays without leaving the room. Entry explains: "Match the tones. Use the replay button, then ring the same order."

Sequence length remains unchanged: levels1-3:3 notes;4-6:4;7-10:5;11-15:6;16+:7. Normal solving still presents the original key, which is collected and used at the locked exit. No key/reward flow was removed. The authoritative visual lifetime is exactly 300 ms; rendered visibility is quantized by display updates. Browser network/audio scheduling can add jitter; this is not a promise of sample-accurate cross-browser audio synchronization.

Eight timing bytes fit in existing room entity-storage union slack, guarded by the legacy-size static_assert. The bypass flag shares existing room flag storage. ESP32 static RAM remains unchanged; no framebuffer or large sprite was added.

## Rogue-only physical exit bypass

Bell, Number Tile, Brazier/Lights and Bertram/Riddle exits all offer Pick Lock to a living, active Rogue physically facing that room's locked exit. Normal chest lock rules are unchanged. Validation repeats on execution: actual controlled actor pointer, class, room, target exit, proximity, locked state and no active combat. The browser still sends original input actions; it cannot submit a check total, player identity, room ID or bypass result.

The real action rolls the existing d20 plus **the acting character's existing Disable Device skill and tool modifier**. That skill currently uses Intelligence; no new statistic was introduced. The centralized Bertram formula remains `19 + ceil(level / 2)` after clamping level to 1-20: DC20 at level1, DC29 at level20. Existing bare-hand (-4), standard (0), masterwork (+2), crowbar (+2), natural-1 failure and tool-break rules are retained. No best-party or remote Rogue is substituted.

Success sets a shared, persistent `puzzleBypassed` flag and permits passage. Physical Bell/Number/Brazier solution state remains unchanged. The Brazier optional chest still requires actual all-lit completion. No bonus key, XP or loot is synthesized. Bertram retains the existing first-attempt "Hey! What are you doing?", passage completion, cat retirement and no-new-key bypass behavior; riddle answers/extra guesses/rewards are otherwise untouched. Failure preserves the locked, normally solvable room and existing retry/tool feedback.

The Number field has a narrow perimeter approach so a Rogue can reach the physical exit lock without first solving the crossing. Walking around the field alone does not solve it or unlock the exit. All party members see a successful shared bypass without teleporting.

## Number Tile audit and fixes

**Before:** already a production puzzle, not a mock or missing implementation. The actual dungeon selector registered it; tiles, 25 rules, plaque, clue text, valid/invalid movement, completion, exit lock and room persistence existed. However, generated stateless path shifts could connect only diagonally while solvability validation/cardinal movement required a bridge. Native stateful crossing progress was shared between players. An occupied reset landing could overlap a peer.

**After:** a connecting safe tile is placed at each stateless lateral shift, using existing roll-array values without another world RNG draw. The finished room must pass cardinal solvability and connectivity checks. Stateful generation tries at most 12 seed pairs, then uses the existing deterministic fallback. Invalid room geometry still rejects that candidate and uses the existing rotating-puzzle fallback; it never silently creates an unsolvable Number room.

Native stateful crossing prefix/previous/current digits now belong to each full Entity. Moving through a bypassed field cannot convert an unfinished crossing into a fake physical completion. Digits, rule, seeds, clue, failed-tile feedback and completed/bypassed state remain authoritative room state. ESP32 keeps the existing single-player traversal storage. The original tile renderer selects the controlled character's progress. A wrong stateful step resets only that character, selects a free near-side landing if occupied, and never relocates its peer. Stateless invalid movement is rejected in place. Neither penalty directly removes HP.

Successful real movement across the numbered field and off its far edge completes the room and unlocks passage for everyone. Leaving/re-entering preserves exact layout, rule, seeds and completion; the generator is not rerun for existing rooms. Character save files remain separate from dungeon persistence.

A visible plaque uses the existing interaction/message UI and shows the chosen rule. Stateful clues also state the starting digit(s). Digits use the existing small font on the 240x240 C++ framebuffer. The existing entry message remains "Follow the rule. Cross using only the correct numbers." and appears once per unsolved room entry.

### All 25 existing rules, unchanged

Stateless: odd; even; only3; only6; greater than3; less than4; multiples of2; multiples of3; prime; composite; multiples of4; greater than5; less than5; inclusive3..7; divisors of12; divisors of18; square is even; square is odd; exactly7.

Stateful, modulo10: double+3; triple+2; square+1; previous+current; alternate +3/double; double-1. Eligibility and difficulty remain existing rules (stateful begins at level10, additional rules at14/18).

Digit ranges: level1-4:1..6; level5-9:0..7; level10+:0..9. Stateful fields use0..9. Stateful selection probability remains25% at10-13,40% at14-17,50% at18-20.

### Reachability and distribution

The unchanged selector weights are Bell34/Number33/Brazier33. Bertram remains a separate pre-boss puzzle. Middle-room category weights remain30%combat/30%ambush/30%puzzle/10%empty. **These weights are not equal observed generated frequencies:** existing geometry requirements/fallbacks favor Braziers. Number requires an opposite doorway pair. The deterministic 640-dungeon production sample yielded Bell337, Number28, Brazier670 and Bertram640;6 Number rooms were stateful. No PUZZLE_NONE broken puzzle was accepted. Rebalancing geometry/distribution is deliberately deferred.

## Verification

Windows Release build and full CTest: **82/86 passed**, plus the subsequently added Number generation matrix passed. The final focused native/headless/puzzle regression run passed8/8 after the last two localized bypass guards. All six new puzzle modes pass. The four failures were reproduced on the prior baseline: `pfme_town_combat_1`, `pfme_town_combat_3`, `pfme_town_combat_4`, `pfme_party_combat_1`. Their existing combat/return-to-Town defects are not hidden or changed by this pass.

Ubuntu GCC15.2.0 clean Release build / full suite: **77/81 passed**, with exactly the same four baseline combat failures (647.96 seconds). All six new puzzle modes passed. An initial interrupted incremental build retained stale objects after a source archive update; its extra camera/exploration failures were discarded as invalid build evidence. A fresh independent build directory resolved those failures without changing gameplay routing or weakening tests. The final bypass-preservation guard and fixture changes were then rebuilt on both platforms; the eight focused native/headless/puzzle regressions passed on Windows and Ubuntu.

The six puzzle modes exercise:

1. Real original C++ framebuffer pixels: four side marks only on the active bell; at299ms still present, at300ms removed; muted strikes/replay; stored notes and pauses; expiry; standing-on-button A replay.
2. All four puzzle bypasses: non-Rogue/foreign/remote/combat rejection, real contextual menu, real natural-1 failed Disable Device action/retry, real successful roll, shared flag, unchanged physical states and sealed optional Brazier reward.
3. Actual production generation of640 dungeons across representative levels, valid Number paths/ranges/clues, all puzzle types, no empty puzzle, unchanged selector weights.
4. **5,120 Number configurations**, levels1-20, four crossing directions,64 deterministic inputs including all-zero fallback input: all25 rules observed, valid digits, cardinal solution, unsolved physical exit approach.
5. Actual production-generated Number rooms at levels1/12/20 (two stateful): real entrance/load, plaque, nonlethal invalid movement, original tile-by-tile tryMovePlayer solution, exact re-entry persistence and permanently unlocked solved exit.
6. Two original C++ controllers: different rooms/framebuffers, same Number layout/clue/completion, independent crossing progress, occupied reset landing, no peer teleport, shared muted Bell timing, local Rogue vs remote Fighter ownership.

Existing browser pixel-codec, browser input/reconnect/audio, authenticated WebSocket and 1-4-player wire regressions are included in native CTest where their prerequisites are available. Windows ran the JavaScript browser regressions; Ubuntu has no Node.js executable, so CMake omits those six JavaScript-only tests. No packages were installed. No browser-only puzzle renderer or HTML puzzle aids were added. The new puzzle marks/interactions are verified in the actual production C++ framebuffer/runtime, **not yet manually played through the hosted website**. The owner's manual website test confirmed the deployed reliability release's movement/audio/refresh/reconnect, not this undeployed puzzle revision. Browser automation could not verify Chrome's current URL and was stopped; no alternate automation bypass was used.

`pfme_headless` built and ran with `status=ok` on Windows. Ubuntu also ran with `status=ok`.

ESP32 release firmware compiled; all26 existing embedded compile-only targets passed. No firmware upload or serial access. RAM **292,704/327,680 bytes (+0)**; flash **1,072,365/4,128,768 bytes (+2,528)** versus reliability1,069,837. These are compile results, not a physical handheld playtest. Saves and ESP-NOW protocol formats are unchanged. Live ESP-NOW handheld synchronization was not exercised. The new shared Bell visual timestamp and explicit bypass flag are authoritative in the native browser runtime; this pass does not add them to the existing ESP-NOW room-detail wire format, so equivalent new feedback on remote physical handheld peers is not claimed. That needs a separate compatibility review and physical-device test before advertising this feature across ESP-NOW peers.

## Git and deliberate limits

Only the puzzle files listed above are included. Unrelated lag/profiling edits remain preserved and unstaged in the main checkout. No generated build files, credentials, server configuration, package installation or live service changes belong to this commit. Final git diff --check and canonical-master publication are recorded in the delivery report; no merge/rebase/force/tag push is used.

Deferred: existing baseline combat failures, geometry/distribution rebalancing, physical ESP32/ESP-NOW new-feedback verification and manual hosted puzzle playtests, durable whole-dungeon saves, and activation of this puzzle revision. No new puzzle types or browser gameplay engine were introduced. The reliability release remains running until a separate game deployment is requested.
