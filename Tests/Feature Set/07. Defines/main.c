#include <stdio.h>

#ifndef HAS_FEATURE
    #error "HAS_FEATURE was not passed to the compiler"
#endif

#if ANSWER != 42
    #error "ANSWER was not passed to the compiler as 42"
#endif

int main(void)
{
    printf("OK defines: ANSWER=%d\n", ANSWER);
    return 0;
}
