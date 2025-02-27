#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "file_io.h"

// Data structure for storing file list information.
typedef struct ListFilesData {
    char (*files)[256];
    int maxFiles;
    int *pCount;
} ListFilesData;

// Data structure for counting files with a given extension.
typedef struct ExtCountData {
    const char *extension;
    int *pCount;
} ExtCountData;

// Safely builds a Windows path by appending fileOrPattern to directory.
static void BuildWindowsPath(char *outBuffer, size_t outSize,
                             const char *directory, const char *fileOrPattern) {
    if (!outBuffer || outSize == 0)
        return;

    if (!directory) {
        snprintf(outBuffer, outSize, "%s", fileOrPattern ? fileOrPattern : "");
        return;
    }

    size_t len = strlen(directory);
    bool endsWithSep = (len > 0) && (directory[len - 1] == '\\' || directory[len - 1] == '/');

    if (endsWithSep)
        snprintf(outBuffer, outSize, "%s%s", directory, fileOrPattern ? fileOrPattern : "");
    else
        snprintf(outBuffer, outSize, "%s\\%s", directory, fileOrPattern ? fileOrPattern : "");
}

// File enumerator callback function pointer.
typedef void (*FileEnumeratorCallback)(const char *fullPath, void *userData);

// Enumerates files (non-recursively) matching pattern in the given directory.
static void EnumerateDirectoryNoRecursion(const char *directory, const char *pattern,
                                          FileEnumeratorCallback fileCallback, void *userData) {
    if (!directory || !pattern || !fileCallback)
        return;

    char searchPattern[MAX_PATH];
    BuildWindowsPath(searchPattern, sizeof(searchPattern), directory, pattern);

    WIN32_FIND_DATAA findData;
    HANDLE hFind = FindFirstFileA(searchPattern, &findData);
    if (hFind == INVALID_HANDLE_VALUE)
        return;

    do {
        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            continue;

        char fullPath[MAX_PATH];
        BuildWindowsPath(fullPath, sizeof(fullPath), directory, findData.cFileName);
        fileCallback(fullPath, userData);
    } while (FindNextFileA(hFind, &findData));
    FindClose(hFind);
}

bool EnsureDirectoryExists(const char *dirPath) {
    if (!CreateDirectoryA(dirPath, NULL)) {
        DWORD err = GetLastError();
        if (err != ERROR_ALREADY_EXISTS)
            return false;
    }
    return true;
}

static void AddFileToListCallback(const char *fullPath, void *userData) {
    ListFilesData *data = (ListFilesData *)userData;
    if (!data || !data->pCount)
        return;
    
    int idx = *(data->pCount);
    if (idx >= data->maxFiles)
        return; // Array full

    const char *baseName = strrchr(fullPath, '/');
    if (!baseName)
        baseName = strrchr(fullPath, '\\');
    baseName = baseName ? baseName + 1 : fullPath;

    snprintf(data->files[idx], 256, "%s", baseName);
    idx++;
    *(data->pCount) = idx;
}

int ListFilesInDirectory(const char *directory, const char *extension,
                         char fileList[][256], int maxFiles) {
    if (!directory || !extension || maxFiles <= 0)
        return 0;
    
    int fileCount = 0;
    ListFilesData data = { fileList, maxFiles, &fileCount };
    
    char wildcard[64];
    snprintf(wildcard, sizeof(wildcard), "*%s", extension);
    
    EnumerateDirectoryNoRecursion(directory, wildcard, AddFileToListCallback, &data);
    return fileCount;
}

static bool EndsWith(const char *str, const char *suffix) {
    if (!str || !suffix)
        return false;
    size_t strLen = strlen(str);
    size_t sufLen = strlen(suffix);
    if (sufLen > strLen)
        return false;
    return _stricmp(str + strLen - sufLen, suffix) == 0;
}

static void CountExtensionCallback(const char *fullPath, void *userData) {
    ExtCountData *data = (ExtCountData *)userData;
    if (!data || !data->pCount)
        return;

    const char *fname = strrchr(fullPath, '\\');
    fname = fname ? fname + 1 : fullPath;

    if (!data->extension || data->extension[0] == '\0')
        (*data->pCount)++;
    else if (EndsWith(fname, data->extension))
        (*data->pCount)++;
}

int CountFilesWithExtension(const char *dir, const char *extension) {
    if (!dir || !extension || extension[0] == '\0')
        return 0;
    
    int count = 0;
    ExtCountData data = { extension, &count };
    
    char wildcard[64];
    snprintf(wildcard, sizeof(wildcard), "*%s", extension);
    
    EnumerateDirectoryNoRecursion(dir, wildcard, CountExtensionCallback, &data);
    return count;
}
