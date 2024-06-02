#pragma once

#include "Globals.h"
#include "Memory/Memory.h"

#define ARRAY_DEFAULT_CAPACITY 1
#define ARRAY_RESIZE_FACTOR 2

/*
 * Memory layout of the array structure
 * u64 Capacity;
 * u64 Num;
 * u64 Stride;
 * u64 OwnsMemory;
 * void* Data;
 */

enum
{
	ArrayField_Capacity = 0,
	ArrayField_Num = 1,
	ArrayField_Stride = 2,
	ArrayField_OwnsMemory = 3,
	ArrayField_Count = 4
};

#define ArrayLocal(Type, Name, Capacity) TArray(Type) Name = _ArrayCreate(Capacity, sizeof(Type))

#define Array_Create(Type) _ArrayCreate(ARRAY_DEFAULT_CAPACITY, sizeof(Type))
#define Array_CreateStatic(Type, Capacity, Memory) _ArrayCreateStatic(Memory, Capacity, sizeof(Type))
#define Array_Reserve(Type, Capacity) _ArrayCreate(Capacity, sizeof(Type))

#define Array_Destroy(Array) _ArrayDestroy(Array)

//#define Array_Resize(Array) _ArrayResize(Array)

#define Array_Add(Array, Value)\
do {\
	typeof((Value)) CONCAT(Temp, __LINE__) = (Value);\
	(Array) = _ArrayAdd((Array), &CONCAT(Temp, __LINE__));\
} while (0)

#define Array_InsertAt(Array, Value, Index)\
do {\
	typeof(Value) CONCAT(Temp, __LINE__) = Value;\
	(Array) = _ArrayInsertAt(Array, &CONCAT(Temp, __LINE__), Index);\
} while (0)

/*
#define Array_Append(Array, OtherArray) 			\
{                              						\
	typeof(OtherArray) Temp = OtherArray;   		\
	(Array) = _ArrayAppend(Array, &Temp);			\
}
*/

#define Array_For(Array) \
u32 NumElements = Array_Num(Array); \
for (u32 i = 0; i < NumElements; ++i)

//#define Array_Remove(Array, Value) _ArrayRemove(Array, Value)
#define Array_Remove(Array, Value)\
do {\
    u64 _Num_ = Array_Num(Array);\
\
    if (_Num_ > 0)\
    {\
        for (u64 _i_ = 0; _i_ < _Num_; _i_++)\
        {\
            if (Array[_i_] == Value)\
            {\
                _ArrayRemoveAt(Array, NULL, _i_);\
                break;\
            }\
        }\
        \
        _ArrayFieldSet(Array, ArrayField_Num, _Num_-1);\
    }\
} while (0)

#define Array_RemoveLast(Array, Value) _ArrayRemoveLast(Array, Value)
#define Array_RemoveAt(Array, Value, Index) _ArrayRemoveAt(Array, &(Value), Index)

#define Array_Empty(Array) _ArrayFieldSet(Array, ArrayField_Num, 0)
#define Array_Capacity(Array) _ArrayFieldGet(Array, ArrayField_Capacity)
#define Array_Num(Array) _ArrayFieldGet(Array, ArrayField_Num)
#define Array_Stride(Array) _ArrayFieldGet(Array, ArrayField_Stride)

#define Array_SetNum(Array, Value) _ArrayFieldSet(Array, ArrayField_Num, Value)

#define Array_IsValidIndex(Array, Index) _ArrayIsValidIndex(Array, Index)

#define Array_Last(Array) (Array)[Array_Num((Array)) == 0 ? 1 : Array_Num((Array)) - 1]

#define SArray_Num(Array) CONCAT(Array, _Count)
#define SArray_Add(Array, Value)\
do {\
    if (CONCAT(Array, _Count) < SArray_Capacity(Array))\
    {\
        Array[CONCAT(Array, _Count)] = Value;\
        CONCAT(Array, _Count)++;\
    }\
} while (0)

RIFT_API void*    _ArrayCreate(u64 Num, u64 Stride);
RIFT_API u64      _ArrayCalculateMemRequirement(u64 Num, u64 Stride);
RIFT_API void*    _ArrayCreateStatic(void* Memory, u64 Num, u64 Stride);
RIFT_API void     _ArrayDestroy(void* Array);

//RIFT_API void*    _ArrayResize(void* Array);

RIFT_API void*    _ArrayAdd(void* Array, const void* ValuePtr);
RIFT_API void*    _ArrayInsertAt(void* Array, const void* ValuePtr, u64 Index);

RIFT_API void     _ArrayRemoveLast(void* Array, void* ValuePtr);
RIFT_API void*    _ArrayRemoveAt(void* Array, void* ValuePtr, u64 Index);

RIFT_API u64      _ArrayFieldGet(void* Array, u64 Field);
RIFT_API void     _ArrayFieldSet(void* Array, u64 Field, u64 Value);

RIFT_API bool     _ArrayIsValidIndex(void* Array, u64 Index);

RIFT_API void*    _Array_MemAlloc(u64 Size);
RIFT_API void     _Array_MemFree(void* Memory);

RIFT_API void*    Array_Null(void);
