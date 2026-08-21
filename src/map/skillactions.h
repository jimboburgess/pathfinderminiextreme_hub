#ifndef PATHFINDERMINIEXTREME_025_SKILLACTIONS_H
#define PATHFINDERMINIEXTREME_025_SKILLACTIONS_H

#include "data/entities.h"

bool useSkill(Skill skill);
bool canCutFreeFromWeb(const Entity& entity);
bool cutFreeFromWeb();
bool canIgniteWeb(const Entity& entity);
bool igniteWeb();

int getIntimidateDC(const Entity& target);

inline int calculateIntimidateDC(int hitDice, int wisdomModifier)
{
    return 10 + hitDice + wisdomModifier;
}

inline bool intimidateSucceeds(int skillTotal, int targetDC)
{
    return skillTotal >= targetDC;
}

enum SocialCheckResult : uint8_t
{
    SOCIAL_NEUTRAL,
    SOCIAL_FAVORABLE
};

SocialCheckResult resolveAutomaticSocialCheck(
    const Character& character,
    Skill skill,
    int dc);
const char* getShopDiplomacyMessage(SocialCheckResult result);

inline SocialCheckResult resolveSocialCheckTotal(int total, int dc)
{
    return total >= dc ? SOCIAL_FAVORABLE : SOCIAL_NEUTRAL;
}

#endif
