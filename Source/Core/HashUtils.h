#ifndef HASHUTILS_H
#define HASHUTILS_H

#include "EngineTypes.h"

#define FNV_OFFSET 14695981039346656037ULL
#define FNV_PRIME  1099511628211ULL

FORCEINLINE NO_DISCARD static u64 FNV1a_Hash(const void* Data, usize Size)
{
    u64 Hash = FNV_OFFSET;

    const u8* Bytes = (const u8*)Data;
    while (Size--)
    {
        Hash ^= (u64)*(Bytes++);
        Hash *= FNV_PRIME;
    }

    return Hash;
}

FORCEINLINE NO_DISCARD static u64 PointerHash(const void* Ptr)
{
	u64 Hash = ((u64)Ptr) >> 4;
	return Hash;
}


#endif // HASHUTILS_H
