// Copyright (c) 2024 Ali El Saleh

#ifndef UNITY_BUILD
#include "Memory.h"
#include "MemoryUtils.h"
#include "Platform.h"
#include "Allocators.h"
#include "Globals.h"
#include "Log.h"
#include "StringUtils.h"
#endif

internal FreeListAllocator GEngineAllocator = { 0 };
internal LinearAllocator GEngineScratchAllocator = { 0 };
internal void* GEngineMemory = NULL;
internal void* GGlobalsMemory = NULL;

static PlatformCriticalSection GCriticalSection = NULL;

#ifdef RIFT_ASAN
extern void __asan_poison_memory_region(void const volatile* addr, usize size);
extern void __asan_unpoison_memory_region(void const volatile* addr, usize size);
extern const char* __asan_default_options(void);
const char* __asan_default_options(void)
{
    return "verbosity=4:allow_user_poisoning=1:abort_on_error=0:detect_stack_use_after_return=1";
}
#endif

#ifdef RIFT_DEBUG_MEMORY
internal void* GEngineMemory_Debug = NULL;
internal FreeListAllocator GEngineAllocator_Debug = { 0 };
static PlatformCriticalSection GCriticalSection_Debug = NULL;
#endif

#ifdef RIFT_DEBUG_MEMORY
bool Memory_Initialize(void* Memory, usize MemSize, void* DebugMemory, usize DebugMemSize, void* ScratchMemory, usize ScratchSize)
#else
bool Memory_Initialize(void* Memory, usize MemSize, void* ScratchMemory, usize ScratchSize)
#endif
{
    GEngineMemory = Memory;

    #ifdef RIFT_DEBUG_MEMORY
    GEngineMemory_Debug = DebugMemory;
    #endif

    FreeListAllocator_Create(&GEngineAllocator, MemSize, GEngineMemory);
    
    if (ScratchSize > 0)
        LinearAllocator_Create(ScratchSize, ScratchMemory, &GEngineScratchAllocator);

    GGlobalsMemory = FreeListAllocator_Allocate(&GEngineAllocator, Kilobytes(16), NULL);
    InitializeGlobals(GGlobalsMemory, Kilobytes(16));

    GCriticalSection = FreeListAllocator_Allocate(&GEngineAllocator, Platform_GetCriticalSectionMemoryRequirement(), NULL);
    Platform_InitializeCriticalSection(GCriticalSection);
    
#ifdef RIFT_DEBUG_MEMORY
    FreeListAllocator_Create(&GEngineAllocator_Debug, DebugMemSize, GEngineMemory_Debug);
    
    GCriticalSection_Debug = FreeListAllocator_Allocate(&GEngineAllocator, Platform_GetCriticalSectionMemoryRequirement(), NULL);
    Platform_InitializeCriticalSection(GCriticalSection_Debug);
#endif

    return true;
}

void Memory_Shutdown(void)
{
#ifdef RIFT_DEBUG_MEMORY
    Platform_ExitCriticalSection(GCriticalSection_Debug);
    FreeListAllocator_Destroy(&GEngineAllocator_Debug);
#endif
    
    Platform_ExitCriticalSection(GCriticalSection);

#if defined(WARN_MEMLEAKS)
    struct MemoryStats SavedStats = GMemorySubsystemState->Stats;
    u64 LifetimeAllocations = GMemorySubsystemState->LifeTimeAllocations;
    u64 LifetimeFrees = GMemorySubsystemState->LifeTimeFrees;
    
    //ASSERT(GEngineAllocator.Allocated == 0)
    if (GEngineAllocator.Allocated > 0)
    {
        char Message[255] = { 0 };

        u64 MemRemaining = GEngineAllocator.Allocated;
        char Unit[4] = "XiB";
        float RemainingAmount;
        if (MemRemaining >= Gigabytes(1))
        {
            Unit[0] = 'G';
            RemainingAmount = (float)MemRemaining / (float)Gigabytes(1);
        }
        else if (MemRemaining >= Megabytes(1))
        {
            Unit[0] = 'M';
            RemainingAmount = (float)MemRemaining / (float)Megabytes(1);
        }
        else if (MemRemaining >= Kilobytes(1))
        {
            Unit[0] = 'K';
            RemainingAmount = (float)MemRemaining / (float)Kilobytes(1);
        }
        else
        {
            Unit[0] = 'B';
            Unit[1] = 0;
            RemainingAmount = (float)MemRemaining;
        }

        u64 TotalMem = GEngineAllocator.TotalSize;
        char UnitTotal[4] = "XiB";
        float TotalAmount;
        if (TotalMem >= Gigabytes(1))
        {
            UnitTotal[0] = 'G';
            TotalAmount = (float)TotalMem / (float)Gigabytes(1);
        }
        else if (TotalMem >= Megabytes(1))
        {
            UnitTotal[0] = 'M';
            TotalAmount = (float)TotalMem / (float)Megabytes(1);
        }
        else if (TotalMem >= Kilobytes(1))
        {
            UnitTotal[0] = 'K';
            TotalAmount = (float)TotalMem / (float)Kilobytes(1);
        }
        else
        {
            UnitTotal[0] = 'B';
            UnitTotal[1] = 0;
            TotalAmount = (float)TotalMem;
        }


        Platform_ConsoleWrite("\n", 2, false);
        CString_Format(Message, "----------------- %.3f%s of memory has not been freed -----------------", 255, RemainingAmount, Unit);
        Platform_ConsoleWrite(Message, 2, false);
        Platform_ConsoleWrite("\n\n", 2, false);
        Platform_ConsoleWrite(Memory_GetUsageInfo(&SavedStats), 2, false);
        Platform_ConsoleWrite("\n", 2, false);

        char Message2[255] = { 0 };
        CString_Format(Message2, "Memory Usage: %.3f%s/%.3f%s\nTotal Lifetime Allocations: %llu\nTotal Lifetime Frees: %llu\n", 255, RemainingAmount, Unit, TotalAmount, UnitTotal, LifetimeAllocations, LifetimeFrees);
        Platform_ConsoleWrite(Message2, 2, false);
    }
#endif

    LinearAllocator_Destroy(&GEngineScratchAllocator);
    FreeListAllocator_Destroy(&GEngineAllocator);
}

#ifdef RIFT_DEBUG_MEMORY
void* MemAlloc_Debug(usize Size)
{
    ASSERT(Size != 0);
    
    Platform_EnterCriticalSection(GCriticalSection_Debug);

    void* Memory = FreeListAllocator_Allocate(&GEngineAllocator_Debug, Size, NULL);

    MemZero(Memory, Size);

    Platform_ExitCriticalSection(GCriticalSection_Debug);

    return Memory;
}

void MemFree_Debug(void* Block)
{
    Platform_EnterCriticalSection(GCriticalSection_Debug);
    
    FreeListAllocator_Free(&GEngineAllocator_Debug, Block, NULL);

    Platform_ExitCriticalSection(GCriticalSection_Debug);
}
#endif

void* MemAlloc(usize Size, EMemoryTag Tag)
{
    ASSERT(Size != 0);

    Platform_EnterCriticalSection(GCriticalSection);

    #ifdef _DEBUG
    if (UNLIKELY(Tag == MemoryTag_Unknown))
    {
        LOG_WARNING("MemAlloc called using MemoryTag_Unknown. Re-class this allocation");
    }
    #endif

    usize BytesAllocated;
    void* Memory = FreeListAllocator_Allocate(&GEngineAllocator, Size, &BytesAllocated);

    /*
    StringLocal(Blah, 128);
    String_Format(&Blah, S("%lluB / %lluB\n"), GEngineAllocator.Allocated, GEngineAllocator.TotalSize);
    Platform_ConsoleWrite_CustomLength(Blah.Data, Blah.Length, 0, false);
    */

    // TODO: option to not expand like this.. or move into the freelist allocator actually
    /*
    if (Memory == GMemoryDump)
    {
        void* NextMemoryBlock = Platform_MemAllocZero(sizeof(FreeListAllocator) + GEngineAllocator.TotalSize);
        GEngineAllocator.Next = NextMemoryBlock;
        FreeListAllocator_Create(GEngineAllocator.Next, GEngineAllocator.TotalSize, (u8*)NextMemoryBlock + sizeof(FreeListAllocator));
    }
    */

    Platform_ExitCriticalSection(GCriticalSection);
    
    return Memory;
}

void MemFree(void* Block, EMemoryTag Tag)
{
    ASSERT(Block != NULL);
    //ASSERT(Block != MemoryDump());

    Platform_EnterCriticalSection(GCriticalSection);

    #ifdef _DEBUG
    if (UNLIKELY(Tag == MemoryTag_Unknown))
    {
        LOG_WARNING("MemFree called using MemoryTag_Unknown. Re-class this allocation");
    }
    #endif

    usize BytesFreed;
    FreeListAllocator_Free(&GEngineAllocator, Block, &BytesFreed);

    Platform_ExitCriticalSection(GCriticalSection);
}

void MemSet(void* Destination, i32 Value, usize Size)
{
    Platform_MemSet(Destination, Value, Size);
}

void MemZero(void* Block, usize Size)
{
    Platform_MemZero(Block, Size);
}

void MemCopy(void* restrict Destination, const void* restrict Source, usize Size)
{
    Platform_MemCopy(Destination, Source, Size);
}

void MemMove(void* restrict Destination, const void* restrict Source, usize Size)
{
    Platform_MemMove(Destination, Source, Size);
}

bool MemEqual(const void* Block1, const void* Block2, usize Size)
{
    return Platform_MemEqual(Block1, Block2, Size);
}

#if PLATFORM_WINDOWS
extern int memcmp(const void* s1, const void* s2, usize len);
bool Platform_MemEqual(const void* Block1, const void* Block2, usize Size)
{
    return memcmp((void*)Block1, (void*)Block2, Size) == 0;
}
#endif

LinearAllocator Memory_GetScratch(void)
{
    return GEngineScratchAllocator;
}

usize Memory_GetEngineMemoryRemaining(void)
{
    return GEngineAllocator.TotalSize - GEngineAllocator.Allocated;
}

usize MemoryUtils_CalculatePaddingWithHeader(usize Ptr, usize Alignment, usize HeaderSize)
{
    ASSERT(LIKELY(Alignment != 0) && ((Alignment & (Alignment-1)) == 0));

    usize p = Ptr;
    usize a = Alignment;
    usize Modulo = p & (a-1);
    usize NeededSpace = HeaderSize;
    usize Padding = 0;
    
    if (Modulo != 0)
    {
        Padding = a - Modulo;
    }
    
    if (Padding < NeededSpace)
    {
        NeededSpace -= Padding;
        
        if ((NeededSpace & (a-1)) != 0)
        {
            Padding += a * (1+(NeededSpace/a));
        }
        else
        {
            Padding += a * (NeededSpace/a);
        }
    }
    
    return Padding;
}

bool IsValid(const void* Memory)
{
    return Memory != NULL;// && Memory != MemoryDump();
}

bool IsValidSlow(const void* Memory)
{
    if (Memory == NULL)
        return false;
    
    if (Memory < GEngineMemory)
        return false;
    
    return true;
}

usize GetAligned(usize Operand, usize Granularity)
{
    return ((Operand + (Granularity-1)) & ~(Granularity-1));
}

MemoryRange GetAlignedRange(usize Offset, usize Size, usize Granularity)
{
    return (MemoryRange){GetAligned(Offset, Granularity), GetAligned(Size, Granularity)};
}




//////////////////////////////////


// Allocators


//////////////////////////////////



void LinearAllocator_Create(usize TotalSize, void* Memory, LinearAllocator* OutAllocator)
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

    #if RIFT_ASAN
    __asan_poison_memory_region(OutAllocator->Memory, TotalSize);
    #endif
}

void LinearAllocator_Destroy(LinearAllocator* Allocator)
{
    if (IsValid(Allocator->Memory))
    {
        if (Allocator->bOwnsMemory)
        {
            #if RIFT_ASAN
            __asan_unpoison_memory_region(Allocator->Memory, Allocator->TotalSize);
            #endif

            MemFree(Allocator->Memory, MemoryTag_LinearAllocator);
        }
        else
        {
            #if RIFT_ASAN
            __asan_poison_memory_region(Allocator->Memory, Allocator->TotalSize);
            #endif
        }
    }

    Allocator->Memory = NULL;
    Allocator->TotalSize = 0;
    Allocator->Allocated = 0;
    Allocator->bOwnsMemory = false;
}

void* LinearAllocator_Allocate(LinearAllocator* Allocator, usize Size)
{
    ASSERT(Size > 0);
    ASSERT(Allocator->Allocated < Allocator->TotalSize);
    ASSERT(Allocator->Allocated + Size <= Allocator->TotalSize);
    
    void* Block = ((u8*)Allocator->Memory) + Allocator->Allocated;

    const usize Alignment = 3;

    if (Allocator->bAlignMemory)
    {
        Allocator->Allocated += ((Size + Alignment) & ~Alignment); // @Enhancement: Make more robust if the memory given was unaligned to begin with
    }
    else
    {
        Allocator->Allocated += Size;
    }

    #if RIFT_ASAN
    __asan_unpoison_memory_region(Block, Size);
    #endif

    return Block;
}

void* LinearAllocator_AllocateAll(LinearAllocator* Allocator)
{
    Allocator->Allocated = Allocator->TotalSize;

    /*
    #if RIFT_ASAN
    __asan_unpoison_memory_region(Allocator->Memory, Allocator->TotalSize);
    #endif
    */

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

    /*
    #if RIFT_ASAN
    __asan_poison_memory_region(Allocator->Memory, Allocator->TotalSize);
    #endif
    */
}

void* LinearAllocator_MemoryHead(LinearAllocator* Allocator)
{
    return ((u8*)Allocator->Memory) + Allocator->Allocated;
}

void LinearAllocator_Reset(LinearAllocator* Allocator, usize Position)
{
    ASSERT(Position <= Allocator->TotalSize);
    ASSERT(Position <= Allocator->Allocated);
    
    Allocator->Allocated = Position;
    
    #if RIFT_ASAN
    __asan_poison_memory_region(LinearAllocator_MemoryHead(Allocator), Allocator->TotalSize - Position);
    #endif
}

/*
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
    usize NumBytesUsed = Scratch->Allocator->Allocated - Scratch->StartPosition;
    if (NumBytesUsed > 0 && NumBytesUsed <= Scratch->Allocator->TotalSize)
    {
        Scratch->Allocator->Allocated = Scratch->StartPosition;

        void* Head = LinearAllocator_MemoryHead(Scratch->Allocator);
        MemZero(Head, NumBytesUsed);

        #if RIFT_ASAN
        __asan_poison_memory_region(Head, NumBytesUsed);
        #endif
    }
}
*/


// ===================================================


#define DEFAULT_FREE_LIST_ALLOCATOR_ALIGNMENT 64

internal FreeListAllocator_Node* Internal_FindFirstFit(FreeListAllocator* Allocator, usize Size, usize* OutPadding, FreeListAllocator_Node** OutPrevNode)
{
    ASSERT(Allocator->Head != NULL);
    
    FreeListAllocator_Node* Node = Allocator->Head;
    FreeListAllocator_Node* PrevNode = NULL;
    
    usize Padding = 0;
    
    while (Node != NULL)
    {
        ASSERT(PrevNode != Node);
        
        Padding = MemoryUtils_CalculatePaddingWithHeader((usize)Node, DEFAULT_FREE_LIST_ALLOCATOR_ALIGNMENT, sizeof(FreeListAllocator_Header));
        
        usize RequiredSize = Size + Padding;
        
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
    
    if (PrevNode && FreeNode->Next)
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

void FreeListAllocator_Create(FreeListAllocator* OutAllocator, usize TotalSize, void* Memory)
{
    OutAllocator->Memory = Memory;
    OutAllocator->TotalSize = TotalSize;

    #if RIFT_ASAN
    __asan_poison_memory_region(OutAllocator->Memory, TotalSize);
    #endif

    FreeListAllocator_FreeAll(OutAllocator);
}

void FreeListAllocator_Destroy(FreeListAllocator* Allocator)
{
    #if RIFT_ASAN
    __asan_unpoison_memory_region(Allocator->Memory, Allocator->TotalSize);
    #endif

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

usize FreeListAllocator_Offset(FreeListAllocator* Allocator, void* Memory)
{
    // Ensure that the memory passed in within this allocator's memory address range
    // If this assertion fails, there is a problem with the memory passed in,
    // as it falls outside the range of this allocator
    ASSERT((u8*)Memory >= (u8*)Allocator->Memory);
    ASSERT((u8*)Memory < (((u8*)Allocator->Memory) + Allocator->TotalSize));
    
    return (usize)((u8*)Memory - (u8*)Allocator->Memory);
}

void* FreeListAllocator_MemoryFromOffset(FreeListAllocator* Allocator, usize Offset)
{
    ASSERT(Offset > sizeof(FreeListAllocator_Header));
    ASSERT(Offset < Allocator->TotalSize);
    
    return (void*)((u8*)Allocator->Memory + Offset);
}

void* FreeListAllocator_Allocate(FreeListAllocator* Allocator, usize Size, usize* OutBytesAllocated)
{
    if (OutBytesAllocated)
        *OutBytesAllocated = 0;
    
    // Allocation size must be at least the size of a node for the free list to work properly
    if (Size < sizeof(FreeListAllocator_Node))
    {
        Size = sizeof(FreeListAllocator_Node);
    }
    
    usize Padding = 0;
    FreeListAllocator_Node* PrevNode = NULL;
    FreeListAllocator_Node* Node = Internal_FindFirstFit(Allocator, Size, &Padding, &PrevNode);
    
    // todo: uncomment, after testing, think we still need this assert anyway
    //ASSERT_MSG(LIKELY(Node != NULL), "Out of memory");
    if (UNLIKELY(Node == NULL))
    {
        //LOG_ERROR("Out of memory! Further allocations will now point to the memory dump!");
        LOG_FATAL("Out of memory!");
        return NULL;// MemoryDump(); // point to the memory dump to prevent NULL crashes
    }
    
    usize AlignmentPadding = Padding - sizeof(FreeListAllocator_Header);
    usize RequiredSpace = Size + Padding;
    usize Remaining = Node->BlockSize - RequiredSpace;

    #if RIFT_ASAN
    __asan_unpoison_memory_region(Node, AlignmentPadding + sizeof(FreeListAllocator_Node) + RequiredSpace);
    #endif

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
        }

        #if RIFT_ASAN
        __asan_poison_memory_region(NewFreeNode, NewFreeNode->BlockSize);
        #endif
    }

    if (!IsValid(PrevNode))	
    {
        Allocator->Head = Node->Next;
    }
    else
    {
        PrevNode->Next = Node->Next;
    }
    
    FreeListAllocator_Header* Header = (FreeListAllocator_Header*)((u8*)Node + AlignmentPadding);
    Header->BlockSize = RequiredSpace;
    Header->AlignmentPadding = AlignmentPadding;
    
    Allocator->Allocated += RequiredSpace;
    
    if (OutBytesAllocated)
        *OutBytesAllocated = RequiredSpace;
    
    return (void*)((u8*)Header + sizeof(FreeListAllocator_Header));
}

void FreeListAllocator_Free(FreeListAllocator* Allocator, void* Memory, usize* OutBytesFreed)
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
    usize Padding = MemoryUtils_CalculatePaddingWithHeader((usize)Header, DEFAULT_FREE_LIST_ALLOCATOR_ALIGNMENT, sizeof(FreeListAllocator_Header));
    usize BlockSizeNoPadding = Header->BlockSize - Padding - Header->AlignmentPadding;
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
    
    usize BlockSize = Header->BlockSize;
    
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

    #if RIFT_ASAN
    __asan_poison_memory_region(FreeNode, BlockSize);
    #endif

    Internal_FreeListAllocator_Coalesce(Allocator, PrevNode, FreeNode);
}


/////////////////////////////////////

// Dynamic Array

/////////////////////////////////////


#ifndef UNITY_BUILD
#include "Array.h"
#endif

void* _ArrayCreate(usize Num, usize Stride)
{
    usize HeaderSize = ArrayField_Count * sizeof(usize);
    usize ArraySize = Num * Stride;
    
    usize* NewArray = (usize*)MemAlloc(HeaderSize + ArraySize, MemoryTag_DynamicArray);
    
    NewArray[ArrayField_Capacity] = Num;
    NewArray[ArrayField_Num] = 0;
    NewArray[ArrayField_Stride] = Stride;
    NewArray[ArrayField_OwnsMemory] = 1;
    
    return (void*)(NewArray + ArrayField_Count);
}

usize _ArrayCalculateMemRequirement(usize Num, usize Stride)
{
    usize HeaderSize = ArrayField_Count * sizeof(usize);
    usize Alignment = 3;
    usize ArraySize = Num * ((Stride + Alignment) & ~Alignment);
    
    return HeaderSize + ArraySize;
}

void* _ArrayCreateStatic(void* Memory, usize Num, usize Stride)
{
    usize* NewArray = (usize*)Memory;

    NewArray[ArrayField_Capacity] = Num;
    NewArray[ArrayField_Num] = 0;
    NewArray[ArrayField_Stride] = Stride;
    NewArray[ArrayField_OwnsMemory] = 0;

    return (void*)(NewArray + ArrayField_Count);
}

void _ArrayDestroy(void* Array)
{
    usize* Header = (usize*)Array - ArrayField_Count;
    
    if (Header[ArrayField_OwnsMemory] == 1)
    {
        MemFree(Header, MemoryTag_DynamicArray);
    }
}

internal void* _ArrayResize(void* Array)
{
    const usize* Header = (usize*)Array - ArrayField_Count;

    if (Header[ArrayField_OwnsMemory] == 1)
    {
        usize Num = Array_Num(Array);
        usize Stride = Array_Stride(Array);
        
        void* NewArray = _ArrayCreate(Array_Capacity(Array) * ARRAY_RESIZE_FACTOR, Stride);

        MemCopy(NewArray, Array, Num * Stride);

        _ArrayFieldSet(Array, ArrayField_Num, Num);
        _ArrayDestroy(Array);
        
        return NewArray;
    }

    return Array;
}

void _ArrayAdd(void* Array, const void* ValuePtr)
{
    usize Num = Array_Num(Array);
    usize Stride = Array_Stride(Array);

    // Resize if needed
    if (Num >= Array_Capacity(Array))
    {
        Array = _ArrayResize(Array);

        // this array is static, thus it cannot be resized, exit out and don't mutate anything
        const usize* Header = (usize*)Array - ArrayField_Count;
        if (Header[ArrayField_OwnsMemory] != 1)
        {
            #if DEVELOPER
            LOG_WARNING("Fixed size TArray is full, cannot resize or add more elements because it does not own the memory.");
            #endif

            return;
        }
    }
    
    usize Addr = (usize)Array;
    Addr += (Num * Stride); // go to end of array

    MemCopy((void*)Addr, ValuePtr, Stride);

    _ArrayFieldSet(Array, ArrayField_Num, Num+1);
}

void _ArrayInsertAt(void* Array, const void* ValuePtr, usize Index)
{
    usize Num = Array_Num(Array);
    usize Stride = Array_Stride(Array);

    if (Index >= Num)
    {
        LOG_WARNING("Index outside the bounds of the array. Num: %llu | Index: %llu", Num, Index);
        return;
    }

    // Resize if needed
    if (Num >= Array_Capacity(Array))
    {
        Array = _ArrayResize(Array);

        // this array is static, thus it cannot be resized, exit out and don't mutate anything
        const usize* Header = (usize*)Array - ArrayField_Count;
        if (Header[ArrayField_OwnsMemory] != 1)
        {
            #if DEVELOPER
            LOG_WARNING("Fixed size TArray is full, cannot resize or add more elements because it does not own the memory.");
            #endif

            return;
        }
    }

    usize Addr = (usize)Array;

    // If not last element, snip out the entry and copy the rest outward
    if (Index != Num-1)
    {
        MemCopy((void*)(Addr + ((Index + 1) * Stride)),
                (void*)(Addr + (Index * Stride)),
                Stride * (Num - Index));
    }

    MemCopy((void*)(Addr + (Index * Stride)), ValuePtr, Stride);

    _ArrayFieldSet(Array, ArrayField_Num, Num+1);
}

void _ArrayRemoveLast(void* Array, void* ValuePtr)
{
    usize Num = Array_Num(Array);
    usize Stride = Array_Stride(Array);

    usize Addr = (usize)Array;
    Addr += ((Num-1) * Stride); // go to 2nd last element

    MemCopy(ValuePtr, (void*)Addr, Stride);

    _ArrayFieldSet(Array, ArrayField_Num, Num-1);
}

void _ArrayRemoveAt(void* Array, void* ValuePtr, usize Index)
{
    usize Num = Array_Num(Array);
    usize Stride = Array_Stride(Array);

    if (Index >= Num)
    {
        LOG_WARNING("Index outside the bounds of the array. Num: %llu | Index: %llu", Num, Index);
        return;
    }

    usize Addr = (usize)Array;

    if (ValuePtr)
    {
        MemCopy(ValuePtr, (void*)(Addr + (Index * Stride)), Stride);
    }

    // If not last element, snip out the entry and copy the rest inward
    if (Index != Num-1)
    {
        volatile void* Dest = (void*)(Addr + (Index * Stride));
        volatile void* Src  = (void*)(Addr + ((Index + 1) * Stride));

        MemMove((void*)Dest, (void*)Src, Stride * (Num - Index));		
    }

    _ArrayFieldSet(Array, ArrayField_Num, Num-1);
}
