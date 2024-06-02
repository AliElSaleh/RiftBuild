#include "EngineTypes.h"

#if DEVELOPER
bool __always__(bool bCondition)
{
    if (!bCondition)
    {
        DEBUG_BREAK();
    }

    return bCondition;
}

bool __never__(bool bCondition)
{
    if (bCondition)
    {
        DEBUG_BREAK();
    }

    return bCondition;
}
#endif
