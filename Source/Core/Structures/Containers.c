// Copyright (c) 2024 Ali El Saleh

#include "Array.h"

#include "Memory/Memory.h"
#include "Log.h"

void* _ArrayCreate(u64 Num, u64 Stride)
{
    u64 HeaderSize = ArrayField_Count * sizeof(u64);
    u64 ArraySize = Num * Stride;
    
    u64* NewArray = (u64*)MemAlloc(HeaderSize + ArraySize, MemoryTag_DynamicArray);
    
    NewArray[ArrayField_Capacity] = Num;
    NewArray[ArrayField_Num] = 0;
    NewArray[ArrayField_Stride] = Stride;
    NewArray[ArrayField_OwnsMemory] = 1;
    
    return (void*)(NewArray + ArrayField_Count);
}

u64 _ArrayCalculateMemRequirement(u64 Num, u64 Stride)
{
    u64 HeaderSize = ArrayField_Count * sizeof(u64);
    u64 Alignment = 3;
    u64 ArraySize = Num * ((Stride + Alignment) & ~Alignment);
    
    return HeaderSize + ArraySize;
}

void* _ArrayCreateStatic(void* Memory, u64 Num, u64 Stride)
{
    u64* NewArray = (u64*)Memory;

    NewArray[ArrayField_Capacity] = Num;
    NewArray[ArrayField_Num] = 0;
    NewArray[ArrayField_Stride] = Stride;
    NewArray[ArrayField_OwnsMemory] = 0;

    return (void*)(NewArray + ArrayField_Count);
}

void _ArrayDestroy(void* Array)
{
    u64* Header = (u64*)Array - ArrayField_Count;
    
    if (Header[ArrayField_OwnsMemory] == 1)
    {
        MemFree(Header, MemoryTag_DynamicArray);
    }
}

internal void* _ArrayResize(void* Array)
{
    const u64* Header = (u64*)Array - ArrayField_Count;

    if (Header[ArrayField_OwnsMemory] == 1)
    {
        u64 Num = Array_Num(Array);
        u64 Stride = Array_Stride(Array);
        
        void* NewArray = _ArrayCreate(Array_Capacity(Array) * ARRAY_RESIZE_FACTOR, Stride);

        MemCopy(NewArray, Array, Num * Stride);

        _ArrayFieldSet(Array, ArrayField_Num, Num);
        _ArrayDestroy(Array);
        
        return NewArray;
    }

    return Array;
}

void* _ArrayAdd(void* Array, const void* ValuePtr)
{
    u64 Num = Array_Num(Array);
    u64 Stride = Array_Stride(Array);

    // Resize if needed
    if (Num >= Array_Capacity(Array))
    {
        Array = _ArrayResize(Array);

        // this array is static, thus it cannot be resized, exit out and don't mutate anything
        const u64* Header = (u64*)Array - ArrayField_Count;
        if (Header[ArrayField_OwnsMemory] != 1)
        {
            #if DEVELOPER
            LOG_WARNING("Fixed size TArray is full, cannot resize or add more elements because it does not own the memory.");
            #endif

            return Array;
        }
    }
    
    u64 Addr = (u64)Array;
    Addr += (Num * Stride); // go to end of array

    MemCopy((void*)Addr, ValuePtr, Stride);

    _ArrayFieldSet(Array, ArrayField_Num, Num+1);
    
    return Array;
}

void* _ArrayInsertAt(void* Array, const void* ValuePtr, u64 Index)
{
    u64 Num = Array_Num(Array);
    u64 Stride = Array_Stride(Array);

    if (Index >= Num)
    {
        LOG_WARNING("Index outside the bounds of the array. Num: %llu | Index: %llu", Num, Index);
        return Array;
    }

    // Resize if needed
    if (Num >= Array_Capacity(Array))
    {
        Array = _ArrayResize(Array);

        // this array is static, thus it cannot be resized, exit out and don't mutate anything
        const u64* Header = (u64*)Array - ArrayField_Count;
        if (Header[ArrayField_OwnsMemory] != 1)
        {
            #if DEVELOPER
            LOG_WARNING("Fixed size TArray is full, cannot resize or add more elements because it does not own the memory.");
            #endif

            return Array;
        }
    }

    u64 Addr = (u64)Array;

    // If not last element, snip out the entry and copy the rest outward
    if (Index != Num-1)
    {
        MemCopy((void*)(Addr + ((Index + 1) * Stride)),
                (void*)(Addr + (Index * Stride)),
                Stride * (Num - Index));
    }

    MemCopy((void*)(Addr + (Index * Stride)), ValuePtr, Stride);

    _ArrayFieldSet(Array, ArrayField_Num, Num+1);
    return Array;
}

/*
void* _ArrayAppend(void* Array, void* OtherArray)
{
    u64 OtherArrayNum = Array_Num(OtherArray);
    
    for (size_t i = 0; i < OtherArrayNum; ++i)
    {
        Array_Add(Array, ((u64*)OtherArray)[i])
    }
    
    return Array;
}
*/

/*
void _ArrayRemove(void* Array, void* ValuePtr)
{
    u64 Num = Array_Num(Array);
    u64 Stride = Array_Stride(Array);

    for (u64 i = 0; i < Num; i++)
    {
        if (*((u8*)Array + i * Stride) == *(u8*)ValuePtr)
        {
            _ArrayRemoveAt(Array, NULL, i);
            break;
        }
    }

    _ArrayFieldSet(Array, ArrayField_Num, Num-1);
}
*/

void _ArrayRemoveLast(void* Array, void* ValuePtr)
{
    u64 Num = Array_Num(Array);
    u64 Stride = Array_Stride(Array);

    u64 Addr = (u64)Array;
    Addr += ((Num-1) * Stride); // go to 2nd last element

    MemCopy(ValuePtr, (void*)Addr, Stride);

    _ArrayFieldSet(Array, ArrayField_Num, Num-1);
}

void* _ArrayRemoveAt(void* Array, void* ValuePtr, u64 Index)
{
    u64 Num = Array_Num(Array);
    u64 Stride = Array_Stride(Array);

    if (Index >= Num)
    {
        LOG_WARNING("Index outside the bounds of the array. Num: %llu | Index: %llu", Num, Index);
        return Array;
    }

    u64 Addr = (u64)Array;

    if (ValuePtr)
    {
        MemCopy(ValuePtr, (void*)(Addr + (Index * Stride)), Stride);
    }

    // If not last element, snip out the entry and copy the rest inward
    if (Index != Num-1)
    {
        MemCopy((void*)(Addr + (Index * Stride)),
                (void*)(Addr + ((Index + 1) * Stride)),
                Stride * (Num - Index));		
    }

    _ArrayFieldSet(Array, ArrayField_Num, Num-1);
    return Array;
}

u64 _ArrayFieldGet(void* Array, u64 Field)
{
    const u64* Header = (u64*)Array - ArrayField_Count;
    return Header[Field];
}

void _ArrayFieldSet(void* Array, u64 Field, u64 Value)
{
    u64* Header = (u64*)Array - ArrayField_Count;
    Header[Field] = Value;
}

bool _ArrayIsValidIndex(void* Array, u64 Index)
{
    return Index < _ArrayFieldGet(Array, ArrayField_Capacity);
}

