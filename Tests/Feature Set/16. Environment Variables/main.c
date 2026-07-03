#include <stdio.h>

#ifdef _WIN32
    #ifndef HOST_OS_Windows_NT
        #error "@OS environment variable did not expand to Windows_NT"
    #endif
#endif

int main(void)
{
    printf("OK environment variables\n");
    return 0;
}
