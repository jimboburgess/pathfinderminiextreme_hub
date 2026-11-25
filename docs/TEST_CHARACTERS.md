# Test characters

`PFME_ENABLE_TEST_CHARACTERS=true` enables an optional character-creation
shortcut in native/server builds. It defaults to disabled. Physical builds
also default to disabled and may opt in with the compile definition
`PFME_ENABLE_TEST_CHARACTERS_DEFAULT=1`.

The authoritative creation runtime recognizes an exact, case-insensitive title
and separating space at the beginning of a newly accepted name:

| Prefix | Starting level |
|---|---:|
| `Sir ` | 5 |
| `Earl ` | 10 |
| `Duke ` | 15 |
| `King ` | 20 |

The title remains in the saved/displayed name and counts toward the existing
15-character limit. The runtime advances through the production Medium-XP
progression, restores derived resources, and grants a bounded test equipment
package. Loading, reconnecting, autosaving, and entering Town do not run this
creation-only adapter.

## Equipment packages

All classes keep their normal starting weapon types. Melee and ranged weapons
receive `+1`, `+2`, `+3`, or `+5` enhancement at levels 5, 10, 15, or 20.
Fighters and Clerics receive enhanced Breastplate, Banded Mail, Half Plate, or
Full Plate plus a matching Heavy Steel Shield. Rogues receive enhanced Studded
Leather at level 5 and an enhanced Chain Shirt thereafter, plus Masterwork
Thieves' Tools. Wizards remain unarmored. Each tier grants four matching cure
potions; Wizards and Clerics also receive 3, 5, 6, or 8 Mana Potions.

Higher-level Wizards use the normal `learnAbility` spellbook path with a
deterministic testing selection bounded by spell level: Scorching Ray,
Fireball, Haste, Lightning Bolt, Wall of Fire, Chain Lightning, and Mind Blank.
Cleric spell access continues to derive from the production divine
progression table.
