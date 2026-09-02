#ifndef PATHFINDERMINIEXTREME_025_MONSTER_IDLE_H
#define PATHFINDERMINIEXTREME_025_MONSTER_IDLE_H

// Advances independent, exploration-only monster movement. The function is
// safe to call every frame; each monster owns its own delayed action time.
void updateMonsterIdleBehavior();

#endif // PATHFINDERMINIEXTREME_025_MONSTER_IDLE_H
