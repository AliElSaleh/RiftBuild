#include <stdio.h>
#include "b.h"
#include "c.h"
#include "d.h"

int main(void)
{
    int Result = 1;

    if (b_value() == 8 && c_value() == 9 && d_value() == 7)
    {
        printf("OK diamond dependency\n");
        Result = 0;
    }
    else
    {
        printf("FAIL diamond dependency\n");
    }

    return Result;
}
