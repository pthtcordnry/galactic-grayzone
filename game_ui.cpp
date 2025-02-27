#include "game_ui.h"
#include "raylib.h"
#include "game_state.h"  // For MAX_PATH_NAME if needed
#include <string.h>

// Provided DrawButton function
bool DrawButton(char *text, Rectangle rect, Color buttonColor, Color textColor, int textSize = 20)
{
    DrawRectangleRec(rect, buttonColor);
    DrawText(text, rect.x + 10, rect.y + 7, textSize, BLACK);

    bool clicked = IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(GetMousePosition(), rect);

    return clicked;
}

// Draw a background in a given rectangle, optionally drawing an image if provided.
void DrawGameBackground(Rectangle rect, Color bgColor, Texture2D *bgImage)
{
    DrawRectangleRec(rect, bgColor);
    if (bgImage && bgImage->id > 0)
    {
        // Draw the image scaled to the rectangle.
        DrawTexturePro(*bgImage, (Rectangle){0, 0, (float)bgImage->width, (float)bgImage->height},
                       rect, (Vector2){0, 0}, 0.0f, WHITE);
    }
}

