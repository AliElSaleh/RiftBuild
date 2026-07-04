#include <stdio.h>

int main(void)
{
    printf("platform      : %s\n", PLATFORM_NAME);
    printf("pointer bits  : %d\n", POINTER_BITS);
    printf("windows build : %s\n", ON_WINDOWS ? "yes" : "no");
    return 0;
}
