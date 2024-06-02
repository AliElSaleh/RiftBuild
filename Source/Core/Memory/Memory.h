#pragma once

#include "Globals.h"
#include "LinearAllocator.h"

typedef enum EMemoryTag
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
} EMemoryTag;

STRUCT(MemoryRange)
{
    u64 Offset;
    u64 Size;
};

#define TEMP_SCRATCH(Name) LinearAllocator_Scratch CONCAT(Scratch_, Name) = Memory_GetScratch(); for (i32 MACRO_VAR(_i_) = 0; !MACRO_VAR(_i_); MACRO_VAR(_i_)+=1, Memory_ReleaseScratch(&CONCAT(Scratch_, Name)))

#ifdef RIFT_DEBUG_MEMORY
RIFT_API bool Memory_Initialize(void* Memory, u64 MemSize, void* DebugMemory, u64 DebugMemSize, void* Dump, void* ScratchMemory, u64 ScratchSize);
#else
RIFT_API bool Memory_Initialize(void* Memory, u64 MemSize, void* Dump, void* ScratchMemory, u64 ScratchSize);
#endif
RIFT_API void Memory_Shutdown(void);

#ifdef RIFT_DEBUG_MEMORY
RIFT_API void* MemAlloc_Debug(u64 Size);
RIFT_API void  MemFree_Debug(void* Block);
#endif

RIFT_API void* MemAlloc(u64 Size, EMemoryTag Tag) RETURN_NON_NULL;
RIFT_API void  MemFree(void* Block, EMemoryTag Tag);
RIFT_API void* MemSet(void* Destination, i32 Value, u64 Size);
RIFT_API void* MemZero(void* Block, u64 Size);
RIFT_API void* MemCopy(void* restrict Destination, const void* restrict Source, u64 Size);
RIFT_API void* MemMove(void* restrict Destination, const void* restrict Source, u64 Size);
RIFT_API bool  MemEqual(const void* Block1, const void* Block2, u64 Size);

RIFT_API LinearAllocator_Scratch Memory_GetScratch(void);
RIFT_API void Memory_ReleaseScratch(LinearAllocator_Scratch* Scratch);

RIFT_API u64 Memory_GetTotalAllocations(void);
RIFT_API u64 Memory_GetEngineMemoryRemaining(void);

RIFT_API void Memory_PrintUsageInfo(void);

RIFT_API const char* MemoryTagToString(EMemoryTag Tag);

RIFT_API bool IsValid(const void* Memory);
RIFT_API bool IsValidSlow(const void* Memory);
