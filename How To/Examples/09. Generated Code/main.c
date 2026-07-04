#include <stdio.h>
#include "generated.h"

int main(void)
{
    printf("%s\n", BUILD_MESSAGE);
    printf("build number %d\n", BUILD_NUMBER);
    return 0;
}
