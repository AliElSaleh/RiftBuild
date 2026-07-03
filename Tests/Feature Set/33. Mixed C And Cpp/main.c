#include <stdio.h>

extern int cpp_bridge(void);

int main(void)
{
    int Result = 1;

    if (cpp_bridge() == 21)
    {
        printf("OK mixed c and cpp\n");
        Result = 0;
    }
    else
    {
        printf("FAIL mixed c and cpp\n");
    }

    return Result;
}
