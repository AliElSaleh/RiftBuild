#include <stdio.h>
#include "mid.h"

#ifdef LEAF_PUBLIC
    #error "a private dependency's public define leaked through Mid into App"
#endif

int main(void)
{
    int Result = 1;

    if (mid_value() == 13)
    {
        printf("OK private dependencies\n");
        Result = 0;
    }
    else
    {
        printf("FAIL private dependencies\n");
    }

    return Result;
}
