#include <stdio.h>
#include "flib.h"

#if LIB_VARIANT != 1
    #error "the | variant filter did not reach the dependency build"
#endif

int main(void)
{
    int Result = 1;

    if (flib_variant() == 1)
    {
        printf("OK dependency filter args\n");
        Result = 0;
    }
    else
    {
        printf("FAIL dependency filter args: library was built without variant\n");
    }

    return Result;
}
