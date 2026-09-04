#include "dungeon/riddles.h"

namespace
{
const RiddleDefinition RIDDLE_DEFINITIONS[] =
{
    {RIDDLE_MOUNTAIN, RIDDLE_EASY, "What has roots as nobody sees, Is taller than trees, Up, up it goes and yet never grows?", {"Mountain", "Tower", "Tree", "Cloud"}, 0}
    ,{RIDDLE_WIND, RIDDLE_MEDIUM, "Voiceless it cries, wingless flutters, toothless bites, mouthless mutters.", {"Wind", "Frost", "Thunder", "Shadow"}, 0}
    ,{RIDDLE_DARK, RIDDLE_HARD, "It cannot be seen, cannot be felt, cannot be heard, cannot be smelt. It lies behind stars and under hills, and empty holes it fills. It comes first and follows after, ends life, kills laughter.", {"Dark", "Silence", "Death", "Time"}, 0}
    ,{RIDDLE_FISH, RIDDLE_MEDIUM, "Alive without breath, as cold as death; never thirsty, ever drinking, all in mail never clinking.", {"Fish", "River", "Snake", "Ice"}, 0}
    ,{RIDDLE_TIME, RIDDLE_MEDIUM, "This thing all things devours, Birds, beasts, trees, flowers, Gnaws iron, bites steel;Grinds hard stones to meal Slays king, ruins town And beats high mountain down.", {"Time", "Fire", "War", "Hunger"}, 0}
    ,{RIDDLE_TEETH, RIDDLE_MEDIUM, "Thirty white horses on a red hill, first they champ, then they stamp, then they stand still.", {"Teeth", "Soldiers", "Waves", "Fingers"}, 0}
    ,{RIDDLE_EGG, RIDDLE_EASY, "A box without hinges, key, or lid, yet golden treasure inside is hid.", {"Egg", "Chest", "Seed", "Coconut"}, 0}
    ,{RIDDLE_LETTER_E, RIDDLE_HARD, "The beginning of eternity, the end of time and space, the beginning of every end, and the end of every place.", {"Letter E", "Letter T", "Letter N", "Letter A"}, 0}
    ,{RIDDLE_MATCH, RIDDLE_MEDIUM, "Take one out and scratch my head. I am now black but once was red.", {"Match", "Coal", "Candle", "Firewood"}, 0}
    ,{RIDDLE_RIVER, RIDDLE_EASY, "What can run but never walks, has a mouth but never talks, has a head but never weeps, has a bed but never sleeps?", {"River", "Wind", "Road", "Time"}, 0}
    ,{RIDDLE_TREE, RIDDLE_MEDIUM, "A hundred arms, a thousand fingers, but it has no eyes to see where it lingers.", {"Tree", "River", "Mountain", "Spider"}, 0}
    ,{RIDDLE_ONION, RIDDLE_EASY, "With a knife, cut open my head, then weep beside me when I am dead.", {"Onion", "Candle", "Tree", "Book"}, 0}
    ,{RIDDLE_ECHO, RIDDLE_MEDIUM, "I am sometimes strong and sometimes weak, but am nobody's fool. There is no language I can't speak, though I never went to school.", {"Echo", "Music", "Wind", "Writing"}, 0}
    ,{RIDDLE_SPLINTER, RIDDLE_HARD, "I went into the woods and got it. I sat down to seek it. I went home because I couldn't find it.", {"Splinter", "Tick", "Thorn", "Lost Coin"}, 0}
    ,{RIDDLE_LEAVES, RIDDLE_MEDIUM, "Walk on the living, they don't even mumble. Walk on the dead, they mutter and grumble.", {"Leaves", "Snow", "Gravel", "Bones"}, 0}
    ,{RIDDLE_GLOVES, RIDDLE_MEDIUM, "Buckets, barrels, baskets, cans. What must you fill with empty hands?", {"Gloves", "Pockets", "Shoes", "Sacks"}, 0}
    ,{RIDDLE_SKULL, RIDDLE_MEDIUM, "I don't have eyes, but once I did see. Once I had thoughts, but now I'm white and empty.", {"Skull", "Eggshell", "Cloud", "Book"}, 0}
    ,{RIDDLE_FOOTSTEPS, RIDDLE_EASY, "The more you take, the more you leave behind. What is it?", {"Footsteps", "Memories", "Years", "Breadcrumbs"}, 0}
    ,{RIDDLE_SILENCE, RIDDLE_EASY, "No sooner spoken than broken. What is it?", {"Silence", "Promise", "Secret", "Darkness"}, 0}
    ,{RIDDLE_FIRE, RIDDLE_EASY, "Feed me and I live, give me drink and I die. What am I?", {"Fire", "Plant", "Candle", "Thirst"}, 0}
};

static_assert(sizeof(RIDDLE_DEFINITIONS) / sizeof(RIDDLE_DEFINITIONS[0]) == RIDDLE_COUNT,
              "Riddle table must match RiddleID");
}

const RiddleDefinition* getRiddleDefinition(RiddleID id)
{
    return id < RIDDLE_COUNT ? &RIDDLE_DEFINITIONS[id] : nullptr;
}

bool isValidRiddleAnswerOrder(const RiddleState& state)
{
    if (getRiddleDefinition(state.id) == nullptr) return false;
    uint8_t seen = 0;
    for (uint8_t index : state.answerOrder)
    {
        if (index >= 4 || (seen & (1u << index)) != 0) return false;
        seen |= static_cast<uint8_t>(1u << index);
    }
    return seen == 0x0F;
}

void initializeRiddleState(RiddleState& state, RiddleID id, const uint8_t shuffleRolls[3])
{
    state = RiddleState{};
    if (getRiddleDefinition(id) == nullptr) return;
    state.id = id;
    for (int8_t index = 3; index > 0; --index)
    {
        const uint8_t swapIndex = shuffleRolls[3 - index] % (index + 1);
        const uint8_t value = state.answerOrder[index];
        state.answerOrder[index] = state.answerOrder[swapIndex];
        state.answerOrder[swapIndex] = value;
    }
}

const char* getDisplayedRiddleAnswer(const RiddleState& state, uint8_t displayIndex)
{
    const RiddleDefinition* definition = getRiddleDefinition(state.id);
    if (definition == nullptr || !isValidRiddleAnswerOrder(state) || displayIndex >= 4) return nullptr;
    return definition->answers[state.answerOrder[displayIndex]];
}

bool answerRiddle(RiddleState& state, uint8_t displayIndex)
{
    const RiddleDefinition* definition = getRiddleDefinition(state.id);
    if (definition == nullptr || !isValidRiddleAnswerOrder(state) ||
        displayIndex >= 4 || state.result != RIDDLE_UNANSWERED) return false;
    state.selectedAnswer = displayIndex;
    state.result = state.answerOrder[displayIndex] == definition->correctAnswerIndex
        ? RIDDLE_ANSWERED_CORRECT : RIDDLE_ANSWERED_INCORRECT;
    return true;
}
