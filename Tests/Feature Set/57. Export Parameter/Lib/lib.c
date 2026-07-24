#include <exportparamlib.h>

/* Unlike .Export keys, (export) values must also apply to the module that
 * declares them - that is the whole point of the parameter. */
#if !defined BOTH_DEFINE || !defined BOTH_COND || !defined BOTH_LIST_A || !defined BOTH_LIST_B
    #error the library itself must see its own (export) defines
#endif

#ifndef LIB_PRIVATE_DEFINE
    #error the library must see its own plain defines
#endif

int ExportParamLibValue(void)
{
    return 42;
}
