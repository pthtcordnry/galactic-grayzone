#ifndef FILE_IO_H
#define FILE_IO_H

#include "memory_arena.h"

#define MAX_FILE_PATH 256

int CountFilesWithExtension(const char *sDir, const char *extension);
void FillFilesWithExtensionContiguous(const char *sDir, const char *extension,
    char filePaths[][MAX_FILE_PATH],
    int *index);
#endif