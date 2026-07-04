#include <stdio.h>

int main(void)
{
    printf("Stamp v%s\n", STAMP_VERSION_STRING);
    printf("major %d, minor %d, patch %d\n",
           STAMP_MAJOR_VERSION, STAMP_MINOR_VERSION, STAMP_PATCH_VERSION);

#if STAMP_MAJOR_VERSION >= 2
    printf("(2.x feature enabled)\n");
#endif

    return 0;
}
