#include <stdio.h>
#include "generated.h"

#if GENERATED_VALUE != 99
    #error "generated.h did not contain the value written by PreBuild.WriteFile"
#endif

int main(void)
{
    printf("OK writefile codegen: GENERATED_VALUE=%d\n", GENERATED_VALUE);
    return 0;
}
