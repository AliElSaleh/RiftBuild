#include <stdio.h>

extern int src_util_value(void);

int main(void)
{
    int Result = 1;

    if (src_util_value() == 33)
    {
        printf("OK source directory\n");
        Result = 0;
    }
    else
    {
        printf("FAIL source directory\n");
    }

    return Result;
}
