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

    // UI
    MENU_MOVE,
    MENU_SELECT,
    MENU_BACK,
    ERROR,
    BUMP,

    // Music
    TITLE_THEME,
    TOWN_THEME,
    DUNGEON_THEME,
    FOREST_THEME,
    COMBAT_THEME,
    BOSS_THEME,
    VICTORY_THEME,

    // Player Actions
    WALK,
    DOOR_OPEN,
    DOOR_LOCKED,
    CHEST_OPEN,
    ITEM_PICKUP,
    POTION,
    DEFEND,

    // Combat
    ATTACK,
    BOW_FIRE,
    MISS,
    CRIT,
    CRIT_FAIL,
    BLOCK,
    DODGE,

    // Magic
    SPELL_CAST,
    SPELL_HIT,
    SPELL_HEAL,
    SPELL_FAIL,

    // Monsters
    GOBLIN_ALERT,
    GOBLIN_ATTACK,
    ENEMY_HIT,
    ENEMY_DIE,

    // Character
    PLAYER_HIT,
    PLAYER_DIE,
    LEVEL_UP,

    // World
    TRAP,
    SECRET_FOUND,
    QUEST_COMPLETE,

    // End Game
    VICTORY,
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

// TODO:
// Separate music and sound effects into independent playback
// channels when the audio system grows.

void playSound(SoundEffect sound);
bool isSoundPlaying();

#endif //PATHFINDERMINIEXTREME_025_AUDIO_H
