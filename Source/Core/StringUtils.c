// Copyright (c) 2024 Ali El Saleh

#ifndef UNITY_BUILD
#include "StringUtils.h"
#include "Memory.h"
#include "Allocators.h"
#include "Globals.h"
#include "Log.h"
#endif

#define STB_SPRINTF_IMPLEMENTATION
#include "stb_sprintf.h"

bool String_IsValid(const String Str)
{
    bool bValid = true;
    if (!Str.Data || Str.Data[0] == 0 || Str.Length == 0 || Str.Data == String_Null().Data)
    {
        bValid = false;
    }

    return bValid;
}

bool StringArray_IsValid(const StringArray Str)
{
    bool bValid = true;
    if (!Str.List || Str.Num == 0 || Str.List == StringArray_Null().List)
    {
        bValid = false;
    }

    return bValid;
}

bool StringList_IsValid(const StringList Str)
{
    bool bValid = true;
    if (!Str.Next || Str.Next == StringList_Null().Next || !String_IsValid(Str.String))
    {
        bValid = false;
    }

    return bValid;
}

String String_Create(LinearAllocator* Arena, const String Source)
{
    String str = String_Null();
    bool bValid = Source.Length > 0 && (Arena->Allocated + Source.Length+1 < Arena->TotalSize);
    if (bValid)
    {
        str.Data = LinearAllocator_Allocate(Arena, Source.Length+1);
        MemCopy(str.Data, Source.Data, Source.Length);
        str.Data[Source.Length] = 0;
        str.Length = Source.Length;
        str.Capacity = Source.Length;
    }

    return str;
}

String String_Duplicate(LinearAllocator* Arena, const String Source)
{
    String str = String_Null();
    bool bValid = Source.Length > 0 && (Arena->Allocated + Source.Length+1 < Arena->TotalSize);
    if (bValid)
    {
        str.Data = LinearAllocator_Allocate(Arena, Source.Length+1);
        MemCopy(str.Data, Source.Data, Source.Length);
        str.Data[Source.Length] = 0;
        str.Length = Source.Length;
        str.Capacity = Source.Length;
    }

    return str;
}

String String_Reserve(LinearAllocator* Arena, u32 Capacity)
{
    String str = String_Null();
    bool bValid = Arena->Allocated + Capacity < Arena->TotalSize;
    if (bValid)
    {
        str.Data = LinearAllocator_Allocate(Arena, Capacity+1);
        str.Length = 0;
        str.Capacity = Capacity;
    }

    return str;
}

String String_ReserveAndCopy(LinearAllocator* Arena, u32 Capacity, const String Source)
{
    String str = String_Null();
    bool bValid = Arena->Allocated + Capacity < Arena->TotalSize;
    if (bValid)
    {
        str.Data = LinearAllocator_Allocate(Arena, Capacity+1);
        if (Source.Length) { MemCopy(str.Data, Source.Data, Source.Length); }
        str.Data[Source.Length] = 0;
        str.Length = Source.Length;
        str.Capacity = Capacity;
    }

    return str;
}

StringArray String_CreateArray(LinearAllocator* Arena, const StringArray Array)
{
    StringArray Result = StringArray_Null();
    bool bValid = (Arena->Allocated + (sizeof(String) * Array.Num)) < Arena->TotalSize;
    if (bValid)
    {
        Result.Num = Array.Num;
        Result.List = LinearAllocator_Allocate(Arena, sizeof(String) * Array.Num);
        
        for (u32 i = 0; i < Array.Num; i++)
        {
            Result.List[i] = String_Create(Arena, Array.List[i]);
        }
    }
    
    return Result;
}

String String_Join(LinearAllocator* Arena, const StringArray Array)
{
    u32 TotalSize = 0;
    for (u32 i = 0; i < Array.Num; i++)
    {
        TotalSize += Array.List[i].Length;
    }

    String JoinedStr = String_Null();

    bool bValid = Arena->Allocated + TotalSize+1 < Arena->TotalSize;
    if (bValid)
    {
        JoinedStr.Data = LinearAllocator_Allocate(Arena, TotalSize+1);
        JoinedStr.Length = TotalSize;
        JoinedStr.Capacity = TotalSize;

        u32 NumCopied = 0;
        for (u32 i = 0; i < Array.Num; i++)
        {
            const String* Str = &Array.List[i];
            u32 Len = Str->Length;
            if (LIKELY(Len > 0))
            {
                MemCopy(&JoinedStr.Data[NumCopied], Str->Data, Len);
                NumCopied += Len;
            }
        }
    }
    
    return JoinedStr;
}

void String_ConcatArray(String* Dest, const StringArray Array, u32 MaxSize)
{
    u32 TotalSize = 0;
    for (u32 i = 0; i < Array.Num; i++)
    {
        TotalSize += Array.List[i].Length;
    }
    
    u32 MaxLength = Min(TotalSize, MaxSize);

    Dest->Length = MaxLength;

    u32 NumCopied = 0;
    for (u32 i = 0; i < Array.Num; i++)
    {
        const String* Str = &Array.List[i];
        u32 Len = Str->Length;

        if (Dest->Length + Len > MaxSize)
        {
            break;
        }

        if (LIKELY(Len > 0))
        {
            MemCopy(&Dest->Data[NumCopied], Str->Data, Len);
            NumCopied += Len;
        }
    }
}

bool String_IsEqual(const String StringA, const String StringB, bool bCaseSensitive)
{
    bool bSameLength = StringA.Length == StringB.Length;
    bool bMatch = bSameLength;

    if (bMatch)
    {
        u32 Length = StringA.Length;

        if (bCaseSensitive)
        {
            for (u32 i = 0; i < Length; i++)
            {
                i32 A = (i32)StringA.Data[i];
                i32 B = (i32)StringB.Data[i];

                if (A != B)
                {
                    bMatch = false;
                    break;
                }
            }
        }
        else
        {
            for (u32 i = 0; i < Length; i++)
            {
                i32 A = (i32)StringA.Data[i];
                i32 B = (i32)StringB.Data[i];
                
                if (A >= 'A' && A <= 'Z')
                {
                    A += 32;
                }

                if (B >= 'A' && B <= 'Z')
                {
                    B += 32;
                }
            
                if (A != B)
                {
                    bMatch = false;
                    break;
                }
            }
        }
    }

    return bMatch;
}

bool String16_IsEqual(const String16 StringA, const String16 StringB, bool bCaseSensitive)
{
    bool bSameLength = StringA.Length == StringB.Length;
    bool bMatch = bSameLength;

    if (bMatch)
    {
        u32 Length = StringA.Length;

        if (bCaseSensitive)
        {
            for (u32 i = 0; i < Length; i++)
            {
                i32 A = (i32)StringA.Data[i];
                i32 B = (i32)StringB.Data[i];

                if (A != B)
                {
                    bMatch = false;
                    break;
                }
            }
        }
        else
        {
            for (u32 i = 0; i < Length; i++)
            {
                i32 A = (i32)StringA.Data[i];
                i32 B = (i32)StringB.Data[i];
                
                if (A >= 'A' && A <= 'Z')
                {
                    A += 32;
                }

                if (B >= 'A' && B <= 'Z')
                {
                    B += 32;
                }
            
                if (A != B)
                {
                    bMatch = false;
                    break;
                }
            }
        }
    }

    return bMatch;
}


bool String_IsInteger32(const String Str)
{
    bool bValid = Str.Length > 0 && Str.Length <= 10; // >10 is not a valid 32-bit integer

    if (bValid)
    {
        for (u32 i = 0; i < Str.Length; i++)
        {
            if (!IsDigit(Str.Data[i]))
            {
                bValid = false;
                break;
            }
        }
    }

    return bValid;
}

bool String_IsInteger(const String Str)
{
    bool bValid = Str.Length > 0 && Str.Length <= 20; // >20 is not a valid 64-bit integer

    for (u32 i = 0; i < Str.Length; i++)
    {
        if (!IsDigit(Str.Data[i]))
        {
            bValid = false;
            break;
        }
    }

    return bValid;
}

bool String_IsFloat(const String Str)
{
    bool bValid = Str.Length > 0;

    if (bValid)
    {
        bool bHasDot = false;
        for (u32 i = 0; i < Str.Length; i++)
        {
            if (!IsDigit(Str.Data[i]))
            {
                bool bShouldBreak = true;
                if (Str.Data[i] == '.' && !bHasDot)
                {
                    bShouldBreak = false;
                    bHasDot = true;
                }

                if (bShouldBreak)
                { 
                    bValid = false;
                    break;
                }
            }
            else
            {
                // no action required
            }
        }
    }

    return bValid;
}

bool String_IsNumeric(const String Str)
{
    return String_IsInteger(Str) || String_IsFloat(Str);
}

bool String_Contains(const String Str, const String SubString, bool bCaseSensitive)
{
    bool bContains = false;
    bool bValid = SubString.Length > 0 && SubString.Length <= Str.Length;

    for (u32 i = 0; bValid && i < Str.Length; i++)
    {
        bool bShouldBreak = Str.Length - i < SubString.Length;
        if (!bShouldBreak)
        {
            const String Offset = StrShiftF(Str, i);
            const String Slice = StrSlice(Offset.Data, SubString.Length);
            if (String_IsEqual(Slice, SubString, bCaseSensitive))
            {
                bContains = true;
                bShouldBreak = true;
            }
        }

        if (bShouldBreak)
        {
            break;
        }
    }

    return bContains;
}

bool String_ContainsPathSeparators(const String Str)
{
    bool bContains = false;

    for (u32 i = 0; i < Str.Length; i++)
    {
        if (Str.Data[i] == '/' || Str.Data[i] == '\\')
        {
            bContains = true;
            break;
        }
    }

    return bContains;
}

bool String_ContainsDigits(const String Str)
{
    bool bContains = false;

    for (u32 i = 0; i < Str.Length; i++)
    {
        if (IsDigit(Str.Data[i]))
        {
            bContains = true;
            break;
        }
    }

    return bContains;
}

bool String_ContainsNonDigits(const String Str)
{
    bool bContains = false;

    for (u32 i = 0; i < Str.Length; i++)
    {
        if (!IsDigit(Str.Data[i]))
        {
            bContains = true;
            break;
        }
    }

    return bContains;
}

bool String_StartsWith(const String Str, const String SubString, bool bCaseSensitive)
{
    bool bSuccess = Str.Length >= SubString.Length && SubString.Length > 0;
    if (bSuccess)
    {
        const String Slice = StrSlice(Str.Data, SubString.Length);
        bSuccess = String_IsEqual(Slice, SubString, bCaseSensitive);
    }

    return bSuccess;
}

bool String_EndsWith(const String Str, const String SubString, bool bCaseSensitive)
{
    bool bSuccess = Str.Length >= SubString.Length && SubString.Length > 0;
    if (bSuccess)
    {
        const String Slice = StrShiftF(Str, Str.Length - SubString.Length);
        bSuccess = String_IsEqual(Slice, SubString, bCaseSensitive);
    }

    return bSuccess;
}

void String_Copy(String* Dest, const String Source)
{
    u32 NumToCopy = Dest->Capacity == 0 ? Source.Length : Min(Dest->Capacity, Source.Length);
    MemCopy(Dest->Data, Source.Data, NumToCopy);
    Dest->Length = NumToCopy;
    Dest->Data[NumToCopy] = 0;
}

void String_CopyN(String* Dest, const String Source, u32 Length)
{
    u32 NumToCopy = Dest->Capacity == 0 ? Min(Source.Length, Length) : Min(Dest->Capacity, Min(Source.Length, Length));
    MemCopy(Dest->Data, Source.Data, NumToCopy);
    Dest->Length = NumToCopy;
    Dest->Data[NumToCopy] = 0;
}

void StringInternal_Concat(String* Dest, const StringArray Array)
{
    for (u8 i = 0; i < Array.Num; i++)
    {
        String_Append(Dest, Array.List[i]);
    }
}

void String_Append(String* Dest, const String Source)
{
    u32 NumToCopy = Min(Dest->Capacity, Source.Length);
    NumToCopy = Min(Dest->Capacity - Dest->Length, NumToCopy);
    MemCopy(&Dest->Data[Dest->Length], Source.Data, NumToCopy);
    Dest->Length += NumToCopy;
    Dest->Data[Dest->Length] = 0;
}

void String_AppendChar(String* Dest, const u8 Source)
{
    u32 NumToCopy = Min(Dest->Capacity, 1);
    NumToCopy = Min(Dest->Capacity - Dest->Length, NumToCopy);
    MemCopy(&Dest->Data[Dest->Length], &Source, NumToCopy);
    Dest->Length += NumToCopy;
    Dest->Data[Dest->Length] = 0;
}

void String_AppendSpace(String* Dest)
{
    String_AppendChar(Dest, ' ');
}

void String_AppendTab(String* Dest)
{
    String_AppendChar(Dest, '\t');
}

void String_AppendNewline(String* Dest)
{
    String_AppendChar(Dest, '\n');
}

void String_AppendPathSeparator(String* Dest)
{
    String_AppendChar(Dest, PATH_SEPARATOR);
}

void String_AppendPathSeparator_Checked(String* Dest)
{
    u8 LastChar = Dest->Data[Dest->Length-1];
    bool bHasPathSep = LastChar == '/' || LastChar == '\\';
    if (!bHasPathSep)
    {
        String_AppendChar(Dest, PATH_SEPARATOR);
    }
}

ECompareResult String_CompareVersion(const String VersionA, const String VersionB)
{
    if (VersionA.Length == 0 || VersionB.Length == 0) { return CompareResult_None; }

    // compare each version separated by '.' or '-'

    ECompareResult Result = CompareResult_None;

    #define MAX_VERSIONS 32

    u64 VersionArrayA[MAX_VERSIONS];
    u64 VersionArrayB[MAX_VERSIONS];
    for (u8 i = 0; i < MAX_VERSIONS; i++) { VersionArrayA[i] = UINT64_MAX; }
    for (u8 i = 0; i < MAX_VERSIONS; i++) { VersionArrayB[i] = UINT64_MAX; }

    u8 VersionIndexA = 0, VersionIndexB = 0;
    u32 OffsetA = 0, OffsetB = 0;

    while (1)
    {
        u32 IndexA = 0, IndexB = 0;
        u32 ThisOffsetA = OffsetA, ThisOffsetB = OffsetB;

        for (u32 i = OffsetA; i < VersionA.Length; i++)
        {
            if (VersionA.Data[i] == '.' || VersionA.Data[i] == '-' || i == VersionA.Length-1)
            {
                IndexA = i - OffsetA;
                OffsetA = i;

                if (i == VersionA.Length-1)
                {
                    IndexA++;
                    OffsetA++;
                }

                break;
            }
        }

        for (u32 i = OffsetB; i < VersionB.Length; i++)
        {
            if (VersionB.Data[i] == '.' || VersionB.Data[i] == '-' || i == VersionB.Length-1)
            {
                IndexB = i - OffsetB;
                OffsetB = i;

                if (i == VersionB.Length-1)
                {
                    IndexB++;
                    OffsetB++;
                }
                
                break;
            }
        }

        const String SubVersionA = StrSlice(StrShiftF(VersionA, ThisOffsetA).Data, IndexA);
        const String SubVersionB = StrSlice(StrShiftF(VersionB, ThisOffsetB).Data, IndexB);

        u64 A = 0, B = 0;
        (void)String_ToU64(SubVersionA, &A);
        (void)String_ToU64(SubVersionB, &B);

        VersionArrayA[VersionIndexA] = A;
        VersionArrayB[VersionIndexB] = B;

        VersionIndexA++;
        VersionIndexB++;

        OffsetA++;
        OffsetB++;

        if (VersionIndexA >= MAX_VERSIONS || VersionIndexB >= MAX_VERSIONS)
        {
            break;
        }

        if (OffsetA > VersionA.Length-1 || OffsetB > VersionB.Length-1)
        {
            break;
        }
    }

    for (u8 i = 0; i < MAX_VERSIONS; i++)
    {
        bool bShouldBreak = VersionArrayA[i] == UINT64_MAX || VersionArrayB[i] == UINT64_MAX;
        if (!bShouldBreak)
        {
            if (VersionArrayA[i] == VersionArrayB[i])
            {
                Result = CompareResult_Equal;
                continue;
            }

            if (VersionArrayA[i] > VersionArrayB[i])
            {
                Result = CompareResult_Greater;
            }
            else
            {
                Result = CompareResult_Less;
            }

            bShouldBreak = true;
        }

        if (bShouldBreak)
        {
            break;
        }
    }

    return Result;
}

void String_Format(String* Dest, const String Format, ...)
{
    va_list Args = {0};
    va_start(Args, Format);
    const i32 NewCap = (i32)Clamp(Dest->Capacity, 0, INT32_MAX); 
    const i32 Written = stbsp_vsnprintf((char*)Dest->Data, NewCap, (char*)Format.Data, Args);
    Dest->Length = (u32)Clamp(Written, 0, INT32_MAX);
    va_end(Args);
}

void String_FormatV(String* Dest, const String Format, u32 Capacity, void* VAList)
{
    const i32 NewCap = (i32)Clamp(Capacity, 0, INT32_MAX); 
    const i32 Written = stbsp_vsnprintf((char*)Dest->Data, NewCap, (char*)Format.Data, VAList);
    Dest->Length = (u32)Clamp(Written, 0, INT32_MAX);
}

void StringInternal_BuildPath(String* Dest, const StringArray Array)
{
    for (u8 i = 0; i < Array.Num; i++)
    {
        const String Param = Array.List[i];

        if (!String_IsValid(Param))
        {
            continue;
        }

        // '.' paths are ignored since they're kinda redundant to be in the path anyway
        if (Param.Length == 1 && Param.Data[0] == '.')
        {
            continue;
        }

        if (Dest->Length > 0)
        {
            u8 LastChar = Dest->Data[Dest->Length-1];
            bool bHasPathSeparator = LastChar == '/' || LastChar == '\\';
            if (!bHasPathSeparator)
            {
                String_AppendPathSeparator(Dest);
            }
        }

        #if PLATFORM_WINDOWS
        String ParamModified = String_EatPathSeparators(Param);
        #else
        // on non-windows systems, we need to keep the first '/', since that matters a lot
        // if we're the first param to add, dont eat the first '/'
        String ParamModified = Dest->Length > 0 ? String_EatPathSeparators(Param) : Param;
        #endif

        ParamModified = String_EatChar(ParamModified, '"');
        ParamModified = String_EatCharFromEnd(ParamModified, '"');

        String_Append(Dest, ParamModified);
        (void)String_EatPathSeparatorsInlineFromEnd(Dest);

        if (Dest->Length > 0 && i != Array.Num-1)
        {
            String_AppendPathSeparator(Dest);
        }
    }

    (void)String_EatPathSeparatorsInlineFromEnd(Dest);
    String_ConvertSlashToPlatformSlash(Dest);
}

void StringInternal_BuildSeparator(String* Dest, u8 Separator, const StringArray Array)
{
    for (u8 i = 0; i < Array.Num; i++)
    {
        String Param = Array.List[i];

        if (!String_IsValid(Param))
        {
            continue;
        }

        if (Dest->Length > 0)
        {
            u8 LastChar = Dest->Data[Dest->Length-1];
            bool bHasSeparator = LastChar == Separator;
            if (!bHasSeparator)
            {
                String_AppendChar(Dest, Separator);
            }
        }

        const String Trimmed = String_EatChar(Param, Separator);
        String_Append(Dest, Trimmed);
        (void)String_EatCharInlineFromEnd(Dest, Separator);

        if (Dest->Length > 0 && i != Array.Num-1)
        {
            String_AppendChar(Dest, Separator);
        }
    }
}

void String_Empty(String* Str)
{
    if (Str->Length > 0)
    {
        MemZero(Str->Data, Str->Length);
        Str->Length = 0;
    }
}

void String_Zero(String* Str)
{
    if (Str->Length > 0)
    {
        MemZero(Str->Data, Str->Length);
    }
}

void String_Fill(String* Str, u8 C)
{
    if (Str->Length > 0)
    {
        MemSet(Str->Data, C, Str->Length);
    }
}

void String_ToLower(String* Str)
{
    for (u32 i = 0; i < Str->Length; i++)
    {
        Str->Data[i] = ToLower(Str->Data[i]);
    }
}

void String_ToUpper(String* Str)
{
    for (u32 i = 0; i < Str->Length; i++)
    {
        Str->Data[i] = ToUpper(Str->Data[i]);
    }
}

void String_ToWide(const String FromString, String16* ToString)
{
    u32 MinLength = Min(FromString.Length, ToString->Capacity);
    for (u32 i = 0; i < MinLength; i++)
    {
        ToString->Data[i] = (wchar)FromString.Data[i];
    }

    ToString->Length = MinLength;
}

void String_ToNarrow(const String16 FromString, String* ToString)
{
    u32 MinLength = Min(FromString.Length, ToString->Capacity);
    for (u32 i = 0; i < MinLength; i++)
    {
        ToString->Data[i] = (uchar)FromString.Data[i];
    }

    ToString->Length = MinLength;
}

void String_BackSlashToForwardSlash(String* Str)
{
    for (u32 i = 0; i < Str->Length; i++)
    {
        if (Str->Data[i] == '\\')
        {
            Str->Data[i] = '/';
        }
    }
}

void String_ForwardSlashToBackSlash(String* Str)
{
    for (u32 i = 0; i < Str->Length; i++)
    {
        if (Str->Data[i] == '/')
        {
            Str->Data[i] = '\\';
        }
    }
}

// TODO
//void String_ToNativePathSeparators(String* Str)
void String_ConvertSlashToPlatformSlash(String* Str)
{
#if PLATFORM_WINDOWS
    String_ForwardSlashToBackSlash(Str);
#else
    String_BackSlashToForwardSlash(Str);
#endif
}

String String_EatSpaces(String Str)
{
    u32 i = 0;
    for (; i < Str.Length; i++)
    {
        if (!IsWhitespace(Str.Data[i]))
        {
            break;
        }
    }

    return StrShiftF(Str, i);
}

String String_EatNewLines(String Str)
{
    u32 i = 0;
    for (; i < Str.Length; i++)
    {
        if (!IsNewline(Str.Data[i]))
        {
            break;
        }
    }

    return StrShiftF(Str, i);
}

bool String_EatSpacesInline(String* Str)
{
    u32 i = 0;
    for (; i < Str->Length; i++)
    {
        if (!IsWhitespace(Str->Data[i]))
        {
            break;
        }
    }

    Str->Data += i;
    Str->Length -= i;

    return i > 0;
}

bool String_ReplaceCharInline(String* Str, u8 Char, u8 ReplaceChar)
{
    bool bAnyChange = false;
    for (u32 i = 0; i < Str->Length; i++)
    {
        if (Str->Data[i] == Char)
        {
            Str->Data[i] = ReplaceChar;
            bAnyChange = true;
        }
    }

    return bAnyChange;
}

bool String_ReplaceNonAlphaNumericCharInline(String* Str, u8 ReplaceChar)
{
    bool bAnyChange = false;
    for (u32 i = 0; i < Str->Length; i++)
    {
        if (!IsAlphabet(Str->Data[i]) && !IsDigit(Str->Data[i]))
        {
            Str->Data[i] = ReplaceChar;
            bAnyChange = true;
        }
    }

    return bAnyChange;
}

bool String_CollapseMatching(String* Dest, const String A, const String B, bool bCaseSensitive)
{
    const String LongestString  = A.Length > B.Length ? A : B;
    const String ShortestString = A.Length > B.Length ? B : A;

    bool bAnyChange = false;
    for (u32 i = 0; i < LongestString.Length; i++)
    {
        bool bShouldBreak = i == ShortestString.Length;
        if (!bShouldBreak)
        {
            i32 C1 = (i32)A.Data[i];
            i32 C2 = (i32)B.Data[i];
        
            if (!bCaseSensitive)
            {
                if (C1 >= 'A' && C1 <= 'Z') { C1 += 32; }
                if (C2 >= 'A' && C2 <= 'Z') { C2 += 32; }
            }

            if (C1 != C2)
            {
                bShouldBreak = true;
            }
        }

        if (bShouldBreak)
        {
            // append the remaining
            const String Trimmed = StrShiftF(LongestString, i);
            String_Append(Dest, Trimmed);
            bAnyChange = true;
            break;
        }
    }

    return bAnyChange;
}

String String_EatChar(String Str, u8 Char)
{
    u32 i = 0;
    for (; i < Str.Length; i++)
    {
        if (Str.Data[i] != Char)
        {
            break;
        }
    }

    return StrShiftF(Str, i);
}

String String_EatPathSeparatorsFromEnd(String Str)
{
    i32 i = ((i32)Str.Length)-1;
    for (; i >= 0; i--)
    {
        if (Str.Data[i] != '/' && Str.Data[i] != '\\')
        {
            i++;
            break;
        }
    }

    String Result = String_Null();
    if (i > 0)
    {
        Result = StrSlice(Str.Data, (u32)i);
    }

    return Result;
}

String String_EatPathSeparators(String Str)
{
    u32 i = 0;
    for (; i < Str.Length; i++)
    {
        if (Str.Data[i] != '/' && Str.Data[i] != '\\')
        {
            break;
        }
    }

    return StrShiftF(Str, i);
}

bool String_EatCharInline(String* Str, u8 Char)
{
    u32 i = 0;
    for (; i < Str->Length; i++)
    {
        if (Str->Data[i] != Char)
        {
            break;
        }
    }

    Str->Data += i;
    Str->Length -= i;

    return i > 0;
}

bool String_EatCharInline_Single(String* Str, u8 Char)
{
    bool bSuccess = false;
    if (Str->Data[0] == Char)
    {
        Str->Data++;
        Str->Length--;
        bSuccess = true;
    }

    return bSuccess;
}

bool String_EatPathSeparatorsInline(String* Str)
{
    u32 i = 0;
    for (; i < Str->Length; i++)
    {
        if (Str->Data[i] != '/' && Str->Data[i] != '\\')
        {
            break;
        }
    }

    Str->Data += i;
    Str->Length -= i;

    return i > 0;
}

String String_EatCharFromEnd(String Str, u8 Char)
{
    i32 i = ((i32)Str.Length)-1;
    for (; i >= 0; i--)
    {
        if (Str.Data[i] != Char)
        {
            i++;
            break;
        }
    }

    String Result = String_Null();
    if (i > 0)
    {
        Result = StrSlice(Str.Data, (u32)i);
    }

    return Result;
}

bool String_EatCharInlineFromEnd(String* Str, u8 Char)
{
    i32 i = ((i32)Str->Length)-1;
    for (; i >= 0; i--)
    {
        if (Str->Data[i] != Char)
        {
            i++;
            break;
        }
    }
    
    bool bAnyChange = i >= 0 && (u32)i < Str->Length;
    Str->Length = i >= 0 ? (u32)i : 0;

    return bAnyChange;
}

String String_EatSpacesFromEnd(String Str)
{
    i32 i = ((i32)Str.Length)-1;
    for (; i >= 0; i--)
    {
        if (!IsWhitespace(Str.Data[i]))
        {
            i++;
            break;
        }
    }

    String Result = String_Null();
    if (i > 0)
    {
        Result = StrSlice(Str.Data, (u32)i);
    }

    return Result;
}

String String_EatNewLinesFromEnd(String Str)
{
    i32 i = ((i32)Str.Length)-1;
    for (; i >= 0; i--)
    {
        if (!IsNewline(Str.Data[i]))
        {
            i++;
            break;
        }
    }

    String Result = String_Null();
    if (i > 0)
    {
        Result = StrSlice(Str.Data, (u32)i);
    }

    return Result;
}

bool String_EatSpacesInlineFromEnd(String* Str)
{
    i32 i = ((i32)Str->Length)-1;
    for (; i >= 0; i--)
    {
        if (!IsWhitespace(Str->Data[i]))
        {
            i++;
            break;
        }
    }
    
    bool bAnyChange = i >= 0 && (u32)i < Str->Length;
    Str->Length = i >= 0 ? (u32)i : 0;

    return bAnyChange;
}

bool String_EatNewLinesInline(String* Str)
{
    u32 i = 0;
    for (; i < Str->Length; i++)
    {
        if (IsNewline(Str->Data[i]))
        {
            break;
        }
    }

    Str->Data += i;
    Str->Length -= i;

    return i > 0;
}

bool String_EatNewLinesInlineFromEnd(String* Str)
{
    i32 i = ((i32)Str->Length)-1;
    for (; i >= 0; i--)
    {
        if (!IsNewline(Str->Data[i]))
        {
            i++;
            break;
        }
    }
    
    bool bAnyChange = i >= 0 && (u32)i < Str->Length;
    Str->Length = i >= 0 ? (u32)i : 0;

    return bAnyChange;
}

bool String_EatPathSeparatorsInlineFromEnd(String* Str)
{
    i32 i = ((i32)Str->Length)-1;
    for (; i >= 0; i--)
    {
        if (Str->Data[i] != '/' && Str->Data[i] != '\\')
        {
            i++;
            break;
        }
    }
    
    bool bAnyChange = i >= 0 && (u32)i < Str->Length;
    Str->Length = i >= 0 ? (u32)i : 0;

    return bAnyChange;
}

String String_ScanUntil(const String* Str, u8 Char)
{
    u32 NewLength = 0;
    for (u32 i = 0; i < Str->Length; i++)
    {
        if (Str->Data[i] == Char)
        {
            NewLength = i;
            break;
        }
    }

    String Result = (String){ .Data = Str->Data, .Length = NewLength, .Capacity = Str->Capacity };
    return Result;
}

/*
void CString_ToLower(char* Str)
{
    for (char* p = Str; *p; ++p)
    {
        *p = (char)ToLower((uchar)*p);
    }
}

void CString_ToUpper(char* Str)
{
    for (char* p = Str; *p; ++p)
    {
        *p = (char)ToUpper((uchar)*p);
    }
}

void CString_ToWide(const char* FromString, wchar* ToString)
{
    u64 Len = String_GetLength((char*)FromString);
    for (u64 i = 0; i < Len; i++)
    {
        ToString[i] = (wchar)FromString[i];
    }
}

void CString_ToNarrow(const wchar* FromString, char* ToString)
{
    u64 Len = String16_GetLength(FromString);
    for (u64 i = 0; i < Len; i++)
    {
        ToString[i] = (char)FromString[i];
    }
}

u32 CString_Copy(char* Dest, const char* Source)
{
    u32 Len = String_GetLength((char*)Source)+1;
    MemCopy(Dest, Source, Len);
    return Len;
}

u32 CString_CopyN(char* Dest, const char* Source, u32 Length)
{
    u32 SourceLen = String_GetLength(Source)+1;
    MemCopy(Dest, Source, SourceLen < Length ? SourceLen : Length);
    return SourceLen;
}

u32 CString_ScanUntil(const char* Str, char Char)
{
    u32 NewLength = 0;
    while (Str[NewLength] != 0)
    {
        if (Str[NewLength] == Char)
        {
            NewLength--;
            return NewLength;
        }

        NewLength++;
    }

    return NewLength;
}


void CString_SubString(char* Dest, const char* Source, u32 Start, u32 Length)
{
    u32 SourceLength = String_GetLength(Source);
    if (Start >= SourceLength)
    {
        Dest[0] = 0;
        return;
    }
    
    if (Length > 0)
    {
        u32 i = Start;
        for (u32 j = 0; j < Length && Source[i]; ++i, ++j)
        {
            Dest[j] = Source[i];
        }
        
        Dest[Start + Length] = 0;
    }
    else
    {
        u32 j = 0;
        for (u32 i = Start; Source[i]; ++i, ++j)
        {
            Dest[j] = Source[i];
        }
        
        Dest[Start + j] = 0;
    }
}

char* CString_Empty(char* Str)
{
    MemZero(Str, String_GetLength(Str));
    return Str;
}

void CString_Zero(char* Str, u32 Length)
{
    if (Length > 0)
    {
        MemZero(Str, Length);
    }
}

void CString_Fill(char* Str, u32 Length, char N)
{
    if (Length > 0)
    {
        MemSet(Str, N, Length);
    }
}

i32 CString_Format(char* Dest, const char* Format, u32 MaxLength, ...)
{
    va_list Args;
    va_start(Args, MaxLength);
    i32 Written = stbsp_vsnprintf(Dest, (i32)MaxLength, Format, Args);
    va_end(Args);

    return Written;
}

i32 CString_FormatV(char* Dest, const char* Format, u32 MaxLength, void* VAList)
{
    return stbsp_vsnprintf(Dest, (i32)MaxLength, Format, VAList);
}

void CString_ToBytes(const char* Data, u32 Length, char* OutBytes)
{
    MemCopy(OutBytes, Data, Length);
}

void CString_FromBytes(const char* Data, u32 Length, char* OutCharacters)
{
    MemCopy(OutCharacters, Data, Length);
}

bool CString_IsEqual(const char* StringA, const char* StringB, bool bCaseSensitive)
{
    u64 Length = String_GetLength(StringA);
    u64 LengthB = String_GetLength(StringB);

    if (UNLIKELY(Length == 0 && LengthB == 0))
    {
        return true;
    }

    if (Length != LengthB)
        return false;

    if (bCaseSensitive)
    {
        for (u64 i = 0; i < Length; i++)
        {
            if (StringA[i] != StringB[i])
            {
                return false;
            }
        }

        return true;
    }

    for (u64 i = 0; i < Length; i++)
    {
        i32 A = (i32)StringA[i];
        i32 B = (i32)StringB[i];

        if ((A >= 'A' && A <= 'Z'))
        {
            A += 32;
        }

        if ((B >= 'A' && B <= 'Z'))
        {
            B += 32;
        }

        if (A != B)
        {
            return false;
        }
    }

    return true;
}

bool CString_IndexOfChar(const char* Str, char C, u32* OutIndex)
{
    u32 i = 0;
    while (Str[i] != 0)
    {
        if (Str[i] == C)
        {
            *OutIndex = i;
            return true;
        }

        i++;
    }

    return false;
}

bool CString_ToBool(const char* Str)
{
    return String_IsEqual(CStrView(Str), S("1"), false) || String_IsEqual(CStrView(Str), S("true"), false);
}
*/

bool String_IndexOfChar(const String Str, u8 C, u32* OutIndex)
{
    bool bFound = false;
    for (u32 i = 0; i < Str.Length; ++i)
    {
        if (Str.Data[i] == C)
        {
            if (OutIndex)
            {
                *OutIndex = i;
            }

            bFound = true;
            break;
        }
    }
    
    return bFound;
}

bool String_IsFirst(const String Str, u8 C)
{
    bool bIsFirst = Str.Length > 0 ? Str.Data[0] == C : false;
    return bIsFirst;
}

bool String_IsLast(const String Str, u8 C)
{
    bool bIsLast = Str.Length > 0 ? Str.Data[Str.Length-1] == C : false;
    return bIsLast;
}

bool String_IndexOfLastChar(const String Str, u8 C, u32* OutIndex)
{
    bool bFound = false;
    for (i32 i = ((i32)Str.Length)-1; i >= 0; i--)
    {
        if (Str.Data[i] == C)
        {
            if (OutIndex)
            {
                *OutIndex = (u32)i;
            }

            bFound = true;
            break;
        }
    }

    return bFound;
}

bool String_IndexOfFirstPathSlash(const String Str, u32* OutIndex)
{
    bool bFound = false;
    for (u32 i = 0; i < Str.Length; ++i)
    {
        if (Str.Data[i] == '/' || Str.Data[i] == '\\')
        {
            if (OutIndex)
            {
                *OutIndex = (u32)i;
            }

            bFound = true;
            break;
        }
    }
    
    return bFound;
}

bool String_IndexOfLastPathSlash(const String Str, u32* OutIndex)
{
    bool bFound = false;
    for (i32 i = ((i32)Str.Length)-1; i >= 0; i--)
    {
        if (Str.Data[i] == '/' || Str.Data[i] == '\\')
        {
            if (OutIndex)
            {
                *OutIndex = (u32)i;
            }

            bFound = true;
            break;
        }
    }

    return bFound;
}

bool String_IndexOfFirstWhitespace(const String Str, u32* OutIndex)
{
    bool bFound = false;
    for (u32 i = 0; i < Str.Length; ++i)
    {
        if (IsWhitespace(Str.Data[i]))
        {
            if (OutIndex)
            {
                *OutIndex = i;
            }

            bFound = true;
            break;
        }
    }
    
    return bFound;
}

bool String_IndexOfLastWhitespace(const String Str, u32* OutIndex)
{
    bool bFound = false;
    for (i32 i = ((i32)Str.Length)-1; i >= 0; i--)
    {
        if (IsWhitespace(Str.Data[i]))
        {
            if (OutIndex)
            {
                *OutIndex = (u32)i;
            }

            bFound = true;
            break;
        }
    }

    return bFound;
}

bool String_IndexOfFirstNewline(const String Str, u32* OutIndex)
{
    bool bFound = false;
    for (u32 i = 0; i < Str.Length; ++i)
    {
        if (IsNewline(Str.Data[i]))
        {
            if (OutIndex)
            {
                *OutIndex = i;
            }
            
            bFound = true;
            break;
        }
    }
    
    return bFound;
}

bool String_IndexOfSubstring(const String Str, const String Substring, bool bCaseSensitive, u32* OutIndex)
{
    bool bFound = false;

    if (Substring.Length > 0)
    {
        for (u32 i = 0; i < Str.Length; ++i)
        {
            const String Slice = StrSlice(Str.Data + i, Substring.Length);
            if (String_IsEqual(Slice, Substring, bCaseSensitive))
            {
                if (OutIndex)
                {
                    *OutIndex = i;
                }

                bFound = true;
                break;
            }
        }
    }

    return bFound;
}

// transforms paths with " in them to paths without them
// for exmaple: "C:\Program Files"\MyApp -> "C:\Program Files\MyApp"

bool String_SanitizeQuotes(String* Dest, const String Source)
{
    bool bHasQuote = false;
    for (u32 i = 0; i < Source.Length; i++)
    {
        const u8 c = Source.Data[i];
        if (c == '"' && bHasQuote)
        {
            // ignore all subsequent quotes
            continue;
        }

        String_AppendChar(Dest, c);

        if (c == '"')
        {
            bHasQuote = true;
        }
    }

    if (bHasQuote)
    {
        String_AppendChar(Dest, '"');
    }

    return Dest->Length > 0;
}

bool String_SanitizePath(String* Dest, const String Source)
{
    bool bAnyChange = false;
    for (u32 i = 0; i < Source.Length; i++)
    {
        if (Source.Data[i] == '"')
        {
            continue;
        }
        
        bAnyChange = true;

        #if PLATFORM_WINDOWS
        u8 C = Source.Data[i] == '/' ? '\\' : Source.Data[i]; 
        #else
        u8 C = Source.Data[i] == '\\' ? '/' : Source.Data[i]; 
        #endif

        if (C == '/' || C == '\\')
        {
            if (Dest->Length > 0)
            {
                u8 LastChar = Dest->Data[Dest->Length-1];
                bool bHasPathSep = LastChar == '/' || LastChar == '\\';
                if (bHasPathSep)
                {
                    continue;
                }
            }
        }

        String_AppendChar(Dest, C);
    }

    return bAnyChange;
}

bool String_SanitizePathAndWrap(String* Dest, const String Source)
{
    bool bAnyChange = false;

    if (Source.Length > 0)
    {
        String_AppendChar(Dest, '"');

        for (u32 i = 0; i < Source.Length; i++)
        {
            if (Source.Data[i] == '"')
            {
                continue;
            }

            bAnyChange = true;

            #if PLATFORM_WINDOWS
            u8 C = Source.Data[i] == '/' ? '\\' : Source.Data[i]; 
            #else
            u8 C = Source.Data[i] == '\\' ? '/' : Source.Data[i]; 
            #endif

            if (C == '/' || C == '\\')
            {
                if (Dest->Length > 0)
                {
                    u8 LastChar = Dest->Data[Dest->Length-1];
                    bool bHasPathSep = LastChar == '/' || LastChar == '\\';
                    if (bHasPathSep)
                    {
                        continue;
                    }
                }
            }

            String_AppendChar(Dest, C);
        }

        String_AppendChar(Dest, '"');
    }

    return bAnyChange;
}

u32 String_CountChar(const String Str, u8 C)
{
    u32 Count = 0;
    for (u32 i = 0; i < Str.Length; i++)
    {
        if (Str.Data[i] == C)
        {
            Count++;
        }
    }

    return Count;
}

u32 String_CountSpaces(const String Str)
{
    u32 Count = 0;
    for (u32 i = 0; i < Str.Length; i++)
    {
        if (IsWhitespace(Str.Data[i]))
        {
            Count++;
        }
    }

    return Count;
}

u32 String_CountPathSeparators(const String Str)
{
    u32 Count = 0;
    for (u32 i = 0; i < Str.Length; i++)
    {
        if (Str.Data[i] == '/' || Str.Data[i] == '\\')
        {
            Count++;
        }
    }

    return Count;
}

void String_StripString(const String Str, const String Substring, String* OutStr)
{
    const u32 MaxLength = Min(OutStr->Capacity, Str.Length);

    for (u32 i = 0; i < MaxLength;)
    {
        if (String_IsEqual(StrSlice(Str.Data + i, Substring.Length), Substring, false))
        {
            i += Substring.Length;
        }
        else
        {
            OutStr->Data[OutStr->Length] = Str.Data[i];
            OutStr->Length++;

            i++;
        }
    }
}

void String_StripChar(const String Str, u8 C, String* OutStr)
{
    const u32 MaxLength = Min(OutStr->Capacity, Str.Length);

    for (u32 i = 0; i < MaxLength; i++)
    {
        if (Str.Data[i] == C)
        {
            OutStr->Data[OutStr->Length] = Str.Data[i];
            OutStr->Length++;
        }
    }
}

void String_StripWhitespace(const String Str, String* OutStr)
{
    const u32 MaxLength = Min(OutStr->Capacity, Str.Length);

    for (u32 i = 0; i < MaxLength; i++)
    {
        if (!IsWhitespace(Str.Data[i]))
        {
            OutStr->Data[OutStr->Length] = Str.Data[i];
            OutStr->Length++;
        }
    }
}

void String_StripNewline(const String Str, String* OutStr)
{
    const u32 MaxLength = Min(OutStr->Capacity, Str.Length);

    for (u32 i = 0; i < MaxLength; i++)
    {
        if (!IsNewline(Str.Data[i]))
        {
            OutStr->Data[OutStr->Length] = Str.Data[i];
            OutStr->Length++;
        }
    }
}

void String_StripDigit(const String Str, String* OutStr)
{
    const u32 MaxLength = Min(OutStr->Capacity, Str.Length);

    for (u32 i = 0; i < MaxLength; i++)
    {
        if (!IsDigit(Str.Data[i]))
        {
            OutStr->Data[OutStr->Length] = Str.Data[i];
            OutStr->Length++;
        }
    }
}

void String_StripSymbol(const String Str, String* OutStr)
{
    const u32 MaxLength = Min(OutStr->Capacity, Str.Length);

    for (u32 i = 0; i < MaxLength; i++)
    {
        if (!IsSymbol(Str.Data[i]))
        {
            OutStr->Data[OutStr->Length] = Str.Data[i];
            OutStr->Length++;
        }
    }
}

void String_StripAlphabet(const String Str, String* OutStr)
{
    const u32 MaxLength = Min(OutStr->Capacity, Str.Length);

    for (u32 i = 0; i < MaxLength; i++)
    {
        if (!IsAlphabet(Str.Data[i]))
        {
            OutStr->Data[OutStr->Length] = Str.Data[i];
            OutStr->Length++;
        }
    }
}

bool String_ToF32(const String Str, f32* OutFloat)
{
    bool bSuccess = false;

    u32 Index = 0;
    i8 Sign = 1;

    if (Str.Length > 0)
    {
        if (Str.Data[0] == '-')
        {
            Sign = -1;
            Index++;
        }
        else if (Str.Data[0] == '+')
        {
            Index++;
        }
        else
        {
            // no action required
        }
    }

    f32 Num = 0.0f;
    bool bDecimalFound = false;
    u8 DecimalPlaces = 0;

    while (Index < Str.Length)
    {
        bool bShouldBreak = false;

        u8 c = Str.Data[Index];
        if (IsDigit(c))
        {
            i32 Digit = c - '0';

            if ((FLT_MAX - (f32)Digit) / 10 < Num)
            {
                Num = 0;
                bSuccess = false;
                bShouldBreak = true;
            }
            else
            {
                if (bDecimalFound)
                {
                    DecimalPlaces++;
                    f32 PowResult = 10.0f;
                    for (u8 i = 1; i < DecimalPlaces; i++)
                    {
                        PowResult *= 10.0f;
                    }

                    Num = Num + (f32)Digit / PowResult;
                }
                else
                {
                    Num = Num * 10.0f + (f32)Digit;
                }
            }
        }
        else if (c == '.')
        {
            if (bDecimalFound)
            {
                Num = 0;
                bSuccess = false;
                bShouldBreak = true;
            }

            bDecimalFound = true;
        }
        else
        {
            bSuccess = true;
            bShouldBreak = true;
        }

        if (bShouldBreak)
        {
            break;
        }

        Index++;
        c = Str.Data[Index];
    }

    if (OutFloat)
    {
        *OutFloat = Num * (f32)Sign;
    }

    return bSuccess;
}

bool String_ToF64(const String Str, f64* OutFloat)
{
    bool bSuccess = false;

    u64 Index = 0;
    i8 Sign = 1;

    if (Str.Length > 0)
    {
        if (Str.Data[0] == '-')
        {
            Sign = -1;
            Index++;
        }
        else if (Str.Data[0] == '+')
        {
            Index++;
        }
        else
        {
            // no action required
        }
    }

    f64 Num = 0;
    bool bDecimalFound = false;
    u64 DecimalPlaces = 0;

    while (Index < Str.Length)
    {
        bool bShouldBreak = false;

        u8 c = Str.Data[Index];
        if (IsDigit(c))
        {
            i32 Digit = c - '0';

            if ((DBL_MAX - (f64)Digit) / 10.0 < Num)
            {
                Num = 0;
                bSuccess = false;
                bShouldBreak = true;
            }
            else
            {
                if (bDecimalFound)
                {
                    DecimalPlaces++;

                    f64 PowResult = 10.0;
                    for (u8 i = 1; i < (u8)DecimalPlaces; i++)
                    {
                        PowResult *= 10.0;
                    }

                    Num = Num + (f64)Digit / PowResult;
                }
                else
                {
                    Num = Num * 10.0 + Digit;
                }
            }
        }
        else if (c == '.')
        {
            if (bDecimalFound)
            {
                Num = 0;
                bSuccess = false;
                bShouldBreak = true;
            }

            bDecimalFound = true;
        }
        else
        {
            bSuccess = true;
            bShouldBreak = true;
        }

        if (bShouldBreak)
        {
            break;
        }

        Index++;
        c = Str.Data[Index];
    }

    if (OutFloat)
    {
        *OutFloat = Num * Sign;
    }

    return bSuccess;
}

// IntType
// 0 - u8
// 1 - u16
// 2 - u32
// 3 - u64
static bool Internal_String_ToUnsignedInt(const String Str, u64* OutInt, u8 IntType)
{
    bool bSuccess = false;
    bool bNegative = false;
    
    u8 Index = 0;
    u64 Num = 0;

    u64 MaxIntValue = UINT64_MAX;
    switch (IntType)
    {
        case 0:  MaxIntValue = UINT8_MAX; break;
        case 1:  MaxIntValue = UINT16_MAX; break;
        case 2:  MaxIntValue = UINT32_MAX; break;
        default: break;
    }

    if (Str.Data[0] == '+')
    {
        Index++;
    }
    else if (Str.Data[0] == '-')
    {
        Index++;
        bNegative = true;
    }
    else
    {
        // no action required
    }

    while (Index < Str.Length)
    {
        bool bShouldBreak = false;

        u8 c = Str.Data[Index];
        if (IsDigit(c))
        {
            i32 Digit = c - '0';

            bool bOutOfRange = (MaxIntValue - (u8)Digit) / 10 < Num;
            if (bOutOfRange)
            {
                Num = 0;
                bSuccess = false;
                bShouldBreak = true;
            }
            else
            {
                Num = (Num * 10 + (u8)Digit);
            }
        }
        else
        {
            bSuccess = true;
            bShouldBreak = true;
        }

        if (bShouldBreak)
        {
            break;
        }

        Index++;
        c = Str.Data[Index];
    }

    if (OutInt)
    {
        *OutInt = Num;

        if (bNegative)
        {
            *OutInt = Num > 0 ? MaxIntValue - Num : 0;
        }
    }

    return bSuccess;
}

// IntType
// 0 - i8
// 1 - i16
// 2 - i32
// 3 - i64
static bool Internal_String_ToSignedInt(const String Str, i64* OutInt, u8 IntType)
{
    bool bSuccess = false;
    
    u8 Index = 0;
    i8 Sign = 1;
    i64 Num = 0;

    i64 MaxIntValue = INT64_MAX;
    switch (IntType)
    {
        case 0:  MaxIntValue = INT8_MAX; break;
        case 1:  MaxIntValue = INT16_MAX; break;
        case 2:  MaxIntValue = INT32_MAX; break;
        default: break;
    }

    if (Str.Data[0] == '+')
    {
        Index++;
    }
    else if (Str.Data[0] == '-')
    {
        Index++;
        Sign = -1;
    }
    else
    {
        // no action required
    }

    while (Index < Str.Length)
    {
        bool bShouldBreak = false;

        u8 c = Str.Data[Index];
        if (IsDigit(c))
        {
            i32 Digit = c - '0';

            bool bOutOfRange = (MaxIntValue - Digit) / 10 < Num;
            if (bOutOfRange)
            {
                Num = 0;
                bSuccess = false;
                bShouldBreak = true;
            }
            else
            {
                Num = Num * 10 + Digit;
            }
        }
        else
        {
            bSuccess = true;
            bShouldBreak = true;
        }

        if (bShouldBreak)
        {
            break;
        }

        Index++;
        c = Str.Data[Index];
    }

    if (OutInt)
    {
        *OutInt = Num * Sign;
    }

    return bSuccess;
}

bool String_ToU8(const String Str, u8* OutInt)
{
    // 0-255

    u64 Num = 0;
    bool bSuccess = Internal_String_ToUnsignedInt(Str, &Num, 0);
    if (bSuccess)
    {
        if (OutInt)
        {
            *OutInt = (u8)Num;
        }
    }

    return bSuccess;
}

bool String_ToU16(const String Str, u16* OutInt)
{
    // 0-65535

    u64 Num = 0;
    bool bSuccess = Internal_String_ToUnsignedInt(Str, &Num, 1);
    if (bSuccess)
    {
        if (OutInt)
        {
            *OutInt = (u16)Num;
        }
    }

    return bSuccess;
}

bool String_ToU32(const String Str, u32* OutInt)
{
    // 0-4294967295

    u64 Num = 0;
    bool bSuccess = Internal_String_ToUnsignedInt(Str, &Num, 2);
    if (bSuccess)
    {
        if (OutInt)
        {
            *OutInt = (u32)Num;
        }
    }

    return bSuccess;
}

bool String_ToU64(const String Str, u64* OutInt)
{
    // 0-18446744073709551615

    u64 Num = 0;
    bool bSuccess = Internal_String_ToUnsignedInt(Str, &Num, 3);
    if (bSuccess)
    {
        if (OutInt)
        {
            *OutInt = Num;
        }
    }

    return bSuccess;
}

bool String_ToI8(const String Str, i8* OutInt)
{
    // -127-127

    i64 Num = 0;
    bool bSuccess = Internal_String_ToSignedInt(Str, &Num, 0);
    if (bSuccess)
    {
        if (OutInt)
        {
            *OutInt = (i8)Num;
        }
    }

    return bSuccess;
}

bool String_ToI16(const String Str, i16* OutInt)
{
    // -32767-32767

    i64 Num = 0;
    bool bSuccess = Internal_String_ToSignedInt(Str, &Num, 1);
    if (bSuccess)
    {
        if (OutInt)
        {
            *OutInt = (i16)Num;
        }
    }

    return bSuccess;
}

bool String_ToI32(const String Str, i32* OutInt)
{
    // -2147483648-2147483647

    i64 Num = 0;
    bool bSuccess = Internal_String_ToSignedInt(Str, &Num, 2);
    if (bSuccess)
    {
        if (OutInt)
        {
            *OutInt = (i32)Num;
        }
    }

    return bSuccess;
}

bool String_ToI64(const String Str, i64* OutInt)
{
    // -9223372036854775807-9223372036854775807

    i64 Num = 0;
    bool bSuccess = Internal_String_ToSignedInt(Str, &Num, 3);
    if (bSuccess)
    {
        if (OutInt)
        {
            *OutInt = Num;
        }
    }

    return bSuccess;
}

bool String_ToBool(const String Str)
{
    return String_IsEqual(Str, S("1"), false) || String_IsEqual(Str, S("true"), false);
}

String* StringArray_Iterate_Next(StringArray *InArray)
{
    String* Current = NULL;

    if (InArray->IterIndex < InArray->Num)
    {
        Current = &InArray->List[InArray->IterIndex];
        InArray->IterCurrent = Current;
        InArray->IterIndex++;
    }
    else
    {
        InArray->IterCurrent = NULL;
    }

    return Current;
}

String* StringArray_Iterate_Begin(StringArray* InArray)
{
    InArray->IterIndex = 0;
    InArray->IterCurrent = NULL;
    return StringArray_Iterate_Next(InArray);
}

bool StringList_Iterate_Check(StringList InList)
{
    bool bValid = InList.String.Data != NULL || InList.Next != NULL;
    return bValid;
}

StringList StringList_Iterate_Next(StringList InList)
{
    StringList Next = {0};

    bool bValid = !(InList.Next == NULL || InList.Next == StringList_Null().Next);
    if (bValid)
    {
        Next = *InList.Next;
    }

    return Next;
}

bool StringArray_Contains(const StringArray InArray, const String SubString, bool bCaseSensitive)
{
    bool bContains = false;

    for (u32 i = 0; i < InArray.Num; i++)
    {
        if (String_IsEqual(InArray.List[i], SubString, bCaseSensitive))
        {
            bContains = true;
            break;
        }
    }

    return bContains;
}

// the problem is that we want to separate one long collection of paths into an array of paths,
// but we cant just do a simple split on the space character because some paths have spaces in them,
// so we need to be able to detect when a space is inside of a " " and ignore it. sigh...
StringList String_SplitIntoList(LinearAllocator* Arena, const String Value, u8 Delimiter, bool bHandleQuotes)
{
    StringList List = {0};
    List.Next = NULL;

    if (Value.Length > 0)
    {
        bool bInsideQuote = false;
        bool bSawDelimiter = false;
        u32 Offset = 0;
        u32 CurrentLength = 0;
        for (u32 i = 0; i < Value.Length+1; i++)
        {
            u8 C = i < Value.Length ? Value.Data[i] : 0;

            bool bLastChar = i == Value.Length-1;
            if (C == Delimiter || bLastChar)
            {
                bSawDelimiter = true;

                if (bLastChar)
                {
                    CurrentLength++;
                }
            }
            else
            {
                if (bSawDelimiter)
                {
                    bSawDelimiter = false;

                    if (!bInsideQuote)
                    {
                        String Slice = String_EatSpacesFromEnd(StrSlice(Value.Data+Offset, CurrentLength-1));

                        if (List.String.Data == NULL)
                        {
                            List.String = String_Create(Arena, Slice);
                        }
                        else
                        {
                            StringList* Entry = LinearAllocator_Allocate(Arena, sizeof(StringList));
                            Entry->String = String_Create(Arena, Slice);
                            Entry->Next = NULL;

                            StringList** Next = &List.Next;
                            while (*Next)
                            {
                                Next = &(*Next)->Next;
                            }

                            *Next = Entry;
                        }

                        Offset += CurrentLength;
                        CurrentLength = 0;
                    }
                }
            }

            if (C == '"' && bHandleQuotes)
            {
                bInsideQuote = !bInsideQuote;
            }

            CurrentLength++;
        }
    }

    return List;
}

StringArray String_SplitIntoArray(LinearAllocator* Arena, const String Str, const String Delimiter, u32 StartingIndex, u32 MaxCount)
{
    return (StringArray){0};
}

StringArray String_ParseIntoArray(LinearAllocator* Arena, const String Str, u8 Delimiter, u32 StartingIndex, u32 MaxCount)
{
    StringArray StrArray = StringArray_Null();

    u32 Num = 0;

    for (u32 i = StartingIndex; i < Str.Length; i++)
    {
        if (Str.Data[i] == Delimiter || i == Str.Length-1)
        {
            Num++;
        }
    }

    if (Num > 0)
    {
        Num = Min(Num, MaxCount);
        StrArray.List = LinearAllocator_Allocate(Arena, sizeof(String) * Num);
        StrArray.Num = Num;

        u32 Offset = StartingIndex;
        u32 ListIndex = 0;
        for (u32 i = StartingIndex; i < Str.Length; i++)
        {
            if (ListIndex >= Num)
            {
                break;
            }

            if (Str.Data[i] == Delimiter && i != Str.Length-1)
            {
                u32 Length = i-Offset;
                StrArray.List[ListIndex].Data = LinearAllocator_Allocate(Arena, Length+1);
                MemCopy(StrArray.List[ListIndex].Data, &Str.Data[Offset], Length);
                StrArray.List[ListIndex].Data[Length] = 0;
                StrArray.List[ListIndex].Length = Length;
                StrArray.List[ListIndex].Capacity = Length;

                Offset = i+1;
                ListIndex++;
            }
        }

        u32 Length = Str.Length-Offset;
        StrArray.List[ListIndex].Data = LinearAllocator_Allocate(Arena, Length+1);
        MemCopy(StrArray.List[ListIndex].Data, &Str.Data[Offset], Length);
        StrArray.List[ListIndex].Data[Length] = 0;
        StrArray.List[ListIndex].Length = Length;
        StrArray.List[ListIndex].Capacity = Length;
    }

    return StrArray;
}

bool StringArray_Find(StringArray Array, const String Source, u32* FoundIndex)
{
    u32 Index = 0;
    bool bFound = false;

    for each_str_i (Index, s, Array)
    {
        if (String_IsEqual(*s, Source, true))
        {
            if (FoundIndex)
            {
                *FoundIndex = Index;
            }

            bFound = true;
            break;
        }
    }

    return bFound;
}

String StringArray_GetStringFromIndex(StringArray Array, u32 Index)
{
    String Result = String_Null();

    u32 i = 0;
    for each_str_i (i, s, Array)
    {
        if (i == Index)
        {
            Result = *s;
            break;
        }
    }

    return Result;
}

bool StringList_Find(StringList List, const String Source, u32* FoundIndex)
{
    u32 Index = 0;
    bool bFound = false;

    for each_str_list_i (Index, List)
    {
        if (String_IsEqual(It.String, Source, true))
        {
            if (FoundIndex)
            {
                *FoundIndex = Index;
            }

            bFound = true;
            break;
        }
    }

    return bFound;
}

String StringList_GetStringFromIndex(StringList List, u32 Index)
{
    String Result = String_Null();

    u32 i = 0;
    for each_str_list_i (i, List)
    {
        if (i == Index)
        {
            Result = It.String;
            break;
        }
    }

    return Result;
}

u32 String_GetLength(const char *Str)
{
    register u32 Len = 0;
    while (Str[Len])
    {
        Len++;
    }

    return Len;
}

u32 String_GetLength_Ex(const char* Str, u32 MaxLength)
{
    register u32 Len = 0;
    while (Len < MaxLength && Str[Len])
    {
        Len++;
    }

    return Len;
}

u32 String16_GetLength(const wchar* Str)
{
    register u32 Len = 0;
    while (Str[Len])
    {
        Len++;
    }

    return Len;
}

u32 String16_GetLength_Ex(const wchar* Str, u32 MaxLength)
{
    register u32 Len = 0;
    while (Len < MaxLength && Str[Len])
    {
        Len++;
    }

    return Len;
}

bool IsAlphabet(u8 Char)
{
    return ((Char >= 'A' && Char <= 'Z') || (Char >= 'a' && Char <= 'z'));
}

bool IsAlphabetUpper(u8 Char)
{
    return Char >= 'A' && Char <= 'Z';
}

bool IsAlphabetLower(u8 Char)
{
    return Char >= 'a' && Char <= 'z';
}

bool IsDigit(u8 Char)
{
    return Char >= '0' && Char <= '9';
}

bool IsWhitespace(u8 Char)
{
    return Char == ' ' || Char == '\r' || Char == '\t' || Char == '\f' || Char == '\v' || Char == '\n';
}

bool IsNewline(u8 Char)
{
    return Char == '\r' || Char == '\f' || Char == '\n';
}

bool IsSymbol(u8 Char)
{
    return Char == '(' || Char == ')' || Char == '{' || Char == '}' ||
           Char == '!' || Char == '@' || Char == '#' || Char == '$' ||
           Char == '%' || Char == '^' || Char == '&' || Char == '*' ||
           Char == '+' || Char == '=';
}

u8 ToUpper(u8 Char)
{
    return Char >= 'a' && Char <= 'z' ? (u8)(Char - 32) : Char;
}

u8 ToLower(u8 Char)
{
    return Char >= 'A' && Char <= 'Z' ? (u8)(Char + 32) : Char;
}

u8 ToForwardSlash(u8 Char)
{
    return Char == '\\' ? '/' : Char;
}

u8 ToBackSlash(u8 Char)
{
    return Char == '/' ? '\\' : Char;
}

/*
void EatSpaces(u8** Str)
{
    if (!(Str == NULL || *Str == NULL || *(*Str) == 0))
    {
        while (*(*Str) > 0 && IsWhitespace(*(*Str)))
        {
            (*Str)++;
        }
    }
}

void EatSpaces_Backwards(u8** Str)
{
    if (!(Str == NULL || *Str == NULL || *(*Str) == 0))
    {
        while (*(*Str) > 0 && IsWhitespace(*(*Str)))
        {
            (*Str)--;
        }
    }
}

void EatBraces(u8** Str)
{
    if (!(Str == NULL || *Str == NULL || *(*Str) == 0))
    {
        u8* S = *Str;
        u8 c = *S;
        while (c > 0 && (c == '{' || c == '}'))
        {
            S++;
            c = *S;
        }
    }
}

void EatBraces_Backwards(u8** Str)
{
    if (!(Str == NULL || *Str == NULL || *(*Str) == 0))
    {
        u8* S = *Str;
        u8 c = *S;
        while (c > 0 && (c == '{' || c == '}'))
        {
            S--;
            c = *S;
        }
    }
}

void EatParenthesis(u8** Str)
{
    if (!(Str == NULL || *Str == NULL || *(*Str) == 0))
    {
        u8* S = *Str;
        u8 c = *S;
        while (c > 0 && (c == '(' || c == ')'))
        {
            S++;
            c = *S;
        }
    }
}

void EatParenthesis_Backwards(u8** Str)
{
    if (!(Str == NULL || *Str == NULL || *(*Str) == 0))
    {
        u8* S = *Str;
        u8 c = *S;
        while (c > 0 && (c == '(' || c == ')'))
        {
            S--;
            c = *S;
        }
    }
}

void EatSymbols(u8** Str)
{
    if (!(Str == NULL || *Str == NULL || *(*Str) == 0))
    {
        u8* S = *Str;
        u8 c = *S;
        while (c > 0 &&
            (c == '(' || c == ')' || c == '{' || c == '}' ||
            c == '!' || c == '@' || c == '#' || c == '$' ||
            c == '%' || c == '^' || c == '&' || c == '*' ||
            c == '+' || c == '='))
        {
            S++;
            c = *S;
        }
    }
}

void EatSymbols_Backwards(u8** Str)
{
    if (!(Str == NULL || *Str == NULL || *(*Str) == 0))
    {
        u8* S = *Str;
        u8 c = *S;
        while (c > 0 &&
            (c == '(' || c == ')' || c == '{' || c == '}' ||
            c == '!' || c == '@' || c == '#' || c == '$' ||
            c == '%' || c == '^' || c == '&' || c == '*' ||
            c == '+' || c == '='))
        {
            S--;
            c = *S;
        }
    }
}
*/
