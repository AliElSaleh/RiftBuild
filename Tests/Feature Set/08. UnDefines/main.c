#include <stdio.h>

#ifndef KEEP_ME
    #error "KEEP_ME should still be defined"
#endif

#ifdef __STDC_HOSTED__
    #error "__STDC_HOSTED__ should have been stripped by the global UnDefines"
#endif

#ifdef __STDC_UTF_16__
    #error "__STDC_UTF_16__ should have been stripped by main.c.UnDefines"
#endif

int main(void)
{
    printf("OK undefines\n");
    return 0;
}
