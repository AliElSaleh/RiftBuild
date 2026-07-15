#include <stdio.h>
#include "config_alpha.h"

#if WILDCARD_COPIED != 7
    #error "config_alpha.h was not copied by the PreBuild wildcard Copy"
#endif

int main(void)
{
    printf("OK wildcard file operations: WILDCARD_COPIED=%d\n", WILDCARD_COPIED);
    return 0;
}
