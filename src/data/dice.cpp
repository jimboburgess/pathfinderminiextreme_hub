//
// Created by james on 7/13/2026.
//
#include "dice.h"

int rollDie(int sides)
{
    return random(1, sides + 1);
}

int rollDice(int number, int sides)
{
    int total = 0;

    for (int i = 0; i < number; i++)
    {
        total += rollDie(sides);
    }

    return total;
}

int rollAbilityScore()
{
    int rolls[4];

    for (int i = 0; i < 4; i++)
    {
        rolls[i] = rollDie(6);
    }

    int lowest = 0;

    for (int i = 1; i < 4; i++)
    {
        if (rolls[i] < rolls[lowest])
        {
            lowest = i;
        }
    }

    int total = 0;

    for (int i = 0; i < 4; i++)
    {
        if (i != lowest)
        {
            total += rolls[i];
        }
    }

    return total;
}