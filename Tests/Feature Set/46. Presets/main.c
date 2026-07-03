#include <stdio.h>

#ifndef SPEEDY
    #error "SPEEDY must always be defined, either 0 or 1"
#endif

int main(void)
{
    printf("speedy=%d\n", SPEEDY);
    return 0;
}
