#ifndef FILE_IO_H
#define FILE_IO_H

#include <stdbool.h>
#include <stddef.h>

#define MAX_FILE_PATH 256

// Ensure that a directory exists (create if necessary).
bool EnsureDirectoryExists(const char *dirPath);

// List files in a directory matching a pattern (e.g., "*.ent").
// Store their paths in fileList and return the number of files found.
int ListFilesInDirectory(const char *directory, const char *pattern, char fileList[][MAX_FILE_PATH], int maxFiles);

// Count files recursively in sDir that match the specified extension.
int CountFilesWithExtension(const char *sDir, const char *extension);

#endif
