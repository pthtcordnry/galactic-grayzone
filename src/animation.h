#ifndef ANIMATION_H
#define ANIMATION_H

#include <raylib.h>
#include "memory_arena.h"

typedef struct AnimationFrames
{
    Rectangle *frames;
    int frameCount;
    float frameTime;
} AnimationFrames;

typedef struct Animation
{
    AnimationFrames *framesData;
    int currentFrame;
    float timer;
    Texture2D texture;
} Animation;

static void UpdateAnimation(Animation *anim, float delta)
{
    anim->timer += delta;
    if (anim->timer >= anim->framesData->frameTime)
    {
        anim->timer = 0;
        anim->currentFrame = (anim->currentFrame + 1) % anim->framesData->frameCount;
    }
}

static void InitEntityAnimation(Animation *anim, AnimationFrames *frames, Texture2D texture)
{
    anim->framesData = frames;
    anim->texture = texture;
    anim->currentFrame = 0;
    anim->timer = 0;
}

#endif