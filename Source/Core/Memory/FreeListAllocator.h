#pragma once

#include "EngineTypes.h"

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

RIFT_API void* FreeListAllocator_Allocate(FreeListAllocator* Allocator, usize Size, usize* OutBytesAllocated);
RIFT_API void FreeListAllocator_Free(FreeListAllocator* Allocator, void* Memory, usize* OutBytesFreed);

RIFT_API void FreeListAllocator_FreeAll(FreeListAllocator* Allocator);

RIFT_API usize FreeListAllocator_Offset(FreeListAllocator* Allocator, void* Memory);
RIFT_API void* FreeListAllocator_MemoryFromOffset(FreeListAllocator* Allocator, usize Offset);
