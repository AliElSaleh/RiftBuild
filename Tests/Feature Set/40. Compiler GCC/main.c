#include <stdio.h>

#if !defined(__GNUC__) || defined(__clang__)
    #error "this test must be compiled by real gcc"
#endif

int main(void)
{
    printf("OK compiler gcc\n");
    return 0;
}
