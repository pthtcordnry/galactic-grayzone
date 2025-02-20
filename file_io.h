#ifndef FILE_IO_H
#define FILE_IO_H

#include <stdbool.h>
#include <stddef.h>

#define MAX_FILE_PATH 256

// Ensures a directory exists (creates if necessary). Returns false on failure.
bool EnsureDirectoryExists(const char *dirPath);

// Lists all files in a directory matching pattern (e.g. "*.ent")
// and stores their paths in fileList. Returns the number of files found.
int ListFilesInDirectory(const char *directory,
                         const char *pattern,
                         char fileList[][256],
                         int maxFiles);

// Counts files recursively in sDir that match extension.
// Example: extension ".txt" or ".ent".
int CountFilesWithExtension(const char *sDir,
                            const char *extension);

// Extracts the directory portion from a filepath into dirBuffer.
static void ExtractDirectoryFromFilename(const char *filepath,
                                         char *dirBuffer,
                                         size_t dirBufferSize)
{
    // Find the last occurrence of '/' or '\\'.
    const char *lastSlash = strrchr(filepath, '/');
    const char *lastBackslash = strrchr(filepath, '\\');

    const char *lastSep = lastSlash;
    if (lastBackslash && (!lastSlash || lastBackslash > lastSlash))
        lastSep = lastBackslash;

    if (lastSep)
    {
        size_t length = (size_t)(lastSep - filepath);
        if (length >= dirBufferSize)
            length = dirBufferSize - 1;
        strncpy(dirBuffer, filepath, length);
        dirBuffer[length] = '\0';
    }
    else
    {
        if (dirBufferSize > 0)
            dirBuffer[0] = '\0';
    }
}

#endif
