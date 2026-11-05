#include "runtime.h"
#include "controller_slots.h"
#include "data/game.h"
#include "dungeon/dungeon.h"
#include "input/menu.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace
{
using namespace pfme::runtime;

const MenuItem TEST_ITEMS[] = {
    {"Option Alpha", "Alpha description", MENU_NONE, nullptr, MENU_CLASS_ALL},
    {"Option Beta", "Beta description", MENU_NONE, nullptr, MENU_CLASS_ALL},
    {"Option Gamma", "Gamma description", MENU_NONE, nullptr, MENU_CLASS_ALL}
};
const Menu TEST_MENU = {
    "Render Test", TEST_ITEMS,
    static_cast<uint8_t>(sizeof(TEST_ITEMS) / sizeof(TEST_ITEMS[0]))
};
const MenuItem CHILD_ITEMS[] = {
    {"Child One", "First child", MENU_NONE, nullptr, MENU_CLASS_ALL},
    {"Child Two", "Second child", MENU_NONE, nullptr, MENU_CLASS_ALL},
    {"Child Back", "Return", MENU_NONE, nullptr, MENU_CLASS_ALL}
};
const Menu CHILD_MENU = {
    "Child", CHILD_ITEMS,
    static_cast<uint8_t>(sizeof(CHILD_ITEMS) / sizeof(CHILD_ITEMS[0]))
};

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

void enterTestDungeon()
{
    tap("a");
    check(gameState == GAME_CHARACTER_CREATION, "character creation");
    tap("a");
    tap("a");
    tap("a");
    tap("a");
    tap("encoder");
    check(gameState == GAME_TOWN, "town");
    tap("cw");
    tap("cw");
    tap("a");
    if (menuState.isOpen) tap("a");
    check(gameState == GAME_DUNGEON && dungeon.runActive, "dungeon");
}

size_t whitePixelsInRow(uint8_t row)
{
    const uint16_t* pixels = controllerPixels(0);
    check(pixels != nullptr, "framebuffer");
    const int top = MENU_Y + MENU_HEADER_HEIGHT + 2 + row * MENU_LINE_HEIGHT + 3;
    size_t count = 0;
    for (int y = top; y < top + 8; ++y)
        for (int x = MENU_TEXT_X; x < MENU_X + MENU_WIDTH - MENU_PADDING; ++x)
            if (pixels[y * 240 + x] == MENU_TEXT) ++count;
    return count;
}

uint32_t descriptionFingerprint()
{
    const uint16_t* pixels = controllerPixels(0);
    const int top = MENU_Y + MENU_HEIGHT - MENU_FOOTER_HEIGHT -
        MENU_DESCRIPTION_HEIGHT;
    uint32_t fingerprint = 2166136261u;
    for (int y = top; y < top + MENU_DESCRIPTION_HEIGHT; ++y)
        for (int x = MENU_X + 1; x < MENU_X + MENU_WIDTH - 1; ++x)
        {
            fingerprint ^= pixels[y * 240 + x];
            fingerprint *= 16777619u;
        }
    return fingerprint;
}

void checkSelectedRow(uint8_t row)
{
    const uint16_t* pixels = controllerPixels(0);
    const int y = MENU_Y + MENU_HEADER_HEIGHT + 2 + row * MENU_LINE_HEIGHT;
    check(pixels[y * 240 + MENU_X + 2] == MENU_HIGHLIGHT,
          "selected row remains highlighted");
}

void checkEveryRowVisible()
{
    for (uint8_t row = 0; row < 3; ++row)
        check(whitePixelsInRow(row) >= 20, "all menu rows remain visible");
}

void capture(const char* name)
{
    std::ofstream output(name, std::ios::binary);
    const uint16_t* pixels = controllerPixels(0);
    for (size_t index = 0; index < 57600; ++index)
    {
        output.put(static_cast<char>(pixels[index]));
        output.put(static_cast<char>(pixels[index] >> 8));
    }
}
}

int main()
{
    try
    {
        initializeControllers(2);
        ControllerState state;
        check(bindController(0, state), "bind controller");
        enterTestDungeon();

        openMenu(&TEST_MENU);
        wait(20);
        checkEveryRowVisible();
        checkSelectedRow(0);
        const uint32_t alphaDescription = descriptionFingerprint();
        capture("menu-initial.rgb565");

        tap("cw");
        check(menuState.cursorIndex == 1, "selection moved to beta");
        checkEveryRowVisible();
        checkSelectedRow(1);
        const uint32_t betaDescription = descriptionFingerprint();
        check(betaDescription != alphaDescription, "description changed to beta");
        capture("menu-down-one.rgb565");

        tap("cw");
        check(menuState.cursorIndex == 2, "selection moved to gamma");
        checkEveryRowVisible();
        checkSelectedRow(2);
        check(descriptionFingerprint() != betaDescription,
              "description changed to gamma");
        capture("menu-down-two.rgb565");

        tap("ccw");
        check(menuState.cursorIndex == 1, "selection moved back to beta");
        checkEveryRowVisible();
        tap("ccw");
        check(menuState.cursorIndex == 0, "selection moved back to alpha");
        checkEveryRowVisible();

        check(pushMenu(&CHILD_MENU), "child opened");
        wait(20);
        tap("cw");
        checkEveryRowVisible();
        menuCancel();
        wait(20);
        check(menuState.depth == 1, "parent restored");
        checkEveryRowVisible();

        closeMenu();
        wait(20);
        openMenu(&TEST_MENU);
        wait(20);
        check(menuState.cursorIndex == 0, "reopen resets selection");
        checkEveryRowVisible();

        std::cout << "Native production menu framebuffer checks passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAILED: " << error.what() << "; " << diagnostic() << '\n';
        return 1;
    }
}
