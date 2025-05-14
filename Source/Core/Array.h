#ifndef ARRAY_H
#define ARRAY_H

#ifndef UNITY_BUILD
#include "EngineTypes.h"
#endif

/*
 * Memory layout of the array structure
 * usize Capacity;
 * usize Num;
 * usize Stride;
 * usize OwnsMemory;
 * void* Data;
 */

enum
{
    ArrayField_Capacity   = 0,
    ArrayField_Num        = 1,
    ArrayField_Stride     = 2,
    ArrayField_OwnsMemory = 3,
    ArrayField_Count      = 4
};

#define Array_FieldGet(Array, Field)                  ((usize*)(Array) - ArrayField_Count)[Field]
#define Array_FieldSet(Array, Field, Value)           ((usize*)(Array) - ArrayField_Count)[Field] = Value

#define ArrayLocal(Type, Name, Capacity)              Type* Name = Internal_ArrayCreate(Capacity, sizeof(Type))
#define ArrayLocal_Arena(Type, Name, Capacity, Arena) Type* Name = Internal_ArrayCreateStatic(LinearAllocator_Allocate(Arena, Array_CalculateMemRequirement(Capacity, sizeof(Type))), Capacity, sizeof(Type))

#define Array_Create(Type)                            Internal_ArrayCreate(4, sizeof(Type))
#define Array_CreateStatic(Type, Capacity, Memory)    Internal_ArrayCreateStatic(Memory, Capacity, sizeof(Type))
#define Array_Reserve(Type, Capacity)                 Internal_ArrayCreate(Capacity, sizeof(Type))

#define Array_Add(Array, Value)                       Internal_ArrayAdd((Array), &Value)
//#define Array_AddRaw(Array, Value)                    do { typeof((Value)) CONCAT(Temp, __LINE__) = (Value); (Array) = _ArrayAdd((Array), &CONCAT(Temp, __LINE__)); } while (0)
//#define Array_InsertAt(Array, Value, Index)           do { typeof((Value)) CONCAT(Temp, __LINE__) = (Value); (Array) = _ArrayInsertAt(Array, &CONCAT(Temp, __LINE__), Index); } while (0)

#define Array_For(Array)                              usize MACRO_VAR(NumElements) = Array_Num(Array); for (usize i = 0; i < MACRO_VAR(NumElements); i++)

#define Array_Remove(Array, Value)                    do { usize _Num_ = Array_Num(Array); if (_Num_ > 0) { for (usize _i_ = 0; _i_ < _Num_; _i_++) { if (Array[_i_] == Value) { Internal_ArrayRemoveAt(Array, NULL, _i_); break; } } Array_FieldSet(Array, ArrayField_Num, _Num_-1); } } while (0)

#define Array_Capacity(Array)                         Array_FieldGet(Array, ArrayField_Capacity)
#define Array_Num(Array)                              Array_FieldGet(Array, ArrayField_Num)
#define Array_Stride(Array)                           Array_FieldGet(Array, ArrayField_Stride)
#define Array_Empty(Array)                            Array_FieldSet(Array, ArrayField_Num, 0)
#define Array_SetNum(Array, Value)                    Array_FieldSet(Array, ArrayField_Num, Value)
#define Array_IsValidIndex(Array, Index)              Index < Array_FieldGet(Array, ArrayField_Capacity)
#define Array_Last(Array)                             (Array)[Array_Num((Array)) == 0 ? 0 : Array_Num((Array)) - 1]

#define SArray_Num(Array)                             CONCAT(Array, _Count)
#define SArray_Add(Array, Value)                      do { if (CONCAT(Array, _Count) < SArray_Capacity(Array)) { Array[CONCAT(Array, _Count)] = Value; CONCAT(Array, _Count)++; }} while (0)

RIFT_API NO_DISCARD void* Internal_ArrayCreate(usize Num, usize Stride);
RIFT_API NO_DISCARD void* Internal_ArrayCreateStatic(void* Memory, usize Num, usize Stride);
RIFT_API NO_DISCARD usize Array_CalculateMemRequirement(usize Num, usize Stride);

RIFT_API            void  Internal_ArrayAdd(void* Array, const void* ValuePtr);
RIFT_API            void  Internal_ArrayInsertAt(void* Array, const void* ValuePtr, usize Index);
RIFT_API            void  Array_RemoveLast(void* Array, void* ValuePtr);
RIFT_API            void  Array_RemoveAt(void* Array, void* ValuePtr, usize Index);
RIFT_API            void  Array_Destroy(void* Array);

#endif // ARRAY_H
