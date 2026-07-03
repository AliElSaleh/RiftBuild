#include <stdio.h>

extern int incremental_value(void);

int main(void)
{
    int Result = 1;

    if (incremental_value() == 55)
    {
        printf("OK incremental rebuild\n");
        Result = 0;
    }

    return Result;
}
