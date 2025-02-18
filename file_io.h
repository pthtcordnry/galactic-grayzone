#ifndef FILE_IO_H
#define FILE_IO_H

#define MAX_FILE_PATH 256

bool EnsureDirectoryExists(const char *dirPath);
int ListFilesInDirectory(const char *directory, const char *pattern, char fileList[][256], int maxFiles);
int CountFilesWithExtension(const char *sDir, const char *extension);
void FillFilesWithExtensionContiguous(const char *sDir, const char *extension,
                                      char filePaths[][MAX_FILE_PATH],
                                      int *index);

static void ExtractDirectoryFromFilename(const char *filepath, char *dirBuffer, size_t dirBufferSize)
{
    // Find the last occurrence of '/' and '\\'
    const char *lastSlash = strrchr(filepath, '/');
    const char *lastBackslash = strrchr(filepath, '\\');

    // Choose the one that occurs later in the string
    const char *lastSeparator = lastSlash;
    if (lastBackslash && (!lastSlash || lastBackslash > lastSlash))
        lastSeparator = lastBackslash;

    if (lastSeparator)
    {
        size_t length = lastSeparator - filepath;
        if (length >= dirBufferSize)
            length = dirBufferSize - 1;
        strncpy(dirBuffer, filepath, length);
        dirBuffer[length] = '\0';
    }
    else
    {
        // No directory part found.
        if (dirBufferSize > 0)
            dirBuffer[0] = '\0';
    }
}

#endif