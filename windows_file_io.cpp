#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "file_io.h"

/**
 * For ListFilesInDirectory:
 *   We store an array of string slots (files), plus a pointer 
 *   to the current fileCount (pCount) and max capacity (maxFiles).
 */
typedef struct ListFilesData
{
    char (*files)[256];
    int   maxFiles;
    int  *pCount;
} ListFilesData;

/**
 * For CountFilesWithExtension:
 *   We store a pointer to the extension string
 *   and a pointer to an integer counter.
 */
typedef struct ExtCountData
{
    const char *extension;
    int        *pCount;
} ExtCountData;

/**
 * BuildWindowsPath:
 *   Safely appends "fileOrPattern" to "directory" using a backslash if needed.
 *   E.g.: BuildWindowsPath("C:\\Data", "File.txt") => "C:\\Data\\File.txt"
 */
static void BuildWindowsPath(char *outBuffer, size_t outSize,
                             const char *directory,
                             const char *fileOrPattern)
{
    if (!outBuffer || outSize == 0) return;

    // If directory is NULL, just copy fileOrPattern if any
    if (!directory)
    {
        snprintf(outBuffer, outSize, "%s", fileOrPattern ? fileOrPattern : "");
        return;
    }

    size_t len = strlen(directory);
    bool endsWithSep = false;
    if (len > 0)
    {
        char c = directory[len - 1];
        endsWithSep = (c == '\\' || c == '/');
    }

    if (endsWithSep)
    {
        // e.g. "C:\Data\" + "File.txt"
        snprintf(outBuffer, outSize, "%s%s",
                 directory, fileOrPattern ? fileOrPattern : "");
    }
    else
    {
        // e.g. "C:\Data" + "\" + "File.txt"
        snprintf(outBuffer, outSize, "%s\\%s",
                 directory, fileOrPattern ? fileOrPattern : "");
    }
}

/**
 * A function pointer type for enumerator callbacks.
 * Called once for each file in the directory.
 */
typedef void (*FileEnumeratorCallback)(const char *fullPath, void *userData);

/**
 *   Enumerates files in 'directory' matching 'pattern' (e.g., "*.ent").
 *   Skips subdirectories. For each file found, calls fileCallback(fullPath, userData).
 */
static void EnumerateDirectoryNoRecursion(const char *directory,
                                          const char *pattern,
                                          FileEnumeratorCallback fileCallback,
                                          void *userData)
{
    if (!directory || !pattern || !fileCallback)
        return;

    // Build e.g. "C:\dir\*.ent"
    char searchPattern[MAX_PATH];
    BuildWindowsPath(searchPattern, sizeof(searchPattern), directory, pattern);

    WIN32_FIND_DATAA findData;
    HANDLE hFind = FindFirstFileA(searchPattern, &findData);
    if (hFind == INVALID_HANDLE_VALUE)
    {
        // Possibly no files or directory doesn't exist
        return;
    }

    do
    {
        // If it's a directory, skip it
        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            continue;

        // Build final path: directory + filename
        char fullPath[MAX_PATH];
        BuildWindowsPath(fullPath, sizeof(fullPath), directory, findData.cFileName);

        // Call the user callback for this file
        fileCallback(fullPath, userData);

    } while (FindNextFileA(hFind, &findData));

    FindClose(hFind);
}

bool EnsureDirectoryExists(const char *dirPath)
{
    if (!CreateDirectoryA(dirPath, NULL))
    {
        DWORD err = GetLastError();
        if (err != ERROR_ALREADY_EXISTS)
            return false;
    }
    return true;
}

static void AddFileToListCallback(const char *fullPath, void *userData)
{
    // Convert the generic userData pointer to a ListFilesData pointer
    ListFilesData *data = (ListFilesData *)userData;
    if (!data || !data->pCount) return;

    int idx = *(data->pCount);
    if (idx >= data->maxFiles) return; // array is full

    const char *baseName = strrchr(fullPath, '/');
    if (!baseName)
        baseName = strrchr(fullPath, '\\');
    if (baseName)
        baseName++;
    else
        baseName = fullPath;

    snprintf(data->files[idx], 256, "%s", baseName);
    idx++;
    *(data->pCount) = idx;
}

/**
 *   Non-recursive. We pass the directory + pattern to EnumerateDirectoryNoRecursion.
 */
int ListFilesInDirectory(const char *directory,
                         const char *extension,
                         char fileList[][256],
                         int maxFiles)
{
    if (!directory || !extension || maxFiles <= 0)
        return 0;

    int fileCount = 0;
    ListFilesData data = { fileList, maxFiles, &fileCount };

    // Build a wildcard pattern like: "C:\someDir\*.txt"
    char wildcard[64];
    snprintf(wildcard, sizeof(wildcard), "*%s", extension);

    EnumerateDirectoryNoRecursion(directory, wildcard, AddFileToListCallback, &data);
    return fileCount;
}

static bool EndsWith(const char *str, const char *suffix)
{
    if (!str || !suffix) return false;
    size_t strLen = strlen(str);
    size_t sufLen = strlen(suffix);
    if (sufLen > strLen) return false;
    return _stricmp(str + strLen - sufLen, suffix) == 0;
}

static void CountExtensionCallback(const char *fullPath, void *userData)
{
    ExtCountData *data = (ExtCountData *)userData;
    if (!data || !data->pCount) return;

    // Extract filename from the path
    const char *fname = strrchr(fullPath, '\\');
    if (!fname) fname = fullPath;
    else        fname++; // skip backslash

    if (!data->extension || data->extension[0] == '\0')
    {
        // If no extension => count everything
        (*data->pCount)++;
    }
    else
    {
        if (EndsWith(fname, data->extension))
            (*data->pCount)++;
    }
}

int CountFilesWithExtension(const char *dir, const char *extension)
{
    if (!dir || !extension || extension[0] == '\0') return 0;

    int count = 0;
    ExtCountData data = { extension, &count };

    // Build a wildcard pattern like: "C:\someDir\*.txt"
    char wildcard[64];
    snprintf(wildcard, sizeof(wildcard), "*%s", extension);

    EnumerateDirectoryNoRecursion(dir, wildcard, CountExtensionCallback, &data);
    return count;
}

