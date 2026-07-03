#include <stdio.h>

#ifdef _WIN32
    #ifndef PLAT_WINDOWS
        #error "Defines:windows did not apply on Windows"
    #endif
    #ifdef PLAT_OTHER
        #error "Defines:!windows applied on Windows"
    #endif
#else
    #ifndef PLAT_OTHER
        #error "Defines:!windows did not apply on a non-Windows platform"
    #endif
    #ifdef PLAT_WINDOWS
        #error "Defines:windows applied on a non-Windows platform"
    #endif
#endif

#if defined(__x86_64__) || defined(_M_X64)
    #ifndef ARCH_X64
        #error "Defines:x64 did not apply on an x64 build"
    #endif
    #ifdef ARCH_OTHER
        #error "Defines:!x64 applied on an x64 build"
    #endif
#endif

int main(void)
{
    printf("OK conditional values\n");
    return 0;
}
