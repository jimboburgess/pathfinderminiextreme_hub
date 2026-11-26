#include "runtime.h"
#include "controller_slots.h"
#include "data/game.h"
#include "graphics/messagelog.h"
#include "graphics/townscene.h"
#include "input/menu.h"
#include "town/town.h"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace
{
using namespace pfme::runtime;

void check(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

void wait(unsigned milliseconds)
{
    while (milliseconds != 0)
    {
        const unsigned step = std::min(milliseconds, 10u);
        advance(step);
        milliseconds -= step;
    }
}

void tap(const char* action)
{
    check(controllerInput(0, action, true), "input down");
    wait(30);
    check(controllerInput(0, action, false), "input up");
    wait(310);
}

void enterTown()
{
    tap("a");
    tap("a");
    tap("a");
    tap("a");
    tap("a");
    tap("encoder");
    check(gameState == GAME_TOWN, "character creation reaches Town");
    wait(20);
}

size_t highlightPixels(const TownLocationDefinition& location)
{
    const uint16_t* frame = controllerPixels(0);
    size_t count = 0;
    for (int y = location.y; y < location.y + location.height; ++y)
        for (int x = location.x; x < location.x + location.width; ++x)
            if (frame[y * 240 + x] == TOWN_SELECTION_COLOR) ++count;
    return count;
}

void checkSingleHighlight(TownOption selected)
{
    uint8_t highlighted = 0;
    for (uint8_t index = 0; index < TOWN_OPTION_COUNT; ++index)
    {
        const TownOption option = static_cast<TownOption>(index);
        const size_t pixels = highlightPixels(getTownLocation(option));
        if (pixels != 0)
        {
            ++highlighted;
            if (option != selected)
            {
                std::cerr << "unexpected highlight selected="
                          << static_cast<int>(selected) << " location="
                          << static_cast<int>(option) << " pixels="
                          << pixels << '\n';
                check(false, "gold outline belongs to selected location");
            }
        }
    }
    check(highlighted == 1, "exactly one Town location is highlighted");
}

uint32_t panelFingerprint()
{
    const uint16_t* frame = controllerPixels(0);
    uint32_t hash = 2166136261u;
    for (int y = TOWN_SCENE_HEIGHT; y < 240; ++y)
        for (int x = 0; x < 240; ++x)
        {
            hash ^= frame[y * 240 + x];
            hash *= 16777619u;
        }
    return hash;
}
}

int main()
{
    try
    {
        initializeControllers(1);
        ControllerState state;
        check(bindController(0, state), "bind controller");
        enterTown();

        check(TOWN_OPTION_COUNT == 8, "all eight Town locations exist");
        for (uint8_t index = 0; index < TOWN_OPTION_COUNT; ++index)
        {
            const TownLocationDefinition& location = getTownLocation(
                static_cast<TownOption>(index));
            check(location.id == index, "Town location IDs remain stable");
            check(location.displayName != nullptr &&
                  location.displayName[0] != '\0', "location has a name");
            check(location.description != nullptr &&
                  location.description[0] != '\0',
                  "location has a description");
            check(location.x >= 0 && location.y >= 0 &&
                  location.x + location.width <= 240 &&
                  location.y + location.height <= TOWN_SCENE_HEIGHT,
                  "location geometry fits the scene");
        }
        check(!getTownLocation(TOWN_BARN).available &&
              !getTownLocation(TOWN_CASTLE).available &&
              !getTownLocation(TOWN_MYSTERIOUS_ALLEY).available,
              "future Town locations remain unavailable");

        check(townSelection == TOWN_STAY_HOME,
              "Town opens focused on the player's house");
        bool visited[TOWN_OPTION_COUNT] = {};
        uint32_t priorPanel = panelFingerprint();
        for (uint8_t step = 0; step < TOWN_OPTION_COUNT; ++step)
        {
            check(!visited[townSelection], "clockwise selection does not repeat early");
            visited[townSelection] = true;
            checkSingleHighlight(townSelection);
            tap("cw");
            check(panelFingerprint() != priorPanel,
                  "information panel updates with selection");
            priorPanel = panelFingerprint();
            const uint16_t* incrementalFrame = controllerPixels(0);
            const std::vector<uint16_t> incremental(
                incrementalFrame, incrementalFrame + 57600);
            invalidateTownScene();
            redrawType = REDRAW_FULL;
            needsRedraw = true;
            wait(20);
            const uint16_t* cleanFrame = controllerPixels(0);
            if (!std::equal(incremental.begin(), incremental.end(), cleanFrame))
            {
                for (size_t index = 0; index < incremental.size(); ++index)
                    if (incremental[index] != cleanFrame[index])
                    {
                        std::cerr << "redraw mismatch step="
                                  << static_cast<int>(step) << " selected="
                                  << static_cast<int>(townSelection) << " x="
                                  << index % 240 << " y=" << index / 240
                                  << " incremental=" << incremental[index]
                                  << " clean=" << cleanFrame[index] << '\n';
                        break;
                    }
                check(false,
                      "incremental selection redraw matches a clean full scene");
            }
        }
        check(townSelection == TOWN_STAY_HOME,
              "clockwise Town selection wraps");

        tap("ccw");
        check(townSelection == TOWN_MYSTERIOUS_ALLEY,
              "counterclockwise Town selection wraps");
        checkSingleHighlight(TOWN_MYSTERIOUS_ALLEY);
        for (TownOption option :
             {TOWN_BARN, TOWN_CASTLE, TOWN_MYSTERIOUS_ALLEY})
        {
            townSelection = option;
            redrawType = REDRAW_FULL;
            needsRedraw = true;
            wait(20);
            const uint32_t futurePanel = panelFingerprint();
            tap("a");
            check(gameState == GAME_TOWN && !menuState.isOpen,
                  "unavailable location keeps control in Town");
            check(std::strstr(getGameMessage(), "currently") != nullptr,
                  "unavailable selection supplies clear feedback");
            check(panelFingerprint() == futurePanel,
                  "unavailable feedback preserves the location panel");
        }

        tap("cw");
        check(townSelection == TOWN_STAY_HOME,
              "moving from the final location wraps to Home");
        tap("a");
        check(isTownHomeOpen(), "player house opens Stay Home");
        tap("b");
        check(!isTownHomeOpen(), "Stay Home returns to Town scene");
        checkSingleHighlight(TOWN_STAY_HOME);

        townSelection = TOWN_SHOP;
        redrawType = REDRAW_FULL;
        needsRedraw = true;
        wait(20);
        tap("a");
        check(menuState.isOpen, "Shop location activates existing shop menu");
        tap("b");
        check(!menuState.isOpen, "Shop returns to Town scene");

        townSelection = TOWN_BULLETIN_BOARD;
        redrawType = REDRAW_FULL;
        needsRedraw = true;
        wait(20);
        tap("a");
        check(menuState.isOpen && menuState.menuStack[0] != nullptr &&
              std::strcmp(menuState.menuStack[0]->title,
                          "Bulletin Board") == 0,
              "Bulletin Board activates existing party menu");

        std::cout << "Town scene rendering and navigation tests passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL " << error.what() << '\n';
        return 1;
    }
}
