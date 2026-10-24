# Git Branch Audit and Cross-Platform Integration Plan

Audit date: 2026-10-24

This document records a read-only comparison of `origin/master` and
`origin/feature/cross-platform`. No merge, cherry-pick, rebase, reset, or source
code change was performed as part of the audit.

## 1. Verified repository state

The comparison used freshly fetched remote branch metadata rather than the
stale local `master` branch.

| Item | Verified value |
| --- | --- |
| Current branch | `feature/cross-platform` |
| Current HEAD | `95105c47dc719f9d336c64d9c484a78e5273bc73` |
| Working tree | Clean before documentation was added |
| Local `master` | `da3e8141940c60811438afad54a2cccdd1acec8a` (one commit behind `origin/master`) |
| `origin/master` | `83c4bf5` |
| `origin/feature/cross-platform` | `95105c47dc719f9d336c64d9c484a78e5273bc73` |
| Remote fetch/push URL | `https://github.com/jimboburgess/pathfinderminiextreme_025.git` |
| Canonical GitHub repository | `jimboburgess/pathfinderminiextreme_hub` |
| Visibility | Public |
| Default branch | `master` |

The configured URL is a legacy repository URL that GitHub resolves to the
canonical repository. It was not changed during this audit.

### ESP32 checkpoint

`esp32-before-cross-platform` remains an annotated tag.

| Item | Verified value |
| --- | --- |
| Local tag object | `f1dc7a77427cbdceeb10b04c41980d626b4f2f55` |
| Local peeled commit | `c7c50cae3c8f26cd259cfa0f63a1bb035a0ce1b8` |
| Remote tag object | `f1dc7a77427cbdceeb10b04c41980d626b4f2f55` |
| Remote peeled commit | `c7c50cae3c8f26cd259cfa0f63a1bb035a0ce1b8` |

The local and remote annotated tag objects are identical. The tag still
protects the requested checkpoint commit, `c7c50ca`.

## 2. Branch topology

The exact common ancestor is:

```text
da3e8141940c60811438afad54a2cccdd1acec8a
```

The relevant history is:

```text
                     83c4bf5  origin/master
                    /
da3e814 -------------
                    \
                     c7c50ca -- 95105c4  origin/feature/cross-platform
```

Commits unique to `master`:

| Commit | Summary |
| --- | --- |
| `83c4bf5` | `Update README.md` |

Commits unique to `feature/cross-platform`:

| Commit | Summary |
| --- | --- |
| `c7c50ca` | `checkpoint: preserve ESP32 baseline before native extraction` |
| `95105c4` | `build: establish portable native C++ foundation` |

`git log --left-right --cherry-pick` found no patch-equivalent commits across
the two sides. The branches therefore contain distinct changes rather than
duplicated commits with different hashes.

## 3. Diff interpretation

The three-dot comparison, `origin/master...origin/feature/cross-platform`,
shows changes made on the feature side since the common ancestor:

```text
27 files changed, 5089 insertions(+), 147 deletions(-)
```

The two-dot comparison, `origin/master..origin/feature/cross-platform`, compares
the two tip trees directly:

```text
27 files changed, 5089 insertions(+), 148 deletions(-)
```

The additional deletion in the tip comparison is the hardware-list line added
to `master`'s `README.md`, which is not yet in the feature branch.

### File overlap

`README.md` is the only file modified on both branches. `master` adds
`A24 Round Tactile Button Caps` to the hardware list. The feature branch adds
links to the cross-platform and native-build documentation in a separate part
of the file.

No file changed exclusively on `master`: its only changed file is the shared
`README.md`. The feature-only changes include:

- `.gitignore` and `CMakeLists.txt`.
- Cross-platform, multiplayer, and native-build documentation.
- Bell puzzle, movement, input, combat, dungeon, display, and message code.
- Multiplayer activity, snapshot, and protocol code.
- Embedded Bell/audio/multiplayer tests and native portable tests.

## 4. Changes on `master`

The only post-divergence `master` change is documentation in commit `83c4bf5`.
It does not modify gameplay, firmware behavior, board support, memory usage,
networking, serialization, or build configuration.

| Area requested for review | Post-divergence `master` change |
| --- | --- |
| Character creation and progression | None |
| Combat and initiative | None |
| Spells and spell effects | None |
| Dungeon generation and transitions | None |
| Monster behavior and AI | None |
| Bell, brazier, and other puzzles | None |
| Loot, equipment, traps, and effects | None |
| ST7789 rendering and sprites | None |
| Encoder, buttons, audio, and piezo | None |
| RAM, PSRAM, and board support | None |
| ESP-NOW and multiplayer state | None |
| Snapshots and serialization | None |

The README hardware-list addition should be retained when the branches are
eventually integrated. It has no accompanying code dependency and does not
belong in the shared gameplay engine.

## 5. Changes on `feature/cross-platform`

Although the audit's primary question is what must be carried forward from
`master`, the feature-only work defines the integration and portability risks.

### 5.1 Bell sequence playback

Commit `c7c50ca` contains the Bell playback repair in:

- `src/dungeon/bellpuzzle.cpp` and `src/dungeon/bellpuzzle.h`.
- `src/map/playermovement.cpp`.
- `src/input/buttons.cpp`.
- `test/test_audio/test_main.cpp`.
- `test/test_bell_puzzle/test_main.cpp`.

The new Bell helpers validate the stored sequence, play it through the existing
audio sequence path, identify the listening rune, and edge-trigger playback
from the player's previous and current positions. Tests cover exact stored
order, maximum length, invalid state, replay, and manual bell sounds.

The validation and tone-sequence construction are portable gameplay concerns.
The audio sink and movement/UI callbacks are platform adapters. Future
extraction should preserve that division without redesigning the puzzle.

### 5.2 Multiplayer combat and independent rooms

Commit `c7c50ca` also contains the Stage 3 multiplayer work, primarily in:

- `src/multiplayer/multiplayer_activity.cpp` and its header.
- `src/multiplayer/network_protocol.cpp` and its header.
- `src/dungeon/combat.cpp` and its header.
- `src/dungeon/dungeon.cpp`.
- `src/map/playermovement.cpp`.
- `src/graphics/display.cpp`.
- `src/graphics/messagelog.cpp` and its header.
- `src/input/menu.cpp`.
- `docs/multiplayer-stage3.md`.

The implementation adds or extends:

- Independent player rooms and room-specific player masks.
- Authoritative room activation, restoration, transitions, and room effects.
- Activity-wide combat with stable combatant references.
- Authoritative attack, ability, movement, defense, and end-turn requests.
- Reliable combat events and acknowledgements.
- Shared gameplay messages and a visible current-combatant indicator.
- Multiplayer-aware combat participation and turn handling.
- PSRAM-backed participant, combat, and room-effect runtime storage.

This change is a combination of shared-engine behavior and ESP32 integration.
The independent-room model, stable identifiers, validated combat commands,
room revisions, deterministic resolution rules, and bounded codecs belong in
the eventual portable engine. ESP-NOW routing, Arduino timing and randomness,
PSRAM allocation, TFT redraws, piezo playback, and existing global-state room
inflation remain platform-specific concerns.

The large `multiplayer_activity.cpp` unit should not be copied into a server as
a second engine. Its portable rules should be extracted incrementally behind
small interfaces, with the ESP32 continuing to compile the same rule sources.

### 5.3 Network protocol

Commit `c7c50ca` changes the multiplayer protocol from version 3 to version 4.
It expands the encoded character state from 30 bytes to 177 bytes and adds
equipment, abilities, condition state, room membership, transitions, shared
messages, and combat request/event/acknowledgement payloads.

The protocol uses bounded explicit codecs rather than treating in-memory C++
struct layout as the wire format. This is a good basis for Linux and browser
interoperability, but it is a confirmed compatibility boundary: protocol-v3
firmware on `master` cannot participate in a protocol-v4 multiplayer session
with the feature firmware. All ESP32 peers in a session must run a compatible
protocol version.

No save-game implementation or persisted save structure changed between the
branches. There is therefore no confirmed save-file format change in this
integration. Future work should keep legacy save/NVS layouts stable and add
explicit migration only when deliberately approved.

### 5.4 Native foundation

Commit `95105c4` establishes a CMake C++11 native target that builds the same
production sources used by the ESP32 project:

- `src/dungeon/riddles.cpp`.
- `src/multiplayer/multiplayer_snapshot.cpp`.
- `src/multiplayer/network_protocol.cpp`.
- The header-only `src/input/encoderdecoder.h`.

`tests/native/portable_tests.cpp` executes ten meaningful tests covering
protocol encoding and validation, snapshot ordering and bounds, movement
sequence wraparound, deterministic spawn search, riddle data/persistence, and
encoder detents and bounce handling. No gameplay module was copied or moved.

This commit also adds focused build documentation and ignores CMake-generated
build directories. It proves that selected production leaf modules compile and
run natively; it does not yet prove that the entire engine is portable.

## 6. Read-only merge assessment

The audit used Git 2.42's non-mutating merge analysis:

```text
git merge-tree --write-tree --messages \
  origin/feature/cross-platform origin/master
```

The command exited successfully and produced candidate tree
`9b5468e7c85a73ef4da3e2dd0ccac7e7758067ed`. Git reported only:

```text
Auto-merging README.md
```

There are no confirmed textual conflicts. Inspection of the candidate tree
shows that it preserves both the feature documentation links and the
`master` hardware-list addition. Relative to the feature tip, the candidate
merge contains one README insertion.

The command did not update a branch, index, or working-tree file.

### Confirmed compatibility findings

- The README changes merge automatically.
- No shared source file has divergent edits on both branch tips.
- No save-file format change is present in the compared commits.
- The feature branch contains the Phase 2 native foundation and the full ESP32
  baseline represented by the checkpoint commit.

### Risks requiring build or runtime verification

These are not Git conflicts, but they matter before integration:

1. **ESP-NOW protocol compatibility:** protocol-v3 and protocol-v4 devices are
   not mutually compatible. Mixed firmware versions must be rejected cleanly
   or upgraded together.
2. **Packet size and airtime:** encoded character state grows from 30 to 177
   bytes. It remains bounded, but ESP-NOW airtime, retry behavior, and snapshot
   pressure need regression coverage.
3. **Internal RAM margin:** the Phase 1 measurement for the feature firmware
   was 292,704 of 327,680 bytes (89.3%) of internal RAM and 1,048,765 of
   4,128,768 bytes (25.4%) of flash. Avoid duplicate session objects, STL heap
   growth, and portable abstractions with hidden allocation on the ESP32.
4. **Global authoritative state:** `Dungeon`, `Combat`, entities, player state,
   and temporary room inflation still depend on globals. A Linux server cannot
   safely host multiple sessions until a later, carefully staged context
   boundary exists.
5. **Deterministic behavior:** authoritative combat and activity code still
   consumes platform/global randomness. Extraction must preserve probability
   distributions and random-call order on the ESP32.
6. **Single-player regression risk:** multiplayer guards now share combat and
   movement paths with single-player behavior. Both modes need continued
   compile and behavioral tests.
7. **Platform coupling:** the native build deliberately excludes the large
   multiplayer activity implementation because it still couples rules to
   Arduino, ESP-NOW, display, audio, globals, and PSRAM.
8. **Deliberate Stage 3 limits:** monster turns, abilities, and other documented
   limitations should not be mistaken for merge regressions or silently
   redesigned during portability work.

No ABI conflict is confirmed because the network protocol uses explicit
serialization. Changes to in-memory network structs still require size and
stack/heap review on ESP32.

## 7. Branch strategy options

### Option A: long-lived parallel branches

Keep `master` as ESP32-only and `feature/cross-platform` as the portable line,
then merge or cherry-pick periodically.

Advantages:

- Maximum short-term isolation for known working ESP32 firmware.
- Cross-platform experiments cannot immediately destabilize `master`.

Disadvantages:

- Highest continuing merge cost.
- Gameplay fixes can land on only one side and diverge silently.
- Protocol and serialization changes become increasingly difficult to align.
- Encourages two authoritative gameplay engines, contrary to the project goal.

This is acceptable only as a short transition and is not recommended as the
permanent workflow.

### Option B: unified mainline

After the cross-platform foundation passes the full ESP32 and native gates,
integrate it into `master`. Keep PlatformIO and CMake side by side and develop
both platforms from short-lived branches based on one mainline.

Advantages:

- One source of truth for gameplay code and protocol definitions.
- Every shared fix reaches ESP32 and native builds through normal development.
- Lowest long-term merge and coordination cost.
- CI can prevent either platform from being broken by a shared change.

Disadvantages:

- Requires strong build, test, size, and review gates before merging.
- Early portability work must remain incremental to protect ESP32 constraints.

This is the recommended permanent strategy.

### Option C: permanent shared-engine branch

Maintain a third branch as an integration layer between platform branches.

Advantages:

- Can stage high-risk extraction work before either platform consumes it.
- May help temporarily if multiple teams work independently.

Disadvantages:

- Creates a third synchronization path and another possible source of truth.
- Routine fixes require coordinated merges across multiple long-lived branches.
- Delays discovery of platform integration failures.

A temporary integration branch or a pull request's generated merge ref can be
useful for validation, but a permanent shared-engine branch is not recommended.

## 8. Recommended workflow

Use Option B once the existing feature branch has passed a final integration
gate. The authoritative gameplay source should then be unified `master`, not an
ESP32 branch, server branch, or copied browser implementation.

Recommended transition:

1. Preserve `esp32-before-cross-platform` permanently and do not move it.
2. Open a pull request from `feature/cross-platform` to `master` when approved.
3. Build the pull request's merged result, not only each branch tip.
4. Require the normal ESP32 firmware build, all embedded compile-test targets,
   the native CMake build, all native tests, `git diff --check`, and an explicit
   RAM/flash comparison.
5. Review the protocol-v4 rollout requirement before using multiplayer with
   older devices.
6. Merge only after those gates pass; then make the updated `master` the common
   base for Phase 3.
7. Retire the long-lived feature branch after a stabilization window. Preserve
   history through the merge and preserve recoverable firmware through tags.

After unification:

- Develop ESP32 hardware changes on short-lived branches from `master`.
- Develop portable rules and server-facing abstractions on short-lived branches
  from the same `master`.
- Put platform-dependent implementations behind narrow compile-time interfaces;
  do not fork or duplicate rule implementations.
- Require native tests for portable rules and ESP32 compile/tests for any shared
  code change.
- Keep browser code focused on presentation, input, and the network protocol.
  The authoritative server should use the shared C++ rules. A future WebAssembly
  build may reuse selected client-safe code but should not become a second
  authority.
- Create annotated known-good ESP32 release tags after verified milestones.

### Merge versus cherry-pick

Use reviewed pull-request merges for coherent development lines and for the
eventual feature integration. This preserves context and keeps all related
protocol, tests, and documentation together.

Before unification, use cherry-pick only for an urgent, isolated fix that must
reach both branches immediately. Record and reconcile it promptly so it does
not become duplicated or divergent work. Do not cherry-pick the intertwined
Stage 3 or native-foundation commits piecemeal.

After unification, routine cross-platform fixes should not need cherry-picking.
Cherry-picks should be limited to explicitly maintained release/hotfix branches.

## 9. Phase 3 starting point

The safest next implementation step after integration approval is a small
portable boundary around one already-tested rule path, not a wholesale global
state rewrite. A suitable sequence is:

1. Keep the existing production sources in place.
2. Extract pure command validation and resolution from one multiplayer action
   path while preserving exact protocol-v4 behavior.
3. Pass time, random values, messages, and transport effects through narrow,
   allocation-free interfaces only where that path requires them.
4. Compile the same rule source under PlatformIO and CMake.
5. Add golden-vector and deterministic behavior tests before selecting the next
   path.

This approach grows one authoritative shared engine while preserving the
original 240x240 UI, encoder controls, audio, sprites, board support, saves, and
ESP-NOW integration as ESP32 platform adapters.

Phase 3 should not begin until the branch integration and full merged-result
verification are explicitly approved.
