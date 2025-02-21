#ifndef MAIN_H
#define MAIN_H

#include <raylib.h>
#include <rlImGui.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "file_io.h"
#include "game_state.h"
#include "memory_arena.h"
#include "editor_mode.h"

#define MAX_PLAYER_BULLETS 20
#define MAX_ENEMY_BULLETS  20
#define MAX_BULLETS        (MAX_PLAYER_BULLETS + MAX_ENEMY_BULLETS)
#define BOSS_MAX_HEALTH    100
#define MAX_PARTICLES      200

// External toggles and data from main.cpp
extern bool     editorMode;
extern Camera2D camera;

#endif
