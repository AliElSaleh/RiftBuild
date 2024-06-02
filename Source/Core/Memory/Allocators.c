
#include "LinearAllocator.h"

#include "Memory.h"
#include "MemoryUtils.h"

#include "Log.h"

void LinearAllocator_Create(u64 TotalSize, void* Memory, LinearAllocator* OutAllocator)
{
    ASSERT(TotalSize > 0);
    ASSERT(OutAllocator != NULL);
    
    OutAllocator->TotalSize = TotalSize;
    OutAllocator->Allocated = 0;
    OutAllocator->bOwnsMemory = !IsValid(Memory);
    OutAllocator->bAlignMemory = true;

    if (Memory)
    {
        OutAllocator->Memory = Memory;
    }
    else
    {
        OutAllocator->Memory = MemAlloc(TotalSize, MemoryTag_LinearAllocator);
    }
}

void LinearAllocator_Destroy(LinearAllocator* Allocator)
{
    if (Allocator->bOwnsMemory && IsValid(Allocator->Memory))
    {
        MemFree(Allocator->Memory, MemoryTag_LinearAllocator);
    }

    Allocator->Memory = nullptr;
    Allocator->TotalSize = 0;
    Allocator->Allocated = 0;
    Allocator->bOwnsMemory = false;
}

void* LinearAllocator_Allocate(LinearAllocator* Allocator, u64 Size)
{
    ASSERT(Size > 0);
    ASSERT(Allocator->Allocated < Allocator->TotalSize);
    ASSERT(Allocator->Allocated + Size <= Allocator->TotalSize);
    
    void* Block = ((u8*)Allocator->Memory) + Allocator->Allocated;

    const u64 Alignment = 3;

    if (Allocator->bAlignMemory)
    {
        Allocator->Allocated += ((Size + Alignment) & ~Alignment); // @Enhancement: Make more robust if the memory given was unaligned to begin with
    }
    else
    {
        Allocator->Allocated += Size;
    }
    
    return Block;
}

void* LinearAllocator_AllocateAll(LinearAllocator* Allocator)
{
    Allocator->Allocated = Allocator->TotalSize;
    return Allocator->Memory;
}

void LinearAllocator_FreeAll(LinearAllocator* Allocator, bool bZeroMemory)
{
    if (Allocator->Allocated > 0)
    {
        Allocator->Allocated = 0;
        
        if (bZeroMemory)
            MemZero(Allocator->Memory, Allocator->TotalSize);
    }
}

void* LinearAllocator_MemoryHead(LinearAllocator* Allocator)
{
    return ((u8*)Allocator->Memory) + Allocator->Allocated;
}

LinearAllocator_Scratch LinearAllocator_GetScratch(LinearAllocator* Allocator)
{
    return (LinearAllocator_Scratch)
    {
        .Allocator = Allocator,
        .StartPosition = Allocator->Allocated
    };
}

void LinearAllocator_GetScratchInline(LinearAllocator* Allocator, LinearAllocator_Scratch* OutScratch)
{
    *OutScratch = LinearAllocator_GetScratch(Allocator);
}

void LinearAllocator_ReleaseScratch(LinearAllocator_Scratch* Scratch)
{
    u64 NumBytesUsed = Scratch->Allocator->Allocated - Scratch->StartPosition;
    if (NumBytesUsed > 0 && NumBytesUsed <= Scratch->Allocator->TotalSize)
    {
        Scratch->Allocator->Allocated = Scratch->StartPosition;
        MemZero(LinearAllocator_MemoryHead(Scratch->Allocator), NumBytesUsed);
    }
}


// ===================================================


#include "FreeListAllocator.h"

#define DEFAULT_FREE_LIST_ALLOCATOR_ALIGNMENT 64

internal FreeListAllocator_Node* Internal_FindFirstFit(FreeListAllocator* Allocator, u64 Size, u64* OutPadding, FreeListAllocator_Node** OutPrevNode)
{
    ASSERT(Allocator->Head != NULL);
    
    FreeListAllocator_Node* Node = Allocator->Head;
    FreeListAllocator_Node* PrevNode = NULL;
    
    u64 Padding = 0;
    
    while (Node != NULL)
    {
        ASSERT(PrevNode != Node);
        
        Padding = MemoryUtils_CalculatePaddingWithHeader((u64)Node, DEFAULT_FREE_LIST_ALLOCATOR_ALIGNMENT, sizeof(FreeListAllocator_Header));
        
        u64 RequiredSize = Size + Padding;
        
        // does it fit?
        if (Node->BlockSize >= RequiredSize)
        {
            break;
        }
        
        PrevNode = Node;
        Node = Node->Next;
    }
    
    if (OutPadding)
        *OutPadding = Padding;
    
    if (PrevNode)
        *OutPrevNode = PrevNode;
    
    return Node;
}

internal void Internal_FreeListAllocator_NodeRemove(FreeListAllocator_Node** HeadPtr, FreeListAllocator_Node* PrevNode, FreeListAllocator_Node* DelNode)
{
    if (!IsValid(PrevNode))
    {
        *HeadPtr = DelNode->Next;
    }
    else
    {
        PrevNode->Next = DelNode->Next;
    }
    
    /*
    DelNode->BlockSize = 0;
    DelNode->Next = NULL;
    */
}

internal void Internal_FreeListAllocator_Coalesce(FreeListAllocator* Allocator, FreeListAllocator_Node* PrevNode, FreeListAllocator_Node* FreeNode)
{
    if (FreeNode->Next)
    {
        if ((void*)((u8*)FreeNode + FreeNode->BlockSize) == FreeNode->Next)
        {
            FreeNode->BlockSize += FreeNode->Next->BlockSize;
            Internal_FreeListAllocator_NodeRemove(&Allocator->Head, FreeNode, FreeNode->Next);
        }
    }
    
    if (PrevNode)
    {
        if (PrevNode->Next != NULL)
        {
            if ((void*)((u8*)PrevNode + PrevNode->BlockSize) == FreeNode)
            {
                PrevNode->BlockSize += FreeNode->Next->BlockSize;
                Internal_FreeListAllocator_NodeRemove(&Allocator->Head, PrevNode, FreeNode);
            }
        }
    }
}

void FreeListAllocator_Create(FreeListAllocator* OutAllocator, u64 TotalSize, void* Memory)
{
    OutAllocator->Memory = Memory;
    OutAllocator->TotalSize = TotalSize;

    FreeListAllocator_FreeAll(OutAllocator);
}

void FreeListAllocator_Destroy(FreeListAllocator* Allocator)
{
    MemZero(Allocator->Memory, Allocator->TotalSize);
    
    Allocator->Memory = NULL;
    Allocator->TotalSize = 0;
    Allocator->Allocated = 0;
    Allocator->Head = NULL;
}

void FreeListAllocator_FreeAll(FreeListAllocator* Allocator)
{
    Allocator->Allocated = 0;
    
    FreeListAllocator_Node* FirstNode = (FreeListAllocator_Node*)Allocator->Memory;
    
    FirstNode->BlockSize = Allocator->TotalSize;
    FirstNode->Next = NULL;
    
    Allocator->Head = FirstNode;
}

u64 FreeListAllocator_Offset(FreeListAllocator* Allocator, void* Memory)
{
    // Ensure that the memory passed in within this allocator's memory address range
    // If this assertion fails, there is a problem with the memory passed in,
    // as it falls outside the range of this allocator
    ASSERT((u8*)Memory >= (u8*)Allocator->Memory);
    ASSERT((u8*)Memory < (((u8*)Allocator->Memory) + Allocator->TotalSize));
    
    return (u64)((u8*)Memory - (u8*)Allocator->Memory);
}

void* FreeListAllocator_MemoryFromOffset(FreeListAllocator* Allocator, u64 Offset)
{
    ASSERT(Offset > sizeof(FreeListAllocator_Header));
    ASSERT(Offset < Allocator->TotalSize);
    
    return (void*)((u8*)Allocator->Memory + Offset);
}

void* FreeListAllocator_Allocate(FreeListAllocator* Allocator, u64 Size, u64* OutBytesAllocated)
{
    if (OutBytesAllocated)
        *OutBytesAllocated = 0;
    
    // Allocation size must be at least the size of a node for the free list to work properly
    if (Size < sizeof(FreeListAllocator_Node))
    {
        Size = sizeof(FreeListAllocator_Node);
    }
    
    u64 Padding = 0;
    FreeListAllocator_Node* PrevNode = NULL;
    FreeListAllocator_Node* Node = Internal_FindFirstFit(Allocator, Size, &Padding, &PrevNode);
    
    // todo: uncomment, after testing, think we still need this assert anyway
    //ASSERT_MSG(LIKELY(Node != NULL), "Out of memory");
    if (UNLIKELY(Node == NULL))
    {
        LOG_ERROR("Out of memory! Further allocations will now point to the memory dump!");
        return MemoryDump(); // point to the memory dump to prevent NULL crashes
    }
    
    //u64 AlignmentPadding = 0;
    u64 AlignmentPadding = Padding - sizeof(FreeListAllocator_Header);
    u64 RequiredSpace = Size + Padding;
    u64 Remaining = Node->BlockSize - RequiredSpace;
    
    if (Remaining > 0)
    {
        FreeListAllocator_Node* NewFreeNode = (FreeListAllocator_Node*)((u8*)Node + RequiredSpace);
        NewFreeNode->BlockSize = Remaining;

        if (!IsValid(Node->Next))
        {
            Node->Next = NewFreeNode;
            NewFreeNode->Next = NULL;
        }
        else
        {
            NewFreeNode->Next = Node->Next;
            Node->Next = NewFreeNode;

            /*
            FreeListAllocator_Node* PrevNextNode = Node->Next;
            // already in list
            if (NewFreeNode != Node && NewFreeNode != Node->Next)
            {
                NewFreeNode->Next = Node->Next;
                Node->Next = NewFreeNode;
            }
            else
            {
                Node->Next = NewFreeNode;
            }
             */
        }
    }

    if (!IsValid(PrevNode))	
    {
        Allocator->Head = Node->Next;
    }
    else
    {
        PrevNode->Next = Node->Next;
    }
    
    //Internal_FreeListAllocator_NodeRemove(&Allocator->Head, PrevNode, Node);

    FreeListAllocator_Header* Header = (FreeListAllocator_Header*)((u8*)Node + AlignmentPadding);
    Header->BlockSize = RequiredSpace;
    Header->AlignmentPadding = AlignmentPadding;
    
    Allocator->Allocated += RequiredSpace;
    
    if (OutBytesAllocated)
        *OutBytesAllocated = RequiredSpace;
    
    return (void*)((u8*)Header + sizeof(FreeListAllocator_Header));
}

void FreeListAllocator_Free(FreeListAllocator* Allocator, void* Memory, u64* OutBytesFreed)
{
    if (OutBytesFreed)
        *OutBytesFreed = 0;
    
    if (!IsValid(Memory))
        return;

    FreeListAllocator_Header* Header = (FreeListAllocator_Header*)((u8*)Memory - sizeof(FreeListAllocator_Header));

    // Ensure that the memory passed in within this allocator's memory address range
    // If this assertion fails, there is a problem with the memory passed in,
    // as it falls outside the range of this allocator
    ASSERT((u8*)Header >= (u8*)Allocator->Memory);
    ASSERT((u8*)Header < (((u8*)Allocator->Memory) + Allocator->TotalSize));

    // zero the memory
    u64 Padding = MemoryUtils_CalculatePaddingWithHeader((u64)Header, DEFAULT_FREE_LIST_ALLOCATOR_ALIGNMENT, sizeof(FreeListAllocator_Header));
    u64 BlockSizeNoPadding = Header->BlockSize - Padding - Header->AlignmentPadding;
    MemZero(Memory, BlockSizeNoPadding);

    // Detect if the memory passed in was already freed
    {
        FreeListAllocator_Node* Node = Allocator->Head;

        const FreeListAllocator_Node* FoundNode = NULL;
        while (Node != NULL)
        {
            if (Node == (FreeListAllocator_Node*)Header)
            {
                FoundNode = Node;
                break;
            }
            
            Node = Node->Next;
        }

        //ASSERT_MSG(!FoundNode, "Double free");
        if (FoundNode)
        {
            return;
        }
    }
    
    u64 BlockSize = Header->BlockSize;
    
    if (OutBytesFreed)
        *OutBytesFreed = BlockSize;
    
    FreeListAllocator_Node* FreeNode = (FreeListAllocator_Node*)Header;
    FreeNode->BlockSize = Header->BlockSize + Header->AlignmentPadding;
    FreeNode->Next = NULL;
    
    FreeListAllocator_Node* Node = Allocator->Head;
    FreeListAllocator_Node* PrevNode = NULL;
    
    while (Node != NULL)
    {
        if (FreeNode < Node)
        {
            if (!PrevNode)
            {
                if (Allocator->Head)
                {
                    FreeNode->Next = Allocator->Head;
                }
                else
                {
                    Allocator->Head = FreeNode;
                }
                
                //FreeNode->Next = Allocator->Head;
                //Allocator->Head = FreeNode;
            }
            else
            {
                if (!IsValid(PrevNode->Next))
                {
                    PrevNode->Next = FreeNode;
                    FreeNode->Next = NULL;
                }
                else
                {
                    FreeNode->Next = PrevNode->Next;
                    PrevNode->Next = FreeNode;
                }
            }
            
            break;
        }
        
        PrevNode = Node;
        Node = Node->Next;
    }
    
    Allocator->Allocated -= BlockSize;

    /*
    if (FreeNode->Next)
    {
        if ((void*)((u8*)FreeNode + FreeNode->BlockSize) == FreeNode->Next)
        {
            FreeNode->BlockSize += FreeNode->Next->BlockSize;
            Internal_FreeListAllocator_NodeRemove(&Allocator->Head, FreeNode, FreeNode->Next);
        }
    }

    if (PrevNode)
    {
        if (PrevNode->Next != NULL)
        {
            if ((void*)((u8*)PrevNode + PrevNode->BlockSize) == FreeNode)
            {
                PrevNode->BlockSize += FreeNode->BlockSize;
                
                if (FreeNode->Next)
                {
                    if ((void*)((u8*)FreeNode + FreeNode->BlockSize) == FreeNode->Next)
                    {
                        FreeNode->BlockSize += FreeNode->Next->BlockSize;
                        Internal_FreeListAllocator_NodeRemove(&Allocator->Head, FreeNode, FreeNode->Next);
                    }
                }
                
                Internal_FreeListAllocator_NodeRemove(&Allocator->Head, PrevNode, FreeNode);
            }
        }
    }
    else
    {
        Allocator->Head = FreeNode;
    }
    */
    
    Internal_FreeListAllocator_Coalesce(Allocator, PrevNode, FreeNode);
}
