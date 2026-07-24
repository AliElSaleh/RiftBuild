/* found through the exported include path */
#include <exportparamlib.h>

#if !defined BOTH_DEFINE || !defined BOTH_COND || !defined BOTH_LIST_A || !defined BOTH_LIST_B
    #error the consumer must inherit every (export) define
#endif

#ifdef LIB_PRIVATE_DEFINE
    #error plain (non-export) defines must not leak to the consumer
#endif

int main(void)
{
    int Result = 1;

    if (ExportParamLibValue() == 42)
    {
        Result = 0;
    }

    return Result;
}
