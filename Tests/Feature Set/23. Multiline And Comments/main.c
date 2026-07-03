#include <stdio.h>

#ifdef SHOULD_NOT_EXIST
    #error "a define inside a ## comment block was parsed"
#endif

#ifdef FIRST_TRY
    #error "Defines` should have replaced the earlier Defines value"
#endif

#ifndef FINAL_ONLY
    #error "the Defines` reset value did not survive"
#endif

extern int helper_value(void);

int main(void)
{
    int Result = 1;

    if (helper_value() == 11)
    {
        printf("OK multiline and comments\n");
        Result = 0;
    }
    else
    {
        printf("FAIL multiline and comments\n");
    }

    return Result;
}
