#include <stdio.h>

int main(void)
{
    printf("launch level : %d\n", LEVEL);
    printf("turbo mode   : %s\n", TURBO ? "ON" : "off");
    return 0;
}
