#include <stdio.h>
#include "lib.h"

#ifndef LIB_PUBLIC_DEFINE
    #error "Defines.Export did not propagate from the dependency"
#endif

#ifdef LIB_PRIVATE_DEFINE
    #error "a non-public define leaked out of the dependency"
#endif

#ifdef _WIN32
/* From winmm, which only links because the dependency exports it via
   Libraries.Export - this call is the propagation check. */
__declspec(dllimport) unsigned long __stdcall timeGetTime(void);
#endif

int main(void)
{
    int Result = 1;
    unsigned long Tick = 1;

    #ifdef _WIN32
    Tick = timeGetTime();
    #endif

    if (lib_value() == 5 && Tick != 0)
    {
        printf("OK public propagation\n");
        Result = 0;
    }
    else
    {
        printf("FAIL public propagation\n");
    }

    return Result;
}
