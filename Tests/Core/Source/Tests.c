#include "Core/EntryPoint.h"
#include "TestManager.h"

#include "Core/Allocators.h"
#include "Core/StringUtils.h"
#include "Core/Platform.h"
#include "Core/Filesystem.h"
#include "Core/MathUtils.h"
#include "Core/Log.h"
#include "Core/HashTable.h"
#include "Core/Array.h"
#include "Core/HashUtils.h"
#include "Core/Memory.h"

#define TEST(Name) static u8 CONCAT(Test_, Name)(void)

static i32 g_IterDirCount = 0;

#define Expect_IsTrue(Actual)                                       if ((Actual) != true)                                   { TestManager_SetExpectString(S("[Expect_IsTrue]\n    --> Expected: true\n    --> Actual:   false\n at: %s:%d"),                               __FILE__, __LINE__); return false; }
#define Expect_IsFalse(Actual)                                      if ((Actual) != false)                                  { TestManager_SetExpectString(S("[Expect_IsFalse]\n    --> Expected: false\n    --> Actual:   true\n at: %s:%d"),                              __FILE__, __LINE__); return false; }
#define Expect_IsEqual(Expected, Actual)                            if ((Actual) != (Expected))                             { TestManager_SetExpectString(S("[Expect_IsEqual]\n    --> Expected: %lld\n    --> Actual:   %lld\n at: %s:%d"),         (Expected), (Actual), __FILE__, __LINE__); return false; }
#define Expect_IsNotEqual(Expected, Actual)                         if ((Actual) == (Expected))                             { TestManager_SetExpectString(S("[Expect_IsNotEqual]\n    --> Expected: %lld\n    --> Actual:   %lld\n at: %s:%d"),      (Expected), (Actual), __FILE__, __LINE__); return false; }
#define Expect_Ptr_IsValid(Actual)                                  if ((Actual) == NULL)                                   { TestManager_SetExpectString(S("[Expect_Ptr_IsValid]\n    --> Expected: %p\n    --> Actual:   %p\n  at: %s:%d"),        (NULL),     (Actual), __FILE__, __LINE__); return false; }
#define Expect_Ptr_IsNotValid(Actual)                               if ((Actual) != NULL)                                   { TestManager_SetExpectString(S("[Expect_Ptr_IsNotValid]\n    --> Expected: %p\n    --> Actual:   %p\n  at: %s:%d"),     (NULL),     (Actual), __FILE__, __LINE__); return false; }
#define Expect_Ptr_IsEqual(Expected, Actual)                        if ((Expected) != (Actual))                             { TestManager_SetExpectString(S("[Expect_Ptr_IsEqual]\n    --> Expected: %p\n    --> Actual:   %p\n  at: %s:%d"),        (Expected), (Actual), __FILE__, __LINE__); return false; }
#define Expect_Ptr_IsNotEqual(Expected, Actual)                     if ((Expected) == (Actual))                             { TestManager_SetExpectString(S("[Expect_Ptr_IsNotEqual]\n    --> Expected: %p\n    --> Actual:   %p\n  at: %s:%d"),     (Expected), (Actual), __FILE__, __LINE__); return false; }
#define Expect_Float_IsEqual(Expected, Actual)                      if (Abs((Expected) - (Actual)) > 0.00001f)              { TestManager_SetExpectString(S("[Expect_Float_IsEqual]\n    --> Expected: %f\n    --> Actual:   %f\n  at: %s:%d"),      (Expected), (Actual), __FILE__, __LINE__); return false; }
#define Expect_Float_IsNotEqual(Expected, Actual)                   if (Abs((Expected) - (Actual)) < 0.00001f)              { TestManager_SetExpectString(S("[Expect_Float_IsNotEqual]\n    --> Expected: %f\n    --> Actual:   %f\n  at: %s:%d"),   (Expected), (Actual), __FILE__, __LINE__); return false; }
#define Expect_Float64_IsEqual(Expected, Actual)                    if (Absf64((Expected) - (Actual)) > 0.00001)            { TestManager_SetExpectString(S("[Expect_Float64_IsEqual]\n    --> Expected: %f\n    --> Actual:   %f\n  at: %s:%d"),    (Expected), (Actual), __FILE__, __LINE__); return false; }
#define Expect_Float64_IsNotEqual(Expected, Actual)                 if (Absf64((Expected) - (Actual)) < 0.00001)            { TestManager_SetExpectString(S("[Expect_Float64_IsNotEqual]\n    --> Expected: %f\n    --> Actual:   %f\n  at: %s:%d"), (Expected), (Actual), __FILE__, __LINE__); return false; }
#define Expect_String_IsEqual(Expected, Actual, bCaseSensitive)     if (!String_IsEqual(Expected, Actual, bCaseSensitive))  { TestManager_SetExpectString(S("[Expect_String_IsEqual]\n    --> Expected: %S\n    --> Actual:   %S\n  at: %s:%d"),     (Expected), (Actual), __FILE__, __LINE__); return false; }
#define Expect_String_IsNotEqual(Expected, Actual, bCaseSensitive)  if (String_IsEqual(Expected, Actual, bCaseSensitive))   { TestManager_SetExpectString(S("[Expect_String_IsNotEqual]\n    --> Expected: %S\n    --> Actual:   %S\n  at: %s:%d"),  (Expected), (Actual), __FILE__, __LINE__); return false; }

#include "Tests.h"

/////////////////////////////////
// Linear allocator tests      //
/////////////////////////////////

TEST(LinearAllocator_CreateAndDestroy)
{
    LinearAllocator Allocator;
    LinearAllocator_Create(sizeof(u64), NULL, &Allocator);

    Expect_IsNotEqual(NULL, Allocator.Memory)
    Expect_IsEqual(sizeof(u64), Allocator.TotalSize)
    Expect_IsEqual(0, Allocator.Allocated)

    LinearAllocator_Destroy(&Allocator);

    Expect_IsEqual(NULL, Allocator.Memory)
    Expect_IsEqual(0, Allocator.TotalSize)
    Expect_IsEqual(0, Allocator.Allocated)

    return true;
}

TEST(LinearAllocator_SingleAllocationAllSpace)
{
    const u16 MaxAllocations = 1024;

    LinearAllocator Allocator;
    LinearAllocator_Create(sizeof(u64) * MaxAllocations, NULL, &Allocator);

    void* AllocatedBlock = LinearAllocator_Allocate(&Allocator, sizeof(u64) * MaxAllocations);

    Expect_IsNotEqual(NULL, AllocatedBlock)
    Expect_IsEqual(sizeof(u64) * MaxAllocations, Allocator.Allocated)

    LinearAllocator_Destroy(&Allocator);

    return true;
}

TEST(LinearAllocator_MultiAllocationAllSpace)
{
    const u16 MaxAllocations = 1024;

    LinearAllocator Allocator;
    LinearAllocator_Create(sizeof(u64) * MaxAllocations, NULL, &Allocator);

    void* Block;
    for (u16 i = 0; i < MaxAllocations; ++i)
    {
        Block = LinearAllocator_Allocate(&Allocator, sizeof(u64));

        Expect_IsNotEqual(NULL, Block)
        Expect_IsEqual(sizeof(u64) * (i + 1), Allocator.Allocated)
    }

    LinearAllocator_Destroy(&Allocator);

    return true;
}

TEST(LinearAllocator_MultiAllocationAllSpaceThenFree)
{
    const u16 MaxAllocations = 1024;

    LinearAllocator Allocator;
    LinearAllocator_Create(sizeof(u64) * MaxAllocations, 0, &Allocator);

    void* Block;
    for (u16 i = 0; i < MaxAllocations; ++i)
    {
        Block = LinearAllocator_Allocate(&Allocator, sizeof(u64));

        Expect_IsNotEqual(NULL, Block)
        Expect_IsEqual(sizeof(u64) * (i + 1), Allocator.Allocated)
    }

    LinearAllocator_Reset(&Allocator, 0);

    Expect_IsEqual(0, Allocator.Allocated)

    LinearAllocator_Destroy(&Allocator);

    return true;
}

TEST(StringUtils_Create)
{
    LinearAllocator a = {0};
    LinearAllocator_Create(1024, NULL, &a);

    const String Test = String_Create(&a, S("hellO"));

    Expect_Ptr_IsValid(Test.Data);
    Expect_IsTrue(Test.Data[Test.Length] == '\0');
    Expect_IsTrue(Test.Capacity == Test.Length);
    Expect_String_IsEqual(S("hellO"), Test, true);

    return true;
}

TEST(StringUtils_Reserve)
{
    LinearAllocator a = {0};
    LinearAllocator_Create(1024, NULL, &a);

    const String Test = String_Reserve(&a, 512);

    Expect_Ptr_IsValid(Test.Data);
    Expect_IsTrue(Test.Length == 0);
    Expect_IsTrue(Test.Capacity == 512);
    Expect_IsTrue(Test.Data[Test.Capacity+1] == '\0');

    return true;
}

TEST(StringUtils_Append)
{
    StringLocal(TestString, 256);
    String_Copy(&TestString, S("Hello"));
    String_Append(&TestString, S(" Sailor!"));
    Expect_String_IsEqual(S("Hello Sailor!"), TestString, true);

    // zero test
    TestString.Length = 0;
    String_Append(&TestString, S(""));
    Expect_IsEqual(0, TestString.Length);
    Expect_String_IsEqual(S(""), TestString, true);

    // overflow test
    TestString.Length = 0;
    String_Append(&TestString, S("this is an overflow test fn asiofhaweuilofh ioaufh awuiofh nauiowf nawu fawu fha this is an overflow test fn asiofhaweuilofh ioaufh awuiofh nauiowf nawu fawu fha this is an overflow test fn asiofhaweuilofh ioaufh awuiofh nauiowf nawu fawu fha this is an overflow test fn asiofhaweuilofh ioaufh awuiofh nauiowf nawu fawu fha"));
    Expect_IsEqual(TestString.Capacity, TestString.Length);
    Expect_String_IsEqual(StrSlice(TestString.Data, TestString.Capacity), TestString, true);

    return true;
}

TEST(StringUtils_Format)
{
    StringLocal(TestString, 256);
    String_Format(&TestString, S("%S this is a format test %i %S"), S("Hello"), 123, S("Sailor!"));
    Expect_String_IsEqual(S("Hello this is a format test 123 Sailor!"), TestString, true);

    // zero test
    TestString.Length = 0;
    String_Format(&TestString, S(""));
    Expect_IsEqual(0, TestString.Length);
    Expect_String_IsEqual(S(""), TestString, true);

    // overflow test
    TestString.Length = 0;
    String_Format(&TestString, S("%S"), S("this is an overflow test fn asiofhaweuilofh ioaufh awuiofh nauiowf nawu fawu fha this is an overflow test fn asiofhaweuilofh ioaufh awuiofh nauiowf nawu fawu fha this is an overflow test fn asiofhaweuilofh ioaufh awuiofh nauiowf nawu fawu fha this is an overflow test fn asiofhaweuilofh ioaufh awuiofh nauiowf nawu fawu fha"));
    Expect_IsEqual(TestString.Capacity, TestString.Length);
    Expect_String_IsEqual(S("this is an overflow test fn asiofhaweuilofh ioaufh awuiofh nauiowf nawu fawu fha this is an overflow test fn asiofhaweuilofh ioaufh awuiofh nauiowf nawu fawu fha this is an overflow test fn asiofhaweuilofh ioaufh awuiofh nauiowf nawu fawu fha this is an ov"), TestString, true);

    return true;
}

TEST(StringUtils_BuildPath)
{
    StringLocal(TestString, 256);
    String_BuildPath(&TestString, S("C:"), S("Users"), S("User"), S("Documents"), S("file.txt"));

    #if PLATFORM_WINDOWS
    Expect_String_IsEqual(S("C:\\Users\\User\\Documents\\file.txt"), TestString, true);
    #else
    Expect_String_IsEqual(S("C:/Users/User/Documents/file.txt"), TestString, true);
    #endif

    // zero test
    TestString.Length = 0;
    String_BuildPath(&TestString, S(""), S(""), S(""), S(""), S(""));
    Expect_IsEqual(0, TestString.Length);
    Expect_String_IsEqual(S(""), TestString, true);

    return true;
}

TEST(StringUtils_BuildSeparator)
{
    StringLocal(TestString, 256);
    String_BuildSeparator(&TestString, ' ', S("C:"), S("Users"), S("User"), S("Documents"), S("file.txt"));
    Expect_String_IsEqual(S("C: Users User Documents file.txt"), TestString, true);

    // zero test
    TestString.Length = 0;
    String_BuildSeparator(&TestString, ' ', S(""), S(""), S(""), S(""), S(""));
    Expect_IsEqual(0, TestString.Length);
    Expect_String_IsEqual(S(""), TestString, true);

    return true;
}

TEST(StringUtils_Empty)
{
    StringLocal(TestString, 256);
    String_Copy(&TestString, S("Hello"));
    String_Empty(&TestString);
    Expect_String_IsEqual(S(""), TestString, true);

    // zero test
    TestString.Length = 0;
    String_Empty(&TestString);
    Expect_IsEqual(0, TestString.Length);
    Expect_String_IsEqual(S(""), TestString, true);

    return true;
}

TEST(StringUtils_Fill)
{
    // zero test
    StringLocal(TestString, 256);
    String_Fill(&TestString, 'a');
    Expect_IsEqual(0, TestString.Length);
    Expect_String_IsEqual(S(""), TestString, true);

    TestString.Length = 255;
    String_Fill(&TestString, 'a');
    Expect_IsEqual(255, TestString.Length);
    Expect_String_IsEqual(S("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"), TestString, true);

    return true;
}

TEST(StringUtils_ToUpper)
{
    StringLocal(TestString, 256);
    String_Copy(&TestString, S("hello&^#R@f"));
    String_ToUpper(&TestString);
    Expect_String_IsEqual(S("HELLO&^#R@F"), TestString, true);

    // zero test
    TestString.Length = 0;
    String_ToUpper(&TestString);
    Expect_IsEqual(0, TestString.Length);
    Expect_String_IsEqual(S(""), TestString, true);

    return true;
}

TEST(StringUtils_ToLower)
{
    StringLocal(TestString, 256);
    String_Copy(&TestString, S("HELLO&^#R@F"));
    String_ToLower(&TestString);
    Expect_String_IsEqual(S("hello&^#r@f"), TestString, true);

    // zero test
    TestString.Length = 0;
    String_ToLower(&TestString);
    Expect_IsEqual(0, TestString.Length);
    Expect_String_IsEqual(S(""), TestString, true);

    return true;
}

TEST(StringUtils_ToNarrow)
{
    // Basic ASCII conversion from wide to narrow
    String16Local(WideStr, 256);
    wchar WideData[] = L"Hello World";
    for (u32 i = 0; i < 11; i++)
    {
        WideStr.Data[i] = WideData[i];
    }
    WideStr.Length = 11;

    StringLocal(NarrowStr, 256);
    String_ToNarrow(WideStr, &NarrowStr);
    Expect_IsEqual(11, NarrowStr.Length);
    Expect_String_IsEqual(S("Hello World"), NarrowStr, true);

    // Empty string
    String16Local(EmptyWide, 256);
    EmptyWide.Length = 0;
    StringLocal(EmptyNarrow, 256);
    String_ToNarrow(EmptyWide, &EmptyNarrow);
    Expect_IsEqual(0, EmptyNarrow.Length);

    // Truncation when destination capacity is smaller than source length
    StringLocal(SmallNarrow, 4);
    String_ToNarrow(WideStr, &SmallNarrow);
    Expect_IsEqual(4, SmallNarrow.Length);
    // Should only have the first 4 characters
    Expect_IsEqual('H', SmallNarrow.Data[0]);
    Expect_IsEqual('e', SmallNarrow.Data[1]);
    Expect_IsEqual('l', SmallNarrow.Data[2]);
    Expect_IsEqual('l', SmallNarrow.Data[3]);

    return true;
}

TEST(StringUtils_ToWide)
{
    // Basic ASCII conversion from narrow to wide
    String NarrowStr = S("Hello World");
    String16Local(WideStr, 256);
    String_ToWide(NarrowStr, &WideStr);
    Expect_IsEqual(11, WideStr.Length);
    for (u32 i = 0; i < NarrowStr.Length; i++)
    {
        Expect_IsEqual((wchar)NarrowStr.Data[i], WideStr.Data[i]);
    }

    // Empty string
    String EmptyNarrow = S("");
    String16Local(EmptyWide, 256);
    String_ToWide(EmptyNarrow, &EmptyWide);
    Expect_IsEqual(0, EmptyWide.Length);

    // Truncation when destination capacity is smaller than source length
    String16Local(SmallWide, 3);
    String_ToWide(NarrowStr, &SmallWide);
    Expect_IsEqual(3, SmallWide.Length);
    Expect_IsEqual((wchar)'H', SmallWide.Data[0]);
    Expect_IsEqual((wchar)'e', SmallWide.Data[1]);
    Expect_IsEqual((wchar)'l', SmallWide.Data[2]);

    // Round-trip: narrow -> wide -> narrow
    String Original = S("RoundTrip123!");
    String16Local(WideBuf, 256);
    String_ToWide(Original, &WideBuf);
    StringLocal(NarrowBack, 256);
    String_ToNarrow(WideBuf, &NarrowBack);
    Expect_IsEqual(Original.Length, NarrowBack.Length);
    Expect_String_IsEqual(Original, NarrowBack, true);

    return true;
}

TEST(StringUtils_BackSlashToForwardSlash)
{
    StringLocal(TestString, 256);
    String_Copy(&TestString, S("C:\\Users\\User\\Documents\\file.txt"));
    String_BackSlashToForwardSlash(&TestString);
    Expect_String_IsEqual(S("C:/Users/User/Documents/file.txt"), TestString, true);

    // zero test
    TestString.Length = 0;
    String_BackSlashToForwardSlash(&TestString);
    Expect_IsEqual(0, TestString.Length);
    Expect_String_IsEqual(S(""), TestString, true);

    return true;
}

TEST(StringUtils_FrontSlashToBackSlash)
{
    StringLocal(TestString, 256);
    String_Copy(&TestString, S("C:/Users/User/Documents/file.txt"));
    String_ForwardSlashToBackSlash(&TestString);
    Expect_String_IsEqual(S("C:\\Users\\User\\Documents\\file.txt"), TestString, true);

    // zero test
    TestString.Length = 0;
    String_ForwardSlashToBackSlash(&TestString);
    Expect_IsEqual(0, TestString.Length);
    Expect_String_IsEqual(S(""), TestString, true);

    return true;
}

TEST(StringUtils_EatSpaces)
{
    StringLocal(TestString, 256);
    String_Copy(&TestString, S("   hello   "));
    bool bAte = String_EatSpacesInline(&TestString);
    Expect_IsTrue(bAte);
    Expect_String_IsEqual(S("hello   "), TestString, true);

    // zero test
    TestString.Length = 0;
    xx String_EatSpaces(TestString);
    Expect_IsEqual(0, TestString.Length);
    Expect_String_IsEqual(S(""), TestString, true);

    return true;
}

TEST(StringUtils_EatPathSeparators)
{
    StringLocal(TestString, 256);
    String_Copy(&TestString, S("\\/////\\/\\/\\/\\/\\\\//\\hello\\"));
    bool bAte = String_EatPathSeparatorsInline(&TestString);
    Expect_IsTrue(bAte);
    Expect_String_IsEqual(S("hello\\"), TestString, true);

    // zero test
    TestString.Length = 0;
    bAte = false;
    bAte = String_EatPathSeparatorsInline(&TestString);
    Expect_IsFalse(bAte);
    Expect_IsEqual(0, TestString.Length);
    Expect_String_IsEqual(S(""), TestString, true);

    return true;
}

TEST(StringUtils_IndexOfChar)
{
    String a = S("hElLO$%@z@%ffa_0oo1_fjioeahf8ADNUIOAT#FGhq98fhehailh");
    u32 Result = 0;
    bool bSuccess = String_IndexOfChar(a, 'z', &Result);
    Expect_IsTrue(bSuccess);
    Expect_IsEqual(8, Result);

    Result = 0;
    bSuccess = String_IndexOfChar(a, ' ', &Result);
    Expect_IsFalse(bSuccess);
    Expect_IsEqual(0, Result);

    return true;
}

TEST(StringUtils_IndexOfWhitespace)
{
    String a = S("hElLO$%@z@%ffa_0oo1 fjioeahf8ADNUIOAT#FGhq98fhehailh");
    u32 Result = 0;
    bool bSuccess = String_IndexOfFirstWhitespace(a, &Result);
    Expect_IsTrue(bSuccess);
    Expect_IsEqual(19, Result);

    Result = 0;
    bSuccess = String_IndexOfFirstWhitespace(S("hElLO$%@z@%ffa_0oo1_fjioeahf8ADNUIOAT#FGhq98fhehailh"), &Result);
    Expect_IsFalse(bSuccess);
    Expect_IsEqual(0, Result);

    return true;
}

TEST(StringUtils_CountChar)
{
    String a = S("hElLO$%@z@%ffa_0oo1_fjioeahf8ADNUzOAT#FGhq98fhehailh");
    u32 Result = String_CountChar(a, 'z');
    Expect_IsEqual(2, Result);

    Result = String_CountChar(a, ' ');
    Expect_IsEqual(0, Result);

    return true;
}

TEST(StringUtils_CountSpaces)
{
    String a = S("hElLO$%@z@%ffa_0oo1 fjioeahf8ADNUIOAT#FGhq98fhehailh");
    u32 Result = String_CountSpaces(a);
    Expect_IsEqual(1, Result);

    Result = String_CountSpaces(S("hElLO$%@z@%ffa_0oo1_fjioeahf8ADNUIOAT#FGhq98fhehailh"));
    Expect_IsEqual(0, Result);

    return true;
}

TEST(StringUtils_CountPathSeparators)
{
    String a = S("hElLO$%@z@%ff/////\\//\\/\\/\\//\\/hq98fhehailh");
    u32 Result = String_CountPathSeparators(a);
    Expect_IsEqual(17, Result);

    Result = String_CountPathSeparators(S("hElLO$%@z@%ffhq98fhehailh"));
    Expect_IsEqual(0, Result);

    return true;
}

TEST(StringUtils_ParseIntoArray)
{
    LinearAllocator Arena = {0};
    LinearAllocator_Create(4096, NULL, &Arena);

    // Basic split by comma
    StringArray Result = String_ParseIntoArray(&Arena, S("hello,world,foo"), ',', 0, 100);
    Expect_IsEqual(3, Result.Num);
    Expect_String_IsEqual(S("hello"), Result.List[0], true);
    Expect_String_IsEqual(S("world"), Result.List[1], true);
    Expect_String_IsEqual(S("foo"),   Result.List[2], true);

    // Single element (no delimiter present)
    LinearAllocator_Reset(&Arena, 0);
    Result = String_ParseIntoArray(&Arena, S("hello"), ',', 0, 100);
    Expect_IsEqual(1, Result.Num);
    Expect_String_IsEqual(S("hello"), Result.List[0], true);

    // MaxCount >= actual count does not truncate
    LinearAllocator_Reset(&Arena, 0);
    Result = String_ParseIntoArray(&Arena, S("a,b,c"), ',', 0, 100);
    Expect_IsEqual(3, Result.Num);
    Expect_String_IsEqual(S("a"), Result.List[0], true);
    Expect_String_IsEqual(S("b"), Result.List[1], true);
    Expect_String_IsEqual(S("c"), Result.List[2], true);

    // StartingIndex skips initial characters
    LinearAllocator_Reset(&Arena, 0);
    Result = String_ParseIntoArray(&Arena, S("skip,hello,world"), ',', 5, 100);
    Expect_IsEqual(2, Result.Num);
    Expect_String_IsEqual(S("hello"), Result.List[0], true);
    Expect_String_IsEqual(S("world"), Result.List[1], true);

    // Split by space
    LinearAllocator_Reset(&Arena, 0);
    Result = String_ParseIntoArray(&Arena, S("one two three"), ' ', 0, 100);
    Expect_IsEqual(3, Result.Num);
    Expect_String_IsEqual(S("one"),   Result.List[0], true);
    Expect_String_IsEqual(S("two"),   Result.List[1], true);
    Expect_String_IsEqual(S("three"), Result.List[2], true);

    // Empty string
    LinearAllocator_Reset(&Arena, 0);
    Result = String_ParseIntoArray(&Arena, S(""), ',', 0, 100);
    Expect_IsEqual(0, Result.Num);

    LinearAllocator_Destroy(&Arena);

    return true;
}

TEST(StringUtils_GetLength)
{
    String a = S("hElLO$%@z@%ffa_0oo1_fjioeahf8ADNUIOAT#FGhq98fhehailDMI(WEDJ(#HDHQ (FH*(HF*(AFH*(AWEH*(HR*(#HQ*ODFHAIOJF ASFNA HD ()@WH QBDAS:NFWBOP#EHQGFBJKASB F AHDA:EHR*(@ G RUIGFASDJKCBAL RUZ#GAOAYR&IGdfuigbawsuirhwe oafgbhaweuir abqfdawf g3gf awbefkawh");
    u32 Result = String_GetLength((const char*)a.Data);
    Expect_IsEqual(a.Length, Result);

    Result = String_GetLength("");
    Expect_IsEqual(0, Result);

    return true;
}

TEST(StringUtils_GetLengthFast)
{
    String a = S("hElLO$%@z@%ffa_0oo1_fjioeahf8ADNUIOAT#FGhq98fhehailDMI(WEDJ(#HDHQ (FH*(HF*(AFH*(AWEH*(HR*(#HQ*ODFHAIOJF ASFNA HD ()@WH QBDAS:NFWBOP#EHQGFBJKASB F AHDA:EHR*(@ G RUIGFASDJKCBAL RUZ#GAOAYR&IGdfuigbawsuirhwe oafgbhaweuir abqfdawf g3gf awbefkawh");
    u32 Result = String_GetLength_Fast((const char*)a.Data);
    Expect_IsEqual(a.Length, Result);

    Result = String_GetLength_Fast("");
    Expect_IsEqual(0, Result);

    return true;
}

TEST(StringUtils_IsAlphabetUpper)
{
    String a = S("ABCDEFGHIJKLMNOPQRSTUVWXYZ");
    for (u32 i = 0; i < a.Length; ++i)
    {
        bool bSuccess = IsAlphabetUpper(a.Data[i]);
        Expect_IsTrue(bSuccess);
    }

    a = S("&*@^#*($& &($))54897491/*9");
    for (u32 i = 0; i < a.Length; ++i)
    {
        bool bSuccess = IsAlphabetUpper(a.Data[i]);
        Expect_IsFalse(bSuccess);
    }

    return true;
}

TEST(StringUtils_IsAlphabetLower)
{
    String a = S("abcdefghijklmnopqrstuvwxyz");
    for (u32 i = 0; i < a.Length; ++i)
    {
        bool bSuccess = IsAlphabetLower(a.Data[i]);
        Expect_IsTrue(bSuccess);
    }

    a = S("&*@^#*($& &($))54897491/*9");
    for (u32 i = 0; i < a.Length; ++i)
    {
        bool bSuccess = IsAlphabetLower(a.Data[i]);
        Expect_IsFalse(bSuccess);
    }

    return true;
}

TEST(StringUtils_IsAlphabet)
{
    String a = S("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ");
    for (u32 i = 0; i < a.Length; ++i)
    {
        bool bSuccess = IsAlphabet(a.Data[i]);
        Expect_IsTrue(bSuccess);
    }

    a = S("&*@^#*($& &($))54897491/*9");
    for (u32 i = 0; i < a.Length; ++i)
    {
        bool bSuccess = IsAlphabet(a.Data[i]);
        Expect_IsFalse(bSuccess);
    }

    return true;
}

TEST(StringUtils_IsDigit)
{
    String a = S("0123456789");
    for (u32 i = 0; i < a.Length; ++i)
    {
        bool bSuccess = IsDigit(a.Data[i]);
        Expect_IsTrue(bSuccess);
    }

    a = S("&*@^#*($& &($))ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz");
    for (u32 i = 0; i < a.Length; ++i)
    {
        bool bSuccess = IsDigit(a.Data[i]);
        Expect_IsFalse(bSuccess);
    }

    return true;
}

TEST(StringUtils_IsWhitespace)
{
    String a = S(" \t\n\r");
    for (u32 i = 0; i < a.Length; ++i)
    {
        bool bSuccess = IsWhitespace(a.Data[i]);
        Expect_IsTrue(bSuccess);
    }

    a = S("&*@^#*($&&($))ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789");
    for (u32 i = 0; i < a.Length; ++i)
    {
        bool bSuccess = IsWhitespace(a.Data[i]);
        Expect_IsFalse(bSuccess);
    }

    return true;
}

TEST(StringUtils_IsNewline)
{
    String a = S("\n\r\f");
    for (u32 i = 0; i < a.Length; ++i)
    {
        bool bSuccess = IsNewline(a.Data[i]);
        Expect_IsTrue(bSuccess);
    }

    a = S("&*@^#*($&&($))ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 \t");
    for (u32 i = 0; i < a.Length; ++i)
    {
        bool bSuccess = IsNewline(a.Data[i]);
        Expect_IsFalse(bSuccess);
    }

    return true;
}

TEST(StringUtils_IsSymbol)
{
    String a = S("(){}!@#$%^&*+=");
    for (u32 i = 0; i < a.Length; ++i)
    {
        bool bSuccess = IsSymbol(a.Data[i]);
        Expect_IsTrue(bSuccess);
    }

    a = S("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 \t\n\r");
    for (u32 i = 0; i < a.Length; ++i)
    {
        bool bSuccess = IsSymbol(a.Data[i]);
        Expect_IsFalse(bSuccess);
    }

    return true;
}

TEST(StringUtils_CheckEquality_CaseSensitive)
{
    String a = S("hElLO$%@z@%ffa_0oo1_fjioeahf8ADNUIOAT#FGhq98fhehailh");
    bool bResult = String_IsEqual(a, S("hElLO$%@z@%ffa_0oo1_fjioeahf8ADNUIOAT#FGhq98fhehailh"), true);

    Expect_IsTrue(bResult);

    return true;
}

TEST(StringUtils_CheckEquality_CaseInsensitive)
{
    String a = S("kELlo$%@Z@%ffa_0OO1_fjioeahf8adnuioat#fghq98fhehailh");
    bool bResult = String_IsEqual(a, S("KElLO$%@z@%ffa_0oo1_fjioeahf8ADNUIOAT#FGhq98fhehailh"), false);

    Expect_IsTrue(bResult);

    return true;
}

TEST(StringUtils_CheckInequality_CaseSensitive)
{
    String a = S("Hdw98hELffa_0OO1");
    bool bResult = String_IsEqual(a, S("zchoco#$late@%ffa_0oo1"), true);

    Expect_IsFalse(bResult);

    return true;
}

TEST(StringUtils_CheckInequality_CaseInsensitive)
{
    String a = S("\"zELhd9wa7glo$%@Z@O1");
    bool bResult = String_IsEqual(a, S("fw;gdawdwaoij@z@%ffa_0oo1"), false);

    Expect_IsFalse(bResult);

    return true;
}

TEST(StringUtils_CheckEquality_Empty)
{
    bool bResult = String_IsEqual(S(""), S(""), false);
    Expect_IsTrue(bResult);
    bResult = String_IsEqual(S(""), S(""), true);
    Expect_IsTrue(bResult);

    return true;
}

TEST(StringUtils_IsValid)
{
    String a = S("Hello");
    bool bResult = String_IsValid(a);
    Expect_IsTrue(bResult);

    String b = {0};
    bResult = String_IsValid(b);
    Expect_IsFalse(bResult);

    return true;
}

TEST(StringUtils_StartsWith)
{
    String a = S("someprefix^&*$%Hello");
    bool bResult = String_StartsWith(a, S("someprefix^&*$%"), true);
    Expect_IsTrue(bResult);

    a = S("");
    bResult = String_StartsWith(a, S(""), true);
    Expect_IsFalse(bResult);

    return true;
}

TEST(StringUtils_EndsWith)
{
    String a = S("someprefix^&*$%Hello");
    bool bResult = String_EndsWith(a, S("Hello"), true);
    Expect_IsTrue(bResult);

    a = S("");
    bResult = String_EndsWith(a, S(""), true);
    Expect_IsFalse(bResult);

    return true;
}

TEST(StringUtils_Contains)
{
    String a = S("someprefix^&*$%Hello");
    bool bResult = String_Contains(a, S("prefix"), true);
    Expect_IsTrue(bResult);

    a = S("");
    bResult = String_Contains(a, S(""), true);
    Expect_IsFalse(bResult);

    return true;
}

TEST(StringUtils_ToU8)
{
    // Test in-range numbers, out of bounds, zero/min/max value and malformed inputs

    String a = S("123");
    u8 Result = 0;
    bool bSuccess = String_ToU8(a, &Result);
    Expect_IsTrue(bSuccess);
    Expect_IsEqual(123, Result);

    a = S("255");
    Result = 0;
    bSuccess = String_ToU8(a, &Result);
    Expect_IsTrue(bSuccess);
    Expect_IsEqual(UINT8_MAX, Result);

    a = S("0");
    Result = 0;
    bSuccess = String_ToU8(a, &Result);
    Expect_IsTrue(bSuccess);
    Expect_IsEqual(0, Result);

    a = S("-99");
    Result = 0;
    bSuccess = String_ToU8(a, &Result);
    Expect_IsFalse(bSuccess);
    Expect_IsEqual(0, Result);

    a = S("548");
    Result = 0;
    bSuccess = String_ToU8(a, &Result);
    Expect_IsFalse(bSuccess);
    Expect_IsEqual(0, Result);

    a = S("dn$");
    Result = 0;
    bSuccess = String_ToU8(a, &Result);
    Expect_IsFalse(bSuccess);
    Expect_IsEqual(0, Result);

    a = S("");
    Result = 0;
    bSuccess = String_ToU8(a, &Result);
    Expect_IsFalse(bSuccess);
    Expect_IsEqual(0, Result);

    return true;
}

TEST(StringUtils_ToU16)
{
    // Test in-range numbers, out of bounds, zero/min/max value and malformed inputs

    String a = S("123");
    u16 Result = 0;
    bool bSuccess = String_ToU16(a, &Result);
    Expect_IsTrue(bSuccess);
    Expect_IsEqual(123, Result);

    a = S("65535");
    Result = 0;
    bSuccess = String_ToU16(a, &Result);
    Expect_IsTrue(bSuccess);
    Expect_IsEqual(UINT16_MAX, Result);

    a = S("0");
    Result = 0;
    bSuccess = String_ToU16(a, &Result);
    Expect_IsTrue(bSuccess);
    Expect_IsEqual(0, Result);

    a = S("-1479");
    Result = 0;
    bSuccess = String_ToU16(a, &Result);
    Expect_IsFalse(bSuccess);
    Expect_IsEqual(0, Result);

    a = S("65536");
    Result = 0;
    bSuccess = String_ToU16(a, &Result);
    Expect_IsFalse(bSuccess);
    Expect_IsEqual(0, Result);

    a = S("XBAI WDG*#$");
    Result = 0;
    bSuccess = String_ToU16(a, &Result);
    Expect_IsFalse(bSuccess);
    Expect_IsEqual(0, Result);

    return true;
}

TEST(StringUtils_ToU32)
{
    // Test in-range numbers, out of bounds, zero/min/max value and malformed inputs

    String a = S("123");
    u32 Result = 0;
    bool bSuccess = String_ToU32(a, &Result);
    Expect_IsTrue(bSuccess);
    Expect_IsEqual(123, Result);

    a = S("4294967295");
    Result = 0;
    bSuccess = String_ToU32(a, &Result);
    Expect_IsTrue(bSuccess);
    Expect_IsEqual(UINT32_MAX, Result);

    a = S("0");
    Result = 0;
    bSuccess = String_ToU32(a, &Result);
    Expect_IsTrue(bSuccess);
    Expect_IsEqual(0, Result);

    a = S("-57813");
    Result = 0;
    bSuccess = String_ToU32(a, &Result);
    Expect_IsFalse(bSuccess);
    Expect_IsEqual(0, Result);

    a = S("4294967296");
    Result = 0;
    bSuccess = String_ToU32(a, &Result);
    Expect_IsFalse(bSuccess);
    Expect_IsEqual(0, Result);

    a = S("dbDY&@#n$");
    Result = 0;
    bSuccess = String_ToU32(a, &Result);
    Expect_IsFalse(bSuccess);
    Expect_IsEqual(0, Result);

    return true;
}

TEST(StringUtils_ToU64)
{
    // Test in-range numbers, out of bounds, zero/min/max value and malformed inputs

    String a = S("123");
    u64 Result = 0;
    bool bSuccess = String_ToU64(a, &Result);
    Expect_IsTrue(bSuccess);
    Expect_IsEqual(123, Result);

    a = S("18446744073709551615");
    Result = 0;
    bSuccess = String_ToU64(a, &Result);
    Expect_IsTrue(bSuccess);
    Expect_IsEqual(UINT64_MAX, Result);

    a = S("0");
    Result = 0;
    bSuccess = String_ToU64(a, &Result);
    Expect_IsTrue(bSuccess);
    Expect_IsEqual(0, Result);

    a = S("-45897");
    Result = 0;
    bSuccess = String_ToU64(a, &Result);
    Expect_IsFalse(bSuccess);
    Expect_IsEqual(0, Result);

    a = S("18446744073709551616");
    Result = 0;
    bSuccess = String_ToU64(a, &Result);
    Expect_IsFalse(bSuccess);
    Expect_IsEqual(0, Result);

    a = S("dja9yh$@#(*GDA)");
    Result = 0;
    bSuccess = String_ToU64(a, &Result);
    Expect_IsFalse(bSuccess);
    Expect_IsEqual(0, Result);

    return true;
}

TEST(StringUtils_ToI8)
{
    // Test in-range numbers, out of bounds, zero/min/max value and malformed inputs

    String a = S("123");
    i8 Result = 0;
    bool bSuccess = String_ToI8(a, &Result);
    Expect_IsTrue(bSuccess);
    Expect_IsEqual(123, Result);

    a = S("-127");
    Result = 0;
    bSuccess = String_ToI8(a, &Result);
    Expect_IsTrue(bSuccess);
    Expect_IsEqual(INT8_MIN, Result);

    a = S("127");
    Result = 0;
    bSuccess = String_ToI8(a, &Result);
    Expect_IsTrue(bSuccess);
    Expect_IsEqual(INT8_MAX, Result);

    a = S("0");
    Result = 0;
    bSuccess = String_ToI8(a, &Result);
    Expect_IsTrue(bSuccess);
    Expect_IsEqual(0, Result);

    a = S("-129");
    Result = 0;
    bSuccess = String_ToI8(a, &Result);
    Expect_IsFalse(bSuccess);
    Expect_IsEqual(0, Result);

    a = S("128");
    Result = 0;
    bSuccess = String_ToI8(a, &Result);
    Expect_IsFalse(bSuccess);
    Expect_IsEqual(0, Result);

    a = S("dn$");
    Result = 0;
    bSuccess = String_ToI8(a, &Result);
    Expect_IsFalse(bSuccess);
    Expect_IsEqual(0, Result);

    return true;
}

TEST(StringUtils_ToI16)
{
    // Test in-range numbers, out of bounds, zero/min/max value and malformed inputs

    String a = S("123");
    i16 Result = 0;
    bool bSuccess = String_ToI16(a, &Result);
    Expect_IsTrue(bSuccess);
    Expect_IsEqual(123, Result);

    a = S("-32767");
    Result = 0;
    bSuccess = String_ToI16(a, &Result);
    Expect_IsTrue(bSuccess);
    Expect_IsEqual(INT16_MIN, Result);

    a = S("32767");
    Result = 0;
    bSuccess = String_ToI16(a, &Result);
    Expect_IsTrue(bSuccess);
    Expect_IsEqual(INT16_MAX, Result);

    a = S("0");
    Result = 0;
    bSuccess = String_ToI16(a, &Result);
    Expect_IsTrue(bSuccess);
    Expect_IsEqual(0, Result);

    a = S("-32769");
    Result = 0;
    bSuccess = String_ToI16(a, &Result);
    Expect_IsFalse(bSuccess);
    Expect_IsEqual(0, Result);

    a = S("32768");
    Result = 0;
    bSuccess = String_ToI16(a, &Result);
    Expect_IsFalse(bSuccess);
    Expect_IsEqual(0, Result);

    a = S("dn$");
    Result = 0;
    bSuccess = String_ToI16(a, &Result);
    Expect_IsFalse(bSuccess);
    Expect_IsEqual(0, Result);

    return true;
}

TEST(StringUtils_ToI32)
{
    // Test in-range numbers, out of bounds, zero/min/max value and malformed inputs

    String a = S("123");
    i32 Result = 0;
    bool bSuccess = String_ToI32(a, &Result);
    Expect_IsTrue(bSuccess);
    Expect_IsEqual(123, Result);

    a = S("-2147483647");
    Result = 0;
    bSuccess = String_ToI32(a, &Result);
    Expect_IsTrue(bSuccess);
    Expect_IsEqual(INT32_MIN, Result);

    a = S("2147483647");
    Result = 0;
    bSuccess = String_ToI32(a, &Result);
    Expect_IsTrue(bSuccess);
    Expect_IsEqual(INT32_MAX, Result);

    a = S("0");
    Result = 0;
    bSuccess = String_ToI32(a, &Result);
    Expect_IsTrue(bSuccess);
    Expect_IsEqual(0, Result);

    a = S("-2147483649");
    Result = 0;
    bSuccess = String_ToI32(a, &Result);
    Expect_IsFalse(bSuccess);
    Expect_IsEqual(0, Result);

    a = S("2147483648");
    Result = 0;
    bSuccess = String_ToI32(a, &Result);
    Expect_IsFalse(bSuccess);
    Expect_IsEqual(0, Result);

    a = S("dn$");
    Result = 0;
    bSuccess = String_ToI32(a, &Result);
    Expect_IsFalse(bSuccess);
    Expect_IsEqual(0, Result);

    return true;
}

TEST(StringUtils_ToI64)
{
    // Test in-range numbers, out of bounds, zero/min/max value and malformed inputs

    String a = S("123");
    i64 Result = 0;
    bool bSuccess = String_ToI64(a, &Result);
    Expect_IsTrue(bSuccess);
    Expect_IsEqual(123, Result);

    a = S("-9223372036854775807");
    Result = 0;
    bSuccess = String_ToI64(a, &Result);
    Expect_IsTrue(bSuccess);
    Expect_IsEqual(INT64_MIN, Result);

    a = S("9223372036854775807");
    Result = 0;
    bSuccess = String_ToI64(a, &Result);
    Expect_IsTrue(bSuccess);
    Expect_IsEqual(INT64_MAX, Result);

    a = S("0");
    Result = 0;
    bSuccess = String_ToI64(a, &Result);
    Expect_IsTrue(bSuccess);
    Expect_IsEqual(0, Result);

    a = S("-9223372036854775808");
    Result = 0;
    bSuccess = String_ToI64(a, &Result);
    Expect_IsFalse(bSuccess);
    Expect_IsEqual(0, Result);

    a = S("9223372036854775808");
    Result = 0;
    bSuccess = String_ToI64(a, &Result);
    Expect_IsFalse(bSuccess);
    Expect_IsEqual(0, Result);

    a = S("dfj893hfanazz$");
    Result = 0;
    bSuccess = String_ToI64(a, &Result);
    Expect_IsFalse(bSuccess);
    Expect_IsEqual(0, Result);

    return true;
}

TEST(StringUtils_ToF32)
{
    // Test in-range numbers, out of bounds, zero/min/max value and malformed inputs

    String a = S("123.456");
    f32 Result = 0;
    bool bSuccess = String_ToF32(a, &Result);
    Expect_IsTrue(bSuccess);
    Expect_Float_IsEqual(123.456f, Result);

    a = S(".1297");
    Result = 0;
    bSuccess = String_ToF32(a, &Result);
    Expect_IsTrue(bSuccess);
    Expect_Float_IsEqual(.1297f, Result);

    a = S(".12973892651938569813571");
    Result = 0;
    bSuccess = String_ToF32(a, &Result);
    Expect_IsTrue(bSuccess);
    Expect_Float_IsEqual(.12973892651938569813571f, Result);

    a = S("-340282346638528859811704183484516925440");
    Result = 0;
    bSuccess = String_ToF32(a, &Result);
    Expect_IsFalse(bSuccess);
    Expect_Float_IsEqual(0.0f, Result);

    a = S("340282346638528859811704183484516925440");
    Result = 0;
    bSuccess = String_ToF32(a, &Result);
    Expect_IsFalse(bSuccess);
    Expect_Float_IsEqual(0.0f, Result);

    a = S("0");
    Result = 0;
    bSuccess = String_ToF32(a, &Result);
    Expect_IsTrue(bSuccess);
    Expect_Float_IsEqual(0.0f, Result);

    a = S("-340282346638528859811704183484516925441");
    Result = 0;
    bSuccess = String_ToF32(a, &Result);
    Expect_IsFalse(bSuccess);
    Expect_Float_IsEqual(0.0f, Result);

    a = S("340282346638528859811704183484516925441");
    Result = 0;
    bSuccess = String_ToF32(a, &Result);
    Expect_IsFalse(bSuccess);
    Expect_Float_IsEqual(0.0f, Result);

    a = S("-dADNW *AW&ETQAdajd 792");
    Result = 0;
    bSuccess = String_ToF32(a, &Result);
    Expect_IsFalse(bSuccess);
    Expect_Float_IsEqual(0.0f, Result);

    return true;
}

TEST(StringUtils_ToF64)
{
    // Test in-range numbers, out of bounds, zero/min/max value and malformed inputs

    String a = S("123.45678912");
    f64 Result = 0;
    bool bSuccess = String_ToF64(a, &Result);
    Expect_IsTrue(bSuccess);
    Expect_Float64_IsEqual(123.45678912, Result);

    a = S(".3295739567912567");
    Result = 0;
    bSuccess = String_ToF64(a, &Result);
    Expect_IsTrue(bSuccess);
    Expect_Float64_IsEqual(.3295739567912567, Result);

    a = S("-179769313486231570814527423731704356798070567525844996598917476803157260780028538760589558632766878171540458953514382464234321326889464182768467546703537516986049910576551282076245490090389328944075868508455133942304583236903222948165808559332123348274797826204144723168738177180919299881250404026184124858368.0");
    Result = 0;
    bSuccess = String_ToF64(a, &Result);
    Expect_IsFalse(bSuccess);
    Expect_Float64_IsEqual(0.0, Result);

    a = S("179769313486231570814527423731704356798070567525844996598917476803157260780028538760589558632766878171540458953514382464234321326889464182768467546703537516986049910576551282076245490090389328944075868508455133942304583236903222948165808559332123348274797826204144723168738177180919299881250404026184124858368.0");
    Result = 0;
    bSuccess = String_ToF64(a, &Result);
    Expect_IsFalse(bSuccess);
    Expect_Float64_IsEqual(0.0, Result);

    a = S("0");
    Result = 0;
    bSuccess = String_ToF64(a, &Result);
    Expect_IsTrue(bSuccess);
    Expect_Float64_IsEqual(0.0, Result);

    a = S("-179769313486231570814527423731704356798070567525844996598917476803157260780028538760589558632766878171540458953514382464234321326889464182768467546703537516986049910576551282076245490090389328944075868508455133942304583236903222948165808559332123348274797826204144723168738177180919299881250404026184124858369.0");
    Result = 0;
    bSuccess = String_ToF64(a, &Result);
    Expect_IsFalse(bSuccess);
    Expect_Float64_IsEqual(0.0, Result);

    a = S("179769313486231570814527423731704356798070567525844996598917476803157260780028538760589558632766878171540458953514382464234321326889464182768467546703537516986049910576551282076245490090389328944075868508455133942304583236903222948165808559332123348274797826204144723168738177180919299881250404026184124858369.0");
    Result = 0;
    bSuccess = String_ToF64(a, &Result);
    Expect_IsFalse(bSuccess);
    Expect_Float64_IsEqual(0.0, Result);

    a = S("@z D&W#GGD An");
    Result = 0;
    bSuccess = String_ToF64(a, &Result);
    Expect_IsFalse(bSuccess);
    Expect_Float64_IsEqual(0.0, Result);

    return true;
}

TEST(StringUtils_ToBool)
{
    // Test true, false, 1, 0, malformed inputs

    String a = S("true");
    bool bResult = String_ToBool(a);
    Expect_IsTrue(bResult);

    a = S("false");
    bResult = String_ToBool(a);
    Expect_IsFalse(bResult);

    a = S("1");
    bResult = String_ToBool(a);
    Expect_IsTrue(bResult);

    a = S("0");
    bResult = String_ToBool(a);
    Expect_IsFalse(bResult);

    a = S("$ndAZ*Wgr");
    bResult = String_ToBool(a);
    Expect_IsFalse(bResult);

    a = S("");
    bResult = String_ToBool(a);
    Expect_IsFalse(bResult);

    return true;
}

TEST(PlatformUtils_GetWorkingDirectory)
{
    StringLocal(WorkingDirectory, MAX_PATH_LENGTH);
    Platform_GetWorkingDirectory(&WorkingDirectory);

    Expect_IsNotEqual(0, WorkingDirectory.Length)

    return true;
}

TEST(FilesystemUtils_GetFilePath)
{
    StringLocal(WorkingDirectory, MAX_PATH_LENGTH);
    Platform_GetWorkingDirectory(&WorkingDirectory);

    FileHandle h = FileHandle_Null();
    bool bSuccess = Filesystem_Open(S("../Tests.build"), FileMode_Read, &h);
    Expect_IsTrue(bSuccess);
    Expect_Ptr_IsValid(h.Data);
    StringLocal(Result, MAX_PATH_LENGTH);
    bool bFoundPath = Filesystem_GetFilePath(h, &Result);
    Expect_IsTrue(bFoundPath);
    Expect_Ptr_IsValid(h.Data);
    StringLocal(Expected, MAX_PATH_LENGTH);
    String_BuildPath(&Expected, WorkingDirectory, S("../Tests.build"));
    bool bConversionSuccess = Filesystem_ConvertRelativeToAbsolutePath(&Expected);
    Expect_IsTrue(bConversionSuccess);
    // Expect_String_IsEqual(Expected, Result, false);

    // test a file that doesnt exist
    h = (FileHandle){0};
    bSuccess = Filesystem_Open(S("blah.txt"), FileMode_Read, &h);
    Expect_IsFalse(bSuccess);
    Expect_Ptr_IsNotValid(h.Data);

    // test with invalid file handle
    String_Empty(&Result);
    bFoundPath = Filesystem_GetFilePath(h, &Result);
    Expect_IsEqual(0, Result.Length)
    Expect_String_IsEqual(S(""), Result, false);
    Expect_IsFalse(bFoundPath);

    return true;
}


///////////////////////////////////////////////
// Filesystem: Pure path string function tests
///////////////////////////////////////////////

TEST(Filesystem_DoesPathHaveFileExtension)
{
    Expect_IsTrue(Filesystem_DoesPathHaveFileExtension(S("file.txt")));
    Expect_IsTrue(Filesystem_DoesPathHaveFileExtension(S("path/to/file.txt")));
    Expect_IsTrue(Filesystem_DoesPathHaveFileExtension(S("path\\to\\file.c")));
    Expect_IsTrue(Filesystem_DoesPathHaveFileExtension(S("archive.tar.gz")));
    Expect_IsTrue(Filesystem_DoesPathHaveFileExtension(S(".gitignore")));      // dot at start, letters after
    Expect_IsTrue(Filesystem_DoesPathHaveFileExtension(S("src/.gitignore")));  // dot file inside a directory
    Expect_IsTrue(Filesystem_DoesPathHaveFileExtension(S("file.7z")));     // digit after dot

    Expect_IsFalse(Filesystem_DoesPathHaveFileExtension(S("no_extension")));
    Expect_IsFalse(Filesystem_DoesPathHaveFileExtension(S("path/to/directory")));
    Expect_IsFalse(Filesystem_DoesPathHaveFileExtension(S("trailing_dot.")));
    Expect_IsFalse(Filesystem_DoesPathHaveFileExtension(S("")));

    return true;
}

TEST(Filesystem_ExtractFilePath_WithSlash)
{
    String Result;

    Result = Filesystem_ExtractFilePath(S("path/to/file.txt"), true);
    Expect_String_IsEqual(S("path/to/"), Result, true);

    Result = Filesystem_ExtractFilePath(S("path\\to\\file.txt"), true);
    Expect_String_IsEqual(S("path\\to\\"), Result, true);

    Result = Filesystem_ExtractFilePath(S("C:\\Users\\test\\file.c"), true);
    Expect_String_IsEqual(S("C:\\Users\\test\\"), Result, true);

    // Single file with no directory component
    Result = Filesystem_ExtractFilePath(S("hello.txt"), true);
    Expect_IsEqual(0, (u64)Result.Length);

    return true;
}

TEST(Filesystem_ExtractFilePath_WithoutSlash)
{
    String Result;

    Result = Filesystem_ExtractFilePath(S("path/to/file.txt"), false);
    Expect_String_IsEqual(S("path/to"), Result, true);

    Result = Filesystem_ExtractFilePath(S("path\\to\\file.txt"), false);
    Expect_String_IsEqual(S("path\\to"), Result, true);

    // Edge case: slash at position 0, e.g. "/somefile.txt"
    // Should return "/" and not empty, even with bIncludeSlash=false
    Result = Filesystem_ExtractFilePath(S("/somefile.txt"), false);
    Expect_String_IsEqual(S("/"), Result, true);

    return true;
}

TEST(Filesystem_ExtractFileName_WithExtension)
{
    String Result;

    Result = Filesystem_ExtractFileName(S("path/to/file.txt"), true);
    Expect_String_IsEqual(S("file.txt"), Result, true);

    Result = Filesystem_ExtractFileName(S("path\\to\\file.txt"), true);
    Expect_String_IsEqual(S("file.txt"), Result, true);

    Result = Filesystem_ExtractFileName(S("C:\\Users\\test\\main.c"), true);
    Expect_String_IsEqual(S("main.c"), Result, true);

    // File with no directory prefix
    Result = Filesystem_ExtractFileName(S("hello.txt"), true);
    Expect_String_IsEqual(S("hello.txt"), Result, true);

    // File with multiple dots
    Result = Filesystem_ExtractFileName(S("archive.tar.gz"), true);
    Expect_String_IsEqual(S("archive.tar.gz"), Result, true);

    return true;
}

TEST(Filesystem_ExtractFileName_WithoutExtension)
{
    String Result;

    Result = Filesystem_ExtractFileName(S("path/to/file.txt"), false);
    Expect_String_IsEqual(S("file"), Result, true);

    Result = Filesystem_ExtractFileName(S("path\\to\\file.txt"), false);
    Expect_String_IsEqual(S("file"), Result, true);

    // Multiple dots: strips only the last extension
    Result = Filesystem_ExtractFileName(S("path/archive.tar.gz"), false);
    Expect_String_IsEqual(S("archive.tar"), Result, true);

    // No extension at all
    Result = Filesystem_ExtractFileName(S("path/to/Makefile"), false);
    Expect_String_IsEqual(S("Makefile"), Result, true);

    return true;
}

TEST(Filesystem_StripFileExtension)
{
    String Result;

    Result = Filesystem_StripFileExtension(S("file.txt"));
    Expect_String_IsEqual(S("file"), Result, true);

    Result = Filesystem_StripFileExtension(S("path/to/file.c"));
    Expect_String_IsEqual(S("path/to/file"), Result, true);

    // Multiple extensions: strips only the last one
    Result = Filesystem_StripFileExtension(S("archive.tar.gz"));
    Expect_String_IsEqual(S("archive.tar"), Result, true);

    // No extension: returns the original
    Result = Filesystem_StripFileExtension(S("Makefile"));
    Expect_String_IsEqual(S("Makefile"), Result, true);

    // Empty string
    Result = Filesystem_StripFileExtension(S(""));
    Expect_IsEqual(0, (u64)Result.Length);

    return true;
}

TEST(Filesystem_ExtractFileExtension_WithDot)
{
    String Result;

    Result = Filesystem_ExtractFileExtension(S("file.txt"), true);
    Expect_String_IsEqual(S(".txt"), Result, true);

    Result = Filesystem_ExtractFileExtension(S("path/to/file.c"), true);
    Expect_String_IsEqual(S(".c"), Result, true);

    // Multiple dots: extracts only the last extension
    Result = Filesystem_ExtractFileExtension(S("archive.tar.gz"), true);
    Expect_String_IsEqual(S(".gz"), Result, true);

    // No extension: returns null/empty
    Result = Filesystem_ExtractFileExtension(S("Makefile"), true);
    Expect_IsEqual(0, (u64)Result.Length);

    return true;
}

TEST(Filesystem_ExtractFileExtension_WithoutDot)
{
    String Result;

    Result = Filesystem_ExtractFileExtension(S("file.txt"), false);
    Expect_String_IsEqual(S("txt"), Result, true);

    Result = Filesystem_ExtractFileExtension(S("path/to/file.c"), false);
    Expect_String_IsEqual(S("c"), Result, true);

    Result = Filesystem_ExtractFileExtension(S("archive.tar.gz"), false);
    Expect_String_IsEqual(S("gz"), Result, true);

    // No extension
    Result = Filesystem_ExtractFileExtension(S("Makefile"), false);
    Expect_IsEqual(0, (u64)Result.Length);

    return true;
}

TEST(Filesystem_IsPathRelative)
{
    // Absolute paths on Windows have a drive letter with colon
    Expect_IsFalse(Filesystem_IsPathRelative(S("C:\\Users\\test")));
    Expect_IsFalse(Filesystem_IsPathRelative(S("D:\\folder")));
    Expect_IsFalse(Filesystem_IsPathRelative(S("C:/Users/test")));

    // Relative paths
    Expect_IsTrue(Filesystem_IsPathRelative(S("relative/path")));
    Expect_IsTrue(Filesystem_IsPathRelative(S("..\\parent")));
    Expect_IsTrue(Filesystem_IsPathRelative(S("./current")));
    Expect_IsTrue(Filesystem_IsPathRelative(S("file.txt")));
    Expect_IsTrue(Filesystem_IsPathRelative(S("")));

    return true;
}

TEST(Filesystem_ArePathsCommon)
{
    // PathA is a prefix of PathB
    Expect_IsTrue(Filesystem_ArePathsCommon(S("C:\\Users"), S("C:\\Users\\test\\file.txt")));

    // Identical paths
    Expect_IsTrue(Filesystem_ArePathsCommon(S("C:\\Foo"), S("C:\\Foo")));

    // PathA is NOT a prefix of PathB
    Expect_IsFalse(Filesystem_ArePathsCommon(S("C:\\Users\\Alice"), S("C:\\Users\\Bob")));

    // Completely different paths
    Expect_IsFalse(Filesystem_ArePathsCommon(S("C:\\Foo"), S("D:\\Bar")));

    return true;
}

TEST(Filesystem_AppendExeExtension)
{
    #if PLATFORM_WINDOWS
    // Should append .exe on Windows
    StringLocal(Path1, 256);
    String_Append(&Path1, S("myprogram"));
    Filesystem_AppendExeExtension(&Path1);
    Expect_String_IsEqual(S("myprogram.exe"), Path1, false);

    // Should NOT double-append .exe
    StringLocal(Path2, 256);
    String_Append(&Path2, S("myprogram.exe"));
    Filesystem_AppendExeExtension(&Path2);
    Expect_String_IsEqual(S("myprogram.exe"), Path2, false);
    #endif

    return true;
}


///////////////////////////////////////////////
// Filesystem: File I/O tests
///////////////////////////////////////////////

TEST(Filesystem_NewFile_DeleteFile_DoesExist)
{
    String TmpFile = S("__test_tmp_file_create.txt");

    // Ensure the file does not exist before we start
    if (Filesystem_DoesFileExist(TmpFile))
    {
        xx Filesystem_DeleteFile(TmpFile);
    }
    Expect_IsFalse(Filesystem_DoesFileExist(TmpFile));

    // Create new file
    bool bCreated = Filesystem_NewFile(TmpFile);
    Expect_IsTrue(bCreated);
    Expect_IsTrue(Filesystem_DoesFileExist(TmpFile));

    // Delete the file
    bool bDeleted = Filesystem_DeleteFile(TmpFile);
    Expect_IsTrue(bDeleted);
    Expect_IsFalse(Filesystem_DoesFileExist(TmpFile));

    return true;
}

TEST(Filesystem_DoesDirectoryExist)
{
    // The current working directory should exist (we are running from Tests/Core)
    Expect_IsTrue(Filesystem_DoesDirectoryExist(S(".")));

    // A non-existent directory
    Expect_IsFalse(Filesystem_DoesDirectoryExist(S("__nonexistent_dir_12345")));

    return true;
}

TEST(Filesystem_OpenWriteReadClose)
{
    String TmpFile = S("__test_tmp_file_rw.txt");

    // Cleanup from any prior failed run
    if (Filesystem_DoesFileExist(TmpFile))
    {
        xx Filesystem_DeleteFile(TmpFile);
    }

    // Create the file
    bool bCreated = Filesystem_NewFile(TmpFile);
    Expect_IsTrue(bCreated);

    // Open for writing
    FileHandle hWrite = FileHandle_Null();
    bool bOpenWrite = Filesystem_Open(TmpFile, FileMode_Write, &hWrite);
    Expect_IsTrue(bOpenWrite);
    Expect_Ptr_IsValid(hWrite.Data);

    // Write data
    String Content = S("Hello, Filesystem!");
    usize BytesWritten = 0;
    bool bWriteOk = Filesystem_Write(hWrite, Content.Length, Content.Data, &BytesWritten);
    Expect_IsTrue(bWriteOk);
    Expect_IsEqual((u64)Content.Length, (u64)BytesWritten);

    Filesystem_Close(&hWrite);

    // Open for reading
    FileHandle hRead = FileHandle_Null();
    bool bOpenRead = Filesystem_Open(TmpFile, FileMode_Read, &hRead);
    Expect_IsTrue(bOpenRead);
    Expect_Ptr_IsValid(hRead.Data);

    // Verify file size
    usize FileSize = 0;
    bool bGotSize = Filesystem_GetFileSize(hRead, &FileSize);
    Expect_IsTrue(bGotSize);
    Expect_IsEqual((u64)Content.Length, (u64)FileSize);

    // Read data back
    u8 ReadBuffer[256] = {0};
    usize BytesRead = 0;
    bool bReadOk = Filesystem_Read(hRead, FileSize, ReadBuffer, &BytesRead);
    Expect_IsTrue(bReadOk);
    Expect_IsEqual((u64)FileSize, (u64)BytesRead);

    String ReadString = StrSlice(ReadBuffer, (u32)BytesRead);
    Expect_String_IsEqual(Content, ReadString, true);

    Filesystem_Close(&hRead);

    // Cleanup
    xx Filesystem_DeleteFile(TmpFile);

    return true;
}

TEST(Filesystem_GetFileSize)
{
    String TmpFile = S("__test_tmp_file_size.txt");

    if (Filesystem_DoesFileExist(TmpFile))
    {
        xx Filesystem_DeleteFile(TmpFile);
    }

    xx Filesystem_NewFile(TmpFile);

    // Write known data
    FileHandle hWrite = FileHandle_Null();
    xx Filesystem_Open(TmpFile, FileMode_Write, &hWrite);
    String Data = S("12345");
    usize BytesWritten = 0;
    Filesystem_Write(hWrite, Data.Length, Data.Data, &BytesWritten);
    Filesystem_Close(&hWrite);

    // Open and check size
    FileHandle hRead = FileHandle_Null();
    xx Filesystem_Open(TmpFile, FileMode_Read, &hRead);
    usize Size = 0;
    bool bGotSize = Filesystem_GetFileSize(hRead, &Size);
    Expect_IsTrue(bGotSize);
    Expect_IsEqual(5, (u64)Size);
    Filesystem_Close(&hRead);

    xx Filesystem_DeleteFile(TmpFile);

    return true;
}

TEST(Filesystem_SeekOperations)
{
    String TmpFile = S("__test_tmp_file_seek.txt");

    if (Filesystem_DoesFileExist(TmpFile))
    {
        xx Filesystem_DeleteFile(TmpFile);
    }

    xx Filesystem_NewFile(TmpFile);

    // Write "ABCDEFGHIJ" (10 bytes)
    FileHandle hWrite = FileHandle_Null();
    xx Filesystem_Open(TmpFile, FileMode_Write, &hWrite);
    String Data = S("ABCDEFGHIJ");
    usize BytesWritten = 0;
    Filesystem_Write(hWrite, Data.Length, Data.Data, &BytesWritten);
    Filesystem_Close(&hWrite);

    // Open for reading
    FileHandle hRead = FileHandle_Null();
    xx Filesystem_Open(TmpFile, FileMode_Read, &hRead);

    // SeekFromBeginning to offset 5, read "FGHIJ"
    bool bSeek = Filesystem_SeekFromBeginning(hRead, 5);
    Expect_IsTrue(bSeek);

    usize Pos = Filesystem_GetCurrentFilePosition(hRead);
    Expect_IsEqual(5, (u64)Pos);

    u8 Buf[16] = {0};
    usize BytesRead = 0;
    xx Filesystem_Read(hRead, 5, Buf, &BytesRead);
    Expect_IsEqual(5, (u64)BytesRead);
    Expect_String_IsEqual(S("FGHIJ"), StrSlice(Buf, (u32)BytesRead), true);

    // SeekToBeginning, read "ABCDE"
    xx Filesystem_SeekToBeginning(hRead);
    Pos = Filesystem_GetCurrentFilePosition(hRead);
    Expect_IsEqual(0, (u64)Pos);

    MemZero(Buf, sizeof(Buf));
    xx Filesystem_Read(hRead, 5, Buf, &BytesRead);
    Expect_String_IsEqual(S("ABCDE"), StrSlice(Buf, (u32)BytesRead), true);

    // SeekToEnd
    xx Filesystem_SeekToEnd(hRead);
    Pos = Filesystem_GetCurrentFilePosition(hRead);
    Expect_IsEqual(10, (u64)Pos);

    Filesystem_Close(&hRead);
    xx Filesystem_DeleteFile(TmpFile);

    return true;
}

TEST(Filesystem_IsNewerIsOlder)
{
    String FileA = S("__test_tmp_file_a.txt");
    String FileB = S("__test_tmp_file_b.txt");

    // Cleanup
    if (Filesystem_DoesFileExist(FileA)) { xx Filesystem_DeleteFile(FileA); }
    if (Filesystem_DoesFileExist(FileB)) { xx Filesystem_DeleteFile(FileB); }

    // Create FileA first
    xx Filesystem_NewFile(FileA);

    // Write something so the timestamp is set
    FileHandle hA = FileHandle_Null();
    xx Filesystem_Open(FileA, FileMode_Write, &hA);
    usize Dummy = 0;
    Filesystem_Write(hA, 1, "A", &Dummy);
    Filesystem_Close(&hA);

    // Small delay to ensure different timestamps
    Platform_Wait(50 milliseconds);

    // Create FileB second (should be newer)
    xx Filesystem_NewFile(FileB);
    FileHandle hB = FileHandle_Null();
    xx Filesystem_Open(FileB, FileMode_Write, &hB);
    Filesystem_Write(hB, 1, "B", &Dummy);
    Filesystem_Close(&hB);

    // FileB should be newer than FileA
    Expect_IsTrue(Filesystem_IsNewer(FileB, FileA));
    Expect_IsFalse(Filesystem_IsNewer(FileA, FileB));

    // FileA should be older than FileB
    Expect_IsTrue(Filesystem_IsOlder(FileA, FileB));
    Expect_IsFalse(Filesystem_IsOlder(FileB, FileA));

    xx Filesystem_DeleteFile(FileA);
    xx Filesystem_DeleteFile(FileB);

    return true;
}

TEST(Filesystem_CopyFile)
{
    String Src = S("__test_tmp_file_copy_src.txt");
    String DstDir = S("__test_tmp_copy_dir");
    String DstFile = S("__test_tmp_copy_dir\\__test_tmp_file_copy_src.txt");

    if (Filesystem_DoesFileExist(Src)) { xx Filesystem_DeleteFile(Src); }
    if (Filesystem_DoesFileExist(DstFile)) { xx Filesystem_DeleteFile(DstFile); }
    if (Filesystem_DoesDirectoryExist(DstDir)) { xx Filesystem_DeleteDirectory(DstDir); }

    // Create source file with content
    xx Filesystem_NewFile(Src);
    FileHandle hWrite = FileHandle_Null();
    xx Filesystem_Open(Src, FileMode_Write, &hWrite);
    String Content = S("copy me");
    usize BytesWritten = 0;
    Filesystem_Write(hWrite, Content.Length, Content.Data, &BytesWritten);
    Filesystem_Close(&hWrite);

    // Copy to a destination directory — Filesystem_Copy auto-appends the source filename
    bool bCopied = Filesystem_Copy(Src, DstDir);
    Expect_IsTrue(bCopied);
    Expect_IsTrue(Filesystem_DoesFileExist(DstFile));

    // Verify destination content matches source
    FileHandle hRead = FileHandle_Null();
    xx Filesystem_Open(DstFile, FileMode_Read, &hRead);
    usize DstSize = 0;
    xx Filesystem_GetFileSize(hRead, &DstSize);
    Expect_IsEqual((u64)Content.Length, (u64)DstSize);

    u8 Buf[64] = {0};
    usize BytesRead = 0;
    xx Filesystem_Read(hRead, DstSize, Buf, &BytesRead);
    Expect_String_IsEqual(Content, StrSlice(Buf, (u32)BytesRead), true);
    Filesystem_Close(&hRead);

    // Source should still exist after copy
    Expect_IsTrue(Filesystem_DoesFileExist(Src));

    xx Filesystem_DeleteFile(Src);
    xx Filesystem_DeleteFile(DstFile);
    xx Filesystem_DeleteDirectory(DstDir);

    return true;
}

TEST(Filesystem_MoveFile)
{
    String Src = S("__test_tmp_file_move_src.txt");
    String Dst = S("__test_tmp_file_move_dst.txt");

    if (Filesystem_DoesFileExist(Src)) { xx Filesystem_DeleteFile(Src); }
    if (Filesystem_DoesFileExist(Dst)) { xx Filesystem_DeleteFile(Dst); }

    // Create source file with content
    xx Filesystem_NewFile(Src);
    FileHandle hWrite = FileHandle_Null();
    xx Filesystem_Open(Src, FileMode_Write, &hWrite);
    String Content = S("move me");
    usize BytesWritten = 0;
    Filesystem_Write(hWrite, Content.Length, Content.Data, &BytesWritten);
    Filesystem_Close(&hWrite);

    // Move (rename)
    bool bMoved = Filesystem_Move(Src, Dst, true);
    Expect_IsTrue(bMoved);
    Expect_IsTrue(Filesystem_DoesFileExist(Dst));
    Expect_IsFalse(Filesystem_DoesFileExist(Src));

    // Verify destination content
    FileHandle hRead = FileHandle_Null();
    xx Filesystem_Open(Dst, FileMode_Read, &hRead);
    u8 Buf[64] = {0};
    usize BytesRead = 0;
    xx Filesystem_Read(hRead, Content.Length, Buf, &BytesRead);
    Expect_String_IsEqual(Content, StrSlice(Buf, (u32)BytesRead), true);
    Filesystem_Close(&hRead);

    xx Filesystem_DeleteFile(Dst);

    return true;
}

TEST(Filesystem_WriteLine)
{
    String TmpFile = S("__test_tmp_file_writeline.txt");

    if (Filesystem_DoesFileExist(TmpFile))
    {
        xx Filesystem_DeleteFile(TmpFile);
    }

    xx Filesystem_NewFile(TmpFile);

    // Write a line
    FileHandle hWrite = FileHandle_Null();
    xx Filesystem_Open(TmpFile, FileMode_Write, &hWrite);
    usize BytesWritten = 0;
    bool bOk = Filesystem_WriteLine(hWrite, S("Hello Line"), &BytesWritten);
    Expect_IsTrue(bOk);
    Expect_IsEqual(10, (u64)BytesWritten);
    Filesystem_Close(&hWrite);

    xx Filesystem_DeleteFile(TmpFile);

    return true;
}

TEST(Filesystem_MemoryMapped)
{
    String TmpFile = S("__test_tmp_file_mmap.txt");

    if (Filesystem_DoesFileExist(TmpFile))
    {
        xx Filesystem_DeleteFile(TmpFile);
    }

    // Create file with known content
    xx Filesystem_NewFile(TmpFile);
    FileHandle hWrite = FileHandle_Null();
    xx Filesystem_Open(TmpFile, FileMode_Write, &hWrite);
    String Content = S("HelloMMAP");
    usize BytesWritten = 0;
    Filesystem_Write(hWrite, Content.Length, Content.Data, &BytesWritten);
    Filesystem_Close(&hWrite);

    // Open memory-mapped for reading
    MemoryMappedFile MmFile = {0};
    bool bOpened = Filesystem_Open_MemoryMapped(TmpFile, FileMode_Read, &MmFile);
    Expect_IsTrue(bOpened);
    Expect_Ptr_IsValid(MmFile.Data);
    Expect_IsEqual((u64)Content.Length, (u64)MmFile.Size);
    Expect_String_IsEqual(Content, StrSlice(MmFile.Data, (u32)MmFile.Size), true);

    Filesystem_Close_MemoryMapped(&MmFile);
    xx Filesystem_DeleteFile(TmpFile);

    return true;
}

TEST(Filesystem_OpenDirectory)
{
    String TmpDir = S("__test_tmp_dir_open");

    if (Filesystem_DoesDirectoryExist(TmpDir))
    {
        xx Filesystem_DeleteDirectory(TmpDir);
    }

    bool bCreated = Filesystem_OpenDirectory(TmpDir);
    Expect_IsTrue(bCreated);
    Expect_IsTrue(Filesystem_DoesDirectoryExist(TmpDir));

    xx Filesystem_DeleteDirectory(TmpDir);

    return true;
}

TEST(Filesystem_OpenDirectory_Ex)
{
    String TmpDir = S("__test_tmp_dir_open_ex");

    if (Filesystem_DoesDirectoryExist(TmpDir))
    {
        xx Filesystem_DeleteDirectory(TmpDir);
    }

    FileHandle hDir = FileHandle_Null();
    bool bCreated = Filesystem_OpenDirectory_Ex(TmpDir, &hDir);
    Expect_IsTrue(bCreated);
    Expect_IsTrue(IsValidFileHandle(hDir));

    Filesystem_Close(&hDir);
    xx Filesystem_DeleteDirectory(TmpDir);

    return true;
}

TEST(Filesystem_Seek_Relative)
{
    String TmpFile = S("__test_tmp_file_seek_rel.txt");

    if (Filesystem_DoesFileExist(TmpFile))
    {
        xx Filesystem_DeleteFile(TmpFile);
    }

    xx Filesystem_NewFile(TmpFile);

    // Write "ABCDEFGHIJ"
    FileHandle hWrite = FileHandle_Null();
    xx Filesystem_Open(TmpFile, FileMode_Write, &hWrite);
    String Data = S("ABCDEFGHIJ");
    usize BytesWritten = 0;
    Filesystem_Write(hWrite, Data.Length, Data.Data, &BytesWritten);
    Filesystem_Close(&hWrite);

    // Open for reading, seek from beginning to 3, then relative seek +2
    FileHandle hRead = FileHandle_Null();
    xx Filesystem_Open(TmpFile, FileMode_Read, &hRead);

    xx Filesystem_SeekFromBeginning(hRead, 3);
    bool bSeek = Filesystem_Seek(hRead, 2);
    Expect_IsTrue(bSeek);

    usize Pos = Filesystem_GetCurrentFilePosition(hRead);
    Expect_IsEqual(5, (u64)Pos);

    u8 Buf[16] = {0};
    usize BytesRead = 0;
    xx Filesystem_Read(hRead, 5, Buf, &BytesRead);
    Expect_IsEqual(5, (u64)BytesRead);
    Expect_String_IsEqual(S("FGHIJ"), StrSlice(Buf, (u32)BytesRead), true);

    Filesystem_Close(&hRead);
    xx Filesystem_DeleteFile(TmpFile);

    return true;
}

TEST(Filesystem_SeekFromEnd)
{
    String TmpFile = S("__test_tmp_file_seek_end.txt");

    if (Filesystem_DoesFileExist(TmpFile))
    {
        xx Filesystem_DeleteFile(TmpFile);
    }

    xx Filesystem_NewFile(TmpFile);

    // Write "ABCDEFGHIJ" (10 bytes)
    FileHandle hWrite = FileHandle_Null();
    xx Filesystem_Open(TmpFile, FileMode_Write, &hWrite);
    String Data = S("ABCDEFGHIJ");
    usize BytesWritten = 0;
    Filesystem_Write(hWrite, Data.Length, Data.Data, &BytesWritten);
    Filesystem_Close(&hWrite);

    // SeekFromEnd with offset 0 should position at end of file
    FileHandle hRead = FileHandle_Null();
    xx Filesystem_Open(TmpFile, FileMode_Read, &hRead);

    bool bSeek = Filesystem_SeekFromEnd(hRead, 0);
    Expect_IsTrue(bSeek);

    usize Pos = Filesystem_GetCurrentFilePosition(hRead);
    Expect_IsEqual(10, (u64)Pos);

    Filesystem_Close(&hRead);
    xx Filesystem_DeleteFile(TmpFile);

    return true;
}

TEST(Filesystem_GetTimes_ByPath)
{
    String TmpFile = S("__test_tmp_file_times.txt");

    if (Filesystem_DoesFileExist(TmpFile))
    {
        xx Filesystem_DeleteFile(TmpFile);
    }

    xx Filesystem_NewFile(TmpFile);
    FileHandle hWrite = FileHandle_Null();
    xx Filesystem_Open(TmpFile, FileMode_Write, &hWrite);
    usize Dummy = 0;
    Filesystem_Write(hWrite, 1, "X", &Dummy);
    Filesystem_Close(&hWrite);

    usize WriteTime  = Filesystem_GetLastWriteTime(TmpFile);
    usize AccessTime = Filesystem_GetLastAccessTime(TmpFile);
    usize CreateTime = Filesystem_GetCreationTime(TmpFile);

    Expect_IsNotEqual(0, (u64)WriteTime);
    Expect_IsNotEqual(0, (u64)AccessTime);
    Expect_IsNotEqual(0, (u64)CreateTime);

    xx Filesystem_DeleteFile(TmpFile);

    return true;
}

TEST(Filesystem_GetTimes_ByHandle)
{
    String TmpFile = S("__test_tmp_file_timesh.txt");

    if (Filesystem_DoesFileExist(TmpFile))
    {
        xx Filesystem_DeleteFile(TmpFile);
    }

    xx Filesystem_NewFile(TmpFile);
    FileHandle hFile = FileHandle_Null();
    xx Filesystem_Open(TmpFile, FileMode_Write, &hFile);
    usize Dummy = 0;
    Filesystem_Write(hFile, 1, "X", &Dummy);

    usize WriteTime  = Filesystem_GetLastWriteTimeH(hFile);
    usize AccessTime = Filesystem_GetLastAccessTimeH(hFile);
    usize CreateTime = Filesystem_GetCreationTimeH(hFile);

    Expect_IsNotEqual(0, (u64)WriteTime);
    Expect_IsNotEqual(0, (u64)AccessTime);
    Expect_IsNotEqual(0, (u64)CreateTime);

    Filesystem_Close(&hFile);
    xx Filesystem_DeleteFile(TmpFile);

    return true;
}

TEST(Filesystem_GetFileTime_ByPath)
{
    String TmpFile = S("__test_tmp_file_ftime.txt");

    if (Filesystem_DoesFileExist(TmpFile))
    {
        xx Filesystem_DeleteFile(TmpFile);
    }

    xx Filesystem_NewFile(TmpFile);
    FileHandle hWrite = FileHandle_Null();
    xx Filesystem_Open(TmpFile, FileMode_Write, &hWrite);
    usize Dummy = 0;
    Filesystem_Write(hWrite, 1, "X", &Dummy);
    Filesystem_Close(&hWrite);

    FileTimeData FTD = Filesystem_GetFileTime(TmpFile);
    Expect_IsNotEqual(0, (u64)FTD.CreationTime);
    Expect_IsNotEqual(0, (u64)FTD.LastAccessTime);
    Expect_IsNotEqual(0, (u64)FTD.LastWriteTime);

    xx Filesystem_DeleteFile(TmpFile);

    return true;
}

TEST(Filesystem_GetFileTime_ByHandle)
{
    String TmpFile = S("__test_tmp_file_ftimeh.txt");

    if (Filesystem_DoesFileExist(TmpFile))
    {
        xx Filesystem_DeleteFile(TmpFile);
    }

    xx Filesystem_NewFile(TmpFile);
    FileHandle hFile = FileHandle_Null();
    xx Filesystem_Open(TmpFile, FileMode_Write, &hFile);
    usize Dummy = 0;
    Filesystem_Write(hFile, 1, "X", &Dummy);

    FileTimeData FTD = Filesystem_GetFileTimeH(hFile);
    Expect_IsNotEqual(0, (u64)FTD.CreationTime);
    Expect_IsNotEqual(0, (u64)FTD.LastAccessTime);
    Expect_IsNotEqual(0, (u64)FTD.LastWriteTime);

    Filesystem_Close(&hFile);
    xx Filesystem_DeleteFile(TmpFile);

    return true;
}

TEST(Filesystem_ReadEntireFile)
{
    String TmpFile = S("__test_tmp_file_readall.txt");

    if (Filesystem_DoesFileExist(TmpFile))
    {
        xx Filesystem_DeleteFile(TmpFile);
    }

    // Write known content
    xx Filesystem_NewFile(TmpFile);
    FileHandle hWrite = FileHandle_Null();
    xx Filesystem_Open(TmpFile, FileMode_Write, &hWrite);
    String Content = S("Read entire file test data!");
    usize BytesWritten = 0;
    Filesystem_Write(hWrite, Content.Length, Content.Data, &BytesWritten);
    Filesystem_Close(&hWrite);

    // Read entire file
    FileHandle hRead = FileHandle_Null();
    xx Filesystem_Open(TmpFile, FileMode_Read, &hRead);
    usize FileSize = 0;
    xx Filesystem_GetFileSize(hRead, &FileSize);
    Expect_IsEqual((u64)Content.Length, (u64)FileSize);

    u8 Buf[128] = {0};
    usize BytesRead = 0;
    bool bOk = Filesystem_ReadEntireFile(hRead, Buf, &BytesRead);
    Expect_IsTrue(bOk);
    Expect_IsEqual((u64)Content.Length, (u64)BytesRead);
    Expect_String_IsEqual(Content, StrSlice(Buf, (u32)BytesRead), true);

    Filesystem_Close(&hRead);
    xx Filesystem_DeleteFile(TmpFile);

    return true;
}

TEST(Filesystem_ReadLine)
{
    String TmpFile = S("__test_tmp_file_readline.txt");

    if (Filesystem_DoesFileExist(TmpFile))
    {
        xx Filesystem_DeleteFile(TmpFile);
    }

    // Write two lines separated by \r\n (Windows line ending)
    xx Filesystem_NewFile(TmpFile);
    FileHandle hWrite = FileHandle_Null();
    xx Filesystem_Open(TmpFile, FileMode_Write, &hWrite);
    String RawContent = S("First Line\r\nSecond Line\r\n");
    usize BytesWritten = 0;
    Filesystem_Write(hWrite, RawContent.Length, RawContent.Data, &BytesWritten);
    Filesystem_Close(&hWrite);

    // Read lines back
    FileHandle hRead = FileHandle_Null();
    xx Filesystem_Open(TmpFile, FileMode_Read, &hRead);

    StringLocal(LineBuf, 256);
    bool bLine1 = Filesystem_ReadLine(hRead, &LineBuf);
    Expect_IsTrue(bLine1);
    Expect_String_IsEqual(S("First Line"), StrMake(LineBuf), true);

    String_Empty(&LineBuf);
    bool bLine2 = Filesystem_ReadLine(hRead, &LineBuf);
    Expect_IsTrue(bLine2);
    Expect_String_IsEqual(S("Second Line"), StrMake(LineBuf), true);

    Filesystem_Close(&hRead);
    xx Filesystem_DeleteFile(TmpFile);

    return true;
}

TEST(Filesystem_WriteLineFormatted)
{
    String TmpFile = S("__test_tmp_file_wlfmt.txt");

    if (Filesystem_DoesFileExist(TmpFile))
    {
        xx Filesystem_DeleteFile(TmpFile);
    }

    xx Filesystem_NewFile(TmpFile);
    FileHandle hWrite = FileHandle_Null();
    xx Filesystem_Open(TmpFile, FileMode_Write, &hWrite);
    usize BytesWritten = 0;
    bool bOk = Filesystem_WriteLineFormatted(hWrite, S("Value is %d"), &BytesWritten, 42);
    Expect_IsTrue(bOk);
    Filesystem_Close(&hWrite);

    // Read back and verify
    FileHandle hRead = FileHandle_Null();
    xx Filesystem_Open(TmpFile, FileMode_Read, &hRead);
    u8 Buf[128] = {0};
    usize BytesRead = 0;
    xx Filesystem_ReadEntireFile(hRead, Buf, &BytesRead);
    Expect_String_IsEqual(S("Value is 42"), StrSlice(Buf, (u32)BytesRead), true);
    Filesystem_Close(&hRead);

    xx Filesystem_DeleteFile(TmpFile);

    return true;
}

TEST(Filesystem_IsFile)
{
    String TmpFile = S("__test_tmp_file_isfile.txt");
    String TmpDir = S("__test_tmp_dir_isfile");

    if (Filesystem_DoesFileExist(TmpFile)) { xx Filesystem_DeleteFile(TmpFile); }
    if (Filesystem_DoesDirectoryExist(TmpDir)) { xx Filesystem_DeleteDirectory(TmpDir); }

    xx Filesystem_NewFile(TmpFile);
    xx Filesystem_OpenDirectory(TmpDir);

    Expect_IsTrue(Filesystem_IsFile(TmpFile));
    Expect_IsFalse(Filesystem_IsFile(TmpDir));
    Expect_IsFalse(Filesystem_IsFile(S("__test_nonexistent_path_xyz")));

    xx Filesystem_DeleteFile(TmpFile);
    xx Filesystem_DeleteDirectory(TmpDir);

    return true;
}

TEST(Filesystem_IsDirectory)
{
    String TmpFile = S("__test_tmp_file_isdir.txt");

    if (Filesystem_DoesFileExist(TmpFile)) { xx Filesystem_DeleteFile(TmpFile); }

    xx Filesystem_NewFile(TmpFile);

    Expect_IsTrue(Filesystem_IsDirectory(S(".")));
    Expect_IsFalse(Filesystem_IsDirectory(TmpFile));
    Expect_IsFalse(Filesystem_IsDirectory(S("__test_nonexistent_dir_xyz")));

    xx Filesystem_DeleteFile(TmpFile);

    return true;
}

TEST(Filesystem_ConvertRelativeToAbsolutePath)
{
    // PathCanonicalize resolves . and .. components but does not prepend a drive letter.
    // Test that "foo\bar\..\baz" canonicalizes to "foo\baz".
    StringLocal(Path, 512);
    String_Copy(&Path, S("foo\\bar\\..\\baz"));

    bool bConverted = Filesystem_ConvertRelativeToAbsolutePath(&Path);
    Expect_IsTrue(bConverted);
    Expect_String_IsEqual(S("foo\\baz"), StrMake(Path), true);

    // Test with a redundant current-dir component: ".\somefile.txt" -> "somefile.txt"
    String_Copy(&Path, S(".\\somefile.txt"));
    bConverted = Filesystem_ConvertRelativeToAbsolutePath(&Path);
    Expect_IsTrue(bConverted);
    Expect_String_IsEqual(S("somefile.txt"), StrMake(Path), true);

    return true;
}

static bool IterateDirectory_Counter(const String FullPath, const String RelativePath, const String FileName, u64 FileSize, bool bIsDirectory, void* UserData)
{
    (void)FullPath;
    (void)RelativePath;
    (void)FileName;
    (void)FileSize;
    (void)bIsDirectory;
    (void)UserData;
    g_IterDirCount++;
    return true;
}

static bool IterateDirectory_CounterEx(const String FullPath, const String RelativePath, const String FileName, u64 FileSize, bool bIsDirectory, void* UserData)
{
    (void)FullPath;
    (void)RelativePath;
    (void)FileName;
    (void)FileSize;
    (void)bIsDirectory;
    i32* Counter = (i32*)UserData;
    (*Counter)++;
    return true;
}

TEST(Filesystem_IterateDirectory)
{
    String TmpDir = S("__test_tmp_dir_iter");
    String File1 = S("__test_tmp_dir_iter\\file1.txt");
    String File2 = S("__test_tmp_dir_iter\\file2.txt");
    String File3 = S("__test_tmp_dir_iter\\file3.txt");

    // Cleanup
    if (Filesystem_DoesFileExist(File1)) { xx Filesystem_DeleteFile(File1); }
    if (Filesystem_DoesFileExist(File2)) { xx Filesystem_DeleteFile(File2); }
    if (Filesystem_DoesFileExist(File3)) { xx Filesystem_DeleteFile(File3); }
    if (Filesystem_DoesDirectoryExist(TmpDir)) { xx Filesystem_DeleteDirectory(TmpDir); }

    xx Filesystem_OpenDirectory(TmpDir);
    xx Filesystem_NewFile(File1);
    xx Filesystem_NewFile(File2);
    xx Filesystem_NewFile(File3);

    g_IterDirCount = 0;
    Filesystem_IterateDirectory(TmpDir, IterateDirectory_Counter, false);
    Expect_IsEqual(3, (u64)g_IterDirCount);

    xx Filesystem_DeleteFile(File1);
    xx Filesystem_DeleteFile(File2);
    xx Filesystem_DeleteFile(File3);
    xx Filesystem_DeleteDirectory(TmpDir);

    return true;
}

TEST(Filesystem_IterateDirectory_Ex)
{
    String TmpDir = S("__test_tmp_dir_iter_ex");
    String File1 = S("__test_tmp_dir_iter_ex\\file1.txt");
    String File2 = S("__test_tmp_dir_iter_ex\\file2.txt");

    // Cleanup
    if (Filesystem_DoesFileExist(File1)) { xx Filesystem_DeleteFile(File1); }
    if (Filesystem_DoesFileExist(File2)) { xx Filesystem_DeleteFile(File2); }
    if (Filesystem_DoesDirectoryExist(TmpDir)) { xx Filesystem_DeleteDirectory(TmpDir); }

    xx Filesystem_OpenDirectory(TmpDir);
    xx Filesystem_NewFile(File1);
    xx Filesystem_NewFile(File2);

    i32 Counter = 0;
    Filesystem_IterateDirectory_Ex(TmpDir, IterateDirectory_CounterEx, false, &Counter);
    Expect_IsEqual(2, (u64)Counter);

    xx Filesystem_DeleteFile(File1);
    xx Filesystem_DeleteFile(File2);
    xx Filesystem_DeleteDirectory(TmpDir);

    return true;
}

TEST(Filesystem_DeleteFiles)
{
    String TmpDir = S("__test_tmp_dir_delfiles");
    String File1 = S("__test_tmp_dir_delfiles\\a.tmp");
    String File2 = S("__test_tmp_dir_delfiles\\b.tmp");
    String File3 = S("__test_tmp_dir_delfiles\\c.txt");

    // Cleanup
    if (Filesystem_DoesFileExist(File1)) { xx Filesystem_DeleteFile(File1); }
    if (Filesystem_DoesFileExist(File2)) { xx Filesystem_DeleteFile(File2); }
    if (Filesystem_DoesFileExist(File3)) { xx Filesystem_DeleteFile(File3); }
    if (Filesystem_DoesDirectoryExist(TmpDir)) { xx Filesystem_DeleteDirectory(TmpDir); }

    xx Filesystem_OpenDirectory(TmpDir);
    xx Filesystem_NewFile(File1);
    xx Filesystem_NewFile(File2);
    xx Filesystem_NewFile(File3);

    // Delete only *.tmp files
    bool bDeleted = Filesystem_DeleteFiles(TmpDir, S("*.tmp"), false);
    Expect_IsTrue(bDeleted);

    // .tmp files should be gone, .txt should remain
    Expect_IsFalse(Filesystem_DoesFileExist(File1));
    Expect_IsFalse(Filesystem_DoesFileExist(File2));
    Expect_IsTrue(Filesystem_DoesFileExist(File3));

    xx Filesystem_DeleteFile(File3);
    xx Filesystem_DeleteDirectory(TmpDir);

    return true;
}

TEST(Filesystem_DeleteDirectory)
{
    String TmpDir = S("__test_tmp_dir_delete");

    if (Filesystem_DoesDirectoryExist(TmpDir))
    {
        xx Filesystem_DeleteDirectory(TmpDir);
    }

    xx Filesystem_OpenDirectory(TmpDir);
    Expect_IsTrue(Filesystem_DoesDirectoryExist(TmpDir));

    bool bDeleted = Filesystem_DeleteDirectory(TmpDir);
    Expect_IsTrue(bDeleted);
    Expect_IsFalse(Filesystem_DoesDirectoryExist(TmpDir));

    return true;
}

TEST(Filesystem_IsValidFileHandle)
{
    // Null handle should be invalid
    FileHandle hNull = FileHandle_Null();
    Expect_IsFalse(IsValidFileHandle(hNull));

    // Opened handle should be valid
    String TmpFile = S("__test_tmp_file_validh.txt");
    if (Filesystem_DoesFileExist(TmpFile)) { xx Filesystem_DeleteFile(TmpFile); }

    xx Filesystem_NewFile(TmpFile);
    FileHandle hFile = FileHandle_Null();
    xx Filesystem_Open(TmpFile, FileMode_Read, &hFile);
    Expect_IsTrue(IsValidFileHandle(hFile));

    Filesystem_Close(&hFile);
    xx Filesystem_DeleteFile(TmpFile);

    return true;
}


/////////////////////////////////
// MathUtils tests             //
/////////////////////////////////

TEST(MathUtils_Abs)
{
    Expect_Float_IsEqual(3.5f, Abs(3.5f));
    Expect_Float_IsEqual(3.5f, Abs(-3.5f));
    Expect_Float_IsEqual(0.0f, Abs(0.0f));
    Expect_Float_IsEqual(0.0f, Abs(-0.0f));

    return true;
}

TEST(MathUtils_Absf64)
{
    Expect_Float64_IsEqual(3.5, Absf64(3.5));
    Expect_Float64_IsEqual(3.5, Absf64(-3.5));
    Expect_Float64_IsEqual(0.0, Absf64(0.0));
    Expect_Float64_IsEqual(0.0, Absf64(-0.0));

    return true;
}

TEST(MathUtils_Absi32)
{
    Expect_IsEqual(5, Absi32(5));
    Expect_IsEqual(5, Absi32(-5));
    Expect_IsEqual(0, Absi32(0));
    Expect_IsEqual(1, Absi32(-1));
    Expect_IsEqual(1, Absi32(1));

    return true;
}

TEST(MathUtils_Absi64)
{
    Expect_IsEqual(5, Absi64(5));
    Expect_IsEqual(5, Absi64(-5));
    Expect_IsEqual(0, Absi64(0));
    Expect_IsEqual((i64)1, Absi64((i64)-1));
    Expect_IsEqual((i64)2147483648LL, Absi64((i64)-2147483648LL));

    return true;
}

TEST(MathUtils_MinMax)
{
    // Min macro
    Expect_IsEqual(3, Min(3, 5));
    Expect_IsEqual(3, Min(5, 3));
    Expect_IsEqual(5, Min(5, 5));
    Expect_IsEqual(-5, Min(-5, 3));

    // Max macro
    Expect_IsEqual(5, Max(3, 5));
    Expect_IsEqual(5, Max(5, 3));
    Expect_IsEqual(5, Max(5, 5));
    Expect_IsEqual(3, Max(-5, 3));

    // Typed variants
    Expect_IsEqual(3, MinI32(3, 5));
    Expect_IsEqual(5, MaxI32(3, 5));
    Expect_IsEqual((u32)3, MinU32(3, 5));
    Expect_IsEqual((u32)5, MaxU32(3, 5));

    return true;
}

TEST(MathUtils_Clamp)
{
    // ClampI32: value within range
    Expect_IsEqual(5, ClampI32(5, 0, 10));
    // ClampI32: value below min
    Expect_IsEqual(0, ClampI32(-5, 0, 10));
    // ClampI32: value above max
    Expect_IsEqual(10, ClampI32(15, 0, 10));
    // ClampI32: value equals min
    Expect_IsEqual(0, ClampI32(0, 0, 10));
    // ClampI32: value equals max
    Expect_IsEqual(10, ClampI32(10, 0, 10));

    // ClampU32
    Expect_IsEqual((u32)5, ClampU32(5, 0, 10));
    Expect_IsEqual((u32)0, ClampU32(0, 0, 10));
    Expect_IsEqual((u32)10, ClampU32(15, 0, 10));

    // ClampI32_Min
    Expect_IsEqual(5, ClampI32_Min(5, 0));
    Expect_IsEqual(0, ClampI32_Min(-5, 0));

    // ClampI32_Max
    Expect_IsEqual(5, ClampI32_Max(5, 10));
    Expect_IsEqual(10, ClampI32_Max(15, 10));

    return true;
}

/////////////////////////////////
// Memory tests                //
/////////////////////////////////

TEST(Memory_MemSetAndMemZero)
{
    u8 Buffer[64];

    // MemSet to a known value
    MemSet(Buffer, 0xAB, 64);
    Expect_IsEqual(0xAB, Buffer[0]);
    Expect_IsEqual(0xAB, Buffer[63]);

    // MemZero should clear all bytes
    MemZero(Buffer, 64);
    Expect_IsEqual(0, Buffer[0]);
    Expect_IsEqual(0, Buffer[31]);
    Expect_IsEqual(0, Buffer[63]);

    // Zero-size operations should not crash
    MemSet(Buffer, 0xFF, 0);
    MemZero(Buffer, 0);
    Expect_IsEqual(0, Buffer[0]);

    return true;
}

TEST(Memory_MemCopy)
{
    u8 Source[32];
    u8 Dest[32];

    MemSet(Source, 0xCC, 32);
    MemZero(Dest, 32);

    MemCopy(Dest, Source, 32);
    Expect_IsTrue(MemEqual(Source, Dest, 32));

    // Partial copy
    MemZero(Dest, 32);
    MemCopy(Dest, Source, 16);
    Expect_IsEqual(0xCC, Dest[0]);
    Expect_IsEqual(0xCC, Dest[15]);
    Expect_IsEqual(0, Dest[16]);

    return true;
}

TEST(Memory_MemMove)
{
    u8 Buffer[32];

    // Set up known data: 0,1,2,...,31
    for (u8 i = 0; i < 32; i++)
    {
        Buffer[i] = i;
    }

    // Overlapping move forward: copy bytes 0-15 to bytes 4-19
    MemMove(Buffer + 4, Buffer, 16);
    Expect_IsEqual(0, Buffer[4]);
    Expect_IsEqual(1, Buffer[5]);
    Expect_IsEqual(15, Buffer[19]);

    return true;
}

TEST(Memory_MemEqual)
{
    u8 A[16];
    u8 B[16];

    MemSet(A, 0x55, 16);
    MemSet(B, 0x55, 16);
    Expect_IsTrue(MemEqual(A, B, 16));

    B[8] = 0xAA;
    Expect_IsFalse(MemEqual(A, B, 16));

    // Zero-size should be equal
    Expect_IsTrue(MemEqual(A, B, 0));

    return true;
}

/////////////////////////////////
// HashUtils tests             //
/////////////////////////////////

TEST(HashUtils_FNV1a_Basic)
{
    // Same input should produce same hash
    u64 Hash1 = FNV1a_Hash("hello", 5);
    u64 Hash2 = FNV1a_Hash("hello", 5);
    Expect_IsEqual(Hash1, Hash2);

    // Different input should produce different hash
    u64 Hash3 = FNV1a_Hash("world", 5);
    Expect_IsNotEqual(Hash1, Hash3);

    // Empty input should still return a valid hash (the offset basis)
    u64 HashEmpty = FNV1a_Hash("", 0);
    Expect_IsEqual(FNV_OFFSET, HashEmpty);

    return true;
}

TEST(HashUtils_PointerHash)
{
    i32 a = 0;
    i32 b = 0;
    u64 HashA = PointerHash(&a);
    u64 HashB = PointerHash(&b);

    // Different pointers should yield different hashes (unless they happen to be 16-byte aligned neighbors, which is very unlikely for stack vars)
    // We can at least verify it returns a non-zero value for non-null
    Expect_IsNotEqual(0, PointerHash(&a));

    // NULL should hash to 0
    Expect_IsEqual(0, PointerHash(NULL));

    // Suppress unused warning
    (void)HashA;
    (void)HashB;

    return true;
}

/////////////////////////////////
// Array tests                 //
/////////////////////////////////

TEST(ArrayUtils_CreateAndDestroy)
{
    TArray(i32) Arr = Array_Create(i32);

    Expect_Ptr_IsValid(Arr);
    Expect_IsEqual(4, Array_Capacity(Arr));
    Expect_IsEqual(0, Array_Num(Arr));
    Expect_IsEqual(sizeof(i32), Array_Stride(Arr));

    Array_Destroy(Arr);

    return true;
}

TEST(ArrayUtils_AddAndAccess)
{
    TArray(i32) Arr = Array_Create(i32);

    i32 Val1 = 10;
    i32 Val2 = 20;
    i32 Val3 = 30;
    Array_Add(Arr, Val1);
    Array_Add(Arr, Val2);
    Array_Add(Arr, Val3);

    Expect_IsEqual(3, Array_Num(Arr));
    Expect_IsEqual(10, Arr[0]);
    Expect_IsEqual(20, Arr[1]);
    Expect_IsEqual(30, Arr[2]);

    Array_Destroy(Arr);

    return true;
}

TEST(ArrayUtils_Reserve)
{
    TArray(u64) Arr = Array_Reserve(u64, 128);

    Expect_Ptr_IsValid(Arr);
    Expect_IsEqual(128, Array_Capacity(Arr));
    Expect_IsEqual(0, Array_Num(Arr));
    Expect_IsEqual(sizeof(u64), Array_Stride(Arr));

    Array_Destroy(Arr);

    return true;
}

TEST(ArrayUtils_EmptyAndSetNum)
{
    TArray(i32) Arr = Array_Create(i32);

    i32 Val = 42;
    Array_Add(Arr, Val);
    Array_Add(Arr, Val);

    Expect_IsEqual(2, Array_Num(Arr));

    Array_Empty(Arr);
    Expect_IsEqual(0, Array_Num(Arr));

    Array_SetNum(Arr, 3);
    Expect_IsEqual(3, Array_Num(Arr));

    Array_Destroy(Arr);

    return true;
}

TEST(ArrayUtils_Last)
{
    TArray(i32) Arr = Array_Create(i32);

    i32 Val1 = 100;
    i32 Val2 = 200;
    i32 Val3 = 300;
    Array_Add(Arr, Val1);
    Expect_IsEqual(100, Array_Last(Arr));

    Array_Add(Arr, Val2);
    Expect_IsEqual(200, Array_Last(Arr));

    Array_Add(Arr, Val3);
    Expect_IsEqual(300, Array_Last(Arr));

    Array_Destroy(Arr);

    return true;
}

TEST(ArrayUtils_RemoveLast)
{
    TArray(i32) Arr = Array_Create(i32);

    i32 Val1 = 10;
    i32 Val2 = 20;
    i32 Val3 = 30;
    Array_Add(Arr, Val1);
    Array_Add(Arr, Val2);
    Array_Add(Arr, Val3);

    i32 Removed = 0;
    Array_RemoveLast(Arr, &Removed);
    Expect_IsEqual(30, Removed);
    Expect_IsEqual(2, Array_Num(Arr));

    Array_RemoveLast(Arr, &Removed);
    Expect_IsEqual(20, Removed);
    Expect_IsEqual(1, Array_Num(Arr));

    Array_Destroy(Arr);

    return true;
}

TEST(ArrayUtils_RemoveAt)
{
    TArray(i32) Arr = Array_Reserve(i32, 16);

    i32 Val1 = 10;
    i32 Val2 = 20;
    i32 Val3 = 30;
    i32 Val4 = 40;
    Array_Add(Arr, Val1);
    Array_Add(Arr, Val2);
    Array_Add(Arr, Val3);
    Array_Add(Arr, Val4);

    // Remove index 1 (value 20)
    i32 Removed = 0;
    Array_RemoveAt(Arr, &Removed, 1);
    Expect_IsEqual(20, Removed);
    Expect_IsEqual(3, Array_Num(Arr));
    Expect_IsEqual(10, Arr[0]);
    Expect_IsEqual(30, Arr[1]);
    Expect_IsEqual(40, Arr[2]);

    Array_Destroy(Arr);

    return true;
}

TEST(ArrayUtils_InsertAt)
{
    TArray(i32) Arr = Array_Reserve(i32, 16);

    i32 Val1 = 10;
    i32 Val2 = 20;
    i32 Val3 = 30;
    Array_Add(Arr, Val1);
    Array_Add(Arr, Val2);
    Array_Add(Arr, Val3);

    // Insert 99 at index 1
    i32 InsertVal = 99;
    Internal_ArrayInsertAt(Arr, &InsertVal, 1);
    Expect_IsEqual(4, Array_Num(Arr));
    Expect_IsEqual(10, Arr[0]);
    Expect_IsEqual(99, Arr[1]);
    Expect_IsEqual(20, Arr[2]);
    Expect_IsEqual(30, Arr[3]);

    Array_Destroy(Arr);

    return true;
}

/////////////////////////////////
// HashTable tests             //
/////////////////////////////////

TEST(HashTable_AddAndFind)
{
    HashTable Table;
    Table.Entries = Array_Reserve(HashTableEntry, DEFAULT_HASHTABLE_CAPACITY);

    HashTable_Add(&Table, S("alpha"), 1, false);
    HashTable_Add(&Table, S("beta"), 2, false);
    HashTable_Add(&Table, S("gamma"), 3, false);

    i32 Value = 0;
    bool bFound = HashTable_Find(Table, S("alpha"), &Value);
    Expect_IsTrue(bFound);
    Expect_IsEqual(1, Value);

    Value = 0;
    bFound = HashTable_Find(Table, S("beta"), &Value);
    Expect_IsTrue(bFound);
    Expect_IsEqual(2, Value);

    Value = 0;
    bFound = HashTable_Find(Table, S("gamma"), &Value);
    Expect_IsTrue(bFound);
    Expect_IsEqual(3, Value);

    HashTable_Destroy(&Table);

    return true;
}

TEST(HashTable_FindMissing)
{
    HashTable Table;
    Table.Entries = Array_Reserve(HashTableEntry, DEFAULT_HASHTABLE_CAPACITY);

    HashTable_Add(&Table, S("exists"), 42, false);

    i32 Value = 0;
    bool bFound = HashTable_Find(Table, S("does_not_exist"), &Value);
    Expect_IsFalse(bFound);
    Expect_IsEqual(0, Value);

    HashTable_Destroy(&Table);

    return true;
}

TEST(HashTable_UpdateExisting)
{
    HashTable Table;
    Table.Entries = Array_Reserve(HashTableEntry, DEFAULT_HASHTABLE_CAPACITY);

    HashTable_Add(&Table, S("key"), 10, false);

    // Add same key with bUpdateExisting=false, value should not change
    HashTable_Add(&Table, S("key"), 99, false);
    i32 Value = 0;
    bool bFound = HashTable_Find(Table, S("key"), &Value);
    Expect_IsTrue(bFound);
    Expect_IsEqual(10, Value);

    // Add same key with bUpdateExisting=true, value should change
    HashTable_Add(&Table, S("key"), 99, true);
    Value = 0;
    bFound = HashTable_Find(Table, S("key"), &Value);
    Expect_IsTrue(bFound);
    Expect_IsEqual(99, Value);

    HashTable_Destroy(&Table);

    return true;
}

TEST(HashTable_GrowOnLoad)
{
    HashTable Table;
    Table.Entries = Array_Reserve(HashTableEntry, 4);

    // Add enough entries to trigger a grow (load > 50%)
    HashTable_Add(&Table, S("a"), 1, false);
    HashTable_Add(&Table, S("b"), 2, false);
    HashTable_Add(&Table, S("c"), 3, false);
    HashTable_Add(&Table, S("d"), 4, false);

    // All entries should still be findable after grow
    i32 Value = 0;
    bool bFound = HashTable_Find(Table, S("a"), &Value);
    Expect_IsTrue(bFound);
    Expect_IsEqual(1, Value);

    Value = 0;
    bFound = HashTable_Find(Table, S("d"), &Value);
    Expect_IsTrue(bFound);
    Expect_IsEqual(4, Value);

    HashTable_Destroy(&Table);

    return true;
}

TEST(HashTable_DestroyCleanup)
{
    HashTable Table;
    Table.Entries = Array_Reserve(HashTableEntry, DEFAULT_HASHTABLE_CAPACITY);

    HashTable_Add(&Table, S("test"), 1, false);

    HashTable_Destroy(&Table);
    Expect_Ptr_IsNotValid(Table.Entries);

    return true;
}

/////////////////////////////////
// FreeListAllocator tests     //
/////////////////////////////////

/*
TEST(FreeListAllocator_CreateAndDestroy)
{
    FreeListAllocator Allocator = {0};
    FreeListAllocator_Create(&Allocator, 1024, NULL);

    Expect_Ptr_IsValid(Allocator.Memory);
    Expect_IsEqual(1024, Allocator.TotalSize);
    Expect_IsEqual(0, Allocator.Allocated);
    Expect_Ptr_IsValid(Allocator.Head);

    FreeListAllocator_Destroy(&Allocator);

    return true;
}

TEST(FreeListAllocator_AllocateAndFree)
{
    FreeListAllocator Allocator = {0};
    FreeListAllocator_Create(&Allocator, 4096, NULL);

    usize BytesAllocated = 0;
    void* Block1 = FreeListAllocator_Allocate(&Allocator, 128, &BytesAllocated);
    Expect_Ptr_IsValid(Block1);
    Expect_IsNotEqual(0, BytesAllocated);

    usize AllocatedAfterFirst = Allocator.Allocated;
    Expect_IsNotEqual(0, AllocatedAfterFirst);

    usize BytesFreed = 0;
    FreeListAllocator_Free(&Allocator, Block1, &BytesFreed);
    Expect_IsNotEqual(0, BytesFreed);
    Expect_IsEqual(0, Allocator.Allocated);

    FreeListAllocator_Destroy(&Allocator);

    return true;
}

TEST(FreeListAllocator_MultipleAllocations)
{
    FreeListAllocator Allocator = {0};
    FreeListAllocator_Create(&Allocator, 4096, NULL);

    usize Bytes1 = 0;
    usize Bytes2 = 0;
    usize Bytes3 = 0;
    void* Block1 = FreeListAllocator_Allocate(&Allocator, 64, &Bytes1);
    void* Block2 = FreeListAllocator_Allocate(&Allocator, 128, &Bytes2);
    void* Block3 = FreeListAllocator_Allocate(&Allocator, 256, &Bytes3);

    Expect_Ptr_IsValid(Block1);
    Expect_Ptr_IsValid(Block2);
    Expect_Ptr_IsValid(Block3);

    // All blocks should be at different addresses
    Expect_Ptr_IsNotEqual(Block1, Block2);
    Expect_Ptr_IsNotEqual(Block2, Block3);
    Expect_Ptr_IsNotEqual(Block1, Block3);

    usize BytesFreed = 0;
    FreeListAllocator_Free(&Allocator, Block2, &BytesFreed);
    FreeListAllocator_Free(&Allocator, Block1, &BytesFreed);
    FreeListAllocator_Free(&Allocator, Block3, &BytesFreed);

    Expect_IsEqual(0, Allocator.Allocated);

    FreeListAllocator_Destroy(&Allocator);

    return true;
}

TEST(FreeListAllocator_FreeAll)
{
    FreeListAllocator Allocator = {0};
    FreeListAllocator_Create(&Allocator, 4096, NULL);

    usize Bytes = 0;
    void* Block1 = FreeListAllocator_Allocate(&Allocator, 64, &Bytes);
    void* Block2 = FreeListAllocator_Allocate(&Allocator, 128, &Bytes);
    (void)Block1;
    (void)Block2;

    Expect_IsNotEqual(0, Allocator.Allocated);

    FreeListAllocator_FreeAll(&Allocator);
    Expect_IsEqual(0, Allocator.Allocated);

    FreeListAllocator_Destroy(&Allocator);

    return true;
}
*/

/////////////////////////////////
// StringUtils additional tests//
/////////////////////////////////

TEST(StringUtils_IsInteger)
{
    // Basic valid integers
    Expect_IsTrue(String_IsInteger(S("0")));
    Expect_IsTrue(String_IsInteger(S("1")));
    Expect_IsTrue(String_IsInteger(S("12345")));
    Expect_IsTrue(String_IsInteger(S("9999999999")));

    // Negative integers
    Expect_IsTrue(String_IsInteger(S("-1")));
    Expect_IsTrue(String_IsInteger(S("-12345")));
    Expect_IsTrue(String_IsInteger(S("-0"))); // technically valid form

    // Max length boundary (20 chars max)
    Expect_IsTrue(String_IsInteger(S("18446744073709551615"))); // 20 chars, at limit
    Expect_IsFalse(String_IsInteger(S("184467440737095516150"))); // 21 chars, over limit

    // Negative at max length (20 chars including minus)
    Expect_IsTrue(String_IsInteger(S("-9223372036854775807"))); // 20 chars total

    // Empty and whitespace
    Expect_IsFalse(String_IsInteger(S("")));
    Expect_IsFalse(String_IsInteger(S(" ")));
    Expect_IsFalse(String_IsInteger(S("\t")));
    Expect_IsFalse(String_IsInteger(S("\n")));

    // Just a minus sign
    Expect_IsFalse(String_IsInteger(S("-")));

    // Decimal / float values
    Expect_IsFalse(String_IsInteger(S("12.34")));
    Expect_IsFalse(String_IsInteger(S(".5")));
    Expect_IsFalse(String_IsInteger(S("1.")));

    // Alphabetic and mixed
    Expect_IsFalse(String_IsInteger(S("abc")));
    Expect_IsFalse(String_IsInteger(S("123abc")));
    Expect_IsFalse(String_IsInteger(S("abc123")));
    Expect_IsFalse(String_IsInteger(S("12 34")));

    // Symbols
    Expect_IsFalse(String_IsInteger(S("+123"))); // plus sign not handled
    Expect_IsFalse(String_IsInteger(S("123-456")));
    Expect_IsFalse(String_IsInteger(S("$100")));
    Expect_IsFalse(String_IsInteger(S("1,000")));

    // Leading/trailing spaces
    Expect_IsFalse(String_IsInteger(S(" 123")));
    Expect_IsFalse(String_IsInteger(S("123 ")));
    Expect_IsFalse(String_IsInteger(S(" 123 ")));

    // Single digit
    Expect_IsTrue(String_IsInteger(S("0")));
    Expect_IsTrue(String_IsInteger(S("9")));

    // Leading zeros
    Expect_IsTrue(String_IsInteger(S("007")));
    Expect_IsTrue(String_IsInteger(S("00")));

    return true;
}

TEST(StringUtils_IsFloat)
{
    // Basic valid floats
    Expect_IsTrue(String_IsFloat(S("123.45")));
    Expect_IsTrue(String_IsFloat(S("0.0")));
    Expect_IsTrue(String_IsFloat(S("0.1")));
    Expect_IsTrue(String_IsFloat(S("1.0")));
    Expect_IsTrue(String_IsFloat(S("99999.99999")));

    // Leading dot (no whole part)
    Expect_IsTrue(String_IsFloat(S(".5")));
    Expect_IsTrue(String_IsFloat(S(".0")));
    Expect_IsTrue(String_IsFloat(S(".123456789")));

    // Trailing dot (no fractional part) - IsFloat allows this since dot is valid and rest are digits
    Expect_IsTrue(String_IsFloat(S("5.")));
    Expect_IsTrue(String_IsFloat(S("0.")));

    // Negative floats
    Expect_IsTrue(String_IsFloat(S("-123.45")));
    Expect_IsTrue(String_IsFloat(S("-.5")));
    Expect_IsTrue(String_IsFloat(S("-0.0")));
    Expect_IsTrue(String_IsFloat(S("-1.0")));

    // Integers are also valid in IsFloat (digits only, no dot required)
    Expect_IsTrue(String_IsFloat(S("12345")));
    Expect_IsTrue(String_IsFloat(S("0")));
    Expect_IsTrue(String_IsFloat(S("-42")));

    // Empty and whitespace
    Expect_IsFalse(String_IsFloat(S("")));
    Expect_IsFalse(String_IsFloat(S(" ")));
    Expect_IsFalse(String_IsFloat(S("\t")));

    // Just a minus sign
    Expect_IsFalse(String_IsFloat(S("-")));

    // Multiple dots
    Expect_IsFalse(String_IsFloat(S("12.34.56")));
    Expect_IsFalse(String_IsFloat(S("..5")));
    Expect_IsFalse(String_IsFloat(S("1.2.3")));

    // Alphabetic and mixed
    Expect_IsFalse(String_IsFloat(S("abc")));
    Expect_IsFalse(String_IsFloat(S("123abc")));
    Expect_IsFalse(String_IsFloat(S("abc123")));
    Expect_IsFalse(String_IsFloat(S("12.3a")));
    Expect_IsFalse(String_IsFloat(S("a.5")));

    // Symbols
    Expect_IsFalse(String_IsFloat(S("+1.5"))); // plus sign not handled
    Expect_IsFalse(String_IsFloat(S("1.5e10"))); // scientific notation
    Expect_IsFalse(String_IsFloat(S("$1.50")));
    Expect_IsFalse(String_IsFloat(S("1,000.5")));

    // Leading/trailing spaces
    Expect_IsFalse(String_IsFloat(S(" 1.5")));
    Expect_IsFalse(String_IsFloat(S("1.5 ")));

    // Just a dot
    Expect_IsTrue(String_IsFloat(S("."))); // single dot with no digits - IsFloat allows this

    return true;
}

TEST(StringUtils_IsNumeric)
{
    // Integers (delegates to IsInteger)
    Expect_IsTrue(String_IsNumeric(S("12345")));
    Expect_IsTrue(String_IsNumeric(S("0")));
    Expect_IsTrue(String_IsNumeric(S("-12345")));
    Expect_IsTrue(String_IsNumeric(S("-0")));

    // Floats (delegates to IsFloat)
    Expect_IsTrue(String_IsNumeric(S("123.45")));
    Expect_IsTrue(String_IsNumeric(S("-123.45")));
    Expect_IsTrue(String_IsNumeric(S(".5")));
    Expect_IsTrue(String_IsNumeric(S("-.5")));
    Expect_IsTrue(String_IsNumeric(S("0.0")));

    // Invalid
    Expect_IsFalse(String_IsNumeric(S("-")));
    Expect_IsFalse(String_IsNumeric(S("abc")));
    Expect_IsFalse(String_IsNumeric(S("")));
    Expect_IsFalse(String_IsNumeric(S("12.34.56")));
    Expect_IsFalse(String_IsNumeric(S("123abc")));
    Expect_IsFalse(String_IsNumeric(S(" 5")));
    Expect_IsFalse(String_IsNumeric(S("+5")));

    return true;
}

TEST(StringUtils_Duplicate)
{
    LinearAllocator Arena = {0};
    LinearAllocator_Create(4096, NULL, &Arena);

    // Basic duplication
    String Original = S("Hello World");
    String Copy = String_Duplicate(&Arena, Original);
    Expect_Ptr_IsValid(Copy.Data);
    Expect_String_IsEqual(Original, Copy, true);
    Expect_IsEqual(Original.Length, Copy.Length);
    Expect_IsEqual(Original.Length, Copy.Capacity);
    Expect_Ptr_IsNotEqual(Original.Data, Copy.Data);

    // Verify null terminator is present
    Expect_IsEqual(0, Copy.Data[Copy.Length]);

    // Duplicate a single character
    String SingleChar = S("X");
    String CopySingle = String_Duplicate(&Arena, SingleChar);
    Expect_Ptr_IsValid(CopySingle.Data);
    Expect_String_IsEqual(S("X"), CopySingle, true);
    Expect_IsEqual(1, CopySingle.Length);
    Expect_Ptr_IsNotEqual(SingleChar.Data, CopySingle.Data);

    // Duplicate string with special characters
    String Special = S("!@#$%^&*()");
    String CopySpecial = String_Duplicate(&Arena, Special);
    Expect_Ptr_IsValid(CopySpecial.Data);
    Expect_String_IsEqual(S("!@#$%^&*()"), CopySpecial, true);
    Expect_IsEqual(Special.Length, CopySpecial.Length);

    // Duplicate string with spaces
    String WithSpaces = S("  hello  world  ");
    String CopySpaces = String_Duplicate(&Arena, WithSpaces);
    Expect_String_IsEqual(S("  hello  world  "), CopySpaces, true);
    Expect_IsEqual(WithSpaces.Length, CopySpaces.Length);

    // Duplicate string with path separators
    String Path = S("C:\\Users\\test/file.txt");
    String CopyPath = String_Duplicate(&Arena, Path);
    Expect_String_IsEqual(Path, CopyPath, true);

    // Empty string returns null (Length == 0 fails the bValid check)
    String Empty = S("");
    String CopyEmpty = String_Duplicate(&Arena, Empty);
    Expect_IsEqual(0, CopyEmpty.Length);

    // Multiple duplications produce independent copies
    String A = String_Duplicate(&Arena, S("AAA"));
    String B = String_Duplicate(&Arena, S("BBB"));
    Expect_String_IsEqual(S("AAA"), A, true);
    Expect_String_IsEqual(S("BBB"), B, true);
    Expect_Ptr_IsNotEqual(A.Data, B.Data);

    LinearAllocator_Destroy(&Arena);

    return true;
}

TEST(StringUtils_Left)
{
    // String_Left returns StrSlice(Data, Index) - first N chars as a slice
    String Src = S("Hello World");

    // Take first 5 chars
    String Result = String_Left(Src, 5);
    Expect_String_IsEqual(S("Hello"), Result, true);
    Expect_IsEqual(5, Result.Length);

    // Take first 1 char
    Result = String_Left(Src, 1);
    Expect_String_IsEqual(S("H"), Result, true);
    Expect_IsEqual(1, Result.Length);

    // Left with index 0 returns empty
    Result = String_Left(Src, 0);
    Expect_IsEqual(0, Result.Length);

    // Full length returns entire string
    Result = String_Left(Src, Src.Length);
    Expect_String_IsEqual(S("Hello World"), Result, true);
    Expect_IsEqual(Src.Length, Result.Length);

    // Capacity is 0 (it's a slice, not modifiable)
    Result = String_Left(Src, 5);
    Expect_IsEqual(0, Result.Capacity);

    // Pointer should point into the original data
    Expect_Ptr_IsEqual(Src.Data, Result.Data);

    // Take just the space character
    Result = String_Left(Src, 6);
    Expect_String_IsEqual(S("Hello "), Result, true);

    // Single character string
    String Single = S("X");
    Result = String_Left(Single, 1);
    Expect_String_IsEqual(S("X"), Result, true);

    return true;
}

TEST(StringUtils_Right)
{
    // String_Right shifts forward by Index chars (StrShiftF), returning the tail of the string
    String Src = S("Hello World");

    // Skip first 6 chars to get "World"
    String Result = String_Right(Src, 6);
    Expect_String_IsEqual(S("World"), Result, true);
    Expect_IsEqual(5, Result.Length);

    // Right with index 0 returns the full string
    Result = String_Right(Src, 0);
    Expect_String_IsEqual(S("Hello World"), Result, true);
    Expect_IsEqual(Src.Length, Result.Length);

    // Skip 1 char
    Result = String_Right(Src, 1);
    Expect_String_IsEqual(S("ello World"), Result, true);
    Expect_IsEqual(10, Result.Length);

    // Right with full length returns empty
    Result = String_Right(Src, Src.Length);
    Expect_IsEqual(0, Result.Length);

    // Capacity is 0 (it's a slice, not modifiable)
    Result = String_Right(Src, 3);
    Expect_IsEqual(0, Result.Capacity);

    // Data pointer is offset into original
    Result = String_Right(Src, 6);
    Expect_Ptr_IsEqual(Src.Data + 6, Result.Data);

    // Skip past the space
    Result = String_Right(Src, 5);
    Expect_String_IsEqual(S(" World"), Result, true);

    // Single character string
    String Single = S("X");
    Result = String_Right(Single, 0);
    Expect_String_IsEqual(S("X"), Result, true);
    Result = String_Right(Single, 1);
    Expect_IsEqual(0, Result.Length);

    // Index beyond length is clamped by StrShiftF (uses Min)
    Result = String_Right(Src, 100);
    Expect_IsEqual(0, Result.Length);

    return true;
}

TEST(StringUtils_IndexOfLastChar)
{
    // Multiple occurrences - should find the last one
    String Src = S("hello.world.test");
    u32 Index = 0;
    bool bFound = String_IndexOfLastChar(Src, '.', &Index);
    Expect_IsTrue(bFound);
    Expect_IsEqual(11, Index);

    // Search for char not present
    Index = 0;
    bFound = String_IndexOfLastChar(Src, '@', &Index);
    Expect_IsFalse(bFound);

    // Single occurrence
    String Single = S("abc.def");
    Index = 0;
    bFound = String_IndexOfLastChar(Single, '.', &Index);
    Expect_IsTrue(bFound);
    Expect_IsEqual(3, Index);

    // Char at the very beginning
    Index = 0;
    bFound = String_IndexOfLastChar(S(".hello"), '.', &Index);
    Expect_IsTrue(bFound);
    Expect_IsEqual(0, Index);

    // Char at the very end
    Index = 0;
    bFound = String_IndexOfLastChar(S("hello."), '.', &Index);
    Expect_IsTrue(bFound);
    Expect_IsEqual(5, Index);

    // Char is both first and last
    Index = 0;
    bFound = String_IndexOfLastChar(S(".hello."), '.', &Index);
    Expect_IsTrue(bFound);
    Expect_IsEqual(6, Index);

    // String is a single character that matches
    Index = 0;
    bFound = String_IndexOfLastChar(S("."), '.', &Index);
    Expect_IsTrue(bFound);
    Expect_IsEqual(0, Index);

    // String is a single character that doesn't match
    Index = 0;
    bFound = String_IndexOfLastChar(S("x"), '.', &Index);
    Expect_IsFalse(bFound);

    // Empty string
    Index = 0;
    bFound = String_IndexOfLastChar(S(""), '.', &Index);
    Expect_IsFalse(bFound);

    // All same characters - should find the last one
    Index = 0;
    bFound = String_IndexOfLastChar(S("...."), '.', &Index);
    Expect_IsTrue(bFound);
    Expect_IsEqual(3, Index);

    // Search for space
    Index = 0;
    bFound = String_IndexOfLastChar(S("hello world test"), ' ', &Index);
    Expect_IsTrue(bFound);
    Expect_IsEqual(11, Index);

    // Search for path separator
    Index = 0;
    bFound = String_IndexOfLastChar(S("C:\\Users\\test\\file.txt"), '\\', &Index);
    Expect_IsTrue(bFound);
    Expect_IsEqual(13, Index);

    return true;
}

TEST(StringUtils_TrimQuotes)
{
    // Basic double-quoted string
    String Result = String_TrimQuotes(S("\"hello\""));
    Expect_String_IsEqual(S("hello"), Result, true);
    Expect_IsEqual(5, Result.Length);

    // No quotes - unchanged
    Result = String_TrimQuotes(S("hello"));
    Expect_String_IsEqual(S("hello"), Result, true);

    // Single quotes are not trimmed (only double quotes)
    Result = String_TrimQuotes(S("'hello'"));
    Expect_String_IsEqual(S("'hello'"), Result, true);

    // Only opening quote - trims the opening quote but no closing
    Result = String_TrimQuotes(S("\"hello"));
    Expect_String_IsEqual(S("hello"), Result, true);

    // Only closing quote - no opening quote means no trimming at all
    Result = String_TrimQuotes(S("hello\""));
    Expect_String_IsEqual(S("hello\""), Result, true);

    // Empty double quotes
    Result = String_TrimQuotes(S("\"\""));
    Expect_IsEqual(0, Result.Length);

    // Single quote character
    Result = String_TrimQuotes(S("\""));
    Expect_IsEqual(0, Result.Length);

    // Empty string
    Result = String_TrimQuotes(S(""));
    Expect_IsEqual(0, Result.Length);

    // Quotes with spaces inside
    Result = String_TrimQuotes(S("\"hello world\""));
    Expect_String_IsEqual(S("hello world"), Result, true);

    // Quotes with special characters inside
    Result = String_TrimQuotes(S("\"!@#$%\""));
    Expect_String_IsEqual(S("!@#$%"), Result, true);

    // Nested double quotes
    Result = String_TrimQuotes(S("\"he\"llo\""));
    Expect_String_IsEqual(S("he\"llo"), Result, true);

    // Path in quotes
    Result = String_TrimQuotes(S("\"C:\\Users\\test\""));
    Expect_String_IsEqual(S("C:\\Users\\test"), Result, true);

    return true;
}

TEST(StringUtils_IsFirst)
{
    // Basic match
    Expect_IsTrue(String_IsFirst(S("Hello"), 'H'));

    // Not first character
    Expect_IsFalse(String_IsFirst(S("Hello"), 'e'));
    Expect_IsFalse(String_IsFirst(S("Hello"), 'o'));

    // Empty string
    Expect_IsFalse(String_IsFirst(S(""), 'H'));
    Expect_IsFalse(String_IsFirst(S(""), '\0'));

    // Single character string
    Expect_IsTrue(String_IsFirst(S("X"), 'X'));
    Expect_IsFalse(String_IsFirst(S("X"), 'Y'));

    // Special characters as first
    Expect_IsTrue(String_IsFirst(S(" Hello"), ' '));
    Expect_IsTrue(String_IsFirst(S("\tHello"), '\t'));
    Expect_IsTrue(String_IsFirst(S("\nHello"), '\n'));
    Expect_IsTrue(String_IsFirst(S("/path"), '/'));
    Expect_IsTrue(String_IsFirst(S("\\path"), '\\'));
    Expect_IsTrue(String_IsFirst(S("\"quoted"), '"'));
    Expect_IsTrue(String_IsFirst(S("-123"), '-'));
    Expect_IsTrue(String_IsFirst(S(".5"), '.'));

    // Digit as first
    Expect_IsTrue(String_IsFirst(S("0test"), '0'));
    Expect_IsTrue(String_IsFirst(S("9test"), '9'));

    return true;
}

TEST(StringUtils_IsLast)
{
    // Basic match
    Expect_IsTrue(String_IsLast(S("Hello"), 'o'));

    // Not last character
    Expect_IsFalse(String_IsLast(S("Hello"), 'H'));
    Expect_IsFalse(String_IsLast(S("Hello"), 'l'));

    // Empty string
    Expect_IsFalse(String_IsLast(S(""), 'o'));
    Expect_IsFalse(String_IsLast(S(""), '\0'));

    // Single character string
    Expect_IsTrue(String_IsLast(S("X"), 'X'));
    Expect_IsFalse(String_IsLast(S("X"), 'Y'));

    // Special characters as last
    Expect_IsTrue(String_IsLast(S("Hello "), ' '));
    Expect_IsTrue(String_IsLast(S("Hello\t"), '\t'));
    Expect_IsTrue(String_IsLast(S("Hello\n"), '\n'));
    Expect_IsTrue(String_IsLast(S("path/"), '/'));
    Expect_IsTrue(String_IsLast(S("path\\"), '\\'));
    Expect_IsTrue(String_IsLast(S("quoted\""), '"'));
    Expect_IsTrue(String_IsLast(S("file."), '.'));

    // Null char is never the last (Length doesn't include null terminator)
    Expect_IsFalse(String_IsLast(S("Hello"), '\0'));

    // Same char appears first and last
    Expect_IsTrue(String_IsLast(S("abca"), 'a'));
    Expect_IsTrue(String_IsFirst(S("abca"), 'a'));

    return true;
}

TEST(StringUtils_FromI32)
{
    StringLocal(Buffer, 32);

    // Basic positive
    bool bSuccess = String_FromI32(&Buffer, 12345);
    Expect_IsTrue(bSuccess);
    Expect_String_IsEqual(S("12345"), Buffer, true);
    Expect_IsEqual(5, Buffer.Length);

    // Zero
    Buffer.Length = 0;
    bSuccess = String_FromI32(&Buffer, 0);
    Expect_IsTrue(bSuccess);
    Expect_String_IsEqual(S("0"), Buffer, true);
    Expect_IsEqual(1, Buffer.Length);

    // Negative
    Buffer.Length = 0;
    bSuccess = String_FromI32(&Buffer, -12345);
    Expect_IsTrue(bSuccess);
    Expect_String_IsEqual(S("-12345"), Buffer, true);
    Expect_IsEqual(6, Buffer.Length);

    // Negative one
    Buffer.Length = 0;
    bSuccess = String_FromI32(&Buffer, -1);
    Expect_IsTrue(bSuccess);
    Expect_String_IsEqual(S("-1"), Buffer, true);

    // Single digit
    Buffer.Length = 0;
    bSuccess = String_FromI32(&Buffer, 7);
    Expect_IsTrue(bSuccess);
    Expect_String_IsEqual(S("7"), Buffer, true);

    // INT32_MAX (2147483647)
    Buffer.Length = 0;
    bSuccess = String_FromI32(&Buffer, INT32_MAX);
    Expect_IsTrue(bSuccess);
    Expect_String_IsEqual(S("2147483647"), Buffer, true);

    // INT32_MIN + 1 = -2147483647
    Buffer.Length = 0;
    bSuccess = String_FromI32(&Buffer, -2147483647);
    Expect_IsTrue(bSuccess);
    Expect_String_IsEqual(S("-2147483647"), Buffer, true);

    // Powers of 10
    Buffer.Length = 0;
    bSuccess = String_FromI32(&Buffer, 10);
    Expect_IsTrue(bSuccess);
    Expect_String_IsEqual(S("10"), Buffer, true);

    Buffer.Length = 0;
    bSuccess = String_FromI32(&Buffer, 100);
    Expect_IsTrue(bSuccess);
    Expect_String_IsEqual(S("100"), Buffer, true);

    Buffer.Length = 0;
    bSuccess = String_FromI32(&Buffer, 1000000);
    Expect_IsTrue(bSuccess);
    Expect_String_IsEqual(S("1000000"), Buffer, true);

    // Buffer too small should fail (capacity < MaxDigits)
    StringLocal(TinyBuffer, 4);
    bSuccess = String_FromI32(&TinyBuffer, 12345);
    Expect_IsFalse(bSuccess);

    return true;
}

TEST(StringUtils_FromU32)
{
    StringLocal(Buffer, 32);

    // Basic value
    bool bSuccess = String_FromU32(&Buffer, 12345);
    Expect_IsTrue(bSuccess);
    Expect_String_IsEqual(S("12345"), Buffer, true);
    Expect_IsEqual(5, Buffer.Length);

    // Zero
    Buffer.Length = 0;
    bSuccess = String_FromU32(&Buffer, 0);
    Expect_IsTrue(bSuccess);
    Expect_String_IsEqual(S("0"), Buffer, true);
    Expect_IsEqual(1, Buffer.Length);

    // One
    Buffer.Length = 0;
    bSuccess = String_FromU32(&Buffer, 1);
    Expect_IsTrue(bSuccess);
    Expect_String_IsEqual(S("1"), Buffer, true);

    // Single digit boundary
    Buffer.Length = 0;
    bSuccess = String_FromU32(&Buffer, 9);
    Expect_IsTrue(bSuccess);
    Expect_String_IsEqual(S("9"), Buffer, true);

    // UINT32_MAX (4294967295)
    Buffer.Length = 0;
    bSuccess = String_FromU32(&Buffer, UINT32_MAX);
    Expect_IsTrue(bSuccess);
    Expect_String_IsEqual(S("4294967295"), Buffer, true);
    Expect_IsEqual(10, Buffer.Length);

    // Powers of 10
    Buffer.Length = 0;
    bSuccess = String_FromU32(&Buffer, 10);
    Expect_IsTrue(bSuccess);
    Expect_String_IsEqual(S("10"), Buffer, true);

    Buffer.Length = 0;
    bSuccess = String_FromU32(&Buffer, 1000);
    Expect_IsTrue(bSuccess);
    Expect_String_IsEqual(S("1000"), Buffer, true);

    // Digit boundaries
    Buffer.Length = 0;
    bSuccess = String_FromU32(&Buffer, 99);
    Expect_IsTrue(bSuccess);
    Expect_String_IsEqual(S("99"), Buffer, true);

    Buffer.Length = 0;
    bSuccess = String_FromU32(&Buffer, 100);
    Expect_IsTrue(bSuccess);
    Expect_String_IsEqual(S("100"), Buffer, true);

    // Buffer too small should fail
    StringLocal(TinyBuffer, 4);
    bSuccess = String_FromU32(&TinyBuffer, 12345);
    Expect_IsFalse(bSuccess);

    return true;
}

TEST(StringUtils_CompareVersion)
{
    // Equal versions - all components match
    ECompareResult Result = String_CompareVersion(S("1.0.0"), S("1.0.0"));
    Expect_IsEqual(CompareResult_Equal, Result);

    // Major version difference
    Result = String_CompareVersion(S("2.0.0"), S("1.0.0"));
    Expect_IsEqual(CompareResult_Greater, Result);

    Result = String_CompareVersion(S("1.0.0"), S("2.0.0"));
    Expect_IsEqual(CompareResult_Less, Result);

    // Minor version difference (major equal)
    Result = String_CompareVersion(S("1.2.0"), S("1.1.0"));
    Expect_IsEqual(CompareResult_Greater, Result);

    Result = String_CompareVersion(S("1.1.0"), S("1.2.0"));
    Expect_IsEqual(CompareResult_Less, Result);

    // Patch version difference (major and minor equal)
    Result = String_CompareVersion(S("1.0.1"), S("1.0.2"));
    Expect_IsEqual(CompareResult_Less, Result);

    Result = String_CompareVersion(S("1.0.2"), S("1.0.1"));
    Expect_IsEqual(CompareResult_Greater, Result);

    // Two-component versions
    Result = String_CompareVersion(S("1.0"), S("1.0"));
    Expect_IsEqual(CompareResult_Equal, Result);

    Result = String_CompareVersion(S("1.1"), S("1.0"));
    Expect_IsEqual(CompareResult_Greater, Result);

    Result = String_CompareVersion(S("1.0"), S("1.1"));
    Expect_IsEqual(CompareResult_Less, Result);

    // Single-component versions
    Result = String_CompareVersion(S("5"), S("5"));
    Expect_IsEqual(CompareResult_Equal, Result);

    Result = String_CompareVersion(S("5"), S("3"));
    Expect_IsEqual(CompareResult_Greater, Result);

    Result = String_CompareVersion(S("3"), S("5"));
    Expect_IsEqual(CompareResult_Less, Result);

    // Large version numbers
    Result = String_CompareVersion(S("100.200.300"), S("100.200.300"));
    Expect_IsEqual(CompareResult_Equal, Result);

    Result = String_CompareVersion(S("100.200.301"), S("100.200.300"));
    Expect_IsEqual(CompareResult_Greater, Result);

    // Zero versions
    Result = String_CompareVersion(S("0.0.0"), S("0.0.0"));
    Expect_IsEqual(CompareResult_Equal, Result);

    Result = String_CompareVersion(S("0.0.1"), S("0.0.0"));
    Expect_IsEqual(CompareResult_Greater, Result);

    // Hyphen separator (used for pre-release versions)
    Result = String_CompareVersion(S("1-0-0"), S("1-0-0"));
    Expect_IsEqual(CompareResult_Equal, Result);

    Result = String_CompareVersion(S("2-0-0"), S("1-0-0"));
    Expect_IsEqual(CompareResult_Greater, Result);

    // Empty version returns None
    Result = String_CompareVersion(S(""), S("1.0.0"));
    Expect_IsEqual(CompareResult_None, Result);

    Result = String_CompareVersion(S("1.0.0"), S(""));
    Expect_IsEqual(CompareResult_None, Result);

    Result = String_CompareVersion(S(""), S(""));
    Expect_IsEqual(CompareResult_None, Result);

    // Four component versions
    Result = String_CompareVersion(S("1.2.3.4"), S("1.2.3.4"));
    Expect_IsEqual(CompareResult_Equal, Result);

    Result = String_CompareVersion(S("1.2.3.5"), S("1.2.3.4"));
    Expect_IsEqual(CompareResult_Greater, Result);

    return true;
}

TEST(StringUtils_Integer_CountDigits)
{
    // Zero is 1 digit
    Expect_IsEqual(1, Integer_CountDigits(0));

    // Single digits 1-9
    Expect_IsEqual(1, Integer_CountDigits(1));
    Expect_IsEqual(1, Integer_CountDigits(5));
    Expect_IsEqual(1, Integer_CountDigits(9));

    // Boundary: 9 -> 10 (1 digit -> 2 digits)
    Expect_IsEqual(2, Integer_CountDigits(10));
    Expect_IsEqual(2, Integer_CountDigits(11));
    Expect_IsEqual(2, Integer_CountDigits(99));

    // Boundary: 99 -> 100 (2 digits -> 3 digits)
    Expect_IsEqual(3, Integer_CountDigits(100));
    Expect_IsEqual(3, Integer_CountDigits(999));

    // Boundary: 999 -> 1000
    Expect_IsEqual(4, Integer_CountDigits(1000));
    Expect_IsEqual(4, Integer_CountDigits(9999));

    // Larger values
    Expect_IsEqual(5, Integer_CountDigits(10000));
    Expect_IsEqual(5, Integer_CountDigits(99999));
    Expect_IsEqual(6, Integer_CountDigits(100000));
    Expect_IsEqual(7, Integer_CountDigits(1000000));

    // u32 max: 4294967295 (10 digits)
    Expect_IsEqual(10, Integer_CountDigits(4294967295ULL));

    // u64 max: 18446744073709551615 (20 digits)
    Expect_IsEqual(20, Integer_CountDigits(18446744073709551615ULL));

    // Powers of 10
    Expect_IsEqual(8, Integer_CountDigits(10000000));
    Expect_IsEqual(9, Integer_CountDigits(100000000));
    Expect_IsEqual(10, Integer_CountDigits(1000000000));

    return true;
}

TEST(StringUtils_ContainsDigits)
{
    // Mixed alpha and digits
    Expect_IsTrue(String_ContainsDigits(S("abc123")));
    Expect_IsTrue(String_ContainsDigits(S("123abc")));
    Expect_IsTrue(String_ContainsDigits(S("a1b2c3")));

    // Only digits
    Expect_IsTrue(String_ContainsDigits(S("0")));
    Expect_IsTrue(String_ContainsDigits(S("5")));
    Expect_IsTrue(String_ContainsDigits(S("9")));
    Expect_IsTrue(String_ContainsDigits(S("0123456789")));

    // Digits at various positions
    Expect_IsTrue(String_ContainsDigits(S("0abc")));
    Expect_IsTrue(String_ContainsDigits(S("abc0")));
    Expect_IsTrue(String_ContainsDigits(S("ab1cd")));

    // Digit in symbols
    Expect_IsTrue(String_ContainsDigits(S("!@#1$%")));
    Expect_IsTrue(String_ContainsDigits(S("test_2_value")));

    // No digits - alphabetic only
    Expect_IsFalse(String_ContainsDigits(S("abc")));
    Expect_IsFalse(String_ContainsDigits(S("ABCDEFGHIJKLMNOPQRSTUVWXYZ")));
    Expect_IsFalse(String_ContainsDigits(S("abcdefghijklmnopqrstuvwxyz")));

    // No digits - symbols only
    Expect_IsFalse(String_ContainsDigits(S("!@#$%^&*()")));
    Expect_IsFalse(String_ContainsDigits(S("---")));

    // No digits - whitespace only
    Expect_IsFalse(String_ContainsDigits(S(" ")));
    Expect_IsFalse(String_ContainsDigits(S("\t\n")));

    // Empty string
    Expect_IsFalse(String_ContainsDigits(S("")));

    return true;
}

TEST(StringUtils_ReplaceCharInline)
{
    StringLocal(TestStr, 128);

    // Replace multiple occurrences
    String_Copy(&TestStr, S("hello-world-test"));
    bool bReplaced = String_ReplaceCharInline(&TestStr, '-', '_');
    Expect_IsTrue(bReplaced);
    Expect_String_IsEqual(S("hello_world_test"), TestStr, true);

    // Replace char that does not exist
    bReplaced = String_ReplaceCharInline(&TestStr, '@', '!');
    Expect_IsFalse(bReplaced);
    Expect_String_IsEqual(S("hello_world_test"), TestStr, true);

    // Replace single occurrence
    TestStr.Length = 0;
    String_Copy(&TestStr, S("hello world"));
    bReplaced = String_ReplaceCharInline(&TestStr, ' ', '-');
    Expect_IsTrue(bReplaced);
    Expect_String_IsEqual(S("hello-world"), TestStr, true);

    // Replace first character
    TestStr.Length = 0;
    String_Copy(&TestStr, S("Xhello"));
    bReplaced = String_ReplaceCharInline(&TestStr, 'X', 'Y');
    Expect_IsTrue(bReplaced);
    Expect_String_IsEqual(S("Yhello"), TestStr, true);

    // Replace last character
    TestStr.Length = 0;
    String_Copy(&TestStr, S("helloX"));
    bReplaced = String_ReplaceCharInline(&TestStr, 'X', 'Y');
    Expect_IsTrue(bReplaced);
    Expect_String_IsEqual(S("helloY"), TestStr, true);

    // Replace all characters (all same char)
    TestStr.Length = 0;
    String_Copy(&TestStr, S("AAAA"));
    bReplaced = String_ReplaceCharInline(&TestStr, 'A', 'B');
    Expect_IsTrue(bReplaced);
    Expect_String_IsEqual(S("BBBB"), TestStr, true);

    // Replace with same char (no visible change but returns true)
    TestStr.Length = 0;
    String_Copy(&TestStr, S("hello"));
    bReplaced = String_ReplaceCharInline(&TestStr, 'h', 'h');
    Expect_IsTrue(bReplaced);
    Expect_String_IsEqual(S("hello"), TestStr, true);

    // Replace backslash with forward slash (common path operation)
    TestStr.Length = 0;
    String_Copy(&TestStr, S("C:\\Users\\test\\file.txt"));
    bReplaced = String_ReplaceCharInline(&TestStr, '\\', '/');
    Expect_IsTrue(bReplaced);
    Expect_String_IsEqual(S("C:/Users/test/file.txt"), TestStr, true);

    // Single character string - match
    TestStr.Length = 0;
    String_Copy(&TestStr, S("X"));
    bReplaced = String_ReplaceCharInline(&TestStr, 'X', 'Y');
    Expect_IsTrue(bReplaced);
    Expect_String_IsEqual(S("Y"), TestStr, true);

    // Single character string - no match
    TestStr.Length = 0;
    String_Copy(&TestStr, S("X"));
    bReplaced = String_ReplaceCharInline(&TestStr, 'Z', 'Y');
    Expect_IsFalse(bReplaced);
    Expect_String_IsEqual(S("X"), TestStr, true);

    // Length unchanged after replace
    TestStr.Length = 0;
    String_Copy(&TestStr, S("a-b-c"));
    u32 OriginalLength = TestStr.Length;
    bReplaced = String_ReplaceCharInline(&TestStr, '-', '_');
    Expect_IsTrue(bReplaced);
    Expect_IsEqual(OriginalLength, TestStr.Length);

    return true;
}

TEST(StringUtils_ContainsPathSeparators)
{
    // Forward slashes
    Expect_IsTrue(String_ContainsPathSeparators(S("path/to/file")));
    Expect_IsTrue(String_ContainsPathSeparators(S("/path")));
    Expect_IsTrue(String_ContainsPathSeparators(S("path/")));
    Expect_IsTrue(String_ContainsPathSeparators(S("/")));

    // Backslashes
    Expect_IsTrue(String_ContainsPathSeparators(S("path\\to\\file")));
    Expect_IsTrue(String_ContainsPathSeparators(S("\\path")));
    Expect_IsTrue(String_ContainsPathSeparators(S("path\\")));
    Expect_IsTrue(String_ContainsPathSeparators(S("\\")));

    // Mixed separators
    Expect_IsTrue(String_ContainsPathSeparators(S("C:\\Users/test")));
    Expect_IsTrue(String_ContainsPathSeparators(S("/home\\user")));

    // Full paths
    Expect_IsTrue(String_ContainsPathSeparators(S("C:\\Users\\test\\file.txt")));
    Expect_IsTrue(String_ContainsPathSeparators(S("/usr/local/bin")));

    // No separators
    Expect_IsFalse(String_ContainsPathSeparators(S("filename")));
    Expect_IsFalse(String_ContainsPathSeparators(S("filename.txt")));
    Expect_IsFalse(String_ContainsPathSeparators(S("hello world")));
    Expect_IsFalse(String_ContainsPathSeparators(S("!@#$%^&*()")));

    // Empty string
    Expect_IsFalse(String_ContainsPathSeparators(S("")));

    // Only separators
    Expect_IsTrue(String_ContainsPathSeparators(S("///\\\\\\")));

    return true;
}

TEST(StringUtils_FromI64)
{
    StringLocal(Buffer, 32);

    // Zero
    bool bSuccess = String_FromI64(&Buffer, 0);
    Expect_IsTrue(bSuccess);
    Expect_String_IsEqual(S("0"), Buffer, true);

    // Positive one
    Buffer.Length = 0;
    bSuccess = String_FromI64(&Buffer, 1);
    Expect_IsTrue(bSuccess);
    Expect_String_IsEqual(S("1"), Buffer, true);

    // Negative one
    Buffer.Length = 0;
    bSuccess = String_FromI64(&Buffer, -1);
    Expect_IsTrue(bSuccess);
    Expect_String_IsEqual(S("-1"), Buffer, true);

    // Larger positive
    Buffer.Length = 0;
    bSuccess = String_FromI64(&Buffer, 123456789);
    Expect_IsTrue(bSuccess);
    Expect_String_IsEqual(S("123456789"), Buffer, true);

    // Larger negative
    Buffer.Length = 0;
    bSuccess = String_FromI64(&Buffer, -123456789);
    Expect_IsTrue(bSuccess);
    Expect_String_IsEqual(S("-123456789"), Buffer, true);

    // INT64_MAX (9223372036854775807)
    Buffer.Length = 0;
    bSuccess = String_FromI64(&Buffer, 9223372036854775807LL);
    Expect_IsTrue(bSuccess);
    Expect_String_IsEqual(S("9223372036854775807"), Buffer, true);

    // Value beyond i32 range
    Buffer.Length = 0;
    bSuccess = String_FromI64(&Buffer, 3000000000LL);
    Expect_IsTrue(bSuccess);
    Expect_String_IsEqual(S("3000000000"), Buffer, true);

    Buffer.Length = 0;
    bSuccess = String_FromI64(&Buffer, -3000000000LL);
    Expect_IsTrue(bSuccess);
    Expect_String_IsEqual(S("-3000000000"), Buffer, true);

    // Buffer too small should fail
    StringLocal(TinyBuffer, 4);
    bSuccess = String_FromI64(&TinyBuffer, 123456789);
    Expect_IsFalse(bSuccess);

    return true;
}

TEST(StringUtils_FromU64)
{
    StringLocal(Buffer, 32);

    // Zero
    bool bSuccess = String_FromU64(&Buffer, 0);
    Expect_IsTrue(bSuccess);
    Expect_String_IsEqual(S("0"), Buffer, true);

    // One
    Buffer.Length = 0;
    bSuccess = String_FromU64(&Buffer, 1);
    Expect_IsTrue(bSuccess);
    Expect_String_IsEqual(S("1"), Buffer, true);

    // Larger value
    Buffer.Length = 0;
    bSuccess = String_FromU64(&Buffer, 123456789);
    Expect_IsTrue(bSuccess);
    Expect_String_IsEqual(S("123456789"), Buffer, true);

    // Value beyond u32 range
    Buffer.Length = 0;
    bSuccess = String_FromU64(&Buffer, 5000000000ULL);
    Expect_IsTrue(bSuccess);
    Expect_String_IsEqual(S("5000000000"), Buffer, true);

    // UINT64_MAX (18446744073709551615)
    Buffer.Length = 0;
    bSuccess = String_FromU64(&Buffer, 18446744073709551615ULL);
    Expect_IsTrue(bSuccess);
    Expect_String_IsEqual(S("18446744073709551615"), Buffer, true);
    Expect_IsEqual(20, Buffer.Length);

    // Powers of 10
    Buffer.Length = 0;
    bSuccess = String_FromU64(&Buffer, 10);
    Expect_IsTrue(bSuccess);
    Expect_String_IsEqual(S("10"), Buffer, true);

    Buffer.Length = 0;
    bSuccess = String_FromU64(&Buffer, 10000000000ULL);
    Expect_IsTrue(bSuccess);
    Expect_String_IsEqual(S("10000000000"), Buffer, true);

    // Buffer too small should fail
    StringLocal(TinyBuffer, 4);
    bSuccess = String_FromU64(&TinyBuffer, 18446744073709551615ULL);
    Expect_IsFalse(bSuccess);

    return true;
}

TEST(StringUtils_Copy)
{
    StringLocal(Dest, 64);

    // Basic copy
    String_Copy(&Dest, S("Hello World"));
    Expect_String_IsEqual(S("Hello World"), Dest, true);
    Expect_IsEqual(11, Dest.Length);

    // Copy overwrites previous content
    String_Copy(&Dest, S("Goodbye"));
    Expect_String_IsEqual(S("Goodbye"), Dest, true);
    Expect_IsEqual(7, Dest.Length);

    // Copy empty string
    Dest.Length = 0;
    String_Copy(&Dest, S(""));
    Expect_IsEqual(0, Dest.Length);

    // Copy single character
    Dest.Length = 0;
    String_Copy(&Dest, S("X"));
    Expect_String_IsEqual(S("X"), Dest, true);
    Expect_IsEqual(1, Dest.Length);

    // Copy string with special characters
    Dest.Length = 0;
    String_Copy(&Dest, S("!@#$%^&*()"));
    Expect_String_IsEqual(S("!@#$%^&*()"), Dest, true);

    // Copy string with spaces
    Dest.Length = 0;
    String_Copy(&Dest, S("  hello  "));
    Expect_String_IsEqual(S("  hello  "), Dest, true);

    // Copy string with path separators
    Dest.Length = 0;
    String_Copy(&Dest, S("C:\\Users/test"));
    Expect_String_IsEqual(S("C:\\Users/test"), Dest, true);

    // Copy string that exceeds capacity - should be clamped to capacity
    StringLocal(SmallDest, 5);
    String_Copy(&SmallDest, S("Hello World"));
    Expect_IsEqual(SmallDest.Capacity, SmallDest.Length);

    // Null terminator is placed correctly
    Dest.Length = 0;
    String_Copy(&Dest, S("test"));
    Expect_IsEqual(0, Dest.Data[Dest.Length]);

    return true;
}

TEST(StringUtils_AppendChar)
{
    StringLocal(Dest, 64);

    // Basic append
    String_Copy(&Dest, S("Hello"));
    String_AppendChar(&Dest, '!');
    Expect_String_IsEqual(S("Hello!"), Dest, true);
    Expect_IsEqual(6, Dest.Length);

    // Append to empty string
    Dest.Length = 0;
    String_AppendChar(&Dest, 'X');
    Expect_String_IsEqual(S("X"), Dest, true);
    Expect_IsEqual(1, Dest.Length);

    // Append multiple chars one at a time
    Dest.Length = 0;
    String_AppendChar(&Dest, 'A');
    String_AppendChar(&Dest, 'B');
    String_AppendChar(&Dest, 'C');
    Expect_String_IsEqual(S("ABC"), Dest, true);
    Expect_IsEqual(3, Dest.Length);

    // Append space
    Dest.Length = 0;
    String_Copy(&Dest, S("hello"));
    String_AppendChar(&Dest, ' ');
    Expect_String_IsEqual(S("hello "), Dest, true);

    // Append path separator
    Dest.Length = 0;
    String_Copy(&Dest, S("path"));
    String_AppendChar(&Dest, '/');
    Expect_String_IsEqual(S("path/"), Dest, true);

    // Append digit
    Dest.Length = 0;
    String_Copy(&Dest, S("item"));
    String_AppendChar(&Dest, '0');
    Expect_String_IsEqual(S("item0"), Dest, true);

    // Append special characters
    Dest.Length = 0;
    String_Copy(&Dest, S("test"));
    String_AppendChar(&Dest, '\t');
    Expect_IsEqual(5, Dest.Length);

    // Null terminator placed after append
    Dest.Length = 0;
    String_Copy(&Dest, S("AB"));
    String_AppendChar(&Dest, 'C');
    Expect_IsEqual(0, Dest.Data[Dest.Length]);

    return true;
}

TEST(StringUtils_IndexOfSubstring)
{
    String Src = S("the quick brown fox jumps");

    // Basic find in middle
    u32 Index = 0;
    bool bFound = String_IndexOfSubstring(Src, S("brown"), true, &Index);
    Expect_IsTrue(bFound);
    Expect_IsEqual(10, Index);

    // Find at beginning
    Index = 0;
    bFound = String_IndexOfSubstring(Src, S("the"), true, &Index);
    Expect_IsTrue(bFound);
    Expect_IsEqual(0, Index);

    // Find at end
    Index = 0;
    bFound = String_IndexOfSubstring(Src, S("jumps"), true, &Index);
    Expect_IsTrue(bFound);
    Expect_IsEqual(20, Index);

    // Not found
    Index = 0;
    bFound = String_IndexOfSubstring(Src, S("lazy"), true, &Index);
    Expect_IsFalse(bFound);

    // Case sensitive - not found when case differs
    Index = 0;
    bFound = String_IndexOfSubstring(Src, S("Brown"), true, &Index);
    Expect_IsFalse(bFound);

    // Case insensitive - found when case differs
    Index = 0;
    bFound = String_IndexOfSubstring(Src, S("Brown"), false, &Index);
    Expect_IsTrue(bFound);
    Expect_IsEqual(10, Index);

    // Find single character as substring
    Index = 0;
    bFound = String_IndexOfSubstring(Src, S("q"), true, &Index);
    Expect_IsTrue(bFound);
    Expect_IsEqual(4, Index);

    // Find space
    Index = 0;
    bFound = String_IndexOfSubstring(Src, S(" "), true, &Index);
    Expect_IsTrue(bFound);
    Expect_IsEqual(3, Index);

    // Empty substring is not found (Length > 0 check)
    Index = 0;
    bFound = String_IndexOfSubstring(Src, S(""), true, &Index);
    Expect_IsFalse(bFound);

    // Substring longer than source - not found
    Index = 0;
    bFound = String_IndexOfSubstring(S("hi"), S("hello world"), true, &Index);
    Expect_IsFalse(bFound);

    // Find entire string as substring
    Index = 0;
    bFound = String_IndexOfSubstring(S("hello"), S("hello"), true, &Index);
    Expect_IsTrue(bFound);
    Expect_IsEqual(0, Index);

    // Multiple occurrences - should find first
    Index = 0;
    bFound = String_IndexOfSubstring(S("abcabc"), S("abc"), true, &Index);
    Expect_IsTrue(bFound);
    Expect_IsEqual(0, Index);

    return true;
}

TEST(StringUtils_StripWhitespace)
{
    StringLocal(OutStr, 128);

    // Spaces between chars
    String_StripWhitespace(S("h e l l o"), &OutStr);
    Expect_String_IsEqual(S("hello"), OutStr, true);

    // No whitespace - unchanged
    OutStr.Length = 0;
    String_StripWhitespace(S("hello"), &OutStr);
    Expect_String_IsEqual(S("hello"), OutStr, true);

    // Leading whitespace
    OutStr.Length = 0;
    String_StripWhitespace(S("   hello"), &OutStr);
    Expect_String_IsEqual(S("hello"), OutStr, true);

    // Trailing whitespace
    OutStr.Length = 0;
    String_StripWhitespace(S("hello   "), &OutStr);
    Expect_String_IsEqual(S("hello"), OutStr, true);

    // Leading and trailing whitespace
    OutStr.Length = 0;
    String_StripWhitespace(S("   hello   "), &OutStr);
    Expect_String_IsEqual(S("hello"), OutStr, true);

    // Multiple types of whitespace (space, tab, newline, carriage return)
    OutStr.Length = 0;
    String_StripWhitespace(S("h\te\nl\rl o"), &OutStr);
    Expect_String_IsEqual(S("hello"), OutStr, true);

    // All whitespace
    OutStr.Length = 0;
    String_StripWhitespace(S("   \t\n\r  "), &OutStr);
    Expect_IsEqual(0, OutStr.Length);

    // Single character with no whitespace
    OutStr.Length = 0;
    String_StripWhitespace(S("X"), &OutStr);
    Expect_String_IsEqual(S("X"), OutStr, true);

    // Single space
    OutStr.Length = 0;
    String_StripWhitespace(S(" "), &OutStr);
    Expect_IsEqual(0, OutStr.Length);

    // Empty string
    OutStr.Length = 0;
    String_StripWhitespace(S(""), &OutStr);
    Expect_IsEqual(0, OutStr.Length);

    // Symbols and digits are preserved
    OutStr.Length = 0;
    String_StripWhitespace(S("a 1 ! b 2 @"), &OutStr);
    Expect_String_IsEqual(S("a1!b2@"), OutStr, true);

    // Multiple consecutive spaces
    OutStr.Length = 0;
    String_StripWhitespace(S("hello     world"), &OutStr);
    Expect_String_IsEqual(S("helloworld"), OutStr, true);

    return true;
}

TEST(StringUtils_EatSpacesFromEnd)
{
    // Trailing spaces removed
    String Result = String_EatSpacesFromEnd(S("hello   "));
    Expect_String_IsEqual(S("hello"), Result, true);
    Expect_IsEqual(5, Result.Length);

    // Single trailing space
    Result = String_EatSpacesFromEnd(S("hello "));
    Expect_String_IsEqual(S("hello"), Result, true);

    // No trailing spaces - unchanged
    Result = String_EatSpacesFromEnd(S("hello"));
    Expect_String_IsEqual(S("hello"), Result, true);
    Expect_IsEqual(5, Result.Length);

    // All spaces - returns empty
    Result = String_EatSpacesFromEnd(S("   "));
    Expect_IsEqual(0, Result.Length);

    // Single space
    Result = String_EatSpacesFromEnd(S(" "));
    Expect_IsEqual(0, Result.Length);

    // Leading spaces preserved, only trailing removed
    Result = String_EatSpacesFromEnd(S("   hello   "));
    Expect_String_IsEqual(S("   hello"), Result, true);

    // Tabs are also whitespace (EatSpacesFromEnd uses IsWhitespace)
    Result = String_EatSpacesFromEnd(S("hello\t"));
    Expect_String_IsEqual(S("hello"), Result, true);

    // Newlines are also whitespace
    Result = String_EatSpacesFromEnd(S("hello\n"));
    Expect_String_IsEqual(S("hello"), Result, true);

    // Mixed trailing whitespace
    Result = String_EatSpacesFromEnd(S("hello \t\n\r"));
    Expect_String_IsEqual(S("hello"), Result, true);

    // Single character with trailing space
    Result = String_EatSpacesFromEnd(S("X "));
    Expect_String_IsEqual(S("X"), Result, true);

    // Single character no trailing space
    Result = String_EatSpacesFromEnd(S("X"));
    Expect_String_IsEqual(S("X"), Result, true);

    // Empty string
    Result = String_EatSpacesFromEnd(S(""));
    Expect_IsEqual(0, Result.Length);

    // Whitespace in the middle is preserved
    Result = String_EatSpacesFromEnd(S("hello world   "));
    Expect_String_IsEqual(S("hello world"), Result, true);

    return true;
}

TEST(MathUtils_ClampF64)
{
    Expect_Float64_IsEqual(5.0, ClampF64(5.0, 0.0, 10.0));
    Expect_Float64_IsEqual(0.0, ClampF64(-5.0, 0.0, 10.0));
    Expect_Float64_IsEqual(10.0, ClampF64(15.0, 0.0, 10.0));

    return true;
}

TEST(MathUtils_MinMaxF64)
{
    Expect_Float64_IsEqual(3.0, MinF64(3.0, 5.0));
    Expect_Float64_IsEqual(5.0, MaxF64(3.0, 5.0));
    Expect_Float64_IsEqual(3.0, MinF64(3.0, 3.0));
    Expect_Float64_IsEqual(3.0, MaxF64(3.0, 3.0));

    return true;
}

/*
███████╗███╗   ██╗████████╗██████╗ ██╗   ██╗    ██████╗  ██████╗ ██╗███╗   ██╗████████╗
██╔════╝████╗  ██║╚══██╔══╝██╔══██╗╚██╗ ██╔╝    ██╔══██╗██╔═══██╗██║████╗  ██║╚══██╔══╝
█████╗  ██╔██╗ ██║   ██║   ██████╔╝ ╚████╔╝     ██████╔╝██║   ██║██║██╔██╗ ██║   ██║
██╔══╝  ██║╚██╗██║   ██║   ██╔══██╗  ╚██╔╝      ██╔═══╝ ██║   ██║██║██║╚██╗██║   ██║
███████╗██║ ╚████║   ██║   ██║  ██║   ██║       ██║     ╚██████╔╝██║██║ ╚████║   ██║
╚══════╝╚═╝  ╚═══╝   ╚═╝   ╚═╝  ╚═╝   ╚═╝       ╚═╝      ╚═════╝ ╚═╝╚═╝  ╚═══╝   ╚═╝
*/

const usize GEngineMemoryAmount = Megabytes(1);
const usize GEngineScratchAmount = 0;

u32 RunApplication(const StringArray Arguments)
{
    Logging_ToggleLogTimeStamp(false);
    Logging_ToggleLogCategory(false);
    Logging_ToggleLogType(false);

    TestManager_Init();

    // TODO:
    // Array
    // Free List allocator
    
    // Test all api functions exposed i guess
    // Filesystem
    // Platform core

    #define X(group, name) REGISTER_TEST(group, name);
    TEST_LIST
    #undef X

    LOG_SUCCESS("All tests registered");

    bool bSuccess = TestManager_Run();

    #if !PLATFORM_MAC
    if (Platform_GetConsoleProcessCount() == 1)
    {
        LOG_INLINE_WARNING("\nLaunched outside an existing terminal. Waiting for any key press to exit ... ");

        Platform_BeginNonBlockingMode();
        while (true)
        {
            Platform_Wait(10 milliseconds);
            if (Platform_IsWindowFocused() && Platform_AnyKeyPressed())
            {
                break;
            }
        }
        Platform_EndNonBlockingMode();
    }
    #endif

    return bSuccess ? 0 : 1;
}
