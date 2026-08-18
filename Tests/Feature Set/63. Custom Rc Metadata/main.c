#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#endif

int main(void)
{
    int Result = 1;

#ifdef _WIN32
    // custom.rc has to reach the exe.
    HRSRC Icon = FindResourceA(NULL, "MY_ICON", (LPCSTR)RT_GROUP_ICON);

    // And the metadata keys must not have generated a version resource next to it.
    HRSRC Version = FindResourceA(NULL, MAKEINTRESOURCEA(1), (LPCSTR)RT_VERSION);

    if (Icon == NULL)
    {
        printf("FAIL: no icon named MY_ICON - custom.rc did not reach the exe\n");
    }
    else if (Version != NULL)
    {
        printf("FAIL: a version resource was generated next to custom.rc\n");
    }
    else
    {
        printf("OK custom rc metadata: custom.rc owns the resources, nothing was generated\n");
        Result = 0;
    }
#else
    printf("OK custom rc metadata: skipped (version resources are windows-only)\n");
    Result = 0;
#endif

    return Result;
}
