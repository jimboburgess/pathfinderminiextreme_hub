# Account-owned character saves

Browser characters are stored on the NUC, not in browser storage. Each verified
account has three slots. A username/password change does not change its stable
`acct-<UUID>` ownership principal. Recreating an account with the same username
does not grant access to the deleted account's files.

## Player workflow

In Town choose **Stay Home**. The original 240x240 C++ Home screen contains Rest,
Save Game, Load Game, Character Sheet and Back, and shows the current character's
name and level. Save/Load lists all three slot names and levels.

1. Create a character normally, then use **Save Game** to assign it an empty slot.
   Replacing an occupied slot requires confirmation (No is the default).
2. **Load Game** selects an existing character. In Town, confirm the switch; the
   outgoing character is saved first. The existing title-screen Load shortcut is
   also retained for returning players after a server restart.
3. **Load Game > New Character** saves the outgoing character, then opens original
   character creation. Save the new character in a different slot or explicitly
   overwrite an existing slot. A new, unassigned character must first be saved
   before another switch; it is never silently discarded.
4. An assigned character is updated in its own slot, not copied to multiple slots.
   Manual Save is available in Town. Autosave runs every 30 seconds of runtime
   ticks, on disconnect, on safe controller release and on normal runtime shutdown.

Switching is allowed only in Town (or initial Load from the title), without an
active world actor, shared combat, rest or a pending trade. Finish those actions
and return to Town first. The production Return to Town action moves only that
character. Loading does **not** abort combat, generate/reset a dungeon, advance
world time, change room contents, move another actor or change party membership.
The account retains its authenticated controller/ownership capability. An
existing party member can use Resume Dungeon (or Enter Forest for an active
forest) to return with the loaded character at the safe entry area, while peers
remain where they are. This resumes only that member, never regenerates the
world, and is rejected during shared combat, a pending trade, defeat or a
completed/unavailable run. Initial party launches still require the leader and
all members ready in Town. New party membership still follows the existing
join-code/Town admission policy; this does not bypass locked-party admission.

Saves are character records, not adventure/world snapshots. A new server process
still starts a new in-memory world; loading restores a character in Town, not an
old room or dungeon. Abrupt termination can lose changes since the last successful
save (normally up to 30 seconds). Separate characters' files and hourly archives
are not a transactional world checkpoint; a hard crash during a multi-character
operation is not an exactly-once trade guarantee.

## State and isolation

The existing data-only character serializer stores name/class, level/XP, ability
scores, HP, MP, inventory/gold, equipment, known abilities, spell learning and
Fighter specialization. Native account format 11 additionally stores conditions,
timed effects and remaining channel-energy uses, so switching cannot clear a
curse or refill spent class resources. Runtime pointers/initiative membership are
reconstructed, never deserialized. Old character versions remain readable.
ESP32 keeps its original version-10 save bytes and menu behavior.

Only Service's verified `ownerPrincipal` binds the account namespace. Neither
numeric player IDs, character names, cookies sent directly to the backend, nor a
shared development password select account storage. Production requires:

```yaml
services:
  game:
    user: "999:980"
    environment:
      PFME_SAVE_DIR: /saves
      PFME_REQUIRE_ACCOUNT_SAVES: "1"
    volumes:
      - characters:/saves
volumes:
  characters:
    external: true
    name: pfme-friends_characters
```

This supplements the existing protected `/back` socket and read-only `/policy`
mounts. Root filesystem remains read-only, network mode remains none, all Linux
capabilities remain dropped and no-new-privileges remains enabled. The game gets
no HDD, account DB, host home directory, Docker socket or network access.

The volume and account directories must be owned by runtime UID 999 and mode
0700; files are 0600. Account principals are hex-encoded into fixed namespaces;
only three fixed slot filenames are accepted. Linux uses descriptor-relative
openat/mkdirat, O_NOFOLLOW, regular-file/link/owner/mode/size checks, exclusive
bounded temporary files, fsync and atomic rename. Storage errors return a Save
error rather than disconnecting the browser. Save-menu text is per-controller;
another user's slot names are not exposed by switching render contexts.

The external volume survives container replacement and `docker compose down -v`.
Do not delete it or attach it to unrelated services. Never reuse the account DB
volume as writable game storage.

## Samsung HDD backups

Verified target: `/mnt/samsung-hdd/pathfinder-backups`, filesystem UUID
`47d5e89e-9753-4420-be55-347a950371cd` on the Samsung HM640JJ. Its directory is
root-owned 0700. The owner authorized changing the existing HDD from read-only to
read-write; existing disk contents were preserved. The corresponding fstab entry
uses rw,nosuid,nodev,noexec,nofail. The previous fstab was backed up before editing.

`apps/accounts/backup-characters.sh` is a bounded, one-shot Docker helper. It:

- Checks the exact HDD UUID and mountpoint; absence fails closed rather than
  writing backups onto the OS drive.
- Uses the pinned local accounts image solely for its Python runtime, with no
  network, read-only root, bounded CPU/memory/PIDs and no-new-privileges.
- Mounts character storage read-only and only the private backup directory writable.
  The game never sees the backup disk. DAC_OVERRIDE is scoped to this separate
  administrative helper; it has no host services, credentials or Docker socket.
- Archives only known regular character files. It skips transient atomic-save
  filenames and rejects unexpected entries, symlinks and path traversal.
- Records per-character SHA256 hashes and an archive checksum, verifies each
  completed archive, and retains the newest 336 archives (about 14 days hourly).

The current user's existing cron service runs it at minute 17 of every hour:

```sh
python3 /home/jimbo/pathfinder-friends/configure-character-backups.py install
sh /home/jimbo/pathfinder-friends/backup-characters.sh
```

The install command first proves a backup succeeds and then adds only its marked
entry; the existing UniFi job is preserved. The last run's bounded output is in
`/home/jimbo/pathfinder-friends/character-backup.latest.log`. No passwords, cookies
or account database contents are included. Permissions protect the unencrypted
archives; disk-level encryption and off-device backup are separate concerns.
A backup on the same NUC does not protect against loss of the whole machine.
Retain the existing account database separately: these character-only archives
exclude password hashes and the username-to-stable-account mapping. Recreating
an account creates a new UUID, so disaster recovery must preserve or explicitly
verify and restore the original ownership mapping.

Disable only this job with:

```sh
python3 /home/jimbo/pathfinder-friends/configure-character-backups.py remove
```

## Deployment and recovery

Storage and backup preparation do not attach `/saves` to a running container.
Deploy the verified game image and updated Compose together only after adventure
restart approval. Update **both** PFME_GAME_IMAGE and PFME_GAME_IMAGE_ID, preserve
other image pins, validate Compose, and recreate only `game` with `--no-deps`.
Accounts, invitations, tunnel, status and all unrelated services stay in place.
The current unsaved RAM-only adventure cannot be recovered across that restart.

Before activation, inspect effective mounts, user, network mode, read-only root,
capabilities, admission policy and save directory permissions. Verify the release
binary checksum. After activation, create/save/load in Home, refresh/reconnect,
and verify a second account cannot see the first account's slot names. Check an
HDD backup contains actual player files; a successful empty-volume backup is not
evidence of saving production characters.

For recovery: stop **only** the game after approval, verify an archive checksum,
restore with `character_backup.py restore ARCHIVE EMPTY_DIRECTORY` in the scoped
administrative helper, then verify native Load from the restored files. Restore
requires an empty target and restores UID999:GID980 / 0700 / 0600. Never extract
untrusted tar paths or restore over a live volume. Prefer a new external recovery
volume and retain the original until validated. Rollback never removes character
or backup volumes; rolling back an executable does not restore an old RAM world.

## Verification and known limitations

`pfme_account_characters` exercises real production creation, Home Load, three
slots, fourth-slot denial, account separation, no character cloning, latest-state
flush, reconnect and stale-lease denial, autosave, corrupt files, Linux symlink and
permission rejection, and another player remaining in an unchanged dungeon.
A separate-process run reads the same three disk slots after initialization.
`tests/server/character_backup_tests.py` checks archive integrity, unexpected
files, symlinks, traversal and refusal to overwrite a nonempty restore directory.
Actual HDD fixture backup/restore checks content hashes and restored permissions.

The real isolated gateway integration can be run on Ubuntu with:

```sh
python3 tests/server/run_accounts.py --image VERIFIED_ACCOUNTS_IMAGE \
  --game-image VERIFIED_GAME_IMAGE --character-saves --game-duration-ms 0
```

It uses ten individually authenticated accounts, four playable controllers and
waiting users, verifies four separate durable save namespaces through Home, and
checks forgery rejection, ownership, reconnect and individual revocation. It
creates and cleans only its labelled disposable containers/volumes.

Four existing combat regressions reproduce on untouched `451a55b`:
`pfme_town_combat_1`, `pfme_town_combat_3`, `pfme_town_combat_4`, and
`pfme_party_combat_1` (survival/defeated-character return-to-Town assumptions).
They are not represented as passing or changed as part of character persistence.
The active game's public browser still needs the new release before its Save
button can persist characters; isolated tests do not constitute live deployment.

Recorded verification for this checkpoint:

- Windows: 53/53 tests excluding the four independently reproduced baseline
  failures; complete initial run 53/57.
- Ubuntu: complete initial run 62/66 with those same four failures; final focused
  save/menu/exploration/security suite 16/16, plus the real account-gateway test.
- Archive security: 6/6, and four fixture characters backed up and restored on
  the actual Samsung HDD with identical contents. Production volume is still
  empty because it is not mounted into the running game.
- ESP32: release and all 26 compile-only targets pass, no upload. Unchanged
  baseline/current RAM 292,704 / 327,680 and flash 1,069,837 / 4,128,768 bytes.
- All 13 existing containers retained their identity, start time and running state.
- Save-menu screenshots were captured from the real C++ RGB565 framebuffer and
  visually checked at nearest-neighbor 3x scaling.
