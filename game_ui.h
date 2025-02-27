#ifndef GAME_UI_H
#define GAME_UI_H

#include "raylib.h"


// Draw a button using Raylib. Returns true if the button was clicked.
bool DrawButton(char *text, Rectangle rect, Color buttonColor, Color textColor, int textSize);

// Draw a background within a specified rectangle. If bgImage is provided and valid, it is drawn scaled.
void DrawGameBackground(Rectangle rect, Color bgColor, Texture2D *bgImage);

#endif