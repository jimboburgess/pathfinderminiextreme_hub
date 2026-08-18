#ifndef PATHFINDERMINIEXTREME_025_AWARENESS_H
#define PATHFINDERMINIEXTREME_025_AWARENESS_H

struct Entity;

void updateAwareness();
void resetAwarenessTimer();
int getStealthSituationModifier(const Entity& player, const Entity& observer);

#endif
