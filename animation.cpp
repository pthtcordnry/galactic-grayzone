#include "animation.h"

void LoadAnimationFrames(MemoryArena *arena, AnimationFrames *anim, Texture2D spriteSheet, int rows, int cols, float frameTime)
{
    anim->frameCount = rows * cols;
    anim->frameTime = frameTime;
    anim->frames = (Rectangle *)arena_alloc(arena, sizeof(Rectangle) * anim->frameCount);

    int frameWidth = spriteSheet.width / cols;
    int frameHeight = spriteSheet.height / rows;

    for (int y = 0; y < rows; y++)
    {
        for (int x = 0; x < cols; x++)
        {
            int index = y * cols + x;
            anim->frames[index] = (Rectangle){x * frameWidth, y * frameHeight, frameWidth, frameHeight};
        }
    }
}

void UpdateAnimation(Animation *anim, float delta)
{
    anim->timer += delta;
    if (anim->timer >= anim->framesData->frameTime)
    {
        anim->timer = 0;
        anim->currentFrame = (anim->currentFrame + 1) % anim->framesData->frameCount;
    }
} 