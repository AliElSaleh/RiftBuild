#include <stdio.h>

#ifdef _WIN32
    #ifndef HAVE_CMD
        #error "program_exists(cmd) should be true on Windows"
    #endif
#endif

#ifdef HAVE_FAKE_TOOL
    #error "program_exists() claimed a nonexistent tool exists"
#endif

int main(void)
{
    printf("OK program exists condition\n");
    return 0;
}
