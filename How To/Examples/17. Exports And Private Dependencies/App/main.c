#include <stdio.h>
#include "engine.h"

/* The Engine's exports reach us... */
#if !defined(ENGINE_API_VERSION)
    #error "the Engine's exported define should reach the App"
#endif

/* ...but its PRIVATE dependency does not: HAS_COMPRESSION is invisible here,
   and #include "compression.h" would fail. That is Depend(private) working. */
#if defined(HAS_COMPRESSION)
    #error "Compression is the Engine's implementation detail - it must not leak"
#endif

int main(void)
{
    printf("engine api v%d\n", ENGINE_API_VERSION);
    printf("save buffer for 1000 bytes: %d\n", Engine_SaveBufferSize(1000));
    return 0;
}
