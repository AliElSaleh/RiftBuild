#ifndef _MEMORY_H_
#define _MEMORY_H_

#ifndef UNITY_BUILD
#include "EngineTypes.h"
#endif

ENUM(EMemoryTag)
{
    MemoryTag_Unknown = 0,
    MemoryTag_Array,
    MemoryTag_Table,
    MemoryTag_Map,
    MemoryTag_LinearAllocator,
    MemoryTag_DynamicArray,
    MemoryTag_Dictionary,
    MemoryTag_RingQueue,
    MemoryTag_Engine,
    MemoryTag_Profiling,
    MemoryTag_BST,
    MemoryTag_String,
    MemoryTag_Application,
    MemoryTag_MetaReflection,
    MemoryTag_Console,
    MemoryTag_Job,
    MemoryTag_Test,
    MemoryTag_Texture,
    MemoryTag_MaterialInstance,
    MemoryTag_Renderer,
    MemoryTag_RendererBackend,
    MemoryTag_Game,
    MemoryTag_Config,
    MemoryTag_Transform,
    MemoryTag_Entity,
    MemoryTag_EntityNode,
    MemoryTag_Scene,
    
    MemoryTag_Count
};

STRUCT(MemoryRange)
{
    usize Offset;
    usize Size;
};

RIFT_API bool Memory_Initialize(void* Memory, usize MemSize, void* ScratchMemory, usize ScratchSize);
RIFT_API void Memory_Shutdown(void);

RIFT_API void* MemAlloc(usize Size, EMemoryTag Tag);// RETURN_NON_NULL;
RIFT_API void  MemFree(void* Block, EMemoryTag Tag);
RIFT_API void* MemSet(void* Destination, i32 Value, usize Size);
RIFT_API void* MemZero(void* Block, usize Size);
RIFT_API void* MemCopy(void* restrict Destination, const void* restrict Source, usize Size);
RIFT_API void* MemMove(void* restrict Destination, const void* restrict Source, usize Size);
RIFT_API bool  MemEqual(const void* Block1, const void* Block2, usize Size);

RIFT_API usize Memory_GetEngineMemoryRemaining(void);

RIFT_API bool IsValid(const void* Memory);
RIFT_API bool IsValidSlow(const void* Memory);

#endif // _MEMORY_H_
