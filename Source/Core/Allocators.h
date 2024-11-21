#ifndef ALLOCATORS_H
#define ALLOCATORS_H

#ifndef UNITY_BUILD
#include "EngineTypes.h"
#endif

STRUCT(LinearAllocator)
{
    void* Memory;

    usize TotalSize;
    usize Allocated;

    b32 bOwnsMemory;
    b32 bAlignMemory;
};

/*
STRUCT(LinearAllocator_Scratch)
{
    LinearAllocator* Allocator;
    usize StartPosition;
};
*/

//#define SCRATCH(Allocator, Name) LinearAllocator_Scratch Name = {0}; DEFER(LinearAllocator_GetScratchInline(Allocator, &(Name)), LinearAllocator_ReleaseScratch(&(Name)))

RIFT_API void LinearAllocator_Create(usize TotalSize, void* Memory, LinearAllocator* OutAllocator);
RIFT_API void LinearAllocator_Destroy(LinearAllocator* Allocator);

RIFT_API NO_DISCARD void* LinearAllocator_Allocate(LinearAllocator* Allocator, usize Size);

RIFT_API NO_DISCARD void* LinearAllocator_MemoryHead(LinearAllocator* Allocator);
RIFT_API            void  LinearAllocator_Reset(LinearAllocator* Allocator, usize Position);


// ================================================================================


STRUCT(FreeListAllocator_Header)
{
	usize BlockSize;
	usize AlignmentPadding;
};

STRUCT(FreeListAllocator_Node)
{
	usize BlockSize;
	struct FreeListAllocator_Node* Next;
};

STRUCT(FreeListAllocator)
{
	void* Memory;
	
	usize TotalSize;
	usize Allocated;
	
	FreeListAllocator_Node* Head;

    // TODO: implement
    struct FreeListAllocator* Next; // pointer to the next memory block
};

RIFT_API void FreeListAllocator_Create(FreeListAllocator* OutAllocator, usize TotalSize, void* Memory);
RIFT_API void FreeListAllocator_Destroy(FreeListAllocator* Allocator);

RIFT_API NO_DISCARD void* FreeListAllocator_Allocate(FreeListAllocator* Allocator, usize Size, usize* OutBytesAllocated);
RIFT_API void FreeListAllocator_Free(FreeListAllocator* Allocator, void* Memory, usize* OutBytesFreed);

RIFT_API void FreeListAllocator_FreeAll(FreeListAllocator* Allocator);

RIFT_API usize FreeListAllocator_Offset(FreeListAllocator* Allocator, void* Memory);
RIFT_API NO_DISCARD void* FreeListAllocator_MemoryFromOffset(FreeListAllocator* Allocator, usize Offset);

#endif // ALLOCATORS_H
