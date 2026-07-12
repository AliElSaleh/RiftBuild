#ifndef HASHTABLE_H
#define HASHTABLE_H

#include "EngineTypes.h"

#include "HashUtils.h"

#include "Array.h"
#include "StringUtils.h"
#include "Memory.h"
#include "Log.h"

#define DEFAULT_HASHTABLE_CAPACITY 16

STRUCT(HashTableEntry)
{
    String Key;
    i32    Value;
    usize  Index;
};

// TODO: we can assume a hashtable entry struct. figure out

STRUCT(HashTable)
{
    TArray(HashTableEntry) Entries;
};

STRUCT(HashTableIterator)
{
    HashTable* Table;
    usize Index;
    HashTableEntry* Current;
};

usize HashTable_Capacity(HashTable Table);
usize HashTable_Length(HashTable Table);
bool _HashTable_Grow(HashTable* Table);
void HashTable_Add(HashTable* Table, String Key, i32 Value, bool bUpdateExisting);
void _HashTable_Add(HashTable* Table, String Key, i32 Value, bool bUpdateExisting);
bool HashTable_Find(HashTable Table, String Key, i32* OutValue);
void HashTable_Destroy(HashTable* Table);
usize HashToIndex(usize Hash, usize TableCapacity);

HashTableEntry* HashTable_Iterate_Next(HashTableIterator* Iter);
HashTableIterator HashTable_Iterate_Begin(HashTable* Table);
bool HashTable_Iterate_Check(HashTableIterator* Iter);

HashTableEntry* HashTable_Iterate_Next(HashTableIterator* Iter)
{
    while (Array_IsValidIndex(Iter->Table->Entries, Iter->Index))
    {
        usize i = Iter->Index;
        Iter->Index++;
        HashTableEntry* Entry = &Iter->Table->Entries[i];
        if (String_IsValid(Entry->Key))
        {
            return Entry;
        }
    }

    return NULL;
}

HashTableIterator HashTable_Iterate_Begin(HashTable* Table)
{
    HashTableIterator Iter;
    Iter.Table = Table;
    Iter.Index = 0;
    Iter.Current = HashTable_Iterate_Next(&Iter);
    return Iter;
}

bool HashTable_Iterate_Check(HashTableIterator* Iter)
{
    bool bValid = Array_IsValidIndex(Iter->Table->Entries, Iter->Index);
    return bValid;
}

#define each_kv_in_table(Table) (HashTableIterator It = HashTable_Iterate_Begin(Table); HashTable_Iterate_Check(&It); It.Current = HashTable_Iterate_Next(&It))

usize HashTable_Capacity(HashTable Table)
{
    return Array_Capacity(Table.Entries);
}

usize HashTable_Length(HashTable Table)
{
    return Array_Num(Table.Entries);
}

void HashTable_Destroy(HashTable* Table)
{
    Array_Destroy(Table->Entries);
    Table->Entries = NULL;
}

void _HashTable_Add(HashTable* Table, String Key, i32 Value, bool bUpdateExisting)
{
    usize Length = Array_Num(Table->Entries);
    usize Capacity = Array_Capacity(Table->Entries);
    ASSERT(Capacity > 0);

    u64 Hash = FNV1a_Hash((void*)Key.Data, Key.Length);
    usize Index = Hash & (Capacity-1);

    while (String_IsValid(Table->Entries[Index].Key))
    {
        HashTableEntry* Entry = &Table->Entries[Index];

        if (String_IsEqual(Key, Entry->Key, true))
        {
            // Found key, update the value
            if (bUpdateExisting)
            {
                Entry->Value = Value;
            }

            return;
        }

        // Wasn't found. move to next slot
        Index++;
        if (Index >= Capacity)
        {
            Index = 0;
        }
    }

    HashTableEntry* Entry = &Table->Entries[Index];
    Entry->Key = Key;
    Entry->Value = Value;
    Entry->Index = Index;
    Array_SetNum(Table->Entries, Length+1);
}

bool _HashTable_Grow(HashTable* Table)
{
    usize OldCapacity = Array_Capacity(Table->Entries);
    usize NewCapacity = OldCapacity * 2;
    if (NewCapacity < OldCapacity)
    {
        // we overflow'd
        return false;
    }

    HashTable NewTable;
    NewTable.Entries = Array_Reserve(HashTableEntry, NewCapacity);
    ASSERT(NewTable.Entries != NULL);

    for (usize i = 0; i < OldCapacity; i++)
    {
        HashTableEntry Entry = Table->Entries[i];

        if (String_IsValid(Entry.Key))
        {
            _HashTable_Add(&NewTable, Entry.Key, Entry.Value, false);
        }
    }

    HashTable_Destroy(Table);
    *Table = NewTable;

    return true;
}

void HashTable_Add(HashTable* Table, String Key, i32 Value, bool bUpdateExisting)
{
    usize Length = Array_Num(Table->Entries);
    usize Capacity = Array_Capacity(Table->Entries);
    ASSERT(Capacity > 0);

    if (Length > Capacity / 2)
    {
        bool bGrowSuccess = _HashTable_Grow(Table);
        if (!bGrowSuccess)
        {
            return;
        }
    }

    _HashTable_Add(Table, Key, Value, bUpdateExisting);
}

usize HashToIndex(usize Hash, usize TableCapacity)
{
    usize Index = Hash & (TableCapacity-1);
    return Index;
}

bool HashTable_Find(HashTable Table, String Key, i32* OutValue)
{
    usize Capacity = Array_Capacity(Table.Entries);
    ASSERT(Capacity > 0);

    u64 Hash = FNV1a_Hash((void*)Key.Data, Key.Length);
    usize Index = Hash & (Capacity-1);

    bool bFound = false;
    while (String_IsValid(Table.Entries[Index].Key))
    {
        HashTableEntry* Entry = &Table.Entries[Index];

        if (String_IsEqual(Key, Entry->Key, true))
        {
            // Found key, return the value.
            if (LIKELY(OutValue))
            {
                *OutValue = Entry->Value;
            }

            bFound = true;

            break;
        }

        // Wasn't found. move to next slot
        Index++;
        if (Index >= Capacity)
        {
            Index = 0;
        }
    }

    return bFound;
}

#endif // HASHTABLE_H
