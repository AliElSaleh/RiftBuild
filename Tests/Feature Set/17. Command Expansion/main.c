#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef BUILD_HOST
    #error "!hostname did not expand into the BUILD_HOST define"
#endif

#define STR2(x) #x
#define STR(x) STR2(x)

static int equals_ignore_case(const char* A, const char* B)
{
    int bEqual = 1;

    while (*A && *B)
    {
        char Ca = (char)((*A >= 'A' && *A <= 'Z') ? *A + 32 : *A);
        char Cb = (char)((*B >= 'A' && *B <= 'Z') ? *B + 32 : *B);
        if (Ca != Cb)
        {
            bEqual = 0;
            break;
        }
        A++;
        B++;
    }

    if (*A != *B && (*A != '\0' || *B != '\0'))
    {
        bEqual = 0;
    }

    return bEqual;
}

int main(void)
{
    int Result = 1;
    const char* FromBuild = STR(BUILD_HOST);
    const char* FromEnv = getenv("COMPUTERNAME");

    if (FromEnv && FromBuild[0] != '\0' && equals_ignore_case(FromBuild, FromEnv))
    {
        printf("OK command expansion: host=%s\n", FromBuild);
        Result = 0;
    }
    else
    {
        printf("FAIL command expansion: build='%s' env='%s'\n", FromBuild, FromEnv ? FromEnv : "(null)");
    }

    return Result;
}
