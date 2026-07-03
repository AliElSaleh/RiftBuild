#include <stdio.h>

extern int extra_value(void);

int main(void)
{
    int Result = 1;

    if (extra_value() == 66)
    {
        printf("OK export compile commands\n");
        Result = 0;
    }

    return Result;
}
