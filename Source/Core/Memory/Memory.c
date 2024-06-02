#include "Memory.h"
#include "MemoryUtils.h"

#include "Platform/Platform.h"
#include "Memory/FreeListAllocator.h"
#include "Log.h"
#include "String/StringUtils.h"

#include "Math/Math.h"

#ifndef RIFT_ASAN
#include "memory/libmemory.c"
#endif

#ifdef META_GENERATED
#include "Memory.generated.c"
#endif

#define MAX_MEM_INFO_BUFFER_LENGTH 2048

struct MemoryStats
{
    u64 TotalAllocated;
    u64 TaggedAllocations[MemoryTag_Count];
    u64 TaggedLifetimeAllocation[MemoryTag_Count];
};

typedef struct MemorySubsystemState
{
    u64 Allocations;
    u64 LifeTimeAllocations;
    u64 LifeTimeFrees;
    struct MemoryStats Stats;
} MemorySubsystemState;

internal MemorySubsystemState* GMemorySubsystemState = NULL;
internal FreeListAllocator GEngineAllocator = { 0 };
internal LinearAllocator GEngineScratchAllocator = { 0 };
internal void* GEngineMemory = NULL;
internal void* GMemoryDump = NULL;
internal void* GGlobalsMemory = NULL;

static PlatformCriticalSection GCriticalSection = NULL;

#ifdef RIFT_DEBUG_MEMORY
internal void* GEngineMemory_Debug = NULL;
internal FreeListAllocator GEngineAllocator_Debug = { 0 };
static PlatformCriticalSection GCriticalSection_Debug = NULL;
#endif

void* nullptr_z = NULL;

internal char* Memory_GetUsageInfo(struct MemoryStats* Stats);

void* MemoryDump(void)
{
    return GMemoryDump;
}

#ifdef RIFT_DEBUG_MEMORY
bool Memory_Initialize(void* Memory, u64 MemSize, void* DebugMemory, u64 DebugMemSize, void* Dump, void* ScratchMemory, u64 ScratchSize)
#else
bool Memory_Initialize(void* Memory, u64 MemSize, void* Dump, void* ScratchMemory, u64 ScratchSize)
#endif
{
    GEngineMemory = Memory;
    GMemoryDump = Dump;
    nullptr_z = Dump;

    #ifdef RIFT_DEBUG_MEMORY
    GEngineMemory_Debug = DebugMemory;
    #endif

    FreeListAllocator_Create(&GEngineAllocator, MemSize, GEngineMemory);
    
    GMemorySubsystemState = FreeListAllocator_Allocate(&GEngineAllocator, sizeof(MemorySubsystemState), NULL);
    GMemorySubsystemState->Allocations = 0;

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
void* MemAlloc_Debug(u64 Size)
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

void* MemAlloc(u64 Size, EMemoryTag Tag)
{
    ASSERT(Size != 0);

    Platform_EnterCriticalSection(GCriticalSection);

    #ifdef _DEBUG
    if (UNLIKELY(Tag == MemoryTag_Unknown))
    {
        LOG_WARNING("MemAlloc called using MemoryTag_Unknown. Re-class this allocation");
    }
    #endif

    u64 BytesAllocated;
    void* Memory = FreeListAllocator_Allocate(&GEngineAllocator, Size, &BytesAllocated);

    /*
    StringLocal(Blah, 128);
    String_Format(&Blah, S("%lluB / %lluB\n"), 128, GEngineAllocator.Allocated, GEngineAllocator.TotalSize);
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

    #ifdef _DEBUG
    GMemorySubsystemState->Stats.TotalAllocated += BytesAllocated;
    GMemorySubsystemState->Stats.TaggedAllocations[Tag] += BytesAllocated;
    GMemorySubsystemState->Stats.TaggedLifetimeAllocation[Tag]++;
    GMemorySubsystemState->Allocations++;
    GMemorySubsystemState->LifeTimeAllocations++;
    #endif

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

    u64 BytesFreed;
    FreeListAllocator_Free(&GEngineAllocator, Block, &BytesFreed);

    #ifdef _DEBUG
    if (LIKELY(BytesFreed > 0))
    {
        GMemorySubsystemState->Stats.TotalAllocated -= BytesFreed;
        GMemorySubsystemState->Stats.TaggedAllocations[Tag] -= BytesFreed;
        GMemorySubsystemState->Allocations--;
        GMemorySubsystemState->LifeTimeFrees++;
    }
    #endif

    Platform_ExitCriticalSection(GCriticalSection);
}

void* MemSet(void* Destination, i32 Value, u64 Size)
{
    return Platform_MemSet(Destination, Value, Size);
}

void* MemZero(void* Block, u64 Size)
{
    return Platform_MemZero(Block, Size);
}

void* MemCopy(void* restrict Destination, const void* restrict Source, u64 Size)
{
    return Platform_MemCopy(Destination, Source, Size);
}

void* MemMove(void* restrict Destination, const void* restrict Source, u64 Size)
{
    return Platform_MemMove(Destination, Source, Size);
}

bool MemEqual(const void* Block1, const void* Block2, u64 Size)
{
    return Platform_MemEqual(Block1, Block2, Size);
}

char* Memory_GetUsageInfo(struct MemoryStats* Stats)
{
    #ifdef META_GENERATED
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wdouble-promotion"

    static char GMemUsageInfoBuffer[MAX_MEM_INFO_BUFFER_LENGTH] = { 0 };
    
    char Buffer[MAX_MEM_INFO_BUFFER_LENGTH] = "System Memory Use (Tagged):\n";
    
    u64 Offset = String_GetLength(Buffer);

    for (u8 i = 0; i < (u8)MemoryTag_Count; ++i)
    {
        char Unit[4] = "XiB";
        float Amount;

        if (Stats->TaggedAllocations[i] >= Gigabytes(1))
        {
            Unit[0] = 'G';
            Amount = (float)Stats->TaggedAllocations[i]/(float)Gigabytes(1);
        }
        else if (Stats->TaggedAllocations[i] >= Megabytes(1))
        {
            Unit[0] = 'M';
            Amount = (float)Stats->TaggedAllocations[i]/(float)Megabytes(1);
        }
        else if (Stats->TaggedAllocations[i] >= Kilobytes(1))
        {
            Unit[0] = 'K';
            Amount = (float)Stats->TaggedAllocations[i]/(float)Kilobytes(1);
        }
        else
        {
            Unit[0] = 'B';
            Unit[1] = 0;
            Amount = (float)Stats->TaggedAllocations[i];
        }

        i32 Length = CString_Format(Buffer + Offset, "  %s: %.2f%s | %llu\n", MAX_MEM_INFO_BUFFER_LENGTH, GMemoryTagStringTable[i], (f32)Amount, Unit, Stats->TaggedLifetimeAllocation[i]);
        //i32 Length = snprintf(Buffer + Offset, MAX_MEM_INFO_BUFFER_LENGTH, "  %s: %.2f%s | %llu\n", GMemoryTagStringTable[i], (f32)Amount, Unit, Stats->TaggedLifetimeAllocation[i]);
        Offset += Length;
    }

    i32 Length = CString_Format(Buffer + Offset, "Total Bytes Allocated: %lluB\n", MAX_MEM_INFO_BUFFER_LENGTH, Stats->TotalAllocated);
    //i32 Length = snprintf(Buffer + Offset, MAX_MEM_INFO_BUFFER_LENGTH, "Total Bytes Allocated: %lluB\n", Stats->TotalAllocated);
    Offset += Length;

    char UnitAllocated[4] = "XiB";
    char UnitTotal[4] = "XiB";
    float AmountAllocated;
    float TotalAmount;

    if (GEngineAllocator.Allocated >= Gigabytes(1))
    {
        UnitAllocated[0] = 'G';
        AmountAllocated = (float)GEngineAllocator.Allocated / (float)Gigabytes(1);
    }
    else if (GEngineAllocator.Allocated >= Megabytes(1))
    {
        UnitAllocated[0] = 'M';
        AmountAllocated = (float)GEngineAllocator.Allocated / (float)Megabytes(1);
    }
    else if (GEngineAllocator.Allocated >= Kilobytes(1))
    {
        UnitAllocated[0] = 'K';
        AmountAllocated = (float)GEngineAllocator.Allocated / (float)Kilobytes(1);
    }
    else
    {
        UnitAllocated[0] = 'B';
        UnitAllocated[1] = 0;
        AmountAllocated = (float)GEngineAllocator.Allocated;
    }

    if (GEngineAllocator.TotalSize >= Gigabytes(1))
    {
        UnitTotal[0] = 'G';
        TotalAmount = (float)GEngineAllocator.TotalSize / (float)Gigabytes(1);
    }
    else if (GEngineAllocator.TotalSize >= Megabytes(1))
    {
        UnitTotal[0] = 'M';
        TotalAmount = (float)GEngineAllocator.TotalSize / (float)Megabytes(1);
    }
    else if (GEngineAllocator.TotalSize >= Kilobytes(1))
    {
        UnitTotal[0] = 'K';
        TotalAmount = (float)GEngineAllocator.TotalSize / (float)Kilobytes(1);
    }
    else
    {
        UnitTotal[0] = 'B';
        UnitTotal[1] = 0;
        TotalAmount = (float)GEngineAllocator.TotalSize;
    }

    u64 MemRemaining = Memory_GetEngineMemoryRemaining();
    char RemainingUnitTotal[4] = "XiB";
    float RemainingAmount;
    if (MemRemaining >= Gigabytes(1))
    {
        RemainingUnitTotal[0] = 'G';
        RemainingAmount = (float)MemRemaining / (float)Gigabytes(1);
    }
    else if (MemRemaining >= Megabytes(1))
    {
        RemainingUnitTotal[0] = 'M';
        RemainingAmount = (float)MemRemaining / (float)Megabytes(1);
    }
    else if (MemRemaining >= Kilobytes(1))
    {
        RemainingUnitTotal[0] = 'K';
        RemainingAmount = (float)MemRemaining / (float)Kilobytes(1);
    }
    else
    {
        RemainingUnitTotal[0] = 'B';
        RemainingUnitTotal[1] = 0;
        RemainingAmount = (float)MemRemaining;
    }

    CString_Format(Buffer + Offset, "Total Memory Allocated: %.3f%s/%.3f%s | Free: %.3f%s", MAX_MEM_INFO_BUFFER_LENGTH, AmountAllocated, UnitAllocated, TotalAmount, UnitTotal, RemainingAmount, RemainingUnitTotal);
    //snprintf(Buffer + Offset, MAX_MEM_INFO_BUFFER_LENGTH, "Total Engine Memory Allocated: %.3f%s/%.3f%s | Free: %.3f%s", AmountAllocated, UnitAllocated, TotalAmount, UnitTotal, RemainingAmount, RemainingUnitTotal);

    CString_Copy(GMemUsageInfoBuffer, Buffer);
    #pragma GCC diagnostic pop 
    
    return GMemUsageInfoBuffer;
    #else
    return "";
    #endif
}

LinearAllocator_Scratch Memory_GetScratch(void)
{
    return LinearAllocator_GetScratch(&GEngineScratchAllocator);
}

void Memory_ReleaseScratch(LinearAllocator_Scratch* Scratch)
{
    LinearAllocator_ReleaseScratch(Scratch);
}

u64 Memory_GetEngineMemoryRemaining(void)
{
    return GEngineAllocator.TotalSize - GEngineAllocator.Allocated;
}

u64 Memory_GetTotalAllocations(void)
{
    return GMemorySubsystemState->Allocations;
}

void Memory_PrintUsageInfo(void)
{
    LOG_INFO("%s", Memory_GetUsageInfo(&GMemorySubsystemState->Stats));
    LOG_INFO("Total Active Mem Allocations: %llu", Memory_GetTotalAllocations());
    LOG_INFO("Total Lifetime Allocations: %llu", GMemorySubsystemState->LifeTimeAllocations);
    LOG_INFO("Total Lifetime Frees: %llu", GMemorySubsystemState->LifeTimeFrees);
}

const char* MemoryTagToString(EMemoryTag Tag)
{
    #ifdef META_GENERATED
    return GMemoryTagStringTable[Tag];
    #else
    return "";
    #endif
}

u64 MemoryUtils_CalculatePaddingWithHeader(u64 Ptr, u64 Alignment, u64 HeaderSize)
{
    ASSERT(LIKELY(Alignment != 0) && ((Alignment & (Alignment-1)) == 0)); // is power of two?

    u64 p = Ptr;
    u64 a = Alignment;
    u64 Modulo = p & (a-1);
    u64 NeededSpace = HeaderSize;
    u64 Padding = 0;
    
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
    return Memory != NULL && Memory != MemoryDump();
}

bool IsValidSlow(const void* Memory)
{
    if (Memory == NULL)
        return false;
    
    if (Memory == GMemoryDump)
        return false;
    
    if (Memory < GEngineMemory)
        return false;
    
    if (Memory > GMemoryDump)
        return false;
    
    return true;
}

u64 GetAligned(u64 Operand, u64 Granularity)
{
    return ((Operand + (Granularity-1)) & ~(Granularity-1));
}

MemoryRange GetAlignedRange(u64 Offset, u64 Size, u64 Granularity)
{
    return (MemoryRange){GetAligned(Offset, Granularity), GetAligned(Size, Granularity)};
}
