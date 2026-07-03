#include "lib.h"

#ifdef LIB_PUBLIC_DEFINE
    #error "Defines.Public is export-only and must not apply to the library itself"
#endif

#ifndef LIB_PRIVATE_DEFINE
    #error "the library itself must see its own plain define"
#endif

int lib_value(void)
{
    return 5;
}
