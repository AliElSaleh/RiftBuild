#include "engine.h"
#include "compression.h"

/* The Engine compiles against Compression's exported header and define... */
#if !defined(HAS_COMPRESSION)
    #error "the Engine itself should see Compression's exported define"
#endif

int Engine_SaveBufferSize(int DataSize)
{
    return Compression_Bound(DataSize) + 16;
}
