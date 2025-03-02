#include "game_ui.h"
#include "raylib.h"
#include "game_state.h"
#include <string.h>

// DrawButton: renders a button and returns true if it was clicked.
bool DrawButton(const char *text, Rectangle rect, Color buttonColor, Color textColor, int textSize /* = 20 */)
{
    DrawRectangleRec(rect, buttonColor);
    DrawText(text, rect.x + 10, rect.y + 7, textSize, textColor);
    return IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(GetMousePosition(), rect);
}

void DrawFilledBar(Vector2 pos, int w, int h, float percentageFilled, Color bgColor, Color fgColor)
{
    DrawRectangle(pos.x, pos.y, w, h, bgColor);
    int filledWidth = (int)(percentageFilled * w);
    DrawRectangle(pos.x, pos.y, filledWidth, h, fgColor);
    DrawRectangleLines(pos.x, pos.y, w, h, BLACK);
}