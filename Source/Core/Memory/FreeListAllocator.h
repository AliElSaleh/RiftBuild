#pragma once

#include "EngineTypes.h"

STRUCT(FreeListAllocator_Header)
{
	u64 BlockSize;
	u64 AlignmentPadding;
};

STRUCT(FreeListAllocator_Node)
{
	u64 BlockSize;
	struct FreeListAllocator_Node* Next;
};

STRUCT(FreeListAllocator)
{
	void* Memory;
	
	u64 TotalSize;
	u64 Allocated;
	
	FreeListAllocator_Node* Head;

    // TODO: implement
    struct FreeListAllocator* Next; // pointer to the next memory block
};

RIFT_API void FreeListAllocator_Create(FreeListAllocator* OutAllocator, u64 TotalSize, void* Memory);
RIFT_API void FreeListAllocator_Destroy(FreeListAllocator* Allocator);

RIFT_API void* FreeListAllocator_Allocate(FreeListAllocator* Allocator, u64 Size, u64* OutBytesAllocated);
RIFT_API void FreeListAllocator_Free(FreeListAllocator* Allocator, void* Memory, u64* OutBytesFreed);

RIFT_API void FreeListAllocator_FreeAll(FreeListAllocator* Allocator);

RIFT_API u64 FreeListAllocator_Offset(FreeListAllocator* Allocator, void* Memory);
RIFT_API void* FreeListAllocator_MemoryFromOffset(FreeListAllocator* Allocator, u64 Offset);
