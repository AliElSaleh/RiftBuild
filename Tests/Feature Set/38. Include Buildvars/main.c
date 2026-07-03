#include <stdio.h>

#if FROM_INCLUDED_FILE != 1
    #error "the included .buildvars file did not contribute its define"
#endif

int main(void)
{
    printf("OK include buildvars\n");
    return 0;
}
