//
// Created by james on 7/12/2026.
//

#ifndef PATHFINDERMINIEXTREME_025_SPRITES_H
#define PATHFINDERMINIEXTREME_025_SPRITES_H

#include <Arduino.h>

//======================================
// 16x16 Sprites
//======================================

const int SPRITE_W = 16;
const int SPRITE_H = 16;

const int LRGSPRITE_W = 32;
const int LRGSPRITE_H = 32;

//======================================
// 64x64 Walking Sprites
//======================================

const int START_W = 64;
const int START_H = 64;

extern const uint16_t fighter16x16[SPRITE_W * SPRITE_H];
extern const uint16_t rogue16x16[SPRITE_W * SPRITE_H];
extern const uint16_t wizard16x16[SPRITE_W * SPRITE_H];
extern const uint16_t cleric16x16[SPRITE_W * SPRITE_H];

extern const uint16_t fighterFrame1[SPRITE_W * SPRITE_H];
extern const uint16_t fighterFrame2[SPRITE_W * SPRITE_H];

extern const uint16_t FIGHTERWALK1[START_W * START_H];
extern const uint16_t FIGHTERWALK2[START_W * START_H];

extern const uint16_t ROGUE64X64[START_W * START_H];
extern const uint16_t CLERIC64X64[START_W * START_H];
extern const uint16_t WIZARD64X64[START_W * START_H];


extern const uint16_t goblinFrame1[SPRITE_W * SPRITE_H];
extern const uint16_t goblinFrame2[SPRITE_W * SPRITE_H];

extern const uint16_t goblinArcherFrame1[SPRITE_W * SPRITE_H];
extern const uint16_t goblinArcherFrame2[SPRITE_W * SPRITE_H];

extern const uint16_t goblinChiefFrame1[SPRITE_W * SPRITE_H];
extern const uint16_t goblinChiefFrame2[SPRITE_W * SPRITE_H];

#endif //PATHFINDERMINIEXTREME_025_SPRITES_H
