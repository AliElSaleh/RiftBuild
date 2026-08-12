#include "Bundle.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__APPLE__)
    #include <stdint.h>
    #include <limits.h>
    #include <mach-o/dyld.h> /* _NSGetExecutablePath */
#elif defined(_WIN32)
    #include <windows.h>
#else
    #include <limits.h>
    #include <unistd.h>
#endif

/* The only platform-specific code in the program: ask the OS where this
   executable is. Everything below works off the answer. */
static int GetExecutablePath(char* Out, size_t OutSize)
{
    int bSuccess = 0;

#if defined(__APPLE__)
    char Raw[BUNDLE_PATH_MAX];
    uint32_t RawSize = (uint32_t)sizeof(Raw);
    if (_NSGetExecutablePath(Raw, &RawSize) == 0)
    {
        /* _NSGetExecutablePath can hand back a path with symlinks and ".."
           still in it, which would break the Contents/MacOS test below. */
        char Resolved[PATH_MAX];
        const char* Final = realpath(Raw, Resolved) != NULL ? Resolved : Raw;

        if (strlen(Final) < OutSize)
        {
            strcpy(Out, Final);
            bSuccess = 1;
        }
    }
#elif defined(_WIN32)
    DWORD Length = GetModuleFileNameA(NULL, Out, (DWORD)OutSize);
    bSuccess = Length > 0 && Length < OutSize;
#else
    ssize_t Length = readlink("/proc/self/exe", Out, OutSize - 1);
    if (Length > 0)
    {
        Out[Length] = '\0';
        bSuccess = 1;
    }
#endif

    return bSuccess;
}

/* Cuts the last path component off Path ("/a/b/c" -> "/a/b"). */
static void TrimLastComponent(char* Path)
{
    char* Cut = NULL;
    char* c = Path;

    while (*c)
    {
        if (*c == '/' || *c == '\\')
        {
            Cut = c;
        }

        c++;
    }

    if (Cut != NULL && Cut != Path)
    {
        *Cut = '\0';
    }
}

static int EndsWith(const char* Text, const char* Suffix)
{
    size_t TextLength = strlen(Text);
    size_t SuffixLength = strlen(Suffix);

    return TextLength >= SuffixLength &&
           strcmp(Text + TextLength - SuffixLength, Suffix) == 0;
}

int Bundle_FindResourceDirectory(char* Out, size_t OutSize)
{
    int bSuccess = 0;

    char ExecutablePath[BUNDLE_PATH_MAX];
    if (GetExecutablePath(ExecutablePath, sizeof(ExecutablePath)))
    {
        TrimLastComponent(ExecutablePath); /* .../Contents/MacOS, or .../Build */

        if (EndsWith(ExecutablePath, "/Contents/MacOS"))
        {
            /* Bundled: Contents/MacOS/../Resources. */
            TrimLastComponent(ExecutablePath);
            bSuccess = snprintf(Out, OutSize, "%s/Resources", ExecutablePath) < (int)OutSize;
        }
        else
        {
            /* Bare executable in Build/: the assets are still one level up,
               next to the build file. */
            TrimLastComponent(ExecutablePath);
            bSuccess = snprintf(Out, OutSize, "%s/assets", ExecutablePath) < (int)OutSize;
        }
    }

    return bSuccess;
}

char* Bundle_ReadTextFile(const char* Directory, const char* FileName)
{
    char* Text = NULL;

    char Path[BUNDLE_PATH_MAX];
    if (snprintf(Path, sizeof(Path), "%s/%s", Directory, FileName) < (int)sizeof(Path))
    {
        FILE* File = fopen(Path, "rb");
        if (File != NULL)
        {
            long Size = -1;
            if (fseek(File, 0, SEEK_END) == 0)
            {
                Size = ftell(File);
                rewind(File);
            }

            if (Size >= 0)
            {
                Text = malloc((size_t)Size + 1);
                if (Text != NULL)
                {
                    size_t Read = fread(Text, 1, (size_t)Size, File);
                    Text[Read] = '\0';
                }
            }

            fclose(File);
        }
    }

    return Text;
}
