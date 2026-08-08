#include "inventorymenu.h"

#include <cstdio>
#include <cstring>

#include "audio/audio.h"
#include "characters/characters.h"
#include "characters/items.h"
#include "data/dice.h"
#include "data/entities.h"
#include "data/entityspawn.h"
#include "data/game.h"
#include "dungeon/activemap.h"
#include "dungeon/combat.h"
#include "dungeon/loot.h"
#include "graphics/display.h"
#include "graphics/messagelog.h"
#include "input/buttons.h"
#include "input/menu.h"

namespace
{
constexpr uint8_t INVENTORY_VISIBLE_ROWS = 8;
constexpr int INVENTORY_LIST_Y =
    MENU_Y + MENU_HEADER_HEIGHT + 2;
constexpr int INVENTORY_DESCRIPTION_Y =
    MENU_Y + MENU_HEIGHT - MENU_FOOTER_HEIGHT - MENU_DESCRIPTION_HEIGHT;

enum InventoryMenuMode : uint8_t
{
    INVENTORY_MENU_PLAYER,
    INVENTORY_MENU_LOOT
};

struct InventoryMenuState
{
    bool open = false;
    InventoryMenuMode mode = INVENTORY_MENU_PLAYER;
    Character* character = nullptr;
    Entity* corpse = nullptr;
    uint8_t cursorIndex = 0;
    uint8_t firstVisibleIndex = 0;
    char status[64] = "";
};

InventoryMenuState inventoryMenu;

Character* getActivePlayerCharacter()
{
    Entity* playerEntity = getActiveMapPlayer();

    return playerEntity != nullptr ? &playerEntity->character : &player;
}

bool isValidLootCorpse(const Entity* corpse)
{
    return corpse != nullptr && corpse->active &&
           corpse->type == ENTITY_MONSTER &&
           corpse->character.state == STATE_DEAD &&
           corpse->loot.generated;
}

uint8_t getInventoryEntryCount()
{
    if (!inventoryMenu.open)
        return 0;

    if (inventoryMenu.mode == INVENTORY_MENU_PLAYER)
    {
        return inventoryMenu.character != nullptr
            ? inventoryMenu.character->inventory.itemCount
            : 0;
    }

    if (!isValidLootCorpse(inventoryMenu.corpse) ||
        inventoryMenu.corpse->loot.itemCount == 0)
    {
        return 0;
    }

    // The final dynamic row is a single Take All command.
    return inventoryMenu.corpse->loot.itemCount + 1;
}

bool isTakeAllSelected()
{
    return inventoryMenu.mode == INVENTORY_MENU_LOOT &&
           isValidLootCorpse(inventoryMenu.corpse) &&
           inventoryMenu.cursorIndex == inventoryMenu.corpse->loot.itemCount;
}

const InventorySlot* getSelectedSlot()
{
    if (!inventoryMenu.open)
        return nullptr;

    if (inventoryMenu.mode == INVENTORY_MENU_PLAYER)
    {
        if (inventoryMenu.character == nullptr ||
            inventoryMenu.cursorIndex >=
                inventoryMenu.character->inventory.itemCount)
        {
            return nullptr;
        }

        return &inventoryMenu.character->inventory.slots[
            inventoryMenu.cursorIndex];
    }

    if (!isValidLootCorpse(inventoryMenu.corpse) || isTakeAllSelected() ||
        inventoryMenu.cursorIndex >= inventoryMenu.corpse->loot.itemCount)
    {
        return nullptr;
    }

    return &inventoryMenu.corpse->loot.slots[inventoryMenu.cursorIndex];
}

void clampInventorySelection()
{
    uint8_t entryCount = getInventoryEntryCount();

    if (entryCount == 0)
    {
        inventoryMenu.cursorIndex = 0;
        inventoryMenu.firstVisibleIndex = 0;
        return;
    }

    if (inventoryMenu.cursorIndex >= entryCount)
        inventoryMenu.cursorIndex = entryCount - 1;

    if (inventoryMenu.firstVisibleIndex > inventoryMenu.cursorIndex)
        inventoryMenu.firstVisibleIndex = inventoryMenu.cursorIndex;

    if (inventoryMenu.cursorIndex >=
        inventoryMenu.firstVisibleIndex + INVENTORY_VISIBLE_ROWS)
    {
        inventoryMenu.firstVisibleIndex =
            inventoryMenu.cursorIndex - INVENTORY_VISIBLE_ROWS + 1;
    }
}

void setInventoryStatus(const char* message)
{
    strncpy(inventoryMenu.status, message,
            sizeof(inventoryMenu.status) - 1);
    inventoryMenu.status[sizeof(inventoryMenu.status) - 1] = '\0';

    setGameMessage(message);
    needsRedraw = true;
}

void clearInventoryStatus()
{
    inventoryMenu.status[0] = '\0';
}

void moveInventorySelection(bool forward)
{
    uint8_t entryCount = getInventoryEntryCount();

    if (entryCount == 0)
        return;

    if (forward)
    {
        if (inventoryMenu.cursorIndex + 1 < entryCount)
            inventoryMenu.cursorIndex++;
    }
    else if (inventoryMenu.cursorIndex > 0)
    {
        inventoryMenu.cursorIndex--;
    }

    clearInventoryStatus();
    clampInventorySelection();
    needsRedraw = true;
}

bool canUseCombatItem(const Character& character)
{
    if (!combat.active)
        return true;

    Entity* combatant = getCurrentCombatant();

    return combatant != nullptr && combatant->type == ENTITY_PLAYER &&
           &combatant->character == &character && combat.waitingForPlayer &&
           !combatant->turn.standardActionUsed;
}

bool useCureLightWounds(Character& character)
{
    if (!hasItem(character, ITEM_POTION_CURE_LIGHT_WOUNDS))
    {
        setInventoryStatus("Potion is no longer available.");
        playSound(SoundEffect::ERROR);
        return false;
    }

    if (character.health.currentHP >= character.health.maxHP)
    {
        setInventoryStatus("Already at full HP.");
        playSound(SoundEffect::ERROR);
        return false;
    }

    if (!canUseCombatItem(character))
    {
        setInventoryStatus("Cannot use an item now.");
        playSound(SoundEffect::ERROR);
        return false;
    }

    int previousHP = character.health.currentHP;
    int healed = rollDice(1, 8) + 1;
    int missingHP = character.health.maxHP - character.health.currentHP;

    if (healed > missingHP)
        healed = missingHP;

    // Heal first, then consume exactly one item. If an unexpected inventory
    // failure occurs, restore HP so the operation remains atomic.
    character.health.currentHP += healed;

    if (!removeItem(character, ITEM_POTION_CURE_LIGHT_WOUNDS))
    {
        character.health.currentHP = previousHP;
        setInventoryStatus("Potion is no longer available.");
        playSound(SoundEffect::ERROR);
        return false;
    }

    char message[48];
    snprintf(message, sizeof(message), "Potion healed %d HP.", healed);

    if (combat.active)
    {
        Entity* combatant = getCurrentCombatant();

        // canUseCombatItem() already validated this exact combatant.
        combatant->turn.standardActionUsed = true;
        checkEndPlayerTurn();

        // If that spent the character's final action, give the healing
        // result time to be read before the next monster begins moving.
        if (combat.active && !isPlayerTurn())
            combat.nextMonsterStep = millis() + COMBAT_MESSAGE_PAUSE_MS;
    }

    playSound(SoundEffect::POTION);
    setInventoryStatus(message);

    return true;
}

void useSelectedPlayerItem()
{
    if (inventoryMenu.character == nullptr)
    {
        closeInventoryMenu();
        return;
    }

    const InventorySlot* slot = getSelectedSlot();

    if (slot == nullptr)
    {
        setInventoryStatus("No item selected.");
        return;
    }

    if (slot->item == ITEM_POTION_CURE_LIGHT_WOUNDS)
    {
        if (useCureLightWounds(*inventoryMenu.character))
            closeInventoryMenu();

        return;
    }

    const Item* item = getItem(slot->item);

    if (item != nullptr && item->consumable)
        setInventoryStatus("That consumable is not ready yet.");
    else
        setInventoryStatus("That item cannot be used.");

    playSound(SoundEffect::ERROR);
}

void takeSelectedCorpseLoot()
{
    Entity* corpse = inventoryMenu.corpse;

    if (!isValidLootCorpse(corpse) || inventoryMenu.character == nullptr)
    {
        closeInventoryMenu();
        return;
    }

    if (isTakeAllSelected())
    {
        uint16_t taken = takeAllCorpseLoot(*corpse, *inventoryMenu.character);

        if (taken == 0)
        {
            setInventoryStatus("Inventory full.");
            playSound(SoundEffect::ERROR);
            return;
        }

        char message[64];

        if (corpse->active)
        {
            snprintf(message, sizeof(message),
                     "Took %u item(s); pack full.",
                     static_cast<unsigned>(taken));
        }
        else
        {
            snprintf(message, sizeof(message), "Took %u item(s).",
                     static_cast<unsigned>(taken));
        }

        playSound(SoundEffect::ITEM_PICKUP);
        setInventoryStatus(message);

        if (!corpse->active)
        {
            closeInventoryMenu();
            return;
        }

        clampInventorySelection();
        needsRedraw = true;
        return;
    }

    const InventorySlot* slot = getSelectedSlot();

    if (slot == nullptr)
    {
        closeInventoryMenu();
        return;
    }

    ItemID itemID = slot->item;
    const Item* item = getItem(itemID);

    if (!takeCorpseLootItem(*corpse, inventoryMenu.cursorIndex,
                             *inventoryMenu.character))
    {
        setInventoryStatus("Inventory full.");
        playSound(SoundEffect::ERROR);
        return;
    }

    char message[64];
    snprintf(message, sizeof(message), "Took %s.",
             item != nullptr ? item->name : "item");
    playSound(SoundEffect::ITEM_PICKUP);
    setInventoryStatus(message);

    if (!corpse->active)
    {
        closeInventoryMenu();
        return;
    }

    clampInventorySelection();
    needsRedraw = true;
}

void drawClippedText(const char* text, uint8_t maxCharacters)
{
    char clipped[32];
    uint8_t length = 0;

    while (text != nullptr && text[length] != '\0' &&
           length < maxCharacters && length < sizeof(clipped) - 1)
    {
        clipped[length] = text[length];
        length++;
    }

    bool truncated = text != nullptr && text[length] != '\0';

    if (truncated && length >= 3)
    {
        clipped[length - 3] = '.';
        clipped[length - 2] = '.';
        clipped[length - 1] = '.';
    }

    clipped[length] = '\0';
    tft.print(clipped);
}

const char* getInventoryTitle()
{
    if (inventoryMenu.mode == INVENTORY_MENU_PLAYER)
        return "Inventory";

    return "Loot";
}

const char* getSelectedDescription()
{
    if (inventoryMenu.status[0] != '\0')
        return inventoryMenu.status;

    if (isTakeAllSelected())
        return "Take every item that fits in your pack.";

    const InventorySlot* slot = getSelectedSlot();
    const Item* item = slot != nullptr ? getItem(slot->item) : nullptr;

    if (item != nullptr && item->description != nullptr)
        return item->description;

    return inventoryMenu.mode == INVENTORY_MENU_PLAYER
        ? "Empty"
        : "No loot remains.";
}

void drawInventoryRow(uint8_t row, uint8_t entryIndex)
{
    bool highlighted = entryIndex == inventoryMenu.cursorIndex;
    int y = INVENTORY_LIST_Y + row * MENU_LINE_HEIGHT;

    tft.fillRect(MENU_X + 1, y, MENU_WIDTH - 2, MENU_LINE_HEIGHT,
                 highlighted ? MENU_HIGHLIGHT : MENU_BG);

    if (highlighted)
    {
        tft.setTextColor(MENU_CURSOR, MENU_HIGHLIGHT);
        tft.setCursor(MENU_CURSOR_X, y + 3);
        tft.print(">");
    }

    tft.setTextColor(MENU_TEXT, highlighted ? MENU_HIGHLIGHT : MENU_BG);
    tft.setCursor(MENU_TEXT_X, y + 3);

    if (inventoryMenu.mode == INVENTORY_MENU_LOOT &&
        isValidLootCorpse(inventoryMenu.corpse) &&
        entryIndex == inventoryMenu.corpse->loot.itemCount)
    {
        tft.print("Take All");
        return;
    }

    const InventorySlot* slot = nullptr;

    if (inventoryMenu.mode == INVENTORY_MENU_PLAYER)
    {
        if (inventoryMenu.character != nullptr &&
            entryIndex < inventoryMenu.character->inventory.itemCount)
        {
            slot = &inventoryMenu.character->inventory.slots[entryIndex];
        }
    }
    else if (isValidLootCorpse(inventoryMenu.corpse) &&
             entryIndex < inventoryMenu.corpse->loot.itemCount)
    {
        slot = &inventoryMenu.corpse->loot.slots[entryIndex];
    }

    const Item* item = slot != nullptr ? getItem(slot->item) : nullptr;
    drawClippedText(item != nullptr ? item->name : "Unknown item", 26);

    if (slot != nullptr && slot->quantity > 1)
    {
        char quantity[8];
        snprintf(quantity, sizeof(quantity), "x%u",
                 static_cast<unsigned>(slot->quantity));
        tft.setCursor(MENU_X + MENU_WIDTH - 28, y + 3);
        tft.print(quantity);
    }
}
}

bool isInventoryMenuOpen()
{
    return inventoryMenu.open;
}

void openPlayerInventoryMenu()
{
    inventoryMenu.open = true;
    inventoryMenu.mode = INVENTORY_MENU_PLAYER;
    inventoryMenu.character = getActivePlayerCharacter();
    inventoryMenu.corpse = nullptr;
    inventoryMenu.cursorIndex = 0;
    inventoryMenu.firstVisibleIndex = 0;
    clearInventoryStatus();
    needsRedraw = true;
}

void openCorpseLootMenu(Entity& corpse)
{
    inventoryMenu.open = true;
    inventoryMenu.mode = INVENTORY_MENU_LOOT;
    inventoryMenu.character = getActivePlayerCharacter();
    inventoryMenu.corpse = &corpse;
    inventoryMenu.cursorIndex = 0;
    inventoryMenu.firstVisibleIndex = 0;
    clearInventoryStatus();
    clampInventorySelection();
    needsRedraw = true;
}

void closeInventoryMenu()
{
    if (!inventoryMenu.open)
        return;

    inventoryMenu.open = false;
    inventoryMenu.character = nullptr;
    inventoryMenu.corpse = nullptr;
    inventoryMenu.cursorIndex = 0;
    inventoryMenu.firstVisibleIndex = 0;
    clearInventoryStatus();

    // This full-screen modal is opaque, so the map/town must be restored in
    // full when it closes. Normal combat/map activity remains dirty-tile
    // driven underneath it.
    backgroundNeedsRedraw = true;
    redrawType = REDRAW_FULL;
    needsRedraw = true;
}

void handleInventoryMenuButtons()
{
    if (!inventoryMenu.open)
        return;

    if (inventoryMenu.mode == INVENTORY_MENU_LOOT &&
        (!isValidLootCorpse(inventoryMenu.corpse) ||
         inventoryMenu.corpse->loot.itemCount == 0))
    {
        closeInventoryMenu();
        return;
    }

    EncoderDirection direction = readEncoder();

    if (direction == ENCODER_CLOCKWISE)
    {
        moveInventorySelection(true);
        playSound(SoundEffect::MENU_MOVE);
    }
    else if (direction == ENCODER_COUNTERCLOCKWISE)
    {
        moveInventorySelection(false);
        playSound(SoundEffect::MENU_MOVE);
    }

    if (buttonAPressed())
    {
        playSound(SoundEffect::MENU_SELECT);

        if (inventoryMenu.mode == INVENTORY_MENU_PLAYER)
            useSelectedPlayerItem();
        else
            takeSelectedCorpseLoot();

        return;
    }

    if (buttonBPressed())
    {
        playSound(SoundEffect::MENU_BACK);
        closeInventoryMenu();
    }
}

void drawInventoryMenu()
{
    if (!inventoryMenu.open)
        return;

    clampInventorySelection();

    tft.fillScreen(MENU_BG);
    tft.drawRect(MENU_X, MENU_Y, MENU_WIDTH, MENU_HEIGHT, MENU_BORDER);
    tft.fillRect(MENU_X, MENU_Y, MENU_WIDTH, MENU_HEADER_HEIGHT,
                 MENU_HEADER_BG);

    tft.setTextColor(MENU_TEXT);
    tft.setTextSize(2);
    tft.setCursor(MENU_X + MENU_PADDING, MENU_Y + 4);
    tft.print(getInventoryTitle());

    if (inventoryMenu.mode == INVENTORY_MENU_LOOT &&
        isValidLootCorpse(inventoryMenu.corpse))
    {
        tft.setTextSize(1);
        tft.setCursor(MENU_X + 92, MENU_Y + 7);
        drawClippedText(getEntityName(inventoryMenu.corpse), 18);
    }

    int listHeight = INVENTORY_VISIBLE_ROWS * MENU_LINE_HEIGHT;
    tft.fillRect(MENU_X + 1, INVENTORY_LIST_Y, MENU_WIDTH - 2,
                 listHeight, MENU_BG);

    uint8_t entryCount = getInventoryEntryCount();

    if (entryCount == 0)
    {
        tft.setTextSize(1);
        tft.setTextColor(MENU_DIM_TEXT);
        tft.setCursor(MENU_TEXT_X, INVENTORY_LIST_Y + 5);
        tft.print("Empty");
    }
    else
    {
        uint8_t rows = entryCount - inventoryMenu.firstVisibleIndex;

        if (rows > INVENTORY_VISIBLE_ROWS)
            rows = INVENTORY_VISIBLE_ROWS;

        for (uint8_t row = 0; row < rows; row++)
        {
            drawInventoryRow(row,
                             inventoryMenu.firstVisibleIndex + row);
        }
    }

    tft.drawLine(MENU_X, INVENTORY_DESCRIPTION_Y, MENU_X + MENU_WIDTH,
                 INVENTORY_DESCRIPTION_Y, MENU_BORDER);
    tft.fillRect(MENU_X + 1, INVENTORY_DESCRIPTION_Y + 1,
                 MENU_WIDTH - 2, MENU_DESCRIPTION_HEIGHT - 2, MENU_BG);
    tft.setTextSize(1);
    tft.setTextColor(MENU_TEXT);
    tft.setCursor(MENU_X + MENU_PADDING, INVENTORY_DESCRIPTION_Y + 8);
    tft.setTextWrap(true);
    tft.print(getSelectedDescription());

    int footerY = MENU_Y + MENU_HEIGHT - MENU_FOOTER_HEIGHT;
    tft.fillRect(MENU_X, footerY, MENU_WIDTH, MENU_FOOTER_HEIGHT,
                 MENU_HEADER_BG);
    tft.drawLine(MENU_X, footerY, MENU_X + MENU_WIDTH, footerY,
                 MENU_BORDER);
    tft.setTextSize(1);
    tft.setTextColor(MENU_TEXT);
    tft.setCursor(MENU_X + MENU_PADDING, footerY + 5);
    tft.print(inventoryMenu.mode == INVENTORY_MENU_PLAYER ? "A Use" : "A Take");
    tft.setCursor(MENU_X + 140, footerY + 5);
    tft.print("B Back");
}
