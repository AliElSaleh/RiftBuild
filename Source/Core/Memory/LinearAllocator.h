#pragma once

#include "Globals.h"

STRUCT(LinearAllocator)
{
    usize TotalSize;
    usize Allocated;
    bool bOwnsMemory;
    bool bAlignMemory;
    
    void* Memory;
};

STRUCT(LinearAllocator_Scratch)
{
    LinearAllocator* Allocator;
    usize StartPosition;
};

#define SCRATCH(Allocator, Name) LinearAllocator_Scratch Name = {0}; DEFER(LinearAllocator_GetScratchInline(Allocator, &(Name)), LinearAllocator_ReleaseScratch(&(Name)))

RIFT_API void LinearAllocator_Create(usize TotalSize, void* Memory, LinearAllocator* OutAllocator);
RIFT_API void LinearAllocator_Destroy(LinearAllocator* Allocator);

RIFT_API void* LinearAllocator_Allocate(LinearAllocator* Allocator, usize Size);
RIFT_API void* LinearAllocator_AllocateAll(LinearAllocator* Allocator);
RIFT_API void LinearAllocator_FreeAll(LinearAllocator* Allocator, bool bZeroMemory);

RIFT_API void* LinearAllocator_MemoryHead(LinearAllocator* Allocator);

RIFT_API LinearAllocator_Scratch LinearAllocator_GetScratch(LinearAllocator* Allocator);
RIFT_API void LinearAllocator_GetScratchInline(LinearAllocator* Allocator, LinearAllocator_Scratch* OutScratch);
RIFT_API void LinearAllocator_ReleaseScratch(LinearAllocator_Scratch* Scratch);
