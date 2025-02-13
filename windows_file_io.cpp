#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "file_io.h"
#include "memory_arena.h"

// Helper function: returns true if 'str' ends with the given 'suffix' (case-insensitive)
bool EndsWith(const char *str, const char *suffix)
{
    if (!str || !suffix)
        return false;
    size_t strLen = strlen(str);
    size_t suffixLen = strlen(suffix);
    if (suffixLen > strLen)
        return false;
    // _stricmp does a case-insensitive comparison on Windows.
    return _stricmp(str + strLen - suffixLen, suffix) == 0;
}

int CountFilesWithExtension(const char *sDir, const char *extension)
{
    int count = 0;
    WIN32_FIND_DATA fdFile;
    HANDLE hFind = NULL;
    char sPath[2048];

    // Build the search pattern (all files and directories)
    sprintf(sPath, "%s\\*.*", sDir);
    hFind = FindFirstFile(sPath, &fdFile);
    if (hFind == INVALID_HANDLE_VALUE)
    {
        // Optionally, you could handle errors differently.
        printf("Path not found: [%s]\n", sDir);
        return 0;
    }

    do
    {
        // Skip the "." and ".." entries.
        if (strcmp(fdFile.cFileName, ".") != 0 &&
            strcmp(fdFile.cFileName, "..") != 0)
        {
            char fullPath[2048];
            sprintf(fullPath, "%s\\%s", sDir, fdFile.cFileName);

            // If it's a directory, recurse into it.
            if (fdFile.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            {
                count += CountFilesWithExtension(fullPath, extension);
            }
            else
            {
                // Check if the file matches the extension (if one was specified).
                if (extension && extension[0] != '\0')
                {
                    if (EndsWith(fdFile.cFileName, extension))
                    {
                        count++;
                    }
                }
                else
                {
                    // No extension filtering – count every file.
                    count++;
                }
            }
        }
    } while (FindNextFile(hFind, &fdFile));

    FindClose(hFind);
    return count;
}

// A modified version that fills a contiguous array of fixed–size strings.
void FillFilesWithExtensionContiguous(const char *sDir, const char *extension,
                                             char filePaths[][MAX_FILE_PATH],
                                             int *index)
{
    WIN32_FIND_DATA fdFile;
    HANDLE hFind = NULL;
    char sPath[2048];

    sprintf(sPath, "%s\\*.*", sDir);
    hFind = FindFirstFile(sPath, &fdFile);
    if (hFind == INVALID_HANDLE_VALUE)
    {
        printf("Path not found: [%s]\n", sDir);
        return;
    }

    do
    {
        if (strcmp(fdFile.cFileName, ".") != 0 &&
            strcmp(fdFile.cFileName, "..") != 0)
        {
            char fullPath[2048];
            sprintf(fullPath, "%s\\%s", sDir, fdFile.cFileName);

            if (extension && extension[0] != '\0')
            {
                if (EndsWith(fdFile.cFileName, extension))
                {
                    // Copy the file path into our contiguous array.
                    strncpy(filePaths[*index], fullPath, MAX_FILE_PATH - 1);
                    filePaths[*index][MAX_FILE_PATH - 1] = '\0'; // Ensure null–termination
                    (*index)++;
                }
            }
            else
            {
                strncpy(filePaths[*index], fullPath, MAX_FILE_PATH - 1);
                filePaths[*index][MAX_FILE_PATH - 1] = '\0';
                (*index)++;
            }        
        }
    } 
    while (FindNextFile(hFind, &fdFile));
    FindClose(hFind);
}
