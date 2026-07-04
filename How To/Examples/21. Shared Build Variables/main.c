#include <stdio.h>

#if !defined(RIFT_PRODUCT_FAMILY)
    #error "CommonDefines from the shared .buildvars file did not arrive"
#endif

int main(void)
{
    printf("Family v%s (a shared version, defined once)\n", FAMILY_VERSION_STRING);
    return 0;
}
