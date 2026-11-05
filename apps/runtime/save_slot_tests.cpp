#include "runtime.h"

#include "characters/charactercreation.h"
#include "data/savegame.h"
#include "data/game.h"
#include "graphics/charcreationscreen.h"
#include "input/buttons.h"
#include "input/menu.h"
#include "multiplayer/multiplayer_runtime.h"

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{
void check(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

void setSaveDirectory(const std::filesystem::path& path)
{
#ifdef _WIN32
    _putenv_s("PFME_SAVE_DIR", path.string().c_str());
#else
    setenv("PFME_SAVE_DIR", path.string().c_str(), 1);
#endif
}

void wait(unsigned milliseconds)
{
    while (milliseconds != 0)
    {
        const unsigned step = milliseconds < 10 ? milliseconds : 10;
        pfme::runtime::advance(step);
        milliseconds -= step;
    }
}

void tap(const char* action)
{
    check(pfme::runtime::input(action, true), "input down");
    wait(30);
    check(pfme::runtime::input(action, false), "input up");
    wait(310);
}

Character makeCharacter(const char* name, CharacterClass characterClass,
                        uint8_t level)
{
    Character character = {};
    createCharacter(character, characterClass,
                    characterClass == CLASS_FIGHTER
                        ? WEAPON_GROUP_BLADES : WEAPON_GROUP_NONE);
    setCharacterName(character, name);
    character.level = level;
    return character;
}

struct Version9Save
{
    uint32_t magic;
    uint8_t version;
    CharacterClass characterClass;
    uint8_t level;
    uint32_t xp;
    AbilityScores abilities;
    HealthData health;
    EquipmentData equipment;
    InventoryData inventory;
    int16_t currentMP;
    uint8_t knownAbilityCount;
    AbilityID knownAbilities[MAX_KNOWN_ABILITIES];
    SpellLearningData learning;
    uint32_t trainedWeaponGroup;
};

Version9Save makeLegacySave(const Character& character)
{
    Version9Save saved = {};
    saved.magic = 0x50464D45;
    saved.version = 9;
    saved.characterClass = character.characterClass;
    saved.level = character.level;
    saved.xp = character.xp;
    saved.abilities = character.abilities;
    saved.health = character.health;
    saved.equipment = character.equipment;
    saved.inventory = character.inventory;
    saved.currentMP = static_cast<int16_t>(character.magic.currentMP);
    saved.knownAbilityCount = character.magic.knownAbilityCount;
    for (uint8_t index = 0; index < saved.knownAbilityCount; index++)
        saved.knownAbilities[index] = character.magic.knownAbilities[index];
    saved.learning = character.magic.learning;
    saved.trainedWeaponGroup = character.trainedWeaponGroup;
    return saved;
}
}

int main()
{
    const std::filesystem::path directory =
        std::filesystem::temp_directory_path() / "pfme-save-slot-tests";
    try
    {
        std::error_code error;
        std::filesystem::remove_all(directory, error);
        setSaveDirectory(directory);
        pfme::runtime::initialize(117);

        Character validated = {};
        setCharacterName(validated, "Aric\x01! 7");
        check(std::strcmp(validated.name, "Aric 7") == 0,
              "unsupported name characters removed");
        setCharacterName(validated, "12345678901234567890");
        check(std::strlen(validated.name) == CHARACTER_NAME_SIZE - 1,
              "name length bounded and terminated");
        setCharacterName(validated, "   ");
        check(std::strcmp(validated.name, DEFAULT_CHARACTER_NAME) == 0,
              "empty visible name falls back");

        for (uint8_t slot = 0; slot < SAVE_SLOT_COUNT; slot++)
        {
            SaveSlotSummary summary = {};
            check(getSaveSlotSummary(slot, summary) == SaveSlotState::Empty,
                  "fresh slot empty");
        }

        Character originals[SAVE_SLOT_COUNT] =
        {
            makeCharacter("Aric", CLASS_FIGHTER, 3),
            makeCharacter("Mira", CLASS_CLERIC, 5),
            makeCharacter("Shade", CLASS_ROGUE, 2)
        };

        for (uint8_t slot = 0; slot < SAVE_SLOT_COUNT; slot++)
        {
            check(saveGameToSlot(originals[slot], slot), "save slot");
            SaveSlotSummary summary = {};
            check(getSaveSlotSummary(slot, summary) ==
                      SaveSlotState::Occupied,
                  "occupied summary");
            check(std::strcmp(summary.characterName,
                              originals[slot].name) == 0 &&
                  summary.characterClass == originals[slot].characterClass &&
                  summary.level == originals[slot].level,
                  "summary name class level");

            Character loaded = {};
            check(loadGameFromSlot(loaded, slot), "load slot");
            check(std::strcmp(loaded.name, originals[slot].name) == 0 &&
                  loaded.characterClass == originals[slot].characterClass &&
                  loaded.level == originals[slot].level,
                  "independent slot payload");
        }

        Character replacement = makeCharacter("Bryn", CLASS_WIZARD, 7);
        replacement.inventory.gold = 777;
        check(saveGameToSlot(replacement, 1), "overwrite slot");
        Character overwritten = {};
        check(loadGameFromSlot(overwritten, 1) &&
              std::strcmp(overwritten.name, "Bryn") == 0 &&
              overwritten.characterClass == CLASS_WIZARD &&
              overwritten.inventory.gold == 777,
              "overwrite fully replaces prior payload");

        {
            std::ofstream corrupt(directory / "save_slot_2.dat",
                                  std::ios::binary | std::ios::trunc);
            corrupt << "not a PFME save";
        }
        SaveSlotSummary corruptSummary = {};
        check(getSaveSlotSummary(1, corruptSummary) == SaveSlotState::Corrupt,
              "corrupt slot detected");
        Character unaffected = {};
        check(loadGameFromSlot(unaffected, 2) &&
              std::strcmp(unaffected.name, "Shade") == 0,
              "other slot survives corruption");

        deleteSaveSlot(0);
        const Character legacyCharacter =
            makeCharacter("Ignored", CLASS_CLERIC, 4);
        const Version9Save legacy = makeLegacySave(legacyCharacter);
        {
            std::ofstream output(directory / "save.dat",
                                 std::ios::binary | std::ios::trunc);
            output.write(reinterpret_cast<const char*>(&legacy),
                         sizeof(legacy));
        }
        SaveSlotSummary legacySummary = {};
        check(getSaveSlotSummary(0, legacySummary) == SaveSlotState::Occupied &&
              std::strcmp(legacySummary.characterName,
                          DEFAULT_CHARACTER_NAME) == 0,
              "legacy save appears in slot one with fallback name");
        Character migrated = {};
        check(loadGameFromSlot(migrated, 0) &&
              std::strcmp(migrated.name, DEFAULT_CHARACTER_NAME) == 0 &&
              std::filesystem::exists(directory / "save_slot_1.dat") &&
              !std::filesystem::exists(directory / "save.dat"),
              "legacy source removed only after successful migration");

        const Character first = makeCharacter("Aric", CLASS_FIGHTER, 4);
        const Character second = makeCharacter("Mira", CLASS_FIGHTER, 4);
        const NetworkPlayerProfile firstProfile =
            makeNetworkPlayerProfile(first);
        const NetworkPlayerProfile secondProfile =
            makeNetworkPlayerProfile(second);
        check(std::strcmp(firstProfile.displayName, "Aric") == 0 &&
              std::strcmp(secondProfile.displayName, "Mira") == 0,
              "multiplayer profile derives display name from character");

        std::filesystem::remove_all(directory, error);
        std::filesystem::create_directories(directory, error);
        tap("a");
        tap("a");
        tap("a");
        tap("a");
        tap("a");
        check(isCharacterNameEntryActive(), "name entry in creation flow");
        for (const char character : std::string("Aric"))
        {
            const std::string action = std::string("key:") + character;
            check(pfme::runtime::input(action, true), "native text input");
        }
        tap("encoder");
        check(gameState == GAME_TOWN && std::strcmp(player.name, "Aric") == 0,
              "typed name accepted into town");

        tap("a");
        tap("cw");
        tap("a");
        check(menuState.isOpen, "save slot menu opened from home");
        tap("a");
        SaveSlotSummary flowSummary = {};
        check(getSaveSlotSummary(0, flowSummary) == SaveSlotState::Occupied &&
              std::strcmp(flowSummary.characterName, "Aric") == 0,
              "save slot selected through production UI");

        setCharacterName(player, "Mira");
        tap("a");
        tap("a");
        check(menuState.depth == 2 && menuState.cursorIndex == 0,
              "occupied slot opens overwrite confirmation defaulting to no");
        tap("a");
        check(menuState.depth == 1, "no returns to slot list");
        check(getSaveSlotSummary(0, flowSummary) == SaveSlotState::Occupied &&
              std::strcmp(flowSummary.characterName, "Aric") == 0,
              "default no preserves occupied slot");
        tap("a");
        tap("cw");
        tap("a");
        check(getSaveSlotSummary(0, flowSummary) == SaveSlotState::Occupied &&
              std::strcmp(flowSummary.characterName, "Mira") == 0,
              "explicit yes overwrites occupied slot");

        closeMenu();
        player = {};
        gameState = GAME_START;
        needsRedraw = true;
        pfme::runtime::releaseInputs();
        resetButtonStates();
        tap("b");
        check(menuState.isOpen, "load slot menu opened from title");
        tap("a");
        check(gameState == GAME_TOWN && std::strcmp(player.name, "Mira") == 0,
              "saved character resumed through production UI");

        std::filesystem::remove_all(directory, error);
        std::cout << "Character names, three save slots, corruption, overwrite, "
                     "legacy migration, and multiplayer display names passed.\n";
        return 0;
    }
    catch (const std::exception& exception)
    {
        std::error_code error;
        std::filesystem::remove_all(directory, error);
        std::cerr << "FAILED: " << exception.what() << '\n';
        return 1;
    }
}
