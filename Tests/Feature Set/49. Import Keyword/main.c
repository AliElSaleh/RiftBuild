#include <stdio.h>

#if FROM_IMPORTED_FILE != 1
    #error "the imported .buildvars file did not contribute its define"
#endif

int main(void)
{
    printf("OK import keyword\n");
    return 0;
}
