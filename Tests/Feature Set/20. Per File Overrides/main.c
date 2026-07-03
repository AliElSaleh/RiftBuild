#include <stdio.h>

#ifdef ONLY_SPECIAL
    #error "special.c.Defines leaked into main.c"
#endif

extern int special_value(void);

int main(void)
{
    int Result = 1;

    if (special_value() == 7)
    {
        printf("OK per file overrides\n");
        Result = 0;
    }
    else
    {
        printf("FAIL per file overrides\n");
    }

    return Result;
}
