#include <stdio.h>

#ifndef TURBO
    #error "TURBO must always be defined, either 0 or 1"
#endif

int main(void)
{
    printf("turbo=%d\n", TURBO);
    return 0;
}
