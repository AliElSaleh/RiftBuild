#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

int main(void)
{
    int Result = 1;

#ifdef _WIN32
    // 101 is RES_DATA_ID from inc/resconfig.h, which res.rc only sees when
    // Resource.Includes reaches the resource compiler
    HRSRC Info = FindResourceA(NULL, MAKEINTRESOURCEA(101), (LPCSTR)RT_RCDATA);
    if (Info != NULL)
    {
        HGLOBAL Handle = LoadResource(NULL, Info);
        DWORD Size = SizeofResource(NULL, Info);
        const char* Data = (const char*)LockResource(Handle);

        if (Data != NULL && Size == 5 && memcmp(Data, "hello", 5) == 0)
        {
            printf("OK resource keys: RCDATA 101 = \"hello\"\n");
            Result = 0;
        }
        else
        {
            printf("FAIL: RCDATA 101 has the wrong contents\n");
        }
    }
    else
    {
        printf("FAIL: RCDATA 101 was not found in the exe\n");
    }
#else
    printf("OK resource keys: skipped (resource compilers are windows-only)\n");
    Result = 0;
#endif

    return Result;
}
