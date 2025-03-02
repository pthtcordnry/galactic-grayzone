#ifndef GAME_UI_H
#define GAME_UI_H

#include "raylib.h"

// Draw a button using Raylib. Returns true if the button was clicked.
bool DrawButton(const char *text, Rectangle rect, Color buttonColor, Color textColor, int textSize);
void DrawFilledBar(Vector2 pos, int w, int h, float percentageFilled, Color bgColor, Color fgColor);
#endif