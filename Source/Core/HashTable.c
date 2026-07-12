
#include "HashTableGeneric.h"

static u64 HashTable_KeyHash_String(String Key)
{
    return FNV1a_Hash((void*)Key.Data, Key.Length);
}

static bool HashTable_KeyCompare_String(String A, String B)
{
    return String_IsEqual(A, B, true);
}

HashTable_Declare(String, i32)


static u64 HashTable_KeyHash_PlatformHandle(PlatformHandle Key)
{
    return PointerHash(Key) + (8365 * 319);
}

static bool HashTable_KeyCompare_PlatformHandle(PlatformHandle A, PlatformHandle B)
{
    return (A == B) || MemEqual(A, B, sizeof(PlatformHandle));
}

HashTable_Declare(PlatformHandle, String)
