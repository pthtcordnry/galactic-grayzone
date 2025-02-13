#ifndef MAIN_H
#define MAIN_H
#include "raylib.h"
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "memory_arena.h"
#include "file_io.h"
#include "editor_mode.h"
#include "game_state.h"
#include "game_ui.h"

#define MAX_PLAYER_BULLETS 20
#define MAX_ENEMY_BULLETS 20
#define MAX_BULLETS (MAX_PLAYER_BULLETS + MAX_ENEMY_BULLETS)

#define BOSS_MAX_HEALTH 100
#define MAX_PARTICLES 200

extern bool editorMode;
extern Camera2D camera;
extern int mapTiles[MAP_ROWS][MAP_COLS];

#endif