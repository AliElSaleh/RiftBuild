#include <stdio.h>
#include <assert.h>

int main(void)
{
    printf("configuration : %s\n", CONFIG_NAME);

#if defined(NDEBUG)
    printf("asserts       : compiled out\n");
#else
    printf("asserts       : active\n");
#endif

    assert(1 + 1 == 2);

    return 0;
}
