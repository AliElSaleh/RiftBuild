#include <stdio.h>

#ifndef __TINYC__
    #error "this test must be compiled by tcc"
#endif

int main(void)
{
    printf("OK compiler tcc\n");
    return 0;
}
