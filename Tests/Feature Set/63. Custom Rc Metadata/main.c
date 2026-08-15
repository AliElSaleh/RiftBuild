#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#endif

int main(void)
{
    int Result = 1;

#ifdef _WIN32
    // The metadata keys produce this one. Before the fix, custom.rc suppressed it entirely.
    HRSRC Version = FindResourceA(NULL, MAKEINTRESOURCEA(1), (LPCSTR)RT_VERSION);

    // And custom.rc still has to reach the exe alongside it.
    HRSRC Icon = FindResourceA(NULL, "MY_ICON", (LPCSTR)RT_GROUP_ICON);

    if (Version == NULL)
    {
        printf("FAIL: no version resource - the custom .rc suppressed the metadata\n");
    }
    else if (Icon == NULL)
    {
        printf("FAIL: no icon named MY_ICON - the custom .rc did not reach the exe\n");
    }
    else
    {
        printf("OK custom rc metadata: version resource and MY_ICON both present\n");
        Result = 0;
    }
#else
    printf("OK custom rc metadata: skipped (version resources are windows-only)\n");
    Result = 0;
#endif

    return Result;
}
