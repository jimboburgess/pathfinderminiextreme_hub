//
// Created by james on 7/12/2026.
//

#ifndef PATHFINDERMINIEXTREME_025_ITEMS_H
#define PATHFINDERMINIEXTREME_025_ITEMS_H

#include <Arduino.h>
#include <stdint.h>
#include "abilities.h"


//==================================================
// Items IDs
//==================================================

enum ItemID : uint8_t
{
    ITEM_NONE = 0,

    //======================================================
    // Simple Melee Weapons
    //======================================================

    ITEM_CLUB,
    ITEM_DAGGER,
    ITEM_GREATCLUB,
    ITEM_LIGHT_HAMMER,
    ITEM_MACE,
    ITEM_MORNINGSTAR,
    ITEM_QUARTERSTAFF,
    ITEM_SHORTSPEAR,
    ITEM_SICKLE,
    ITEM_SPEAR,

    //======================================================
    // Martial Melee Weapons
    //======================================================

    ITEM_BATTLEAXE,
    ITEM_FALCHION,
    ITEM_FLAIL,
    ITEM_GREATAXE,
    ITEM_GREATSWORD,
    ITEM_HEAVY_PICK,
    ITEM_LANCE,
    ITEM_LONGSWORD,
    ITEM_RAPIER,
    ITEM_SCIMITAR,
    ITEM_TRIDENT,
    ITEM_WARHAMMER,

    //======================================================
    // Ranged Weapons
    //======================================================

    ITEM_SHORTBOW,
    ITEM_LONGBOW,
    ITEM_COMPOSITE_LONGBOW,
    ITEM_LIGHT_CROSSBOW,
    ITEM_HEAVY_CROSSBOW,
    ITEM_SLING,

    //======================================================
    // Natural Weapons
    //======================================================

    ITEM_BITE,
    ITEM_CLAWS,
    ITEM_SLAM,
    ITEM_TENTACLE,
    ITEM_PSEUDOPOD,

    //======================================================
    // Armor
    //======================================================

    ITEM_PADDED_ARMOR,
    ITEM_QUILTED_ARMOR,
    ITEM_LEATHER_ARMOR,
    ITEM_STUDDED_LEATHER,
    ITEM_CHAIN_SHIRT,

    ITEM_HIDE_ARMOR,
    ITEM_SCALE_MAIL,
    ITEM_BREASTPLATE,
    ITEM_CHAINMAIL,

    ITEM_SPLINT_MAIL,
    ITEM_BANDED_MAIL,
    ITEM_HALF_PLATE,
    ITEM_FULL_PLATE,

    //======================================================
    // Natural Armor
    //======================================================

    ITEM_NATURAL_ARMOR_1,
    ITEM_NATURAL_ARMOR_2,
    ITEM_NATURAL_ARMOR_3,
    ITEM_NATURAL_ARMOR_4,
    ITEM_NATURAL_ARMOR_5,

    //======================================================
    // Shields
    //======================================================

    ITEM_BUCKLER,
    ITEM_LIGHT_WOODEN_SHIELD,
    ITEM_LIGHT_STEEL_SHIELD,
    ITEM_HEAVY_WOODEN_SHIELD,
    ITEM_HEAVY_STEEL_SHIELD,
    ITEM_TOWER_SHIELD,

    //======================================================
    // Potions
    //======================================================

    ITEM_POTION_CURE_LIGHT_WOUNDS,
    ITEM_POTION_CURE_MODERATE_WOUNDS,
    ITEM_POTION_CURE_SERIOUS_WOUNDS,
    ITEM_POTION_CURE_CRITICAL_WOUNDS,

    ITEM_POTION_BULLS_STRENGTH,
    ITEM_POTION_CATS_GRACE,
    ITEM_POTION_BEARS_ENDURANCE,
    ITEM_POTION_FOXS_CUNNING,
    ITEM_POTION_OWLS_WISDOM,
    ITEM_POTION_EAGLES_SPLENDOR,

    ITEM_POTION_BARKSKIN,
    ITEM_POTION_SHIELD_OF_FAITH,
    ITEM_POTION_RESIST_ENERGY,
    ITEM_POTION_PROTECTION_FROM_EVIL,
    ITEM_POTION_REMOVE_FEAR,

    ITEM_POTION_INVISIBILITY,
    ITEM_POTION_HASTE,
    ITEM_POTION_HEROISM,
    ITEM_POTION_FLY,
    ITEM_POTION_WATER_BREATHING,

    //======================================================
    // Adventuring Gear
    //======================================================

    ITEM_TORCH,
    ITEM_ROPE,
    ITEM_BEDROLL,
    ITEM_RATIONS,
    ITEM_WATERSKIN,
    ITEM_CROWBAR,
    ITEM_FLINT_AND_STEEL,
    ITEM_GRAPPLING_HOOK,
    ITEM_THIEVES_TOOLS,
    ITEM_SHOVEL,
    ITEM_HAMMER,
    ITEM_IRON_SPIKES,

    //======================================================
    // Treasure
    //======================================================

    ITEM_SILVER_RING,
    ITEM_GOLD_RING,
    ITEM_PEARL,
    ITEM_RUBY,
    ITEM_EMERALD,
    ITEM_SAPPHIRE,
    ITEM_DIAMOND,
    ITEM_SILVER_GOBLET,
    ITEM_GOLDEN_IDOL,
    ITEM_JEWELED_CROWN,

    //======================================================
    // Quest Items
    //======================================================

    ITEM_DRAGON_EGG,
    ITEM_ANCIENT_RELIC,
    ITEM_ROYAL_SEAL,
    ITEM_GOBLIN_KINGS_CROWN,
    ITEM_LOST_NECKLACE,
    ITEM_SACRED_CHALICE,
    ITEM_MYSTERIOUS_CRYSTAL,
    ITEM_KINGS_LETTER,

    // Appended so every previously persisted ItemID keeps its numeric value.
    ITEM_SCROLL_MAGIC_MISSILE,
    ITEM_SCROLL_SLEEP,
    ITEM_SCROLL_GREASE,

    ITEM_COUNT
};

static_assert(ITEM_COUNT <= UINT8_MAX,
              "ItemID no longer fits in its compact storage type.");

enum WeaponEnhancement : uint8_t
{
    WEAPON_ENHANCEMENT_NONE,
    WEAPON_ENHANCEMENT_FLAMING,
    WEAPON_ENHANCEMENT_FROST,
    WEAPON_ENHANCEMENT_SHOCK
};

struct ItemInstance
{
    ItemID itemID;
    int8_t enhancementBonus;
    WeaponEnhancement weaponEnhancement;
};

static_assert(sizeof(ItemInstance) == 3,
              "ItemInstance must remain compact for entity storage.");

inline ItemInstance makeItemInstance(ItemID itemID)
{
    ItemInstance item =
    {
        itemID,
        0,
        WEAPON_ENHANCEMENT_NONE
    };

    return item;
}

inline bool operator==(const ItemInstance& left,
                       const ItemInstance& right)
{
    return left.itemID == right.itemID &&
           left.enhancementBonus == right.enhancementBonus &&
           left.weaponEnhancement == right.weaponEnhancement;
}

inline bool operator!=(const ItemInstance& left,
                       const ItemInstance& right)
{
    return !(left == right);
}

enum ItemIcon
{
    ICON_NONE,

    //========================
    // Weapons
    //========================

    ICON_SWORD,
    ICON_AXE,
    ICON_DAGGER,
    ICON_MACE,
    ICON_CLUB,
    ICON_HAMMER,
    ICON_SPEAR,
    ICON_BOW,
    ICON_CROSSBOW,
    ICON_STAFF,

    //========================
    // Equipment
    //========================

    ICON_ARMOR,
    ICON_SHIELD,

    //========================
    // Magic
    //========================

    ICON_POTION,
    ICON_SCROLL,
    ICON_WAND,

    //========================
    // Adventuring Gear
    //========================

    ICON_TORCH,
    ICON_ROPE,
    ICON_LANTERN,
    ICON_TOOLS,
    ICON_KEY,
    ICON_BAG,

    //========================
    // Treasure
    //========================

    ICON_GOLD,
    ICON_GEM,
    ICON_RING,
    ICON_GOBLET,
    ICON_CROWN,
    ICON_IDOL,

    //========================
    // Quest Items
    //========================

    ICON_EGG,
    ICON_RELIC,
    ICON_LETTER,
    ICON_CRYSTAL,

    //========================
    // Misc
    //========================

    ICON_MISC,

    ICON_MAX
};

enum ItemType
{
    ITEMTYPE_NONE,

    ITEMTYPE_WEAPON,
    ITEMTYPE_ARMOR,
    ITEMTYPE_SHIELD,

    ITEMTYPE_POTION,
    ITEMTYPE_SCROLL,
    ITEMTYPE_WAND,
    ITEMTYPE_ROD,

    ITEMTYPE_RING,
    ITEMTYPE_HEAD,
    ITEMTYPE_HEADBAND,
    ITEMTYPE_NECK,
    ITEMTYPE_CHEST,
    ITEMTYPE_BODY,
    ITEMTYPE_BELT,
    ITEMTYPE_HANDS,
    ITEMTYPE_FEET,
    ITEMTYPE_SHOULDERS,
    ITEMTYPE_WRISTS,

    ITEMTYPE_ADVENTURING_GEAR,

    ITEMTYPE_TREASURE,

    ITEMTYPE_QUEST,

    ITEMTYPE_MISC
};


enum EquipmentSlot
{
    SLOT_MELEE_WEAPON,
    SLOT_RANGED_WEAPON,
    SLOT_SHIELD,
    SLOT_ARMOR,

    SLOT_BELT,
    SLOT_BODY,
    SLOT_CHEST,
    SLOT_EYES,
    SLOT_HANDS,
    SLOT_HEAD,
    SLOT_HEADBAND,
    SLOT_NECK,

    SLOT_RING_1,
    SLOT_RING_2,

    SLOT_SHOULDERS,
    SLOT_WRISTS,

    NUM_EQUIPMENT_SLOTS
};

enum Rarity
{
    RARITY_COMMON,
    RARITY_UNCOMMON,
    RARITY_RARE,
    RARITY_EPIC,
    RARITY_LEGENDARY
};

enum DungeonTheme
{
    THEME_ANY,
    THEME_GENERAL,

    // Common dungeon types
    THEME_GOBLIN,
    THEME_SPIDER,
    THEME_UNDEAD,
    THEME_DARK_MAGIC,
    THEME_OOZE,
    THEME_SWARM,
    THEME_DRAGON,

    // Environment
    THEME_CAVE,
    THEME_MINE,
    THEME_CRYPT,
    THEME_TEMPLE,
    THEME_CASTLE,
    THEME_FOREST,
    THEME_RUINS,

    // Rare / Special
    THEME_TREASURE,
    THEME_BOSS
};

struct Item
{
    const char* name;

    ItemType type;

    uint16_t effectIndex;

    uint16_t value; // Base shop buy price; zero means not normally tradable.
    uint16_t weight;

    ItemIcon icon;

    bool stackable;
    bool consumable;

    Rarity rarity;
    DungeonTheme theme;

    const char* description;
};

//==================================================
// Damage Types
//==================================================

//==================================================
// Weapon
//==================================================

enum WeaponType
{
    WEAPON_MELEE,
    WEAPON_RANGED
};

enum WeaponProperties
{
    WEAPON_PROP_NONE            = 0,

    WEAPON_PROP_FINESSE         = 1 << 0,
    WEAPON_PROP_THROWN          = 1 << 1,
    WEAPON_PROP_REACH           = 1 << 2,
    WEAPON_PROP_TWO_HANDED      = 1 << 3,
    WEAPON_PROP_LIGHT           = 1 << 4,
    WEAPON_PROP_TRIP            = 1 << 5,
    WEAPON_PROP_DISARM          = 1 << 6,
    WEAPON_PROP_BRACE           = 1 << 7,
    WEAPON_PROP_DOUBLE          = 1 << 8,
    WEAPON_PROP_NONLETHAL       = 1 << 9,
};


//==================================================
// Armor
//==================================================

enum ArmorCategory
{
    ARMOR_LIGHT,
    ARMOR_MEDIUM,
    ARMOR_HEAVY
};


//==================================================
// Item Stucts
//==================================================

struct Weapon
{
    WeaponType type;

    DamageType damageType;

    uint8_t damageDice;
    uint8_t damageSides;

    uint8_t criticalThreat;      // Lowest die roll that threatens a crit (18,19,20)
    uint8_t criticalMultiplier;  // 2,3,4

    uint8_t rangeIncrement;      // 0 for melee

    uint32_t properties;
};

struct Armor
{
    uint8_t armorBonus;
    int8_t maxDexBonus;
    int8_t armorCheckPenalty;
    uint8_t speed30;
    uint8_t speed20;
    uint8_t arcaneSpellFailure;
};

struct Shield
{
    uint8_t shieldBonus;
    int8_t maxDexBonus;
    int8_t armorCheckPenalty;
    uint8_t arcaneSpellFailure;
};

struct Scroll
{
    AbilityID taughtAbility;
    uint8_t casterLevel;
};

enum ScrollID : uint8_t
{
    SCROLL_MAGIC_MISSILE,
    SCROLL_SLEEP,
    SCROLL_GREASE,
    SCROLL_COUNT
};

enum ScrollLearnResult : uint8_t
{
    SCROLL_LEARN_SUCCESS,
    SCROLL_LEARN_ALREADY_KNOWN,
    SCROLL_LEARN_NOT_ARCANE_CASTER,
    SCROLL_LEARN_INVALID_SCROLL,
    SCROLL_LEARN_SPELLBOOK_FULL,
    SCROLL_LEARN_CANNOT_LEARN
};

struct Wand
{
    AbilityID ability;
    uint8_t casterLevel;
    uint8_t maxCharges;
};
//==================================================
// Databases
//==================================================

// Master item database
extern const Item itemDatabase[];

// Specialized databases
extern const Weapon weaponDatabase[];

extern const Armor armorDatabase[];

extern const Shield shieldDatabase[];

extern const Scroll scrollDatabase[];

const Item* getItem(ItemID item);

const Weapon* getWeapon(ItemID item);

const Armor* getArmor(ItemID item);

const Shield* getShield(ItemID item);

const Scroll* getScroll(ItemID item);

ScrollLearnResult learnSpellFromScroll(
    Character& character,
    AbilityID abilityID);

ScrollLearnResult useSpellScroll(
    Character& character,
    const ItemInstance& scrollItem);

#endif // PATHFINDERMINIEXTREME_025_ITEMS_H
