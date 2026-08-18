//
// Created by james on 7/20/2026.
//
#ifndef PATHFINDERMINIEXTREME_025_FOREST_H
#define PATHFINDERMINIEXTREME_025_FOREST_H

#include "graphics/tiles.h"
#include <stdint.h>

#include "dungeon/dungeon.h"
#include "../data/entities.h"

constexpr uint8_t FOREST_WIDTH  = 15;
constexpr uint8_t FOREST_HEIGHT = 14;

TileType getForestTile(int x, int y);


void enterForest();

extern Entity forestEntities[MAX_ENTITIES];
extern uint8_t forestEntityCount;


#endif
