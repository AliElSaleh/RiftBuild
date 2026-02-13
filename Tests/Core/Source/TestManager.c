#include "TestManager.h"

#include "Core/Array.h"
#include "Core/Clock.h"
#include "Core/Log.h"
#include "Core/Memory.h"
#include "Core/Allocators.h"
#include "Core/StringUtils.h"

STRUCT(TestEntry)
{
    TestFuncCallback TestFunction;
    String Category;
    String Description;
};

static TestEntry* GTests = NULL;
static LinearAllocator GTestMemoryAllocator = {0};
void* GTestMemory = NULL;

String GExpectString = SC("");

static u32 LongestName = 0;
static u32 LongestCategory = 0;

// TODO: move to array.h
static u64 _ArrayCalculateMemRequirement(u64 Num, u64 Stride)
{
    u64 HeaderSize = ArrayField_Count * sizeof(u64);
    u64 Alignment = 3;
    u64 ArraySize = Num * ((Stride + Alignment) & ~Alignment);

    return HeaderSize + ArraySize;
}

void TestManager_Init(void)
{
    LOG_INFO("Initializing test manager...");

    u64 MemoryAmount = Kibibytes(128);
    GTestMemory = MemAlloc(MemoryAmount, MemoryTag_Test);
    LinearAllocator_Create(MemoryAmount, GTestMemory, &GTestMemoryAllocator);

    u64 TestArrayMemoryRequirement = _ArrayCalculateMemRequirement(2048, sizeof(TestEntry));
    void* TestArrayMemory = TestManager_MemAlloc(TestArrayMemoryRequirement);
    GTests = Array_CreateStatic(TestEntry, 2048, TestArrayMemory);

    GExpectString = String_Reserve(&GTestMemoryAllocator, 2048);
}

void* TestManager_MemAlloc(u64 Size)
{
    return LinearAllocator_Allocate(&GTestMemoryAllocator, Size);
}

void TestManager_RegisterTest(TestFuncCallback TestFunction, const String Category, const String Description)
{
    for each (TestEntry, e, GTests)
    {
        if  (e.TestFunction == TestFunction)
        {
            LOG_WARNING("Duplicate test found. Test \"[%S] %S\" has already been registered", Category, Description);
            return;
        }
    }

    TestEntry Entry;
    Entry.TestFunction = TestFunction;
    Entry.Category = Category;
    Entry.Description = Description;

    Array_Add(GTests, Entry);

    if (LongestName < Description.Length)
    {
        LongestName = Description.Length;
    }

    if (LongestCategory < Category.Length)
    {
        LongestCategory = Category.Length;
    }
}

bool TestManager_Run(void)
{
    LOG_INFO("Running tests...");

    u16 Passed = 0;
    u16 Failed = 0;
    u16 Skipped = 0;

    const u16 NumTests = (u16)Array_Num(GTests);

    if (NumTests == 0)
        return true;

    f64 TotalRunTime = 0.0;

    for (u16 i = 0; i < NumTests; ++i)
    {
        u32 NameLength =  GTests[i].Description.Length;
        u32 CatLength =  GTests[i].Category.Length;
        u32 TotalLength = NameLength+CatLength+1;

        u32 LongestLength = LongestCategory + LongestName + 1;

        u32 Difference = (LongestLength - TotalLength) + 1;
        u32 CatDiff = LongestCategory-CatLength;

        StringLocal(CatSpaces, 128);
        for (u8 j = 0; j < CatDiff; j++)
        {
            String_AppendChar(&CatSpaces, ' ');
        }

        StringLocal(Spaces, 128);
        for (u8 j = 0; j < Difference-CatDiff; j++)
        {
            String_AppendChar(&Spaces, ' ');
        }

        StringLocal(TestInfoMsg, 256);
        String_Format(&TestInfoMsg, S("Test %.3u: [%S]%S %S%S"), i+1, GTests[i].Category, CatSpaces, GTests[i].Description, Spaces);
        //LOG_INLINE("%S", TestInfoMsg);

        Logging_Disable();

        Clock TestClock;
        Clock_Start(&TestClock);

        const u8 Result = GTests[i].TestFunction();

        Clock_Tick(&TestClock);

        Logging_Enable();

        TotalRunTime += TestClock.ElapsedTime;

        StringLocal(ElapsedTimeString, 16);
        Clock_GetElapsedTime_ToString(&TestClock, true, &ElapsedTimeString);

        if (Result == true)
        {
            Passed++;
            //LOG_SUCCESS("[PASSED] %S", ElapsedTimeString);
            LOG_INLINE_SUCCESS("[PASSED] ");
            LOG_INLINE("%S", TestInfoMsg);
            LOG_INLINE("%S\n", ElapsedTimeString);
        }
        else if (Result == BYPASS)
        {
            Skipped++;
            //LOG_WARNING("[SKIPPED] %S", ElapsedTimeString);
            LOG_INLINE_WARNING("[------] ");
            LOG_INLINE("%S", TestInfoMsg);
            LOG_INLINE("%S\n", ElapsedTimeString);
        }
        else
        {
            Failed++;
            //LOG_ERROR("[FAILED] %S", ElapsedTimeString);
            LOG_INLINE_ERROR("[FAILED] ");
            LOG_INLINE("%S", TestInfoMsg);
            LOG_INLINE("%S\n", ElapsedTimeString);
        }

        if (GExpectString.Length > 0)
        {
            LOG_WARNING("%S", GExpectString);
            String_Empty(&GExpectString);
            LOG_LINE_BREAK();
        }
    }

    LOG_LINE_BREAK();

    LOG("[Test Results]");
    if (Passed > 0)
        LOG_SUCCESS("    *** %u PASSED ***", Passed);

    if (Failed > 0)
        LOG_ERROR("    *** %u FAILED ***", Failed);

    if (Skipped > 0)
        LOG_WARNING("    *** %u SKIPPED ***", Skipped);

    LOG("    Executed %u tests (%u Passed | %u Failed | %u Skipped)", NumTests, Passed, Failed, Skipped);

    StringLocal(ElapsedTimeString, 16);
    Time_ToString(TotalRunTime, true, &ElapsedTimeString);
    LOG("    Tests completed in %S", ElapsedTimeString);

    LinearAllocator_Destroy(&GTestMemoryAllocator);

    MemFree(GTestMemory, MemoryTag_Test);

    return Failed == 0;
}
