#include <stdio.h>
#include "alpha.h"
#include "beta.h"

#if ALPHA_FROM_HEADER != 1
    #error "alpha.h was not found via the Includes key"
#endif

#if BETA_FROM_HEADER != 2
    #error "beta.h was not found via the Includes key"
#endif

int main(void)
{
    printf("OK includes\n");
    return 0;
}
