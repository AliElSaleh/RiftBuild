#include <stdio.h>

#ifndef VERSIONINFO_VERSION_STRING
    #error "Version(define) did not create VERSIONINFO_VERSION_STRING"
#endif

#if VERSIONINFO_MAJOR_VERSION != 3
    #error "MAJOR_VERSION was not 3"
#endif

#if VERSIONINFO_MINOR_VERSION != 1
    #error "MINOR_VERSION was not 1"
#endif

#if VERSIONINFO_PATCH_VERSION != 4
    #error "PATCH_VERSION was not 4"
#endif

int main(void)
{
    printf("OK version info: %s\n", VERSIONINFO_VERSION_STRING);
    return 0;
}
