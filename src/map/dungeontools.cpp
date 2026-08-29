#include "map/dungeontools.h"

DisableDeviceToolType getDisableDeviceTool(const Character& character)
{
    if (hasItem(character, ITEM_MASTERWORK_THIEVES_TOOLS))
        return DISABLE_TOOL_MASTERWORK;

    if (hasItem(character, ITEM_THIEVES_TOOLS))
        return DISABLE_TOOL_STANDARD;

    return DISABLE_TOOL_NONE;
}

int getDisableDeviceToolModifier(DisableDeviceToolType tool)
{
    switch (tool)
    {
        case DISABLE_TOOL_MASTERWORK:
            return 2;

        case DISABLE_TOOL_STANDARD:
            return 0;

        case DISABLE_TOOL_NONE:
        default:
            return -4;
    }
}

int getDisableDeviceToolModifier(const Character& character)
{
    return getDisableDeviceToolModifier(getDisableDeviceTool(character));
}

int getLockDisableDeviceModifier(const Character& character)
{
    return getDisableDeviceToolModifier(character) +
        (hasItem(character, ITEM_CROWBAR) ? 2 : 0);
}

int getForceOpenToolModifier(const Character& character)
{
    return hasItem(character, ITEM_CROWBAR) ? 2 : 0;
}

bool isDisableDeviceAutomaticFailure(int naturalRoll)
{
    return naturalRoll == 1;
}

bool handleDisableDeviceToolBreak(
    Character& character,
    DisableDeviceToolType tool,
    int naturalRoll)
{
    if (!isDisableDeviceAutomaticFailure(naturalRoll))
        return false;

    switch (tool)
    {
        case DISABLE_TOOL_MASTERWORK:
            return removeItem(character, ITEM_MASTERWORK_THIEVES_TOOLS, 1);

        case DISABLE_TOOL_STANDARD:
            return removeItem(character, ITEM_THIEVES_TOOLS, 1);

        case DISABLE_TOOL_NONE:
        default:
            return false;
    }
}
