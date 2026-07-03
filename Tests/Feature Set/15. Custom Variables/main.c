#include <stdio.h>

#if CORE_ON != 1 || EXTRA_ON != 2
    #error "$SharedFlagDefines did not expand into Defines"
#endif

#ifndef TAG_LOWER_rift
    #error "$-ProjectTag did not lowercase the variable value"
#endif

#ifndef TAG_UPPER_RIFT
    #error "$^ProjectTag did not uppercase the variable value"
#endif

int main(void)
{
    printf("OK custom variables\n");
    return 0;
}
