#ifndef PATHFINDERMINIEXTREME_025_DUNGEONTOOLS_H
#define PATHFINDERMINIEXTREME_025_DUNGEONTOOLS_H

#include <stdint.h>

#include "characters/characters.h"

// Carried dungeon gear selected for a Disable Device attempt. Later lock and
// environmental interactions can use this layer without putting item rules in
// trap or input code.
enum DisableDeviceToolType : uint8_t
{
    DISABLE_TOOL_NONE,
    DISABLE_TOOL_STANDARD,
    DISABLE_TOOL_MASTERWORK
};

DisableDeviceToolType getDisableDeviceTool(const Character& character);
int getDisableDeviceToolModifier(DisableDeviceToolType tool);
int getDisableDeviceToolModifier(const Character& character);
// Contextual modifiers for locks. The first includes the normal thieves'
// tools result (-4, 0, or +2) plus a carried crowbar; force-open only uses
// the crowbar contribution.
int getLockDisableDeviceModifier(const Character& character);
int getForceOpenToolModifier(const Character& character);
bool isDisableDeviceAutomaticFailure(int naturalRoll);

// Natural 1 is the only result that breaks a carried tool set. Returns true
// only when one selected set was removed from the authoritative inventory.
bool handleDisableDeviceToolBreak(
    Character& character,
    DisableDeviceToolType tool,
    int naturalRoll);

#endif // PATHFINDERMINIEXTREME_025_DUNGEONTOOLS_H
