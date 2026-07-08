#include <stdio.h>
#include "config.h"

int main(void)
{
    printf("%s\n", BUILD_MESSAGE);
    printf("build number %d [%s]\n", BUILD_NUMBER, BUILD_TIMESTAMP);
    return 0;
}
