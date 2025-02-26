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

void UpdateAnimation(Animation *anim, float delta);

#endif