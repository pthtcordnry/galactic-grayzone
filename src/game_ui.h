#ifndef GAME_UI_H
#define GAME_UI_H

#include "raylib.h"

// Draw a button using Raylib. Returns true if the button was clicked.
bool DrawButton(char *text, Rectangle rect, Color buttonColor, Color textColor, int textSize);

#endif