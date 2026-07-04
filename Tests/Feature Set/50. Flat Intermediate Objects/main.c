#include <stdio.h>

extern int flat_util_value(void);

int main(void)
{
    int Result = 1;

    if (flat_util_value() == 77)
    {
        printf("OK flat intermediate objects\n");
        Result = 0;
    }
    else
    {
        printf("FAIL flat intermediate objects\n");
    }

    return Result;
}
