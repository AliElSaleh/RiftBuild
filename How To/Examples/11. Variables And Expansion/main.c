#include <stdio.h>

#if !defined(TAG_COMET)
    #error "$^Project should have expanded to COMET"
#endif

#if !defined(HOST_OS)
    #define HOST_OS "(not windows)"
#endif

int main(void)
{
    printf("project    : %s\n", PROJECT_NAME);
    printf("host os    : %s\n", HOST_OS);
    printf("built on   : %s\n", BUILD_HOST);
    return 0;
}
