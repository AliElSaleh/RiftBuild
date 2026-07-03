#include "flib.h"

#ifndef LIB_VARIANT
    #error "LIB_VARIANT must always be defined (0 or 1)"
#endif

int flib_variant(void)
{
    return LIB_VARIANT;
}
