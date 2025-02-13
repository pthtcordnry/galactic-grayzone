#include "game_ui.h"

bool DrawButton(char *text, Rectangle rect, Color buttonColor, Color textColor, int textSize = 20)
{
    DrawRectangleRec(rect, buttonColor);
    DrawText(text, rect.x + 10, rect.y + 7, textSize, BLACK);

    bool clicked = IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(GetMousePosition(), rect);

    return clicked;
}