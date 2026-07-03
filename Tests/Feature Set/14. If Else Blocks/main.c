#include <stdio.h>

#ifdef _WIN32
    #ifndef FROM_IF_BLOCK
        #error "if windows block did not apply"
    #endif
    #ifdef FROM_ELSE_BLOCK
        #error "else block applied even though the if matched"
    #endif
#else
    #ifndef FROM_ELSE_BLOCK
        #error "else block did not apply"
    #endif
#endif

#if defined(__x86_64__) || defined(_M_X64)
    #ifndef INLINE_IF
        #error "single-line if did not apply"
    #endif
#endif

int main(void)
{
    printf("OK if else blocks\n");
    return 0;
}
