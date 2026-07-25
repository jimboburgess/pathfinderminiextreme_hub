//
// Created by james on 7/12/2026.
//

#include <Arduino.h>
#include "audio/audio.h"
#include "config.h"

static const Note* currentSound = nullptr;
static uint8_t currentNote = 0;
static unsigned long noteStartTime = 0;
static bool playing = false;

bool isSoundPlaying() {
  return playing;
}

constexpr Note END_SOUND = { 0, 0 };

const Note menuMoveSound[] = {
  { 1200, 25 },
  END_SOUND
};

const Note menuSelectSound[] = {
  { 1400, 25 },
  { 1800, 50 },
  END_SOUND
};

const Note bumpSound[] = {
    {220, 70},
    END_SOUND
};

//HOMEWARD BOUND AKA MONTY PYTHON HOLY GRAIL THEME
const Note titleTheme[] = {
  { 0, 500 },    // REST
  { 523, 375 },  // C5
  { 523, 125 },  // C5
  { 659, 250 },  // E5
  { 698, 500 },  // F5
  { 784, 250 },  // G5
  { 784, 125 },  // G5
  { 0, 125 },    // REST

  { 987, 250 },  // B5
  { 880, 250 },  // A5
  { 880, 125 },  // A5
  { 0, 60 },    // REST
  { 880, 125 },  // A5
  { 698, 125 },  // F5
  { 0, 60 },     // REST
  { 698, 200},  // F5
  { 0, 60 },     // REST
  { 784, 500 },  // G5

  END_SOUND
};

const Note attackSound[] =
{
    {1800, 20},
    {1200, 45},
    END_SOUND
};

const Note missSound[] =
{
    {900, 20},
    {700, 40},
    END_SOUND
};

const Note critSound[] =
{
    {1800, 25},
    {2200, 25},
    {2800, 40},
    {3400, 80},
    END_SOUND
};

const Note critFailSound[] =
{
    {294, 100},
    {220, 200},
    {147, 300},
    {330, 120},
    {220, 150},
    {165, 250},
    {294, 100},
    {220, 200},
    {147, 300},
    END_SOUND
};

const Note defendSound[] =
{
    {2500, 20},
    {1800, 60},
    END_SOUND
};

const Note potionSound[] =
{
    {700, 40},
    {900, 40},
    {1100, 60},
    {1400, 80},
    END_SOUND
};

const Note goblinAttackSound[] =
{
    {500, 30},
    {350, 60},
    END_SOUND
};

const Note enemyDieSound[] =
{
    {1000, 30},
    {700, 40},
    {500, 60},
    {300, 80},
    END_SOUND
};

const Note victorySound[] =
{
    {523, 120},
    {659, 120},
    {784, 180},
    {1046, 350},
    END_SOUND
};

const Note gameOverSound[] =
{
    {784, 150},
    {698, 150},
    {587, 200},
    {523, 400},
    END_SOUND
};

const Note levelUpSound[] =
{
    {523, 80},
    {659, 80},
    {784, 80},
    {1046, 200},
    {1318, 300},
    END_SOUND
};

static const Note* soundTable[] =
{
    nullptr,            // NONE

    menuMoveSound,      // MENU_MOVE
    menuSelectSound,    // MENU_SELECT
    bumpSound,          // BUMP

    titleTheme,         // TITLE_THEME

    attackSound,        // ATTACK
    missSound,          // MISS
    critSound,          // CRIT
    critFailSound,      // CRIT_FAIL
    defendSound,        // DEFEND
    potionSound,        // POTION

    goblinAttackSound,  // GOBLIN_ATTACK
    enemyDieSound,      // ENEMY_DIE

    victorySound,       // VICTORY
    levelUpSound,       // LEVEL_UP
    gameOverSound       // GAME_OVER
};

void initAudio() {
  pinMode(PIEZO_PIN, OUTPUT);
}

// Advances the currently playing sound.
// Call once every frame from loop().

void updateAudio() {
  if (!playing || currentSound == nullptr)
    return;

  unsigned long now = millis();
  const Note& note = currentSound[currentNote];

  // End of sound
  if (note.frequency == 0 && note.duration == 0) {
    noTone(PIEZO_PIN);
    playing = false;
    currentSound = nullptr;
    return;
  }

  // Start this note
  if (noteStartTime == 0) {
    if (note.frequency == 0)
      noTone(PIEZO_PIN);  // Rest
    else
      tone(PIEZO_PIN, note.frequency);

    noteStartTime = now;
    return;
  }

  // Time for the next note?
  if (now - noteStartTime >= note.duration) {
    currentNote++;
    noteStartTime = 0;
  }
}



void playSound(SoundEffect sound) {
  currentSound = soundTable[static_cast<uint8_t>(sound)];

  if (currentSound == nullptr)
    return;

  currentNote = 0;
  noteStartTime = 0;
  playing = true;
}