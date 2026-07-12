#ifndef HASHTABLE_H
#define HASHTABLE_H

#include "EngineTypes.h"

#include "HashUtils.h"

#include "Array.h"
#include "StringUtils.h"
#include "Memory.h"
#include "Log.h"

#define DEFAULT_HASHTABLE_CAPACITY 16
#define HASHTABLE_LOAD_FACTOR 0.7f

PRAGMA_DISABLE_PADDING_WARNINGS

#define HASHTABLE_TYPE(Prefix, KeyType, ValueType)        CONCAT(Prefix, CONCAT(KeyType, ValueType))
#define HASHTABLE_FN(Prefix, KeyType, ValueType, Postfix) CONCAT(CONCAT(Prefix, CONCAT(KeyType, ValueType)), Postfix)


#define HashTable_Declare(KeyType, ValueType)\
    STRUCT(HASHTABLE_TYPE(HashTableEntry_, KeyType, ValueType))                                                 \
    {                                                                                                           \
        usize Index;                                                                                            \
        u64 Hash;                                                                                               \
        KeyType Key;                                                                                            \
        ValueType Value;                                                                                        \
    };                                                                                                          \
    STRUCT(HASHTABLE_TYPE(HashTable_, KeyType, ValueType))                                                      \
    {                                                                                                           \
        TArray(HASHTABLE_TYPE(HashTableEntry_, KeyType, ValueType)) Entries;                                    \
    };                                                                                                          \
    STRUCT(HASHTABLE_TYPE(HashTableIterator_, KeyType, ValueType))                                              \
    {                                                                                                           \
        HASHTABLE_TYPE(HashTable_,      KeyType, ValueType)* Table;                                             \
        HASHTABLE_TYPE(HashTableEntry_, KeyType, ValueType)* Current;                                           \
        usize Index;                                                                                            \
    };                                                                                                          \
    read_only HASHTABLE_TYPE(HashTable_, KeyType, ValueType) CONCAT(HASHTABLE_TYPE(HashTable_, KeyType, ValueType), _Nil) = {0};\
    bool  HASHTABLE_FN(HashTable_, KeyType, ValueType, _Grow       )(HASHTABLE_TYPE(HashTable_, KeyType, ValueType)* Table);\
    void  HASHTABLE_FN(HashTable_, KeyType, ValueType, _Add        )(HASHTABLE_TYPE(HashTable_, KeyType, ValueType)* Table, KeyType Key, ValueType Value);\
    void  HASHTABLE_FN(HashTable_, KeyType, ValueType, _AddOrUpdate)(HASHTABLE_TYPE(HashTable_, KeyType, ValueType)* Table, KeyType Key, ValueType Value);\
    void  HASHTABLE_FN(HashTable_, KeyType, ValueType, _Remove     )(HASHTABLE_TYPE(HashTable_, KeyType, ValueType)* Table, KeyType Key);\
    bool  HASHTABLE_FN(HashTable_, KeyType, ValueType, _Find       )(HASHTABLE_TYPE(HashTable_, KeyType, ValueType)  Table, KeyType Key, ValueType* OutValue);\
    void  HASHTABLE_FN(HashTable_, KeyType, ValueType, _Destroy    )(HASHTABLE_TYPE(HashTable_, KeyType, ValueType)* Table);\
    \
    void  HASHTABLE_FN(_InternalHashTable_, KeyType, ValueType, _AddCommon )(HASHTABLE_TYPE(HashTable_, KeyType, ValueType)* Table, KeyType Key, ValueType Value, bool bUpdateExisting);\
    void  HASHTABLE_FN(_InternalHashTable_, KeyType, ValueType, _Add       )(void* Entries, KeyType Key, ValueType Value, bool bUpdateExisting);\
    HASHTABLE_TYPE(HashTableEntry_, KeyType, ValueType)* HASHTABLE_FN(_InternalHashTable_, KeyType, ValueType, _Find)(TArray(HASHTABLE_TYPE(HashTableEntry_, KeyType, ValueType)) Entries, KeyType Key, u64 Hash);\
    \
    HASHTABLE_TYPE(HashTableEntry_, KeyType, ValueType)* HASHTABLE_FN(HashTable_, KeyType, ValueType, _Iterate_Next)(HASHTABLE_TYPE(HashTableIterator_, KeyType, ValueType)* Iter);\
    HASHTABLE_TYPE(HashTableIterator_, KeyType, ValueType) HASHTABLE_FN(HashTable_, KeyType, ValueType, _Iterate_Begin)(HASHTABLE_TYPE(HashTable_, KeyType, ValueType)* Table);\
    bool HASHTABLE_FN(HashTable_, KeyType, ValueType, _Iterate_Check)(HASHTABLE_TYPE(HashTableIterator_, KeyType, ValueType)* Iter);\
    \
    HASHTABLE_TYPE(HashTableEntry_, KeyType, ValueType)*                                                        \
    HASHTABLE_FN(HashTable_, KeyType, ValueType, _Iterate_Next)                                                 \
    (HASHTABLE_TYPE(HashTableIterator_, KeyType, ValueType)* Iter)                                              \
    {                                                                                                           \
        while (Array_IsValidIndex(Iter->Table->Entries, Iter->Index))                                           \
        {                                                                                                       \
            usize i = Iter->Index;                                                                              \
            Iter->Index++;                                                                                      \
                                                                                                                \
            HASHTABLE_TYPE(HashTableEntry_, KeyType, ValueType)* Entry = &Iter->Table->Entries[i];              \
            if (Entry->Hash != 0)                                                                               \
            {                                                                                                   \
                return Entry;                                                                                   \
            }                                                                                                   \
        }                                                                                                       \
                                                                                                                \
        return NULL;                                                                                            \
    }                                                                                                           \
    HASHTABLE_TYPE(HashTableIterator_, KeyType, ValueType)                                                      \
    HASHTABLE_FN(HashTable_, KeyType, ValueType, _Iterate_Begin)                                                \
    (HASHTABLE_TYPE(HashTable_, KeyType, ValueType)* Table)                                                     \
    {                                                                                                           \
        HASHTABLE_TYPE(HashTableIterator_, KeyType, ValueType) Iter;                                            \
        Iter.Table = Table;                                                                                     \
        Iter.Index = 0;                                                                                         \
        Iter.Current = HASHTABLE_FN(HashTable_, KeyType, ValueType, _Iterate_Next)(&Iter);                      \
        return Iter;                                                                                            \
    }                                                                                                           \
    bool HASHTABLE_FN(HashTable_, KeyType, ValueType, _Iterate_Check)                                           \
    (HASHTABLE_TYPE(HashTableIterator_, KeyType, ValueType)* Iter)                                              \
    {                                                                                                           \
        bool bValid = Iter->Current != NULL;                                                                    \
        return bValid;                                                                                          \
    }                                                                                                           \
    void HASHTABLE_FN(_InternalHashTable_, KeyType, ValueType, _Add)                                            \
    (void* _Entries, KeyType Key, ValueType Value, bool bUpdateExisting)                                        \
    {                                                                                                           \
        usize Length = Array_Num(_Entries);                                                                     \
        usize Capacity = Array_Capacity(_Entries);                                                              \
        ASSERT(Capacity > 0);                                                                                   \
        {                                                                                                       \
            MACRO_COMMENT(                                                                                      \
            "if memory is non-owning, then we are a static table"                                               \
            "therefore exit out early to prevent an infinite search loop")                                      \
                                                                                                                \
            if (UNLIKELY(Length >= Capacity))                                                                   \
            {                                                                                                   \
                return;                                                                                         \
            }                                                                                                   \
        }                                                                                                       \
                                                                                                                \
        u64 Hash = CONCAT(HashTable_KeyHash_, KeyType)(Key);                                                    \
        usize Index = Hash & (Capacity-1);                                                                      \
                                                                                                                \
        TArray(HASHTABLE_TYPE(HashTableEntry_, KeyType, ValueType)) Entries = _Entries;                         \
                                                                                                                \
        while (Entries[Index].Hash != 0)                                                                        \
        {                                                                                                       \
            HASHTABLE_TYPE(HashTableEntry_, KeyType, ValueType)* Entry = &Entries[Index];                       \
                                                                                                                \
            if (CONCAT(HashTable_KeyCompare_, KeyType)(Key, Entry->Key))                                        \
            {                                                                                                   \
                if (bUpdateExisting)                                                                            \
                {                                                                                               \
                    Entry->Value = Value;                                                                       \
                }                                                                                               \
                                                                                                                \
                return;                                                                                         \
            }                                                                                                   \
                                                                                                                \
            Index++;                                                                                            \
            if (Index >= Capacity)                                                                              \
            {                                                                                                   \
                Index = 0;                                                                                      \
            }                                                                                                   \
        }                                                                                                       \
                                                                                                                \
        HASHTABLE_TYPE(HashTableEntry_, KeyType, ValueType)* Entry = &Entries[Index];                           \
        Entry->Key = Key;                                                                                       \
        Entry->Value = Value;                                                                                   \
        Entry->Index = Index;                                                                                   \
        Entry->Hash = Hash;                                                                                     \
        Array_SetNum(Entries, Length+1);                                                                        \
    }                                                                                                           \
    bool HASHTABLE_FN(HashTable_, KeyType, ValueType, _Grow)(HASHTABLE_TYPE(HashTable_, KeyType, ValueType)* Table)            \
    {                                                                                                                          \
        if (!Array_OwnsMemory(Table->Entries))                                                                                 \
        {                                                                                                                      \
            return false;                                                                                                      \
        }                                                                                                                      \
                                                                                                                               \
        usize OldCapacity = Array_Capacity(Table->Entries);                                                                    \
        usize NewCapacity = OldCapacity * 2;                                                                                   \
        if (NewCapacity < OldCapacity)                                                                                         \
        {                                                                                                                      \
            return false;                                                                                                      \
        }                                                                                                                      \
                                                                                                                               \
        HASHTABLE_TYPE(HashTable_, KeyType, ValueType) NewTable;                                                               \
        NewTable.Entries = Array_Reserve(HASHTABLE_TYPE(HashTableEntry_, KeyType, ValueType), NewCapacity);                    \
        ASSERT(NewTable.Entries != NULL);                                                                                      \
                                                                                                                               \
        for (usize i = 0; i < OldCapacity; i++)                                                                                \
        {                                                                                                                      \
            HASHTABLE_TYPE(HashTableEntry_, KeyType, ValueType)* Entry = &Table->Entries[i];                                   \
                                                                                                                               \
            if (Entry->Hash != 0)                                                                                              \
            {                                                                                                                  \
                HASHTABLE_FN(_InternalHashTable_, KeyType, ValueType, _Add)(NewTable.Entries, Entry->Key, Entry->Value, false);\
            }                                                                                                                  \
        }                                                                                                                      \
                                                                                                                               \
        HASHTABLE_FN(HashTable_, KeyType, ValueType, _Destroy)(Table);                                                         \
        *Table = NewTable;                                                                                                     \
                                                                                                                               \
        return true;                                                                                                           \
    }                                                                                                                          \
    void HASHTABLE_FN(_InternalHashTable_, KeyType, ValueType, _AddCommon)                                                              \
    (HASHTABLE_TYPE(HashTable_, KeyType, ValueType)* Table, KeyType Key, ValueType Value, bool bUpdateExisting)       \
    {                                                                                                                          \
        usize Length = Array_Num(Table->Entries);                                                                              \
        usize Capacity = Array_Capacity(Table->Entries);                                                                       \
        ASSERT(Capacity > 0);                                                                                                  \
                                                                                                                               \
        if (Array_OwnsMemory(Table->Entries) && (Length > (usize)((f32)Capacity * HASHTABLE_LOAD_FACTOR)))                                                       \
        {                                                                                                                      \
            bool bGrowSuccess = HASHTABLE_FN(HashTable_, KeyType, ValueType, _Grow)(Table);                                    \
            if (!bGrowSuccess)                                                                                                 \
            {                                                                                                                  \
                return;                                                                                                        \
            }                                                                                                                  \
        }                                                                                                                      \
                                                                                                                               \
        HASHTABLE_FN(_InternalHashTable_, KeyType, ValueType, _Add)(Table->Entries, Key, Value, bUpdateExisting);              \
    }                                                                                                                          \
    void HASHTABLE_FN(HashTable_, KeyType, ValueType, _Add)                                                                    \
    (HASHTABLE_TYPE(HashTable_, KeyType, ValueType)* Table, KeyType Key, ValueType Value)                                      \
    {                                                                                                                          \
        HASHTABLE_FN(_InternalHashTable_, KeyType, ValueType, _AddCommon)(Table, Key, Value, false);                  \
    }                                                                                                                          \
    void HASHTABLE_FN(HashTable_, KeyType, ValueType, _AddOrUpdate)                                                            \
    (HASHTABLE_TYPE(HashTable_, KeyType, ValueType)* Table, KeyType Key, ValueType Value)                                      \
    {                                                                                                                          \
        HASHTABLE_FN(_InternalHashTable_, KeyType, ValueType, _AddCommon)(Table, Key, Value, true);                   \
    }                                                                                                                          \
    void HASHTABLE_FN(HashTable_, KeyType, ValueType, _Remove)                                                                 \
    (HASHTABLE_TYPE(HashTable_, KeyType, ValueType)* Table, KeyType Key)                                                       \
    {                                                                                                                          \
        u64 Hash = CONCAT(HashTable_KeyHash_, KeyType)(Key);                                                                   \
                                                                                                                               \
        HASHTABLE_TYPE(HashTableEntry_, KeyType, ValueType)* Found =                                                           \
        HASHTABLE_FN(_InternalHashTable_, KeyType, ValueType, _Find)(Table->Entries, Key, Hash);                               \
                                                                                                                               \
        if (Found)                                                                                                             \
        {                                                                                                                      \
            HASHTABLE_TYPE(HashTableEntry_, KeyType, ValueType) NilEntry = {0};                                                \
            *Found = NilEntry;                                                                                                 \
        }                                                                                                                      \
    }                                                                                                                          \
    void HASHTABLE_FN(HashTable_, KeyType, ValueType, _Destroy)(HASHTABLE_TYPE(HashTable_, KeyType, ValueType)* Table)         \
    {                                                                                                                          \
        Array_Destroy(Table->Entries);                                                                                         \
        Table->Entries = NULL;                                                                                                 \
    }                                                                                                                          \
    HASHTABLE_TYPE(HashTableEntry_, KeyType, ValueType)*                                                                       \
    HASHTABLE_FN(_InternalHashTable_, KeyType, ValueType, _Find)                                                               \
    (TArray(HASHTABLE_TYPE(HashTableEntry_, KeyType, ValueType)) Entries, KeyType Key, u64 Hash)                               \
    {                                                                                                                          \
        usize Length = Array_Num(Entries);                                                                                     \
        usize Capacity = Array_Capacity(Entries);                                                                              \
                                                                                                                               \
        usize Index = Hash & (Capacity-1);                                                                                     \
                                                                                                                               \
        usize NumSearched = 0;                                                                                                 \
        HASHTABLE_TYPE(HashTableEntry_, KeyType, ValueType)* Found = NULL;                                                     \
        while (Entries[Index].Hash != 0)                                                                                       \
        {                                                                                                                      \
            HASHTABLE_TYPE(HashTableEntry_, KeyType, ValueType)* Entry = &Entries[Index];                                      \
                                                                                                                               \
            if (CONCAT(HashTable_KeyCompare_, KeyType)(Key, Entry->Key))                                                       \
            {                                                                                                                  \
                Found = Entry;                                                                                                 \
                break;                                                                                                         \
            }                                                                                                                  \
                                                                                                                               \
            NumSearched++;                                                                                                     \
            if (NumSearched >= Length)                                                                                         \
            {                                                                                                                  \
                Found = NULL;                                                                                                  \
                break;                                                                                                         \
            }                                                                                                                  \
                                                                                                                               \
            Index++;                                                                                                           \
            if (Index >= Capacity)                                                                                             \
            {                                                                                                                  \
                Index = 0;                                                                                                     \
            }                                                                                                                  \
        }                                                                                                                      \
                                                                                                                               \
        return Found;                                                                                                          \
    }                                                                                                                          \
    bool HASHTABLE_FN(HashTable_, KeyType, ValueType, _Find)(HASHTABLE_TYPE(HashTable_, KeyType, ValueType) Table, KeyType Key, ValueType* OutValue) \
    {                                                                                                                          \
        u64 Hash = CONCAT(HashTable_KeyHash_, KeyType)(Key);                                                                   \
                                                                                                                               \
        HASHTABLE_TYPE(HashTableEntry_, KeyType, ValueType)* Found =                                                           \
        HASHTABLE_FN(_InternalHashTable_, KeyType, ValueType, _Find)(Table.Entries, Key, Hash);                                     \
                                                                                                                               \
        if (Found)                                                                                                             \
        {                                                                                                                      \
            if (LIKELY(OutValue))                                                                                              \
            {                                                                                                                  \
                *OutValue = Found->Value;                                                                                      \
            }                                                                                                                  \
        }                                                                                                                      \
                                                                                                                               \
        bool bFound = Found != NULL;                                                                                           \
        return bFound;                                                                                                         \
    }                                                                                                                          \

#define each_kv_in_map(KeyType, ValueType, Table)\
    (HASHTABLE_TYPE(HashTableIterator_, KeyType, ValueType) It = HASHTABLE_FN(HashTable_, KeyType, ValueType, _Iterate_Begin)(Table);\
    HASHTABLE_FN(HashTable_, KeyType, ValueType, _Iterate_Check)(&It);\
    It.Current = HASHTABLE_FN(HashTable_, KeyType, ValueType, _Iterate_Next)(&It))

#define HashTable(Name, KeyType, ValueType)\
    HASHTABLE_TYPE(HashTable_, KeyType, ValueType) Name = {0};\
    Name.Entries = Array_Reserve(HASHTABLE_TYPE(HashTableEntry_, KeyType, ValueType), DEFAULT_HASHTABLE_CAPACITY)

#define HashTable_Arena(Name, KeyType, ValueType, Arena, Capacity)\
    HASHTABLE_TYPE(HashTable_, KeyType, ValueType) Name = {0};\
    Name.Entries = Internal_ArrayCreateStatic(LinearAllocator_Allocate(Arena, Array_CalculateMemRequirement(Capacity, sizeof(HASHTABLE_TYPE(HashTableEntry_, KeyType, ValueType)))), Capacity, sizeof(HASHTABLE_TYPE(HashTableEntry_, KeyType, ValueType)));\
    MemZero(Name.Entries, sizeof(HASHTABLE_TYPE(HashTableEntry_, KeyType, ValueType)) * Capacity)

static u64 HashTable_KeyHash_String(String Key)
{
    return FNV1a_Hash((void*)Key.Data, Key.Length);
}

static bool HashTable_KeyCompare_String(String A, String B)
{
    return String_IsEqual(A, B, true);
}

static u64 HashTable_KeyHash_u32(u32 Key)
{
    return Key * 983471 + 111111111111;
}

static bool HashTable_KeyCompare_u32(u32 A, u32 B)
{
    return A == B;
}

HashTable_Declare(String, i32)
HashTable_Declare(String, u32)
HashTable_Declare(u32, u32)

/*
static u64 HashTable_KeyHash_PlatformHandle(PlatformHandle Key)
{
    return PointerHash(Key) + (8365 * 319);
}

static bool HashTable_KeyCompare_PlatformHandle(PlatformHandle A, PlatformHandle B)
{
    return (A == B) || MemEqual(A, B, sizeof(PlatformHandle));
}

HashTable_Declare(PlatformHandle, String)
*/

PRAGMA_ENABLE_WARNINGS

#endif // HASHTABLE_H
