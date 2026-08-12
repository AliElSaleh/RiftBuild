#include <stdio.h>

#ifndef PLISTKEYS_VERSION_STRING
    #error "Version(define) did not create PLISTKEYS_VERSION_STRING"
#endif

int main(void)
{
    printf("OK plist keys: %s\n", PLISTKEYS_VERSION_STRING);
    return 0;
}
