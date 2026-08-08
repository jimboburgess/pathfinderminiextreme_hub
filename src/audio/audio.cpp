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

//UI


const Note menuMoveSound[] = {
  { 1200, 25 },
  END_SOUND
};

const Note menuSelectSound[] = {
  { 1400, 25 },
  { 1800, 50 },
  END_SOUND
};

const Note menuBackSound[] =
{
    {1800, 25},
    {1300, 35},
    END_SOUND
};

const Note bumpSound[] =
{
    {220, 70},
    END_SOUND
};

const Note errorSound[] =
{
    {250, 80},
    {180, 120},
    END_SOUND
};

//MUSIC
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

const Note dungeonTheme[] =
{
    {49, 300},   // G3
    {0,   40},

    {44, 300},   // F3
    {0,   50},

    {40, 450},   // D3
    {0,  10},

    {40, 450},   // D3
    {0,  10},

    {40, 450},   // D3
    {0,  10},

    {40, 450},   // D3
    {0,  10},

    {49, 300},   // G3
    {0,   40},


    END_SOUND
};

const Note townTheme[] =
{
    {784,120},
    {880,120},
    {988,180},
    {880,120},
    {784,250},
    END_SOUND
};

const Note forestTheme[] =
{
    {523,140},
    {659,140},
    {587,140},
    {784,180},
    {659,220},
    END_SOUND
};

const Note combatTheme[] =
{
    {440,100},
    {523,100},
    {659,100},
    {523,100},
    {784,180},
    END_SOUND
};

const Note bossTheme[] =
{
    {220,180},
    {294,180},
    {196,180},
    {330,250},
    {147,350},
    END_SOUND
};

const Note victoryTheme[] =
{
    {523,100},
    {659,100},
    {784,120},
    {1046,200},
    {1318,350},
    END_SOUND
};


//WORLD
const Note walkSound[] =
{
    {300,15},
    END_SOUND
};

const Note doorOpenSound[] =
{
    {500,40},
    {700,60},
    END_SOUND
};

const Note doorLockedSound[] =
{
    {300,50},
    {250,50},
    {300,50},
    END_SOUND
};

const Note chestOpenSound[] =
{
    {600,40},
    {800,40},
    {1000,80},
    END_SOUND
};

const Note itemPickupSound[] =
{
    {900,30},
    {1200,60},
    END_SOUND
};


//cOMBAT
const Note attackSound[] =
{
    {1800, 20},
    {1200, 45},
    END_SOUND
};

// A short descending pluck for bow shots.
const Note bowFireSound[] =
{
    {2000, 12},
    {1550, 14},
    {1100, 28},
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

const Note blockSound[] =
{
    {2200,20},
    {1700,40},
    END_SOUND
};

const Note dodgeSound[] =
{
    {1500,20},
    {2200,40},
    END_SOUND
};

const Note enemyHitSound[] =
{
    {800,25},
    {550,50},
    END_SOUND
};

const Note playerHitSound[] =
{
    {650,40},
    {450,70},
    END_SOUND
};

const Note playerDieSound[] =
{
    {500,120},
    {400,150},
    {300,200},
    {200,300},
    END_SOUND
};


//MAGIC
const Note potionSound[] =
{
    {700, 40},
    {900, 40},
    {1100, 60},
    {1400, 80},
    END_SOUND
};

const Note spellCastSound[] =
{
    {900,30},
    {1200,30},
    {1500,60},
    END_SOUND
};

const Note spellHitSound[] =
{
    {1800,20},
    {1400,20},
    {900,50},
    END_SOUND
};

const Note spellHealSound[] =
{
    {600,40},
    {800,40},
    {1000,40},
    {1200,80},
    END_SOUND
};

const Note spellFailSound[] =
{
    {500,50},
    {450,50},
    {400,80},
    END_SOUND
};

//monster sounds

const Note goblinAttackSound[] =
{
    {500, 30},
    {350, 60},
    END_SOUND
};

const Note goblinAlertSound[] =
{
    {450,40},
    {550,40},
    {450,60},
    END_SOUND
};

const Note trapSound[] =
{
    {1800,20},
    {900,40},
    {400,80},
    END_SOUND
};

const Note secretFoundSound[] =
{
    {900,40},
    {1100,40},
    {1400,80},
    {1700,120},
    END_SOUND
};

const Note questCompleteSound[] =
{
    {523,80},
    {659,80},
    {784,80},
    {988,120},
    {1318,220},
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

//overtop

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

menuMoveSound,
menuSelectSound,
menuBackSound,
errorSound,
bumpSound,

titleTheme,
townTheme,
dungeonTheme,
forestTheme,
combatTheme,
bossTheme,
victoryTheme,

walkSound,
doorOpenSound,
doorLockedSound,
chestOpenSound,
itemPickupSound,
potionSound,
defendSound,

attackSound,
bowFireSound,
missSound,
critSound,
critFailSound,
blockSound,
dodgeSound,

spellCastSound,
spellHitSound,
spellHealSound,
spellFailSound,

goblinAlertSound,
goblinAttackSound,
enemyHitSound,
enemyDieSound,

playerHitSound,
playerDieSound,
levelUpSound,

trapSound,
secretFoundSound,
questCompleteSound,

victorySound,
gameOverSound
};

static_assert(
    sizeof(soundTable) / sizeof(soundTable[0]) ==
        static_cast<uint8_t>(SoundEffect::COUNT),
    "SoundEffect and soundTable are out of sync.");

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
