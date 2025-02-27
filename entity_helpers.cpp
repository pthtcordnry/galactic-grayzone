#include "entity.h"
#include "game_state.h"
#include "game_storage.h"

void InitEntityAnimation(Animation *anim, AnimationFrames *frames, Texture2D texture)
{
    anim->framesData = frames;
    anim->texture = texture;
    anim->currentFrame = 0;
    anim->timer = 0;
}

const char *GetEntityKindString(EntityKind kind)
{
    switch (kind)
    {
    case ENTITY_PLAYER:
        return "Player";
    case ENTITY_ENEMY:
        return "Enemy";
    case ENTITY_BOSS:
        return "Boss";
    default:
        return "Unknown";
    }
}

EntityAsset *GetEntityAssetById(uint64_t id)
{
    for (int i = 0; i < entityAssetCount; i++)
    {
        if (entityAssets[i].id == id)
            return &entityAssets[i];
    }

    return NULL;
}

// Helper function: Write an animation's frame data into the given buffer.
// This function appends to the provided buffer pointer.
static void AppendAnimationFrames(char **buffer, size_t *bufSize, const char *animName, const AnimationFrames *anim)
{
    // We'll use a temporary buffer to format each animation block.
    char temp[1024];
    int len = snprintf(temp, sizeof(temp),
                       "    \"%s\": {\n"
                       "      \"frameCount\": %d,\n"
                       "      \"frameTime\": %.2f,\n"
                       "      \"frames\": [\n",
                       animName, anim->frameCount, anim->frameTime);
    strncat(*buffer, temp, *bufSize - strlen(*buffer) - 1);

    for (int i = 0; i < anim->frameCount; i++)
    {
        Rectangle r = anim->frames[i];
        len = snprintf(temp, sizeof(temp),
                       "        {\"x\": %.2f, \"y\": %.2f, \"width\": %.2f, \"height\": %.2f}%s\n",
                       r.x, r.y, r.width, r.height,
                       (i < anim->frameCount - 1) ? "," : "");
        strncat(*buffer, temp, *bufSize - strlen(*buffer) - 1);
    }
    snprintf(temp, sizeof(temp),
             "      ]\n"
             "    }");
    strncat(*buffer, temp, *bufSize - strlen(*buffer) - 1);
}

static int ParseAnimation(const char *json, const char *animName, AnimationFrames *anim)
{
    // Find the animation block by name.
    // It should appear as: "idle": { ... }
    const char *pos = strstr(json, animName);
    if (!pos)
    {
        TraceLog(LOG_WARNING, "Animation '%s' not found in JSON.", animName);
        return 0;
    }

    // Find and parse "frameCount".
    const char *fcPos = strstr(pos, "\"frameCount\"");
    if (!fcPos)
    {
        TraceLog(LOG_WARNING, "frameCount key not found for animation '%s'.", animName);
        return 0;
    }
    int frameCount;
    if (sscanf(fcPos, " \"frameCount\" : %d", &frameCount) != 1)
    {
        TraceLog(LOG_WARNING, "Failed to parse frameCount for animation '%s'.", animName);
        return 0;
    }

    // Find and parse "frameTime".
    const char *ftPos = strstr(pos, "\"frameTime\"");
    if (!ftPos)
    {
        TraceLog(LOG_WARNING, "frameTime key not found for animation '%s'.", animName);
        return 0;
    }
    float frameTime;
    if (sscanf(ftPos, " \"frameTime\" : %f", &frameTime) != 1)
    {
        TraceLog(LOG_WARNING, "Failed to parse frameTime for animation '%s'.", animName);
        return 0;
    }

    anim->frameCount = frameCount;
    anim->frameTime = frameTime;

    // Find the start of the frames array.
    const char *framesPos = strstr(pos, "\"frames\"");
    if (!framesPos)
    {
        TraceLog(LOG_WARNING, "Frames array not found for animation '%s'.", animName);
        return 0;
    }
    framesPos = strchr(framesPos, '[');
    if (!framesPos)
        return 0;
    framesPos++; // Skip '['

    // Allocate the frames array.
    anim->frames = (Rectangle *)malloc(sizeof(Rectangle) * frameCount);
    if (!anim->frames)
    {
        TraceLog(LOG_ERROR, "Memory allocation failed for animation '%s'.", animName);
        return 0;
    }

    // Parse each frame. Expected format:
    // {"x": 9.00, "y": 102.00, "width": 30.00, "height": 30.00}
    for (int i = 0; i < frameCount; i++)
    {
        float x, y, w, h;
        // Skip whitespace.
        while (*framesPos == ' ' || *framesPos == '\n' || *framesPos == '\r')
            framesPos++;
        if (*framesPos != '{')
        {
            TraceLog(LOG_WARNING, "Expected '{' at frame %d for animation '%s'.", i, animName);
            free(anim->frames);
            anim->frames = NULL;
            return 0;
        }
        // Parse the frame.
        if (sscanf(framesPos, " { \"x\" : %f , \"y\" : %f , \"width\" : %f , \"height\" : %f }",
                   &x, &y, &w, &h) != 4)
        {
            TraceLog(LOG_WARNING, "Failed to parse frame %d for animation '%s'.", i, animName);
            free(anim->frames);
            anim->frames = NULL;
            return 0;
        }
        anim->frames[i].x = x;
        anim->frames[i].y = y;
        anim->frames[i].width = w;
        anim->frames[i].height = h;
        // Move to the next frame (skip past '}').
        framesPos = strchr(framesPos, '}');
        if (!framesPos)
            break;
        framesPos++; // Skip '}'
        // Skip commas and whitespace.
        while (*framesPos == ',' || *framesPos == ' ' || *framesPos == '\n' || *framesPos == '\r')
            framesPos++;
    }
    return 1;
}

// Converts an EntityAsset to a JSON-formatted string in memory.
// The returned char* must be freed by the caller.
char *EntityAssetToJSON(const EntityAsset *asset)
{
    // Allocate a buffer large enough for the JSON string.
    size_t bufSize = 8192;
    char *json = (char *)malloc(bufSize);
    if (!json)
        return NULL;
    json[0] = '\0'; // Start with an empty string

    char temp[1024];
    // Write the basic fields.
    snprintf(temp, sizeof(temp),
             "{\n"
             "  \"id\": %llu,\n"
             "  \"name\": \"%s\",\n"
             "  \"kind\": %d,\n"
             "  \"physicsType\": %d,\n"
             "  \"baseRadius\": %.2f,\n"
             "  \"baseHp\": %d,\n"
             "  \"baseSpeed\": %.2f,\n"
             "  \"baseAttackSpeed\": %.2f,\n"
             "  \"texturePath\": \"%s\",\n",
             asset->id,
             asset->name,
             (int)asset->kind,
             (int)asset->physicsType,
             asset->baseRadius,
             asset->baseHp,
             asset->baseSpeed,
             asset->baseAttackSpeed,
             asset->texturePath);
    strncat(json, temp, bufSize - strlen(json) - 1);

    // Write the animations block.
    strncat(json, "  \"animations\": {\n", bufSize - strlen(json) - 1);
    AppendAnimationFrames(&json, &bufSize, "idle", &asset->idle);
    strncat(json, ",\n", bufSize - strlen(json) - 1);
    AppendAnimationFrames(&json, &bufSize, "walk", &asset->walk);
    strncat(json, ",\n", bufSize - strlen(json) - 1);
    AppendAnimationFrames(&json, &bufSize, "ascend", &asset->ascend);
    strncat(json, ",\n", bufSize - strlen(json) - 1);
    AppendAnimationFrames(&json, &bufSize, "fall", &asset->fall);
    strncat(json, ",\n", bufSize - strlen(json) - 1);
    AppendAnimationFrames(&json, &bufSize, "shoot", &asset->shoot);
    strncat(json, ",\n", bufSize - strlen(json) - 1);
    AppendAnimationFrames(&json, &bufSize, "die", &asset->die);
    strncat(json, "\n  }\n", bufSize - strlen(json) - 1);
    strncat(json, "}\n", bufSize - strlen(json) - 1);

    return json;
}

// Modified top-level function to parse a JSON string into an EntityAsset.
// Returns 1 on success, 0 on failure.
bool EntityAssetFromJSON(const char *json, EntityAsset *asset)
{
    if (!json || !asset)
        return false;

    int ret = sscanf(json,
                     " {"
                     " \"id\": %llu ,"
                     " \"name\": \"%63[^\"]\" ,"
                     " \"kind\": %d ,"
                     " \"physicsType\": %d ,"
                     " \"baseRadius\": %f ,"
                     " \"baseHp\": %d ,"
                     " \"baseSpeed\": %f ,"
                     " \"baseAttackSpeed\": %f ,"
                     " \"texturePath\": \"%127[^\"]\" ,",
                     &asset->id,
                     asset->name,
                     (int *)&asset->kind,
                     (int *)&asset->physicsType,
                     &asset->baseRadius,
                     &asset->baseHp,
                     &asset->baseSpeed,
                     &asset->baseAttackSpeed,
                     asset->texturePath);
    if (ret < 9)
        return false;

    // If a texture path was specified, use the texture cache.
    if (strlen(asset->texturePath) > 0)
    {
        Texture2D cached = GetCachedTexture(asset->texturePath);
        if (cached.id != 0)
        {
            asset->texture = cached;
        }
        else
        {
            asset->texture = LoadTexture(asset->texturePath);
            if (asset->texture.id != 0)
            {
                AddTextureToCache(asset->texturePath, asset->texture);
            }
            else
            {
                TraceLog(LOG_WARNING, "Failed to load texture for asset %s from path %s", asset->name, asset->texturePath);
            }
        }
    }
    else
    {
        // No texture path provided: set an empty texture.
        asset->texture = (Texture2D){0};
    }

    // Parse each animation block.
    ParseAnimation(json, "idle", &asset->idle);
    ParseAnimation(json, "walk", &asset->walk);
    ParseAnimation(json, "ascend", &asset->ascend);
    ParseAnimation(json, "fall", &asset->fall);
    ParseAnimation(json, "shoot", &asset->shoot);
    ParseAnimation(json, "die", &asset->die);

    return true;
}