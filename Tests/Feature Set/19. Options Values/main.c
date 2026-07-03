#include <stdio.h>

#ifndef LEVEL
    #error "LEVEL must always be defined via the %level option expansion"
#endif

int main(void)
{
    printf("level=%d\n", LEVEL);
    return 0;
}
