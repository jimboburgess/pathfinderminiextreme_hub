#include "map/skillactions.h"

#include <Arduino.h>
#include <cstdio>

#include "audio/audio.h"
#include "characters/conditions.h"
#include "data/dice.h"
#include "data/entitytraits.h"
#include "data/entityspawn.h"
#include "data/game.h"
#include "dungeon/combat.h"
#include "graphics/messagelog.h"
#include "map/activemap.h"
#include "map/awareness.h"

namespace
{
bool canUseStandardSkillAction(Entity& player)
{
    return !combat.active ||
        (isPlayerTurn() && combat.waitingForPlayer &&
         canCharacterAct(player.character) &&
         !player.turn.standardActionUsed);
}

void spendStandardSkillAction(Entity& player)
{
    if (!combat.active)
        return;

    player.turn.standardActionUsed = true;
    combat.waitingForPlayer = true;
    checkEndPlayerTurn();
}

Entity* getClosestVisibleHostile(Entity& player)
{
    uint8_t count = 0;
    Entity* entities = getActiveMapEntities(count);
    Entity* best = nullptr;
    int bestDistance = 32767;

    for (uint8_t i = 0; entities != nullptr && i < count; i++)
    {
        Entity& candidate = entities[i];
        if (!candidate.active || candidate.type != ENTITY_MONSTER ||
            candidate.character.team != TEAM_MONSTER ||
            candidate.character.state != STATE_ALIVE ||
            !candidate.visibleToPlayer)
        {
            continue;
        }

        const int distance = getEntityGridDistance(player, candidate);
        if (distance < bestDistance)
        {
            best = &candidate;
            bestDistance = distance;
        }
    }

    return best;
}

bool usePerception(Entity& player)
{
    if (!canUseStandardSkillAction(player))
        return false;

    bool found = false;
    uint8_t count = 0;
    Entity* entities = getActiveMapEntities(count);
    for (uint8_t i = 0; entities != nullptr && i < count; i++)
        found = tryPlayerDetectMonster(player, entities[i]) || found;

    // Secret-door room search belongs here when that persistent geometry
    // system is added. This single entry point prevents menu-driven rerolls
    // from bypassing its eventual per-character/per-door attempt state.
    setGameMessage(found ? "You notice a hidden creature!"
                         : "You find nothing unusual.");
    spendStandardSkillAction(player);
    return true;
}

bool useStealth(Entity& player)
{
    if (combat.active)
    {
        setGameMessage("You cannot hide during combat.");
        playSound(SoundEffect::ERROR);
        return false;
    }

    uint8_t count = 0;
    Entity* entities = getActiveMapEntities(count);
    bool detected = false;
    for (uint8_t i = 0; entities != nullptr && i < count; i++)
    {
        if (tryMonsterDetectPlayer(entities[i], player))
        {
            detected = true;
            break;
        }
    }

    if (detected)
    {
        setGameMessage("Stealth failed!");
        startCombat();
    }
    else
    {
        setGameMessage("You move quietly.");
    }

    return true;
}

bool useIntimidate(Entity& player)
{
    if (!combat.active || !canUseStandardSkillAction(player))
    {
        setGameMessage("No Intimidate target available.");
        playSound(SoundEffect::ERROR);
        return false;
    }

    Entity* target = getClosestVisibleHostile(player);
    if (target == nullptr)
    {
        setGameMessage("No Intimidate target available.");
        playSound(SoundEffect::ERROR);
        return false;
    }

    const int total = rollDie(20) +
        getSkillBonus(player.character, SKILL_INTIMIDATE);
    const int dc = getIntimidateDC(*target);
    char message[64];

    if (intimidateSucceeds(total, dc) &&
        addCondition(target->character, CONDITION_FRIGHTENED, 0, 1))
    {
        snprintf(message, sizeof(message), "%s is frightened!",
                 getEntityName(target));
    }
    else
    {
        snprintf(message, sizeof(message), "Intimidate failed.");
    }

    setGameMessage(message);
    spendStandardSkillAction(player);
    return true;
}
}

int getIntimidateDC(const Entity& target)
{
    return calculateIntimidateDC(
        getEffectiveHitDice(target),
        getAbilityModifier(target.character, ABILITY_WISDOM));
}

bool useSkill(Skill skill)
{
    Entity* player = getActiveMapPlayer();
    if (player == nullptr || player->character.state != STATE_ALIVE)
        return false;

    switch (skill)
    {
        case SKILL_PERCEPTION: return usePerception(*player);
        case SKILL_STEALTH: return useStealth(*player);
        case SKILL_INTIMIDATE: return useIntimidate(*player);
        case SKILL_ACROBATICS:
            setGameMessage("No Acrobatics action available.");
            return false;
        default:
            setGameMessage("That skill is unavailable.");
            return false;
    }
}

SocialCheckResult resolveAutomaticSocialCheck(
    const Character& character,
    Skill skill,
    int dc)
{
    return resolveSocialCheckTotal(
        rollDie(20) + getSkillBonus(character, skill), dc);
}

const char* getShopDiplomacyMessage(SocialCheckResult result)
{
    return result == SOCIAL_FAVORABLE
        ? "The shopkeeper smiles as you walk in."
        : "The shopkeeper looks your way, waiting for you to act.";
}
