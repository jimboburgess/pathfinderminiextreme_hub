#include "dungeon/puzzleinstructions.h"

const char* getPuzzleEntryInstruction(DungeonPuzzleType puzzleType)
{
    switch (puzzleType)
    {
        case PUZZLE_BELLS:
            return "Match the tones. Ring the bells in the same order.";
        case PUZZLE_NUMBER_TILES:
            return "Follow the rule. Cross using only the correct numbers.";
        case PUZZLE_BRAZIERS:
            return "Light all the braziers at the same time.";
        case PUZZLE_RIDDLEMAN:
        case PUZZLE_NONE:
        default:
            return nullptr;
    }
}
