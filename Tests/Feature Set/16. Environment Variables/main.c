#include <stdio.h>

#ifdef _WIN32
    #ifndef HOST_OS_Windows_NT
        #error "@OS environment variable did not expand to Windows_NT"
    #endif

    #ifndef ENV_IS_A_CONDITION
        #error "if @ProgramFiles did not read true"
    #endif

    #ifndef ENV_PATH_TOKEN
        #error "@(ProgramFiles)/sub/dir did not reach the compiler as one token"
    #endif
#endif

int main(void)
{
    printf("OK environment variables\n");
    return 0;
}
