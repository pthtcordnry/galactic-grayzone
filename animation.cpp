#include "animation.h"

void UpdateAnimation(Animation *anim, float delta)
{
    anim->timer += delta;
    if (anim->timer >= anim->framesData->frameTime)
    {
        anim->timer = 0;
        anim->currentFrame = (anim->currentFrame + 1) % anim->framesData->frameCount;
    }
} 