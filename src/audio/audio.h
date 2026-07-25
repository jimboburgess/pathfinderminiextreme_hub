//
// Created by james on 7/12/2026.
//

#ifndef PATHFINDERMINIEXTREME_025_AUDIO_H
#define PATHFINDERMINIEXTREME_025_AUDIO_H

#include <Arduino.h>

//playSound(SoundEffect::SOUND NAME);

enum class SoundEffect : uint8_t
{
    NONE,

    MENU_MOVE,
    MENU_SELECT,
    BUMP,

  // Music
    TITLE_THEME,

    ATTACK,
    MISS,
    CRIT,
    CRIT_FAIL,
    DEFEND,
    POTION,

    GOBLIN_ATTACK,
    ENEMY_DIE,

    VICTORY,
    LEVEL_UP,
    GAME_OVER,


    COUNT
};


struct Note
{
    uint16_t frequency;
    uint16_t duration;
};

void initAudio();
void updateAudio();

void playSound(SoundEffect sound);
bool isSoundPlaying();

#endif //PATHFINDERMINIEXTREME_025_AUDIO_H
