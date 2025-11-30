#include "Core/EntryPoint.h"
#include "TestManager.h"

#include "Core/Allocators.h"
#include "Core/StringUtils.h"
#include "Core/Platform.h"
#include "Core/Filesystem.h"
#include "Core/MathUtils.h"
#include "Core/Log.h"

#define TEST(Name) static u8 Name(void)

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
    return BYPASS;
}

TEST(StringUtils_ToWide)
{
    return BYPASS;
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
    return BYPASS;
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
