//
// Created by james on 7/12/2026.
//

#include "items.h"

const Item itemDatabase[] =
{
	// None
	{ "None", ITEMTYPE_NONE, 0, 0, 0, ICON_NONE, false, false, RARITY_COMMON, THEME_ANY, "" },

	//======================================================
	// Simple Melee Weapons
	//======================================================
{ "Club",              ITEMTYPE_WEAPON,  0,   0, 0, ICON_CLUB,       false, false, RARITY_COMMON, THEME_ANY, "A simple wooden club." },
{ "Dagger",            ITEMTYPE_WEAPON,  1,  10, 1, ICON_DAGGER,     false, false, RARITY_COMMON, THEME_ANY, "A balanced dagger." },
{ "Greatclub",         ITEMTYPE_WEAPON,  2,   5, 8, ICON_CLUB,       false, false, RARITY_COMMON, THEME_ANY, "A massive wooden club." },
{ "Light Hammer",      ITEMTYPE_WEAPON,  3,   1, 2, ICON_HAMMER,     false, false, RARITY_COMMON, THEME_ANY, "A small throwing hammer." },
{ "Mace",              ITEMTYPE_WEAPON,  4,  15, 8, ICON_MACE,       false, false, RARITY_COMMON, THEME_ANY, "A heavy iron mace." },
{ "Morningstar",       ITEMTYPE_WEAPON,  5,   8, 6, ICON_MACE,       false, false, RARITY_COMMON, THEME_ANY, "A spiked mace." },
{ "Quarterstaff",      ITEMTYPE_WEAPON,  6,   0, 4, ICON_STAFF,      false, false, RARITY_COMMON, THEME_ANY, "A sturdy wooden staff." },
{ "Shortspear",        ITEMTYPE_WEAPON,  7,   1, 3, ICON_SPEAR,      false, false, RARITY_COMMON, THEME_ANY, "A light throwing spear." },
{ "Sickle",            ITEMTYPE_WEAPON,  8,   6, 2, ICON_SWORD,      false, false, RARITY_COMMON, THEME_ANY, "A curved farming blade." },
{ "Spear",             ITEMTYPE_WEAPON,  9,   2, 6, ICON_SPEAR,      false, false, RARITY_COMMON, THEME_ANY, "A long spear." },
	//======================================================
	// Martial Melee Weapons
	//======================================================
{ "Battleaxe",         ITEMTYPE_WEAPON, 10,  10, 6, ICON_AXE,        false, false, RARITY_COMMON, THEME_ANY, "A heavy battleaxe." },
{ "Falchion",          ITEMTYPE_WEAPON, 11,  75, 8, ICON_SWORD,      false, false, RARITY_UNCOMMON, THEME_ANY, "A curved two-handed sword." },
{ "Flail",             ITEMTYPE_WEAPON, 12,   8, 5, ICON_MACE,      false, false, RARITY_COMMON, THEME_ANY, "A chained striking weapon." },
{ "Greataxe",          ITEMTYPE_WEAPON, 13,  20,12, ICON_AXE,        false, false, RARITY_COMMON, THEME_ANY, "A massive two-handed axe." },
{ "Greatsword",        ITEMTYPE_WEAPON, 14,  50, 8, ICON_SWORD,     false, false, RARITY_COMMON, THEME_ANY, "A classic two-handed sword." },
{ "Heavy Pick",        ITEMTYPE_WEAPON, 15,   8, 6, ICON_HAMMER,     false, false, RARITY_UNCOMMON, THEME_ANY, "A brutal armor-piercing pick." },
{ "Lance",             ITEMTYPE_WEAPON, 16,  10,10, ICON_SPEAR,      false, false, RARITY_UNCOMMON, THEME_ANY, "A long cavalry lance." },
{ "Longsword",         ITEMTYPE_WEAPON, 17,  20, 4, ICON_SWORD,      false, false, RARITY_COMMON, THEME_ANY, "The standard knight's sword." },
{ "Rapier",            ITEMTYPE_WEAPON, 18,  20, 2, ICON_SWORD,     false, false, RARITY_COMMON, THEME_ANY, "A slender dueling blade." },
{ "Scimitar",          ITEMTYPE_WEAPON, 19,  15, 4, ICON_SWORD,      false, false, RARITY_COMMON, THEME_GOBLIN, "A curved cavalry sword." },
{ "Trident",           ITEMTYPE_WEAPON, 20,  15, 4, ICON_SPEAR,      false, false, RARITY_UNCOMMON, THEME_ANY, "A three-pronged spear." },
{ "Warhammer",         ITEMTYPE_WEAPON, 21,  12, 5, ICON_HAMMER,     false, false, RARITY_COMMON, THEME_ANY, "A powerful warhammer." },

	//======================================================
	// Ranged Weapons
	//======================================================
{ "Shortbow",           ITEMTYPE_WEAPON, 22, 30, 2, ICON_BOW,       false, false, RARITY_COMMON, THEME_ANY, "A simple shortbow." },
{ "Longbow",            ITEMTYPE_WEAPON, 23, 75, 3, ICON_BOW,       false, false, RARITY_COMMON, THEME_ANY, "A powerful longbow." },
{ "Composite Longbow",  ITEMTYPE_WEAPON, 24,100,3, ICON_BOW,        false, false, RARITY_UNCOMMON, THEME_ANY, "A reinforced longbow." },
{ "Light Crossbow",     ITEMTYPE_WEAPON, 25, 35, 4, ICON_CROSSBOW,  false, false, RARITY_COMMON, THEME_ANY, "A light crossbow." },
{ "Heavy Crossbow",     ITEMTYPE_WEAPON, 26, 50, 8, ICON_CROSSBOW,  false, false, RARITY_COMMON, THEME_ANY, "A heavy crossbow." },
{ "Sling",              ITEMTYPE_WEAPON, 27,  0, 0, ICON_MISC,      false, false, RARITY_COMMON, THEME_ANY, "A simple leather sling." },

	//======================================================
// Natural weapons
//======================================================
{ "Bite",               ITEMTYPE_WEAPON, 28,    0, 0, ICON_MISC, false, false, RARITY_COMMON, THEME_ANY, "A vicious natural bite attack." },
{ "Claws",              ITEMTYPE_WEAPON, 29,    0, 0, ICON_MISC, false, false, RARITY_COMMON, THEME_ANY, "Sharp natural claws." },
{ "Slam",               ITEMTYPE_WEAPON, 30,    0, 0, ICON_MISC, false, false, RARITY_COMMON, THEME_ANY, "A heavy slam attack." },
{ "Tentacle",           ITEMTYPE_WEAPON, 31,    0, 0, ICON_MISC, false, false, RARITY_COMMON, THEME_ANY, "A grasping tentacle attack." },
{ "Pseudopod",          ITEMTYPE_WEAPON, 32,    0, 0, ICON_MISC, false, false, RARITY_COMMON, THEME_ANY, "A crushing pseudopod attack." },

	//======================================================
// Light Armor
//======================================================

{ "Padded Armor",      ITEMTYPE_ARMOR,  0,    5, 10, ICON_ARMOR, false, false, RARITY_COMMON,   THEME_ANY, "Quilted cloth armor offering minimal protection." },
{ "Quilted Armor",     ITEMTYPE_ARMOR,  1,   10,  8, ICON_ARMOR, false, false, RARITY_COMMON,   THEME_ANY, "Comfortable layered cloth armor." },
{ "Leather Armor",     ITEMTYPE_ARMOR,  2,   25, 15, ICON_ARMOR, false, false, RARITY_COMMON,   THEME_ANY, "Hardened leather armor." },
{ "Studded Leather",   ITEMTYPE_ARMOR,  3,   25, 20, ICON_ARMOR, false, false, RARITY_COMMON,   THEME_ANY, "Leather reinforced with metal studs." },
{ "Chain Shirt",       ITEMTYPE_ARMOR,  4,  100, 25, ICON_ARMOR, false, false, RARITY_COMMON,   THEME_ANY, "A light shirt of interlocking rings." },

//======================================================
// Medium Armor
//======================================================

{ "Hide Armor",        ITEMTYPE_ARMOR,  5,   15, 25, ICON_ARMOR, false, false, RARITY_COMMON,   THEME_FOREST,  "Armor crafted from thick animal hides." },
{ "Scale Mail",        ITEMTYPE_ARMOR,  6,   50, 30, ICON_ARMOR, false, false, RARITY_COMMON,   THEME_ANY, "Metal scales sewn onto leather." },
{ "Breastplate",       ITEMTYPE_ARMOR,  7,  200, 30, ICON_ARMOR, false, false, RARITY_UNCOMMON, THEME_ANY, "A solid steel breastplate." },
{ "Chainmail",         ITEMTYPE_ARMOR,  8,   75, 40, ICON_ARMOR, false, false, RARITY_COMMON,   THEME_ANY, "Interlocking steel rings protecting the body." },

//======================================================
// Heavy Armor
//======================================================

{ "Splint Mail",       ITEMTYPE_ARMOR,  9,  200, 45, ICON_ARMOR, false, false, RARITY_UNCOMMON, THEME_ANY, "Steel splints over heavy padding." },
{ "Banded Mail",       ITEMTYPE_ARMOR, 10,  250, 35, ICON_ARMOR, false, false, RARITY_UNCOMMON, THEME_ANY, "Wide overlapping steel bands." },
{ "Half Plate",        ITEMTYPE_ARMOR, 11,  600, 50, ICON_ARMOR, false, false, RARITY_RARE,      THEME_ANY, "Heavy armor with articulated plates." },
{ "Full Plate",        ITEMTYPE_ARMOR, 12, 1500, 50, ICON_ARMOR, false, false, RARITY_EPIC,      THEME_ANY, "The finest protection a warrior can wear." },

	//======================================================
	// natural armor
	//======================================================
{ "Natural Armor I",    ITEMTYPE_ARMOR,  13,    0, 0, ICON_ARMOR, false, false, RARITY_COMMON, THEME_ANY, "Tough natural hide." },
{ "Natural Armor II",   ITEMTYPE_ARMOR,  14,    0, 0, ICON_ARMOR, false, false, RARITY_COMMON, THEME_ANY, "Thick natural protection." },
{ "Natural Armor III",  ITEMTYPE_ARMOR,  15,    0, 0, ICON_ARMOR, false, false, RARITY_COMMON, THEME_ANY, "Exceptional natural armor." },
{ "Natural Armor IV",   ITEMTYPE_ARMOR,  16,    0, 0, ICON_ARMOR, false, false, RARITY_COMMON, THEME_ANY, "Extremely resilient natural armor." },
{ "Natural Armor V",    ITEMTYPE_ARMOR,  17,    0, 0, ICON_ARMOR, false, false, RARITY_COMMON, THEME_ANY, "Nearly impenetrable natural armor." },

	//======================================================
	// Shields
	//======================================================

	{ "Buckler",             ITEMTYPE_SHIELD, 0,    5,  5, ICON_SHIELD, false, false, RARITY_COMMON, THEME_ANY, "A small shield strapped to the forearm." },
	{ "Light Wooden Shield", ITEMTYPE_SHIELD, 1,    3,  5, ICON_SHIELD, false, false, RARITY_COMMON, THEME_ANY, "A lightweight wooden shield." },
	{ "Light Steel Shield",  ITEMTYPE_SHIELD, 2,    9,  6, ICON_SHIELD, false, false, RARITY_COMMON, THEME_ANY, "A lightweight steel shield." },
	{ "Heavy Wooden Shield", ITEMTYPE_SHIELD, 3,   40, 10, ICON_SHIELD, false, false, RARITY_COMMON, THEME_ANY, "A sturdy wooden shield." },
	{ "Heavy Steel Shield",  ITEMTYPE_SHIELD, 4,   20, 15, ICON_SHIELD, false, false, RARITY_COMMON, THEME_ANY, "A heavy steel shield." },
	{ "Tower Shield",        ITEMTYPE_SHIELD, 5,   30, 45, ICON_SHIELD, false, false, RARITY_RARE,   THEME_ANY, "An enormous shield providing exceptional cover." },

	// ======================================================
	// Potions
	// ======================================================

	{ "Potion of Cure Light Wounds",      ITEMTYPE_POTION, 0, 50,   1, ICON_POTION, true, true, RARITY_COMMON,   THEME_ANY,    "Restores a small amount of health." },
	{ "Potion of Cure Moderate Wounds",   ITEMTYPE_POTION, 1, 300,  1, ICON_POTION, true, true, RARITY_UNCOMMON, THEME_ANY,    "Restores a moderate amount of health." },
	{ "Potion of Cure Serious Wounds",    ITEMTYPE_POTION, 2, 750,  1, ICON_POTION, true, true, RARITY_RARE,      THEME_ANY,    "Restores a large amount of health." },
	{ "Potion of Cure Critical Wounds",   ITEMTYPE_POTION, 3, 1400, 1, ICON_POTION, true, true, RARITY_EPIC,      THEME_ANY,    "Restores a massive amount of health." },

	{ "Potion of Bull's Strength",         ITEMTYPE_POTION, 4, 300,  1, ICON_POTION, true, true, RARITY_UNCOMMON, THEME_ANY,    "Temporarily increases Strength." },
	{ "Potion of Cat's Grace",             ITEMTYPE_POTION, 5, 300,  1, ICON_POTION, true, true, RARITY_UNCOMMON, THEME_ANY,    "Temporarily increases Dexterity." },
	{ "Potion of Bear's Endurance",        ITEMTYPE_POTION, 6, 300,  1, ICON_POTION, true, true, RARITY_UNCOMMON, THEME_ANY,    "Temporarily increases Constitution." },
	{ "Potion of Fox's Cunning",           ITEMTYPE_POTION, 7, 300,  1, ICON_POTION, true, true, RARITY_UNCOMMON, THEME_ANY,    "Temporarily increases Intelligence." },
	{ "Potion of Owl's Wisdom",            ITEMTYPE_POTION, 8, 300,  1, ICON_POTION, true, true, RARITY_UNCOMMON, THEME_ANY,    "Temporarily increases Wisdom." },
	{ "Potion of Eagle's Splendor",        ITEMTYPE_POTION, 9, 300,  1, ICON_POTION, true, true, RARITY_UNCOMMON, THEME_ANY,    "Temporarily increases Charisma." },

	{ "Potion of Barkskin",                ITEMTYPE_POTION,10, 300,  1, ICON_POTION, true, true, RARITY_UNCOMMON, THEME_ANY,     "Temporarily hardens the skin." },
	{ "Potion of Shield of Faith",         ITEMTYPE_POTION,11, 300,  1, ICON_POTION, true, true, RARITY_UNCOMMON, THEME_ANY,    "Grants a divine protection bonus." },
	{ "Potion of Resist Energy",           ITEMTYPE_POTION,12, 300,  1, ICON_POTION, true, true, RARITY_UNCOMMON, THEME_DRAGON,     "Reduces elemental damage." },
	{ "Potion of Protection from Evil",   ITEMTYPE_POTION,13, 300,  1, ICON_POTION, true, true, RARITY_UNCOMMON, THEME_DARK_MAGIC, "Provides protection against evil creatures." },
	{ "Potion of Remove Fear",             ITEMTYPE_POTION,14, 50,   1, ICON_POTION, true, true, RARITY_COMMON,   THEME_ANY,    "Removes fear effects." },

	{ "Potion of Invisibility",            ITEMTYPE_POTION,15, 300,  1, ICON_POTION, true, true, RARITY_RARE,      THEME_ANY,    "Become invisible for a short time." },
	{ "Potion of Haste",                   ITEMTYPE_POTION,16, 750,  1, ICON_POTION, true, true, RARITY_RARE,      THEME_ANY,    "Move and act more quickly." },
	{ "Potion of Heroism",                 ITEMTYPE_POTION,17,1000,  1, ICON_POTION, true, true, RARITY_RARE,      THEME_ANY,    "Grants courage and combat skill." },
	{ "Potion of Fly",                     ITEMTYPE_POTION,18, 750,  1, ICON_POTION, true, true, RARITY_RARE,      THEME_DRAGON,     "Allows the drinker to fly." },
	{ "Potion of Water Breathing",         ITEMTYPE_POTION,19, 750,  1, ICON_POTION, true, true, RARITY_RARE,      THEME_ANY,    "Allows breathing underwater." },


	// ======================================================
	// Scrolls
	// ======================================================


	// ======================================================
	// Wands
	// ======================================================

	// ======================================================
	// Adventuring Gear
	// ======================================================

	{ "Torch",             ITEMTYPE_ADVENTURING_GEAR, 0,   1,    1, ICON_MISC, false, false, RARITY_COMMON,   THEME_ANY, "Lights dark areas." },
	{ "Rope (50 ft)",      ITEMTYPE_ADVENTURING_GEAR, 1,  10,   10, ICON_MISC, false, false, RARITY_COMMON,   THEME_ANY, "Useful for climbing." },
	{ "Bedroll",           ITEMTYPE_ADVENTURING_GEAR, 2,   1,    5, ICON_MISC, false, false, RARITY_COMMON,   THEME_ANY, "A comfortable night's sleep." },
	{ "Rations",           ITEMTYPE_ADVENTURING_GEAR, 3,   5,    2, ICON_MISC, true,  true,  RARITY_COMMON,   THEME_ANY, "Food for adventuring." },
	{ "Waterskin",         ITEMTYPE_ADVENTURING_GEAR, 4,   1,    4, ICON_MISC, false, false, RARITY_COMMON,   THEME_ANY, "Fresh drinking water." },
	{ "Crowbar",           ITEMTYPE_ADVENTURING_GEAR, 5,   2,    5, ICON_MISC, false, false, RARITY_COMMON,   THEME_ANY, "Helps pry open objects." },
	{ "Flint & Steel",     ITEMTYPE_ADVENTURING_GEAR, 6,   1,    0, ICON_MISC, false, false, RARITY_COMMON,   THEME_ANY, "Starts fires." },
	{ "Grappling Hook",    ITEMTYPE_ADVENTURING_GEAR, 7,   1,    4, ICON_MISC, false, false, RARITY_UNCOMMON, THEME_ANY, "Useful with rope." },
	{ "Thieves' Tools",    ITEMTYPE_ADVENTURING_GEAR, 8, 100,    1, ICON_MISC, false, false, RARITY_UNCOMMON, THEME_ANY, "Required for picking locks." },
	{ "Shovel",            ITEMTYPE_ADVENTURING_GEAR, 9,   2,    8, ICON_MISC, false, false, RARITY_COMMON,   THEME_ANY, "Useful for digging." },
	{ "Hammer",            ITEMTYPE_ADVENTURING_GEAR,10,   5,    2, ICON_MISC, false, false, RARITY_COMMON,   THEME_ANY, "General-purpose hammer." },
	{ "Iron Spikes (10)",  ITEMTYPE_ADVENTURING_GEAR,11,   1,    5, ICON_MISC, true,  false, RARITY_COMMON,   THEME_ANY, "Secure doors or climb." },

	// ======================================================
	// Treasure
	// ======================================================

	{ "Silver Ring",    ITEMTYPE_TREASURE, 0,  100, 0, ICON_RING, false, false, RARITY_COMMON,   THEME_ANY, "A simple silver ring." },
	{ "Gold Ring",      ITEMTYPE_TREASURE, 1,  250, 0, ICON_RING, false, false, RARITY_UNCOMMON, THEME_ANY, "A finely crafted gold ring." },
	{ "Pearl",          ITEMTYPE_TREASURE, 2,  500, 0, ICON_GEM,      false, false, RARITY_UNCOMMON, THEME_ANY, "A beautiful white pearl." },
	{ "Ruby",           ITEMTYPE_TREASURE, 3, 1000, 0, ICON_GEM,      false, false, RARITY_RARE,     THEME_ANY, "A brilliant red ruby." },
	{ "Emerald",        ITEMTYPE_TREASURE, 4, 1000, 0, ICON_GEM,      false, false, RARITY_RARE,     THEME_ANY, "A deep green emerald." },
	{ "Sapphire",       ITEMTYPE_TREASURE, 5, 1000, 0, ICON_GEM,      false, false, RARITY_RARE,     THEME_ANY, "A sparkling blue sapphire." },
	{ "Diamond",        ITEMTYPE_TREASURE, 6, 5000, 0, ICON_GEM,      false, false, RARITY_EPIC,     THEME_ANY, "An exceptionally valuable diamond." },
	{ "Silver Goblet",  ITEMTYPE_TREASURE, 7,  350, 2, ICON_GOBLET, false, false, RARITY_UNCOMMON, THEME_ANY, "An ornate silver goblet." },
	{ "Golden Idol",    ITEMTYPE_TREASURE, 8, 2500, 8, ICON_IDOL, false, false, RARITY_EPIC,     THEME_ANY, "A priceless golden idol." },
	{ "Jeweled Crown",  ITEMTYPE_TREASURE, 9, 5000, 4, ICON_CROWN, false, false, RARITY_LEGENDARY,THEME_ANY, "A crown fit for royalty." },

	// ======================================================
	// Quest Items
	// ======================================================

	{ "Dragon Egg",          ITEMTYPE_QUEST, 0, 2500, 15, ICON_EGG, false, false, RARITY_EPIC,      THEME_ANY, "An unhatched dragon egg." },
	{ "Ancient Relic",       ITEMTYPE_QUEST, 1, 1800,  5, ICON_RELIC, false, false, RARITY_RARE,      THEME_ANY, "An artifact from a forgotten age." },
	{ "Royal Seal",          ITEMTYPE_QUEST, 2, 1200,  1, ICON_LETTER, false, false, RARITY_RARE,      THEME_ANY, "Proof of royal authority." },
	{ "Goblin King's Crown", ITEMTYPE_QUEST, 3, 1500,  3, ICON_CROWN, false, false, RARITY_RARE,      THEME_GOBLIN, "The crown of the Goblin King." },
	{ "Lost Necklace",       ITEMTYPE_QUEST, 4,  800,  0, ICON_RING, false, false, RARITY_UNCOMMON,  THEME_ANY, "Someone is searching for this." },
	{ "Sacred Chalice",      ITEMTYPE_QUEST, 5, 2200,  4, ICON_GOBLET, false, false, RARITY_EPIC,      THEME_TEMPLE, "A holy relic." },
	{ "Mysterious Crystal",  ITEMTYPE_QUEST, 6, 3000,  2, ICON_GEM, false, false, RARITY_EPIC,      THEME_CAVE, "It glows with magical energy." },
	{ "King's Letter",       ITEMTYPE_QUEST, 7,  500,  0, ICON_LETTER, false, false, RARITY_UNCOMMON,  THEME_ANY, "A sealed royal letter." },

};

static_assert(
	ITEM_COUNT == (sizeof(itemDatabase) / sizeof(itemDatabase[0])),
	"Item database is out of sync with ItemID."
);

const Weapon weaponDatabase[] =
{
	//======================================================
	// Simple Melee
	//======================================================

	// Club
	{ WEAPON_MELEE, DAMAGE_BLUDGEONING, 1, 6, 20, 2,   0, WEAPON_PROP_NONE },
	// Dagger
	{ WEAPON_MELEE, DAMAGE_PIERCING,    1, 4, 19, 2,  10, WEAPON_PROP_FINESSE | WEAPON_PROP_THROWN },
	// Greatclub
	{ WEAPON_MELEE, DAMAGE_BLUDGEONING, 1,10, 20, 2,   0, WEAPON_PROP_TWO_HANDED },
	// Light Hammer
	{ WEAPON_MELEE, DAMAGE_BLUDGEONING, 1, 4, 20, 2,  20, WEAPON_PROP_THROWN },
	// Mace
	{ WEAPON_MELEE, DAMAGE_BLUDGEONING, 1, 8, 20, 2,   0, WEAPON_PROP_NONE },
	// Morningstar
	{ WEAPON_MELEE, DAMAGE_PIERCING,    1, 8, 20, 2,   0, WEAPON_PROP_NONE },
	// Quarterstaff
	{ WEAPON_MELEE, DAMAGE_BLUDGEONING, 1, 6, 20, 2,   0, WEAPON_PROP_TWO_HANDED },
	// Shortspear
	{ WEAPON_MELEE, DAMAGE_PIERCING,    1, 6, 20, 2,  20, WEAPON_PROP_THROWN },
	// Sickle
	{ WEAPON_MELEE, DAMAGE_SLASHING,    1, 6, 20, 2,   0, WEAPON_PROP_NONE },
	// Spear
	{ WEAPON_MELEE, DAMAGE_PIERCING,    1, 8, 20, 3,  20, WEAPON_PROP_THROWN | WEAPON_PROP_TWO_HANDED },

	//======================================================
	// Martial Melee
	//======================================================

	// Battleaxe
	{ WEAPON_MELEE, DAMAGE_SLASHING,    1, 8, 20, 3,   0, WEAPON_PROP_NONE },
	// Falchion
	{ WEAPON_MELEE, DAMAGE_SLASHING,    2, 4, 18, 2,   0, WEAPON_PROP_TWO_HANDED },
	// Flail
	{ WEAPON_MELEE, DAMAGE_BLUDGEONING, 1, 8, 20, 2,   0, WEAPON_PROP_NONE },
	// Greataxe
	{ WEAPON_MELEE, DAMAGE_SLASHING,    1,12, 20, 3,   0, WEAPON_PROP_TWO_HANDED },
	// Greatsword
	{ WEAPON_MELEE, DAMAGE_SLASHING,    2, 6, 19, 2,   0, WEAPON_PROP_TWO_HANDED },
	// Heavy Pick
	{ WEAPON_MELEE, DAMAGE_PIERCING,    1, 6, 20, 4,   0, WEAPON_PROP_NONE },
	// Lance
	{ WEAPON_MELEE, DAMAGE_PIERCING,    1, 8, 20, 3,   0, WEAPON_PROP_REACH },
	// Longsword
	{ WEAPON_MELEE, DAMAGE_SLASHING,    1, 8, 19, 2,   0, WEAPON_PROP_NONE },
	// Rapier
	{ WEAPON_MELEE, DAMAGE_PIERCING,    1, 6, 18, 2,   0, WEAPON_PROP_FINESSE },
	// Scimitar
	{ WEAPON_MELEE, DAMAGE_SLASHING,    1, 6, 18, 2,   0, WEAPON_PROP_FINESSE },
	// Trident
	{ WEAPON_MELEE, DAMAGE_PIERCING,    1, 8, 20, 2,  10, WEAPON_PROP_THROWN },
	// Warhammer
	{ WEAPON_MELEE, DAMAGE_BLUDGEONING, 1, 8, 20, 3,   0, WEAPON_PROP_NONE },

	//======================================================
	// Ranged
	//======================================================

	// Shortbow
	{ WEAPON_RANGED, DAMAGE_PIERCING,   1, 6, 20, 3,  60, WEAPON_PROP_TWO_HANDED },
	// Longbow
	{ WEAPON_RANGED, DAMAGE_PIERCING,   1, 8, 20, 3, 100, WEAPON_PROP_TWO_HANDED },
	// Composite Longbow
	{ WEAPON_RANGED, DAMAGE_PIERCING,   1, 8, 20, 3, 110, WEAPON_PROP_TWO_HANDED },
	// Light Crossbow
	{ WEAPON_RANGED, DAMAGE_PIERCING,   1, 8, 19, 2,  80, WEAPON_PROP_TWO_HANDED },
	// Heavy Crossbow
	{ WEAPON_RANGED, DAMAGE_PIERCING,   1,10, 19, 2, 120, WEAPON_PROP_TWO_HANDED },
	// Sling
	{ WEAPON_RANGED, DAMAGE_BLUDGEONING,1, 4, 20, 2,  50, WEAPON_PROP_NONE },

	//======================================================
	// Natural Weapons
	//======================================================

	// Bite
	{ WEAPON_MELEE, DAMAGE_PIERCING,    1, 6, 20, 2,   0, WEAPON_PROP_NONE },
	// Claws
	{ WEAPON_MELEE, DAMAGE_SLASHING,    1, 4, 20, 2,   0, WEAPON_PROP_NONE },
	// Slam
	{ WEAPON_MELEE, DAMAGE_BLUDGEONING, 1, 6, 20, 2,   0, WEAPON_PROP_NONE },
	// Tentacle
	{ WEAPON_MELEE, DAMAGE_BLUDGEONING, 1, 4, 20, 2,   0, WEAPON_PROP_REACH },
	// Pseudopod
	{ WEAPON_MELEE, DAMAGE_BLUDGEONING, 1, 6, 20, 2,   0, WEAPON_PROP_NONE }
};

const Armor armorDatabase[] =
{
	// armorBonus, maxDex, ACP, speed30, speed20, ASF

	{ 1,  8,  0, 30, 20,  5 },   // Padded Armor
	{ 1,  8,  0, 30, 20,  5 },   // Quilted Armor
	{ 2,  6,  0, 30, 20, 10 },   // Leather Armor
	{ 3,  5, -1, 30, 20, 15 },   // Studded Leather
	{ 4,  4, -2, 30, 20, 20 },   // Chain Shirt

	{ 4,  4, -3, 20, 15, 20 },   // Hide Armor
	{ 5,  3, -4, 20, 15, 25 },   // Scale Mail
	{ 6,  3, -4, 20, 15, 25 },   // Breastplate
	{ 6,  2, -5, 20, 15, 30 },   // Chainmail

	{ 7,  0, -7, 20, 15, 40 },   // Splint Mail
	{ 7,  1, -6, 20, 15, 35 },   // Banded Mail
	{ 8,  0, -7, 20, 15, 40 },   // Half Plate
	{ 9,  1, -6, 20, 15, 35 },   // Full Plate

	//======================================================
	// Natural Armor
	//======================================================

	{ 1, 99,  0, 30, 20, 0 },   // Natural Armor I
	{ 2, 99,  0, 30, 20, 0 },   // Natural Armor II
	{ 3, 99,  0, 30, 20, 0 },   // Natural Armor III
	{ 4, 99,  0, 30, 20, 0 },   // Natural Armor IV
	{ 5, 99,  0, 30, 20, 0 },   // Natural Armor V
};

const Shield shieldDatabase[] =
{
	// shield, maxDex, ACP, ASF

	{ 1, 99, -1,  5 },   // Buckler
	{ 1, 99, -1,  5 },   // Light Wooden
	{ 1, 99, -1,  5 },   // Light Steel
	{ 2, 99, -2, 15 },   // Heavy Wooden
	{ 2, 99, -2, 15 },   // Heavy Steel
	{ 4,  2, -10, 50 },  // Tower Shield
};

const Item* getItem(ItemID item)
{
	if (item < ITEM_NONE || item >= ITEM_COUNT)
		return nullptr;

	return &itemDatabase[item];
}

const Weapon* getWeapon(ItemID item)
{
	const Item* itemInfo = getItem(item);

	if (itemInfo == nullptr || itemInfo->type != ITEMTYPE_WEAPON)
		return nullptr;

	return &weaponDatabase[itemInfo->effectIndex];
}

const Armor* getArmor(ItemID item)
{
	const Item* itemInfo = getItem(item);

	if (itemInfo == nullptr || itemInfo->type != ITEMTYPE_ARMOR)
		return nullptr;

	return &armorDatabase[itemInfo->effectIndex];
}

const Shield* getShield(ItemID item)
{
	const Item* itemInfo = getItem(item);

	if (itemInfo == nullptr || itemInfo->type != ITEMTYPE_SHIELD)
		return nullptr;

	return &shieldDatabase[itemInfo->effectIndex];
}
