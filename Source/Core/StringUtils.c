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
    if (!Str.Data || Str.Data[0] == 0 || Str.Length == 0)
    {
        return false;
    }

    if (Str.Data == String_Null().Data)
    {
        return false;
    }

    return true;
}

bool StringArray_IsValid(const StringArray Str)
{
    if (!Str.List || Str.Num == 0 || Str.List == StringArray_Null().List)
    {
        return false;
    }

    return true;
}

bool StringList_IsValid(const StringList Str)
{
    if (!Str.Next || Str.Next == StringList_Null().Next || !String_IsValid(Str.String))
    {
        return false;
    }

    return true;
}

String String_Create(LinearAllocator* Arena, const String Source)
{
    if (Source.Length == 0 || Arena->Allocated + Source.Length+1 > Arena->TotalSize)
    {
        return String_Null();
    }

    String str;
    str.Data = LinearAllocator_Allocate(Arena, Source.Length+1);
    MemCopy(str.Data, Source.Data, Source.Length);
    str.Data[Source.Length] = 0;
    str.Length = Source.Length;
    str.Capacity = Source.Length;
    return str;
}

String String_Duplicate(LinearAllocator* Arena, const String Source)
{
    if (Source.Length == 0 || Arena->Allocated + Source.Length+1 > Arena->TotalSize)
    {
        return String_Null();
    }

    String str;
    str.Data = LinearAllocator_Allocate(Arena, Source.Length+1);
    MemCopy(str.Data, Source.Data, Source.Length);
    str.Data[Source.Length] = 0;
    str.Length = Source.Length;
    str.Capacity = Source.Length;
    return str;
}

String String_Reserve(LinearAllocator* Arena, u32 Capacity)
{
    if (Arena->Allocated + Capacity > Arena->TotalSize)
    {
        return String_Null();
    }

    String str;
    str.Data = LinearAllocator_Allocate(Arena, Capacity+1);
    str.Length = 0;
    str.Capacity = Capacity;
    return str;
}

String String_ReserveAndCopy(LinearAllocator* Arena, u32 Capacity, const String Source)
{
    if (Arena->Allocated + Capacity > Arena->TotalSize)
    {
        return String_Null();
    }

    String str;
    str.Data = LinearAllocator_Allocate(Arena, Capacity+1);
    if (Source.Length) MemCopy(str.Data, Source.Data, Source.Length);
    str.Data[Source.Length] = 0;
    str.Length = Source.Length;
    str.Capacity = Capacity;
    return str;
}

StringArray String_CreateArray(LinearAllocator* Arena, const StringArray Array)
{
    if (Arena->Allocated + sizeof(String) * Array.Num > Arena->TotalSize)
    {
        return StringArray_Null();
    }

    StringArray Result = {0};
    Result.Num = Array.Num;
    Result.List = LinearAllocator_Allocate(Arena, sizeof(String) * Array.Num);
    
    for (u32 i = 0; i < Array.Num; i++)
    {
        Result.List[i] = String_Create(Arena, Array.List[i]);
    }
    
    return Result;
}

String String_Join(LinearAllocator* Arena, const StringArray Array)
{
    String JoinedStr;
    
    u32 TotalSize = 0;
    for (u32 i = 0; i < Array.Num; i++)
    {
        TotalSize += Array.List[i].Length;
    }

    if (Arena->Allocated + TotalSize+1 > Arena->TotalSize)
    {
        return String_Null();
    }
    
    JoinedStr.Length = TotalSize;
    JoinedStr.Data = LinearAllocator_Allocate(Arena, TotalSize+1);
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
            return;

        if (LIKELY(Len > 0))
        {
            MemCopy(&Dest->Data[NumCopied], Str->Data, Len);
            NumCopied += Len;
        }
    }
}

bool String_IsEqual(const String StringA, const String StringB, bool bCaseSensitive)
{
    if (UNLIKELY(StringA.Length == 0 && StringB.Length == 0))
    {
        return true;
    }

    u32 Length = StringA.Length;
    if (Length != StringB.Length)
        return false;
    
    if (bCaseSensitive)
    {
        for (u32 i = 0; i < Length; i++)
        {
            if (StringA.Data[i] != StringB.Data[i])
            {
                return false;
            }
        }
        
        return true;
    }

    for (u32 i = 0; i < Length; i++)
    {
        i32 A = (i32)StringA.Data[i];
        i32 B = (i32)StringB.Data[i];
        
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

bool String_IsInteger32(const String Str)
{
    if (Str.Length > 10) return false; // not a valid 32-bit integer
    if (Str.Length == 0) return false;

    for (u32 i = 0; i < Str.Length; i++)
    {
        if (!IsDigit(Str.Data[i]))
        {
            return false;
        }
    }

    return true;
}

bool String_IsInteger(const String Str)
{
    if (Str.Length == 0) return false;

    for (u32 i = 0; i < Str.Length; i++)
    {
        if (!IsDigit(Str.Data[i]))
        {
            return false;
        }
    }

    return true;
}

bool String_IsFloat(const String Str)
{
    if (Str.Length == 0) return false;

    bool bHasDot = false;
    for (u32 i = 0; i < Str.Length; i++)
    {
        if (Str.Data[i] == '.')
        {
            if (bHasDot) // already have a dot, means it's not a valid float
                return false;

            bHasDot = true;
        }
        else if (!IsDigit(Str.Data[i]))
        {
            return false;
        }
    }

    return true;
}

bool String_IsNumeric(const String Str)
{
    return String_IsInteger(Str) || String_IsFloat(Str);
}

bool String_Contains(const String Str, const String SubString, bool bCaseSensitive)
{
    for (u32 i = 0; i < Str.Length; i++)
    {
        if (Str.Length - i < SubString.Length)
            return false;

        const String S = StrShiftF(Str, i);
        if (String_IsEqual(StrSlice(S.Data, SubString.Length), SubString, bCaseSensitive))
        {
            return true;
        }
    }

    return false;
}

bool String_ContainsPathSeparators(const String Str)
{
    for (u32 i = 0; i < Str.Length; i++)
    {
        if (Str.Data[i] == '/' || Str.Data[i] == '\\')
        {
            return true;
        }
    }

    return false;
}

bool String_ContainsDigits(const String Str)
{
    for (u32 i = 0; i < Str.Length; i++)
    {
        if (IsDigit(Str.Data[i]))
        {
            return true;
        }
    }

    return false;
}

bool String_ContainsNonDigits(const String Str)
{
    for (u32 i = 0; i < Str.Length; i++)
    {
        if (!IsDigit(Str.Data[i]))
        {
            return true;
        }
    }

    return false;
}

bool String_StartsWith(const String Str, const String SubString, bool bCaseSensitive)
{
    if (Str.Length < SubString.Length || SubString.Length == 0)
        return false;

    return String_IsEqual(StrSlice(Str.Data, SubString.Length), SubString, bCaseSensitive);
}

bool String_EndsWith(const String Str, const String SubString, bool bCaseSensitive)
{
    if (Str.Length < SubString.Length || SubString.Length == 0)
        return false;

    return String_IsEqual(StrShiftF(Str, Str.Length - SubString.Length), SubString, bCaseSensitive);
}

bool String16_IsEqual(const String16 StringA, const String16 StringB, bool bCaseSensitive)
{
    if (UNLIKELY(StringA.Length == 0 && StringB.Length == 0))
    {
        return true;
    }

    u64 Length = StringA.Length;
    if (Length != StringB.Length)
        return false;
    
    if (bCaseSensitive)
    {
        for (u64 i = 0; i < Length; i++)
        {
            if (StringA.Data[i] != StringB.Data[i])
            {
                return false;
            }
        }
        
        return true;
    }

    for (u64 i = 0; i < Length; i++)
    {
        i32 A = (i32)StringA.Data[i];
        i32 B = (i32)StringB.Data[i];
        
        if ((A >= L'A' && A <= L'Z'))
        {
            A += 32;
        }

        if ((B >= L'A' && B <= L'Z'))
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

void String_Copy(String* Dest, const String Source)
{
    if (UNLIKELY(Source.Length == 0))
        return;

    u32 NumToCopy = Dest->Capacity == 0 ? Source.Length : Min(Dest->Capacity, Source.Length);
    MemCopy(Dest->Data, Source.Data, NumToCopy);
    Dest->Length = NumToCopy;
    Dest->Data[NumToCopy] = 0;
}

void String_CopyN(String* Dest, const String Source, u32 Length)
{
    if (UNLIKELY(Source.Length == 0))
        return;

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
    if (Dest->Length + Source.Length > Dest->Capacity || Source.Length == 0)
        return;

    u32 NumToCopy = Min(Dest->Capacity, Source.Length);
    MemCopy(&Dest->Data[Dest->Length], Source.Data, NumToCopy);
    Dest->Length += NumToCopy;
    Dest->Data[Dest->Length] = 0;
}

void String_AppendChar(String* Dest, const char Source)
{
    if (Dest->Length + 1 > Dest->Capacity || Dest->Capacity == 0)
        return;

    Dest->Data[Dest->Length] = Source;
    Dest->Length += 1;
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
#if PLATFORM_WINDOWS
    String_AppendChar(Dest, '\\');
#else
    String_AppendChar(Dest, '/');
#endif
}

void String_AppendPathSeparator_Checked(String* Dest)
{
    char LastChar = Dest->Data[Dest->Length-1];
    bool bHasPathSep = LastChar == '/' || LastChar == '\\';
    if (bHasPathSep)
        return;

#if PLATFORM_WINDOWS
    String_AppendChar(Dest, '\\');
#else
    String_AppendChar(Dest, '/');
#endif
}

ECompareResult String_CompareVersion(const String VersionA, const String VersionB)
{
    if (VersionA.Length == 0 || VersionB.Length == 0)
    {
        return CompareResult_None;
    }

    // compare each version separated by '.' or '-'

    ECompareResult Result = CompareResult_None;

    u64 VersionArrayA[32];
    u64 VersionArrayB[32];
    for (u8 i = 0; i < 32; i++) VersionArrayA[i] = UINT64_MAX;
    for (u8 i = 0; i < 32; i++) VersionArrayB[i] = UINT64_MAX;

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
        String_ToU64(SubVersionA, &A);
        String_ToU64(SubVersionB, &B);

        VersionArrayA[VersionIndexA] = A;
        VersionArrayB[VersionIndexB] = B;

        VersionIndexA++;
        VersionIndexB++;

        OffsetA++;
        OffsetB++;

        if (VersionIndexA >= 32 || VersionIndexB >= 32)
            break;

        if (OffsetA > VersionA.Length-1 || OffsetB > VersionB.Length-1)
            break;
    }

    for (u8 i = 0; i < 32; i++)
    {
        if (VersionArrayA[i] == UINT64_MAX || VersionArrayB[i] == UINT64_MAX)
            break;

        if (VersionArrayA[i] == VersionArrayB[i])
        {
            Result = CompareResult_Equal;
            continue;
        }

        if (VersionArrayA[i] > VersionArrayB[i])
        {
            Result = CompareResult_Greater;
            break;
        }

        //if (VersionArrayA[i] < VersionArrayB[i])
        //{
            Result = CompareResult_Less;
            break;
        //}
    }

    return Result;
}

i32 String_Format(String* Dest, const String Format, u32 Capacity, ...)
{
    va_list Args;
    va_start(Args, Capacity);
    i32 Written = stbsp_vsnprintf(Dest->Data, (i32)Capacity, Format.Data, Args);
    Dest->Length = (u32)Written;
    va_end(Args);

    return Written;
}

u32 String_FormatV(String* Dest, const String Format, u32 Capacity, void* VAList)
{
    Dest->Length = (u32)stbsp_vsnprintf(Dest->Data, (i32)Capacity, Format.Data, VAList);
    return Dest->Length;
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

        if (Param.Length == 1 && Param.Data[0] == '.') // '.' paths are ignored since they're kinda redundant to be in the path anyway
        {
            continue;
        }

        if (Dest->Length > 0)
        {
            char LastChar = Dest->Data[Dest->Length-1];
            bool bHasPathSeparator = LastChar == '/' || LastChar == '\\';
            if (!bHasPathSeparator)
                String_AppendPathSeparator(Dest);
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
        String_EatPathSeparatorsInlineFromEnd(Dest);

        if (Dest->Length > 0 && i != Array.Num-1)
            String_AppendPathSeparator(Dest);
    }

    String_EatPathSeparatorsInlineFromEnd(Dest);
    String_ConvertSlashToPlatformSlash(Dest);
}

void StringInternal_BuildSeparator(String* Dest, char Separator, const StringArray Array)
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
            char LastChar = Dest->Data[Dest->Length-1];
            bool bHasSeparator = LastChar == Separator;
            if (!bHasSeparator)
                String_AppendChar(Dest, Separator);
        }

        String_Append(Dest, String_EatChar(Param, Separator));
        String_EatCharInlineFromEnd(Dest, Separator);

        if (Dest->Length > 0 && i != Array.Num-1)
            String_AppendChar(Dest, Separator);
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

void String_Fill(String* Str, char C)
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
        ToString->Data[i] = (char)FromString.Data[i];
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
    //return StrCompC(Str.Data + i, Str.Length - i, Str.Capacity);
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
    //return StrCompC(Str.Data + i, Str.Length - i, Str.Capacity);
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

bool String_ReplaceCharInline(String* Str, char Char, char ReplaceChar)
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

bool String_ReplaceNonAlphaNumericCharInline(String* Str, char ReplaceChar)
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
    const String* LongestString = A.Length > B.Length ? &A : &B;
    const String* ShortestString = A.Length > B.Length ? &B : &A;

    bool bAnyChange = false;
    for (u32 i = 0; i < LongestString->Length; i++)
    {
        if (i == ShortestString->Length)
        {
            // append the remaining
            String_Append(Dest, StrShiftF(*LongestString, i));
            bAnyChange = true;
            break;
        }

        i32 C1 = (i32)A.Data[i];
        i32 C2 = (i32)B.Data[i];
    
        if (!bCaseSensitive)
        {
            if ((C1 >= 'A' && C1 <= 'Z')) C1 += 32;
            if ((C2 >= 'A' && C2 <= 'Z')) C2 += 32;
        }

        if (C1 != C2)
        {
            // append the remaining
            String_Append(Dest, StrShiftF(*LongestString, i));
            bAnyChange = true;
            break;
        }
    }

    return bAnyChange;
}

String String_EatChar(String Str, char Char)
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
    //return StrCompC(Str.Data + i, Str.Length - i, Str.Capacity);
}

String String_EatPathSeparatorsFromEnd(String Str)
{
    if (Str.Length == 0 || (Str.Data[Str.Length-1] != '/' && Str.Data[Str.Length-1] != '\\'))
        return Str;

    u32 i = Str.Length-1;
    for (; i > 0; i--)
    {
        if (Str.Data[i] != '/' && Str.Data[i] != '\\')
        {
            i++;
            break;
        }
    }

    if (i == 0 && (Str.Data[0] != '/' && Str.Data[0] != '\\'))
    {
        i++;
    }

    return StrSlice(Str.Data, i);
    //return StrCompC(Str.Data, i, Str.Capacity);
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
    //return StrCompC(Str.Data + i, Str.Length - i, Str.Capacity);
}

bool String_EatCharInline(String* Str, char Char)
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

bool String_EatCharInline_Single(String* Str, char Char)
{
    if (Str->Data[0] == Char)
    {
        Str->Data++;
        Str->Length--;
        return true;
    }

    return false;
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

String String_EatCharFromEnd(String Str, char Char)
{
    if (Str.Length == 0 || Str.Data[Str.Length-1] != Char)
        return Str;

    u32 i = Str.Length-1;
    for (; i > 0; i--)
    {
        if (Str.Data[i] != Char)
        {
            i++;
            break;
        }
    }

    if (i == 0 && Str.Data[0] != Char)
    {
        i++;
    }

    return StrSlice(Str.Data, i);
    //return StrCompC(Str.Data, i, Str.Capacity);
}

bool String_EatCharInlineFromEnd(String* Str, char Char)
{
    if (Str->Length == 0 || Str->Data[Str->Length-1] != Char)
        return false;

    u32 i = Str->Length-1;
    for (; i > 0; i--)
    {
        if (Str->Data[i] != Char)
        {
            i++;
            break;
        }
    }

    if (i == 0 && Str->Data[0] != Char)
    {
        i++;
    }

    bool bAnyChange = i < Str->Length;
    Str->Length = i;

    return bAnyChange;
}

String String_EatSpacesFromEnd(String Str)
{
    if (Str.Length == 0 || !IsWhitespace(Str.Data[Str.Length-1]))
        return Str;

    u32 i = Str.Length-1;
    for (; i > 0; i--)
    {
        if (!IsWhitespace(Str.Data[i]))
        {
            i++;
            break;
        }
    }

    if (i == 0 && !IsWhitespace(Str.Data[0]))
    {
        i++;
    }

    return StrSlice(Str.Data, i);
    //return StrCompC(Str.Data, i, Str.Capacity);
}

String String_EatNewLinesFromEnd(String Str)
{
    if (Str.Length == 0 || !IsNewline(Str.Data[Str.Length-1]))
        return Str;

    u32 i = Str.Length-1;
    for (; i > 0; i--)
    {
        if (!IsNewline(Str.Data[i]))
        {
            i++;
            break;
        }
    }

    if (i == 0 && !IsNewline(Str.Data[0]))
    {
        i++;
    }

    return StrSlice(Str.Data, i);
    //return StrCompC(Str.Data, i, Str.Capacity);
}

bool String_EatSpacesInlineFromEnd(String* Str)
{
    if (Str->Length == 0 || !IsWhitespace(Str->Data[Str->Length-1]))
        return false;

    u32 i = Str->Length-1;
    for (; i > 0; i--)
    {
        if (!IsWhitespace(Str->Data[i]))
        {
            i++;
            break;
        }
    }

    if (i == 0 && !IsWhitespace(Str->Data[0]))
    {
        i++;
    }

    bool bAnyChange = i < Str->Length;
    Str->Length = i;
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
    if (Str->Length == 0 || !IsNewline(Str->Data[Str->Length-1]))
        return false;

    u32 i = Str->Length-1;
    for (; i > 0; i--)
    {
        if (!IsNewline(Str->Data[i]))
        {
            i++;
            break;
        }
    }

    if (i == 0 && !IsNewline(Str->Data[0]))
    {
        i++;
    }

    bool bAnyChange = i < Str->Length;
    Str->Length = i;
    return bAnyChange;
}

bool String_EatPathSeparatorsInlineFromEnd(String* Str)
{
    if (Str->Length == 0 || (Str->Data[Str->Length-1] != '/' && Str->Data[Str->Length-1] != '\\'))
        return false;

    u32 i = Str->Length-1;
    for (; i > 0; i--)
    {
        if (Str->Data[i] != '/' && Str->Data[i] != '\\')
        {
            i++;
            break;
        }
    }

    if (i == 0 && (Str->Data[0] != '/' && Str->Data[0] != '\\'))
    {
        i++;
    }

    bool bAnyChange = i < Str->Length;
    Str->Length = i;

    return bAnyChange;
}

String String_ScanUntil(const String* Str, char Char)
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

    return (String){ .Data = Str->Data, .Length = NewLength, .Capacity = Str->Capacity };
}

void CString_ToLower(char* Str)
{
    char* p = Str;
    for (; *p; ++p) *p = ToLower(*p);
}

void CString_ToUpper(char* Str)
{
    char* p = Str;
    for (; *p; ++p) *p = ToUpper(*p);
}

void CString_ToWide(const char* FromString, wchar* ToString)
{
    u64 Len = String_GetLength(FromString);
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
    u32 Len = String_GetLength(Source)+1;
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


void CString_ToBytes(const char* Data, u32 Length, u8* OutBytes)
{
    MemCopy(OutBytes, Data, Length);
}

void CString_FromBytes(const u8* Data, u32 Length, char* OutCharacters)
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

bool String_IndexOfChar(const String Str, char C, u32* OutIndex)
{
    for (u32 i = 0; i < Str.Length; ++i)
    {
        if (Str.Data[i] == C)
        {
            if (OutIndex)
                *OutIndex = i;

            return true;
        }
    }
    
    return false;
}

bool String_IsFirst(const String Str, char C)
{
    if (Str.Length == 0)
        return false;

    return Str.Data[0] == C;
}

bool String_IsLast(const String Str, char C)
{
    if (Str.Length == 0)
        return false;

    return Str.Data[Str.Length-1] == C;
}

bool String_IndexOfLastChar(const String Str, char C, u32* OutIndex)
{
    if (Str.Length == 0)
    {
        return false;
    }

    for (u32 i = Str.Length-1; i > 0; i--)
    {
        if (Str.Data[i] == C)
        {
            if (OutIndex)
                *OutIndex = i;

            return true;
        }
    }

    if (Str.Data[0] == C)
    {
        if (OutIndex)
            *OutIndex = 0;

        return true;
    }
    
    return false;
}

bool String_IndexOfFirstPathSlash(const String Str, u32* OutIndex)
{
    for (u32 i = 0; i < Str.Length; ++i)
    {
        if (Str.Data[i] == '/' || Str.Data[i] == '\\')
        {
            if (OutIndex)
                *OutIndex = i;

            return true;
        }
    }
    
    return false;
}

bool String_IndexOfLastPathSlash(const String Str, u32* OutIndex)
{
    if (Str.Length == 0)
    {
        return false;
    }

    for (u32 i = Str.Length-1; i > 0; i--)
    {
        if (Str.Data[i] == '/' || Str.Data[i] == '\\')
        {
            if (OutIndex)
                *OutIndex = i;

            return true;
        }
    }

    return false;
}

bool String_IndexOfFirstWhitespace(const String Str, u32* OutIndex)
{
    for (u32 i = 0; i < Str.Length; ++i)
    {
        if (IsWhitespace(Str.Data[i]))
        {
            if (OutIndex)
                *OutIndex = i;

            return true;
        }
    }
    
    return false;
}

bool String_IndexOfLastWhitespace(const String Str, u32* OutIndex)
{
    if (Str.Length == 0)
    {
        return false;
    }

    for (u32 i = Str.Length-1; i > 0; i--)
    {
        if (IsWhitespace(Str.Data[i]))
        {
            if (OutIndex)
                *OutIndex = i;

            return true;
        }
    }

    if (IsWhitespace(Str.Data[0]))
    {
        if (OutIndex)
            *OutIndex = 0;

        return true;
    }
    
    return false;
}

bool String_IndexOfSubstring(const String Str, const String Substring, bool bCaseSensitive, u32* OutIndex)
{
    if (Str.Length == 0 || Substring.Length == 0)
    {
        return false;
    }

    for (u32 i = 0; i < Str.Length; ++i)
    {
        if (String_IsEqual(StrSlice(Str.Data + i, Substring.Length), Substring, bCaseSensitive))
        {
            if (OutIndex)
                *OutIndex = i;

            return true;
        }
    }
    
    return false;
}

// transforms paths with " in them to paths without them
// for exmaple: "C:\Program Files"\MyApp -> "C:\Program Files\MyApp"

bool String_SanitizeQuotes(String* Dest, const String Source)
{
    bool bHasQuote = false;
    for (u32 i = 0; i < Source.Length; i++)
    {
        const char c = Source.Data[i];
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
            continue;
        
        bAnyChange = true;

        #if PLATFORM_WINDOWS
        char C = Source.Data[i] == '/' ? '\\' : Source.Data[i]; 
        #else
        char C = Source.Data[i] == '\\' ? '/' : Source.Data[i]; 
        #endif

        if (C == '/' || C == '\\')
        {
            if (Dest->Length > 0)
            {
                char LastChar = Dest->Data[Dest->Length-1];
                bool bHasPathSep = LastChar == '/' || LastChar == '\\';
                if (bHasPathSep)
                    continue;
            }
        }

        String_AppendChar(Dest, C);
    }

    return bAnyChange;
}

bool String_SanitizePathAndWrap(String* Dest, const String Source)
{
    if (Source.Length == 0)
        return false;

    String_AppendChar(Dest, '"');

    bool bAnyChange = false;
    for (u32 i = 0; i < Source.Length; i++)
    {
        if (Source.Data[i] == '"')
            continue;

        bAnyChange = true;

        #if PLATFORM_WINDOWS
        char C = Source.Data[i] == '/' ? '\\' : Source.Data[i]; 
        #else
        char C = Source.Data[i] == '\\' ? '/' : Source.Data[i]; 
        #endif

        if (C == '/' || C == '\\')
        {
            if (Dest->Length > 0)
            {
                char LastChar = Dest->Data[Dest->Length-1];
                bool bHasPathSep = LastChar == '/' || LastChar == '\\';
                if (bHasPathSep)
                    continue;
            }
        }

        String_AppendChar(Dest, C);
    }

    String_AppendChar(Dest, '"');

    return bAnyChange;
}

u32 String_CountChar(const String Str, char C)
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

bool String_StripString(const String Str, const String Substring, String* OutStr)
{
    if (!String_IsValid(Str)) return false;
    if (NEVER(OutStr == NULL)) return false;

    const u32 MaxLength = Min(OutStr->Capacity, Str.Length);

    for (u32 i = 0; i < MaxLength;)
    {
        if (String_IsEqual(StrSlice(Str.Data + i, Substring.Length), Substring, false))
        {
            i += Substring.Length;
        }
        {
            OutStr->Data[OutStr->Length] = Str.Data[i];
            OutStr->Length++;

            i++;
        }
    }

    return true;
}

bool String_StripChar(const String Str, char C, String* OutStr)
{
    if (!String_IsValid(Str)) return false;
    if (NEVER(OutStr == NULL)) return false;

    const u32 MaxLength = Min(OutStr->Capacity, Str.Length);

    for (u32 i = 0; i < MaxLength; i++)
    {
        if (Str.Data[i] == C)
        {
            OutStr->Data[OutStr->Length] = Str.Data[i];
            OutStr->Length++;
        }
    }

    return true;
}

bool String_StripWhitespace(const String Str, String* OutStr)
{
    if (!String_IsValid(Str)) return false;
    if (NEVER(OutStr == NULL)) return false;

    const u32 MaxLength = Min(OutStr->Capacity, Str.Length);

    for (u32 i = 0; i < MaxLength; i++)
    {
        if (!IsWhitespace(Str.Data[i]))
        {
            OutStr->Data[OutStr->Length] = Str.Data[i];
            OutStr->Length++;
        }
    }

    return true;
}

bool String_StripNewline(const String Str, String* OutStr)
{
    if (!String_IsValid(Str)) return false;
    if (NEVER(OutStr == NULL)) return false;

    const u32 MaxLength = Min(OutStr->Capacity, Str.Length);

    for (u32 i = 0; i < MaxLength; i++)
    {
        if (!IsNewline(Str.Data[i]))
        {
            OutStr->Data[OutStr->Length] = Str.Data[i];
            OutStr->Length++;
        }
    }

    return true;
}

bool String_StripDigit(const String Str, String* OutStr)
{
    if (!String_IsValid(Str)) return false;
    if (NEVER(OutStr == NULL)) return false;

    const u32 MaxLength = Min(OutStr->Capacity, Str.Length);

    for (u32 i = 0; i < MaxLength; i++)
    {
        if (!IsDigit(Str.Data[i]))
        {
            OutStr->Data[OutStr->Length] = Str.Data[i];
            OutStr->Length++;
        }
    }

    return true;
}

bool String_StripSymbol(const String Str, String* OutStr)
{
    if (!String_IsValid(Str)) return false;
    if (NEVER(OutStr == NULL)) return false;

    const u32 MaxLength = Min(OutStr->Capacity, Str.Length);

    for (u32 i = 0; i < MaxLength; i++)
    {
        if (!IsSymbol(Str.Data[i]))
        {
            OutStr->Data[OutStr->Length] = Str.Data[i];
            OutStr->Length++;
        }
    }

    return true;
}

bool String_StripAlphabet(const String Str, String* OutStr)
{
    if (!String_IsValid(Str)) return false;
    if (NEVER(OutStr == NULL)) return false;

    const u32 MaxLength = Min(OutStr->Capacity, Str.Length);

    for (u32 i = 0; i < MaxLength; i++)
    {
        if (!IsAlphabet(Str.Data[i]))
        {
            OutStr->Data[OutStr->Length] = Str.Data[i];
            OutStr->Length++;
        }
    }

    return true;
}

bool String_ToF32(const String Str, f32* OutFloat)
{
    if (!String_IsValid(Str))
    {
        return false;
    }

    u64 Index = 0;

    i8 Sign = 1;
    if (Str.Data[0] == '-')
    {
        Sign = -1;
        Index++;
    }
    else if (Str.Data[0] == '+')
    {
        Index++;
    }

    if (Index >= Str.Length)
        return false;

    f32 Num = 0;
    bool bDecimalFound = false;
    u8 DecimalPlaces = 0;
    char c = Str.Data[Index];
    if (!IsDigit(c))
    {
        *OutFloat = 0;
        return false;
    }

    while (Index < Str.Length)
    {
        if (IsDigit(c))
        {
            i32 Digit = c - '0';

            if ((FLT_MAX - (f32)Digit) / 10 < Num)
            {
                *OutFloat = 0;
                return false;
            }

            if (bDecimalFound)
            {
                DecimalPlaces++;
                f32 PowResult = 10.0f;
                for (u8 i = 1; i < (u8)DecimalPlaces; i++)
                {
                    PowResult *= 10.0f;
                }

                Num = Num + (f32)Digit / PowResult; //Pow(10.0f, (f32)DecimalPlaces);
            }
            else
            {
                Num = Num * 10.0f + (f32)Digit;
            }
        }
        else if (c == '.')
        {
            bDecimalFound = true;
        }
        else
        {
            *OutFloat = Num * (f32)Sign;
            return true;
        }

        Index++;
        c = Str.Data[Index];
    }

    *OutFloat = Num * Sign;
    return true;
}

bool String_ToF64(const String Str, f64* OutFloat)
{
    if (!String_IsValid(Str))
    {
        return false;
    }

    u64 Index = 0;

    i8 Sign = 1;
    if (Str.Data[0] == '-')
    {
        Sign = -1;
        Index++;
    }
    else if (Str.Data[0] == '+')
    {
        Index++;
    }

    if (Index >= Str.Length)
        return false;

    f64 Num = 0;
    bool bDecimalFound = false;
    u64 DecimalPlaces = 0;
    char c = Str.Data[Index];
    if (!IsDigit(c))
    {
        *OutFloat = 0;
        return false;
    }

    while (c != 0)
    {
        if (IsDigit(c))
        {
            i32 Digit = c - '0';

            if ((DBL_MAX - (f64)Digit) / 10.0 < Num)
            {
                *OutFloat = 0;
                return false;
            }

            if (bDecimalFound)
            {
                DecimalPlaces++;

                f64 PowResult = 10.0;
                for (u8 i = 1; i < (u8)DecimalPlaces; i++)
                {
                    PowResult *= 10.0;
                }

                Num = Num + (f64)Digit / PowResult; //Powd(10.0, (f64)DecimalPlaces);
            }
            else
            {
                Num = Num * 10.0 + Digit;
            }
        }
        else if (c == '.')
        {
            bDecimalFound = true;
        }
        else
        {
            *OutFloat = Num * Sign;
            return true;
        }

        Index++;
        c = Str.Data[Index];
    }

    *OutFloat = Num * Sign;
    return true;
}

bool String_ToU8(const String Str, u8* OutInt)
{
    // 0-255
    
    if (!String_IsValid(Str) || Str.Length > 3)
    {
        return false;
    }

    u64 Index = 0;

    if (Str.Data[0] == '-' || Str.Data[0] == '+')
    {
        Index++;
        if (Str.Data[0] == '-')
            return false;
    }

    if (Index >= Str.Length)
        return false;

    u8 Num = 0;
    char c = Str.Data[Index];
    if (!IsDigit(c))
    {
        *OutInt = 0;
        return false;
    }

    while (Index < Str.Length)
    {
        if (IsDigit(c))
        {
            i32 Digit = c - '0';

            if ((UINT8_MAX - (u8)Digit) / 10 < Num)
            {
                *OutInt = 0;
                return false;
            }

            Num = (u8)(Num * 10 + Digit);
        }
        else
        {
            *OutInt = Num;
            return true;
        }

        Index++;
        c = Str.Data[Index];
    }

    *OutInt = Num;
    return true;
}

bool String_ToU16(const String Str, u16* OutInt)
{
    // 0-65535
    
    if (!String_IsValid(Str) || Str.Length > 5)
    {
        return false;
    }

    u64 Index = 0;

    if (Str.Data[0] == '-' || Str.Data[0] == '+')
    {
        Index++;
        if (Str.Data[0] == '-')
            return false;
    }

    if (Index >= Str.Length)
        return false;

    u16 Num = 0;
    char c = Str.Data[Index];
    if (!IsDigit(c))
    {
        *OutInt = 0;
        return false;
    }

    while (Index < Str.Length)
    {
        if (IsDigit(c))
        {
            i32 Digit = c - '0';

            if ((UINT16_MAX - (u16)Digit) / 10 < Num)
            {
                *OutInt = 0;
                return false;
            }

            Num = (u16)(Num * 10 + Digit);
        }
        else
        {
            *OutInt = Num;
            return true;
        }

        Index++;
        c = Str.Data[Index];
    }

    *OutInt = Num;
    return true;
}

bool String_ToU32(const String Str, u32* OutInt)
{
    // 0-4294967295
    
    if (!String_IsValid(Str))
    {
        return false;
    }

    u64 Index = 0;

    if (Str.Data[0] == '-' || Str.Data[0] == '+')
    {
        Index++;
        if (Str.Data[0] == '-')
            return false;
    }

    if (Index >= Str.Length)
        return false;

    u32 Num = 0;
    char c = Str.Data[Index];
    if (!IsDigit(c))
    {
        *OutInt = 0;
        return false;
    }

    while (Index < Str.Length)
    {
        if (IsDigit(c))
        {
            i32 Digit = c - '0';

            if ((UINT32_MAX - (u32)Digit) / 10 < Num)
            {
                *OutInt = 0;
                return false;
            }

            Num = (u32)(Num * 10 + (u32)Digit);
        }
        else
        {
            *OutInt = Num;
            return true;
        }

        Index++;
        c = Str.Data[Index];
    }

    *OutInt = Num;
    return true;
}

bool String_ToU64(const String Str, u64* OutInt)
{
    // 0-18446744073709551615

    if (!String_IsValid(Str))
    {
        return false;
    }
    
    u64 Index = 0;

    if (Str.Data[0] == '-' || Str.Data[0] == '+')
    {
        Index++;
        if (Str.Data[0] == '-')
            return false;
    }

    if (Index >= Str.Length)
        return false;

    u64 Num = 0;
    char c = Str.Data[Index];
    if (!IsDigit(c))
    {
        *OutInt = 0;
        return false;
    }

    while (Index < Str.Length)
    {
        if (IsDigit(c))
        {
            i32 Digit = c - '0';

            if ((UINT64_MAX - (u64)Digit) / 10 < Num)
            {
                *OutInt = 0;
                return false;
            }

            Num = (u64)(Num * 10 + (u64)Digit);
        }
        else
        {
            *OutInt = Num;
            return true;
        }

        Index++;
        c = Str.Data[Index];
    }

    *OutInt = Num;
    return true;
}

bool String_ToI8(const String Str, i8* OutInt)
{
    // -127-127

    if (!String_IsValid(Str))
    {
        return false;
    }
    
    u64 Index = 0;

    i8 Sign = 1;
    if (Str.Data[0] == '-')
    {
        Sign = -1;
        Index++;
    }
    else if (Str.Data[0] == '+')
    {
        Index++;
    }

    if (Index >= Str.Length)
        return false;

    i8 Num = 0;
    char c = Str.Data[Index];
    if (!IsDigit(c))
    {
        *OutInt = 0;
        return false;
    }

    while (Index < Str.Length)
    {
        if (IsDigit(c))
        {
            i32 Digit = c - '0';

            if ((INT8_MAX - (i8)Digit) / 10 < Num)
            {
                *OutInt = 0;
                return false;
            }

            Num = (i8)(Num * 10 + Digit);
        }
        else
        {
            *OutInt = (i8)(Num * Sign);
            return true;
        }

        Index++;
        c = Str.Data[Index];
    }

    *OutInt = (i8)(Num * Sign);
    return true;
}

bool String_ToI16(const String Str, i16* OutInt)
{
    // -32767-32767

    if (!String_IsValid(Str))
    {
        return false;
    }
    
    u64 Index = 0;

    i8 Sign = 1;
    if (Str.Data[0] == '-')
    {
        Sign = -1;
        Index++;
    }
    else if (Str.Data[0] == '+')
    {
        Index++;
    }

    if (Index >= Str.Length)
        return false;

    i16 Num = 0;
    char c = Str.Data[Index];
    if (!IsDigit(c))
    {
        *OutInt = 0;
        return false;
    }

    while (Index < Str.Length)
    {
        if (IsDigit(c))
        {
            i32 Digit = c - '0';

            if ((INT16_MAX - (i16)Digit) / 10 < Num)
            {
                *OutInt = 0;
                return false;
            }

            Num = (i16)(Num * 10 + Digit);
        }
        else
        {
            *OutInt = (i16)(Num * Sign);
            return true;
        }

        Index++;
        c = Str.Data[Index];
    }

    *OutInt = (i16)(Num * Sign);
    return true;
}

bool String_ToI32(const String Str, i32* OutInt)
{
    // -2147483648-2147483647
    
    u64 Index = 0;

    i8 Sign = 1;
    if (Str.Data[0] == '-')
    {
        Sign = -1;
        Index++;
    }
    else if (Str.Data[0] == '+')
    {
        Index++;
    }

    if (Index >= Str.Length)
        return false;

    i32 Num = 0;
    char c = Str.Data[Index];
    if (!IsDigit(c))
    {
        *OutInt = 0;
        return false;
    }

    while (Index < Str.Length)
    {
        if (IsDigit(c))
        {
            i32 Digit = c - '0';

            if ((INT32_MAX - Digit) / 10 < Num)
            {
                *OutInt = 0;
                return false;
            }

            Num = Num * 10 + Digit;
        }
        else
        {
            *OutInt = Num * Sign;
            return true;
        }

        Index++;
        c = Str.Data[Index];
    }

    *OutInt = Num * Sign;
    return true;
}

bool String_ToI64(const String Str, i64* OutInt)
{
    // -9223372036854775807-9223372036854775807
    
    u64 Index = 0;

    i8 Sign = 1;
    if (Str.Data[0] == '-')
    {
        Sign = -1;
        Index++;
    }
    else if (Str.Data[0] == '+')
    {
        Index++;
    }

    i64 Num = 0;
    char c = Str.Data[Index];
    if (!IsDigit(c))
    {
        *OutInt = 0;
        return false;
    }

    while (Index < Str.Length)
    {
        if (IsDigit(c))
        {
            i64 Digit = c - '0';

            if ((INT64_MAX - Digit) / 10 < Num)
            {
                *OutInt = 0;
                return false;
            }

            Num = Num * 10 + Digit;
        }
        else
        {
            *OutInt = Num * Sign;
            return true;
        }

        Index++;
        c = Str.Data[Index];
    }

    *OutInt = Num * Sign;
    return true;
}

bool String_ToBool(const String Str)
{
    return String_IsEqual(Str, S("1"), false) || String_IsEqual(Str, S("true"), false);
}

String* StringArray_Iterate_Next(StringArray *InArray)
{
    if (InArray->IterIndex >= InArray->Num)
    {
        InArray->IterCurrent = NULL;
        return NULL;
    }

    String* Current = &InArray->List[InArray->IterIndex];
    InArray->IterCurrent = Current;
    InArray->IterIndex++;

    return Current;
}

String* StringArray_Iterate_Begin(StringArray* InArray)
{
    InArray->IterIndex = 0;
    InArray->IterCurrent = NULL;
    return StringArray_Iterate_Next(InArray);
}

StringList StringList_Iterate_Begin(StringList InList)
{
    return InList;
}

StringList StringList_Iterate_Next(StringList InList)
{
    if (InList.Next == NULL || InList.Next == StringList_Null().Next)
    {
        StringList Empty = { 0 };
        return Empty;
    }

    return *InList.Next;
}

bool StringArray_Contains(const StringArray InArray, const String SubString, bool bCaseSensitive)
{
    for (u8 i = 0; i < InArray.Num; i++)
    {
        if (String_IsEqual(InArray.List[i], SubString, bCaseSensitive))
        {
            return true;
        }
    }

    return false;
}

// the problem is that we want to separate one long collection of paths into an array of paths,
// but we cant just do a simple split on the space character because some paths have spaces in them,
// so we need to be able to detect when a space is inside of a " " and ignore it. sigh...
StringList String_SplitIntoList(LinearAllocator* Arena, const String Value, char Delimiter, bool bHandleQuotes)
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
            char C = i < Value.Length ? Value.Data[i] : 0;

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

StringArray String_ParseIntoArray(LinearAllocator* Arena, const String Str, char Delimiter, u32 StartingIndex, u32 MaxCount)
{
    StringArray StrArray = {0};

    u32 Num = 0;

    for (u32 i = StartingIndex; i < Str.Length; i++)
    {
        if (Str.Data[i] == Delimiter || i == Str.Length-1)
        {
            Num++;
        }
    }

    if (Num == 0)
        return StrArray;

    Num = Min(Num, MaxCount);
    StrArray.List = LinearAllocator_Allocate(Arena, sizeof(String) * Num);
    StrArray.Num = Num;

    u32 Offset = StartingIndex;
    u32 ListIndex = 0;
    for (u32 i = StartingIndex; i < Str.Length; i++)
    {
        if (ListIndex >= Num)
            break;

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

    return StrArray;
}

StringArray String_ParseIntoArray_IntoExistingBuffer(String* ArrayBuffer, const String Str, char Delimiter, u32 StartingIndex, u32 MaxCount)
{
    StringArray StrArray = {0};

    u32 Num = 0;

    for (u32 i = StartingIndex; i < Str.Length; i++)
    {
        if (Str.Data[i] == Delimiter || i == Str.Length-1)
        {
            Num++;
        }
    }

    if (Num == 0)
        return StrArray;

    Num = Min(Num, MaxCount);
    StrArray.List = ArrayBuffer;
    StrArray.Num = Num;

    u32 Offset = StartingIndex;
    u32 ListIndex = 0;
    for (u32 i = StartingIndex; i < Str.Length; i++)
    {
        if (ListIndex >= Num)
            return StrArray;

        if (Str.Data[i] == Delimiter && i != Str.Length-1)
        {
            u32 Length = i-Offset;
            MemCopy(StrArray.List[ListIndex].Data, &Str.Data[Offset], Length);
            StrArray.List[ListIndex].Data[Length] = 0;
            StrArray.List[ListIndex].Length = Length;

            Offset = i+1;
            ListIndex++;
        }
    }

    u32 Length = Str.Length-Offset;
    MemCopy(StrArray.List[ListIndex].Data, &Str.Data[Offset], Length);
    StrArray.List[ListIndex].Data[Length] = 0;
    StrArray.List[ListIndex].Length = Length;

    return StrArray;
}

bool StringArray_Find(StringArray Array, const String Source, u32* FoundIndex)
{
    u32 Index = 0;
    for each_str_i (Index, s, Array)
    {
        if (String_IsEqual(*s, Source, true))
        {
            if (FoundIndex)
            {
                *FoundIndex = Index;
            }

            return true;
        }
    }

    return false;
}

String StringArray_GetStringFromIndex(StringArray Array, u32 Index)
{
    u32 i = 0;
    for each_str_i (i, s, Array)
    {
        if (i == Index)
            return *s;
    }

    return String_Null();
}

u32 String_GetLength(const char *Str)
{
    char* Start = (char*)Str;
    while (*Start != 0)
    {
        Start++;
    }

    return (u32)(Start-Str);
}

u32 String_GetLength_Ex(const char* Str, u32 MaxLength)
{
    u32 Len = 0;

    while (Len < MaxLength && Str[Len] != 0)
    {
        Len++;
    }

    return Len;
}

u32 String16_GetLength(const wchar* Str)
{
    u32 Len = 0;

    while (Str[Len] != 0)
    {
        Len++;
    }

    return Len;
}

u32 String16_GetLength_Ex(const wchar* Str, u32 MaxLength)
{
    u32 Len = 0;

    while (Len < MaxLength && Str[Len] != 0)
    {
        Len++;
    }

    return Len;
}

bool IsAlphabet(char Char)
{
	return ((Char >= 'A' && Char <= 'Z') || (Char >= 'a' && Char <= 'z'));
}

bool IsAlphabetUpper(char Char)
{
	return Char >= 'A' && Char <= 'Z';
}

bool IsAlphabetLower(char Char)
{
	return Char >= 'a' && Char <= 'z';
}

bool IsDigit(char Char)
{
	return Char >= '0' && Char <= '9';
}

bool IsWhitespace(char Char)
{
	return Char == ' ' || Char == '\r' || Char == '\t' || Char == '\f' || Char == '\v' || Char == '\n';
}

bool IsNewline(char Char)
{
	return Char == '\r' || Char == '\f' || Char == '\n';
}

bool IsSymbol(char Char)
{
	return Char == '(' || Char == ')' || Char == '{' || Char == '}' ||
			Char == '!' || Char == '@' || Char == '#' || Char == '$' ||
			Char == '%' || Char == '^' || Char == '&' || Char == '*' ||
			Char == '+' || Char == '=';
}

char ToUpper(char Char)
{
	return Char >= 'a' && Char <= 'z' ? (char)(Char - 32) : Char;
}

char ToLower(char Char)
{
	return Char >= 'A' && Char <= 'Z' ? (char)(Char + 32) : Char;
}

char ToForwardSlash(char Char)
{
	return Char == '\\' ? '/' : Char;
}

char ToBackSlash(char Char)
{
	return Char == '/' ? '\\' : Char;
}

void EatSpaces(char** Str)
{
    if (!(Str == NULL || *Str == NULL || *(*Str) == 0))
    {
        while (*(*Str) > 0 && IsWhitespace(*(*Str)))
        {
            (*Str)++;
        }
    }
}

void EatSpaces_Backwards(char** Str)
{
    if (!(Str == NULL || *Str == NULL || *(*Str) == 0))
    {
        while (*(*Str) > 0 && IsWhitespace(*(*Str)))
        {
            (*Str)--;
        }
    }
}

void EatBraces(char** Str)
{
    if (!(Str == NULL || *Str == NULL || *(*Str) == 0))
    {
        char* S = *Str;
        char c = *S;
        while (c > 0 && (c == '{' || c == '}'))
        {
            S++;
            c = *S;
        }
    }
}

void EatBraces_Backwards(char** Str)
{
    if (!(Str == NULL || *Str == NULL || *(*Str) == 0))
    {
        char* S = *Str;
        char c = *S;
        while (c > 0 && (c == '{' || c == '}'))
        {
            S--;
            c = *S;
        }
    }
}

void EatParenthesis(char** Str)
{
    if (!(Str == NULL || *Str == NULL || *(*Str) == 0))
    {
        char* S = *Str;
        char c = *S;
        while (c > 0 && (c == '(' || c == ')'))
        {
            S++;
            c = *S;
        }
    }
}

void EatParenthesis_Backwards(char** Str)
{
    if (!(Str == NULL || *Str == NULL || *(*Str) == 0))
    {
        char* S = *Str;
        char c = *S;
        while (c > 0 && (c == '(' || c == ')'))
        {
            S--;
            c = *S;
        }
    }
}

void EatSymbols(char** Str)
{
    if (!(Str == NULL || *Str == NULL || *(*Str) == 0))
    {
        char* S = *Str;
        char c = *S;
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

void EatSymbols_Backwards(char** Str)
{
    if (!(Str == NULL || *Str == NULL || *(*Str) == 0))
    {
        char* S = *Str;
        char c = *S;
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
