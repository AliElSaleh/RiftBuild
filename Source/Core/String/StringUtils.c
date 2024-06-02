#include "StringUtils.h"

#include "Globals.h"
#include "Log.h"
#include "Math/Math.h"
#include "Memory/LinearAllocator.h"
#include "Memory/Memory.h"
#include "String/BaseString.h"

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
    str.Length = Source.Length;
    str.Data = LinearAllocator_Allocate(Arena, Source.Length+1);
    MemCopy(str.Data, Source.Data, Source.Length);
    str.Data[Source.Length] = 0;
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
    str.Length = Source.Length;
    str.Data = LinearAllocator_Allocate(Arena, Source.Length+1);
    MemCopy(str.Data, Source.Data, Source.Length);
    str.Data[Source.Length] = 0;
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
    str.Length = 0;
    str.Data = LinearAllocator_Allocate(Arena, Capacity+1);
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
        u64 Len = Str->Length;
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

    u64 NumCopied = 0;
    for (u32 i = 0; i < Array.Num; i++)
    {
        const String* Str = &Array.List[i];
        u64 Len = Str->Length;

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
        char A = StringA.Data[i];
        char B = StringB.Data[i];
        
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

bool String_Contains(const String Str, const String SubString, bool bCaseSensitive)
{
    for (u32 i = 0; i < Str.Length; i++)
    {
        const String S = StrShiftF(Str, i);
        if (Str.Length - i < SubString.Length)
            return false;

        if (String_IsEqual(StrSlice(S.Data, SubString.Length), SubString, bCaseSensitive))
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
        wchar A = StringA.Data[i];
        wchar B = StringB.Data[i];
        
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
    //va_list Args;
    //va_start(Args, NumArgs);

    for (u8 i = 0; i < Array.Num; i++)
    {
        //String Param = va_arg(Args, String);

        String_Append(Dest, Array.List[i]);
    }

    //va_end(Args);
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

i32 String_Format(String* Dest, const String Format, u32 MaxLength, ...)
{
    va_list Args;
    va_start(Args, MaxLength);
    i32 Written = stbsp_vsnprintf(Dest->Data, (i32)MaxLength, Format.Data, Args);
    Dest->Length = (u32)Written;
    va_end(Args);

    return Written;
}

u32 String_FormatV(String* Dest, const String Format, u32 MaxLength, void* VAList)
{
    Dest->Length = (u32)stbsp_vsnprintf(Dest->Data, (i32)MaxLength, Format.Data, VAList);
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
    MemZero(Str->Data, Str->Length);
    Str->Length = 0;
}

void String_Zero(String* Str)
{
    ASSERT(Str->Length != 0);

    MemZero(Str, Str->Length);
}

void String_Fill(String* Str, char C)
{
    ASSERT(Str->Length != 0);

    MemSet(Str, C, Str->Length);
}

void String_ToLower(String* Str)
{
    for (u64 i = 0; i < Str->Length; i++)
    {
        Str->Data[i] = ToLower(Str->Data[i]);
    }
}

void String_ToUpper(String* Str)
{
    for (u64 i = 0; i < Str->Length; i++)
    {
        Str->Data[i] = ToUpper(Str->Data[i]);
    }
}

void String_ToWide(const String FromString, String16* ToString)
{
    for (u64 i = 0; i < FromString.Length; i++)
    {
        ToString->Data[i] = (wchar)FromString.Data[i];
    }

    ToString->Length = FromString.Length;
}

void String_ToNarrow(const String16 FromString, String* ToString)
{
    for (u64 i = 0; i < FromString.Length; i++)
    {
        ToString->Data[i] = (char)FromString.Data[i];
    }

    ToString->Length = FromString.Length;
}

void String_BackSlashToForwardSlash(String* Str)
{
    for (u64 i = 0; i < Str->Length; i++)
    {
        if (Str->Data[i] == '\\')
        {
            Str->Data[i] = '/';
        }
    }
}

void String_ForwardSlashToBackSlash(String* Str)
{
    for (u64 i = 0; i < Str->Length; i++)
    {
        if (Str->Data[i] == '/')
        {
            Str->Data[i] = '\\';
        }
    }
}

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

    return StrCompC(Str.Data + i, Str.Length - i, Str.Capacity);
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

        char C1 = A.Data[i];
        char C2 = B.Data[i];
    
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

    return StrCompC(Str.Data + i, Str.Length - i, Str.Capacity);
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

    return StrCompC(Str.Data, i, Str.Capacity);
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

    return StrCompC(Str.Data + i, Str.Length - i, Str.Capacity);
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

    return StrCompC(Str.Data, i, Str.Capacity);
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

    return StrCompC(Str.Data, i, Str.Capacity);
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

bool String_ToF32(const String Str, f32* OutFloat)
{
    if (!String_IsValid(Str))
    {
        return false;
    }

    u64 Index = 0;

    i8 Sign = 1;
    if (Str.Data[0] == '-' || Str.Data[0] == '+')
    {
        Sign = -1;
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
            i8 Digit = c - (i8)'0';

            if ((FLT_MAX - Digit) / 10 < Num)
            {
                *OutFloat = FLT_MAX * Sign;
                return false;
            }

            if (bDecimalFound)
            {
                DecimalPlaces++;
                Num = Num + Digit / Pow(10.0f, (f32)DecimalPlaces);
            }
            else
            {
                Num = Num * 10.0f + Digit;
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

bool String_ToF64(const String Str, f64* OutFloat)
{
    if (!String_IsValid(Str))
    {
        return false;
    }

    u64 Index = 0;

    i8 Sign = 1;
    if (Str.Data[0] == '-' || Str.Data[0] == '+')
    {
        Sign = -1;
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
            i8 Digit = c - '0';

            if ((DBL_MAX - Digit) / 10 < Num)
            {
                *OutFloat = DBL_MAX * Sign;
                return false;
            }

            if (bDecimalFound)
            {
                DecimalPlaces++;
                Num = Num + Digit / Powd(10.0, (f64)DecimalPlaces);
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
    
    if (!String_IsValid(Str))
    {
        return false;
    }

    u64 Index = 0;

    if (Str.Data[0] == '-' || Str.Data[0] == '+')
    {
        Index++;
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
            i8 Digit = c - '0';

            if ((UINT8_MAX - Digit) / 10 < Num)
            {
                *OutInt = UINT8_MAX;
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
    
    if (!String_IsValid(Str))
    {
        return false;
    }

    u64 Index = 0;

    if (Str.Data[0] == '-' || Str.Data[0] == '+')
    {
        Index++;
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
            i8 Digit = c - '0';

            if ((UINT16_MAX - Digit) / 10 < Num)
            {
                *OutInt = UINT16_MAX;
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
            i8 Digit = c - '0';

            if ((UINT32_MAX - (u32)Digit) / 10 < Num)
            {
                *OutInt = UINT32_MAX;
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
            i8 Digit = c - '0';

            if ((UINT64_MAX - (u64)Digit) / 10 < Num)
            {
                *OutInt = UINT64_MAX;
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
            i8 Digit = c - '0';

            if ((INT8_MAX - Digit) / 10 < Num)
            {
                *OutInt = INT8_MAX * Sign;
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
            i16 Digit = c - '0';

            if ((INT16_MAX - Digit) / 10 < Num)
            {
                *OutInt = INT16_MAX * Sign;
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

bool String_ToI32(const String Str, i32* OutInt)
{
    // -2147483647-2147483647
    
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
                *OutInt = INT32_MAX * Sign;
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
                *OutInt = INT64_MAX * Sign;
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
    return String_IsEqual(Str, StrLit("1"), false) || String_IsEqual(Str, StrLit("true"), false);
}

String* StringArray_Iterate_Next(StringArray *InArray)
{
    if (InArray->Iterator.Index >= InArray->Num)
    {
        InArray->Iterator.Current = NULL;
        return NULL;
    }

    String* Current = &InArray->List[InArray->Iterator.Index];
    InArray->Iterator.Current = Current;
    InArray->Iterator.Index++;

    return Current;
}

String* StringArray_Iterate_Begin(StringArray* InArray)
{
    InArray->Iterator.Index = 0;
    InArray->Iterator.Current = NULL;
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
	return Char >= 'a' && Char <= 'z' ? Char - 32 : Char;
}

char ToLower(char Char)
{
	return Char >= 'A' && Char <= 'Z' ? Char + 32 : Char;
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
        char c = *(*Str);
        while (c > 0 && (c == '{' || c == '}'))
        {
            (*Str)++;
        }
    }
}

void EatBraces_Backwards(char** Str)
{
    if (!(Str == NULL || *Str == NULL || *(*Str) == 0))
    {
        char c = *(*Str);
        while (c > 0 && (c == '{' || c == '}'))
        {
            (*Str)--;
        }
    }
}

void EatParenthesis(char** Str)
{
    if (!(Str == NULL || *Str == NULL || *(*Str) == 0))
    {
        char c = *(*Str);
        while (c > 0 && (c == '(' || c == ')'))
        {
            (*Str)++;
        }
    }
}

void EatParenthesis_Backwards(char** Str)
{
    if (!(Str == NULL || *Str == NULL || *(*Str) == 0))
    {
        char c = *(*Str);
        while (c > 0 && (c == '(' || c == ')'))
        {
            (*Str)--;
        }
    }
}

void EatSymbols(char** Str)
{
    if (!(Str == NULL || *Str == NULL || *(*Str) == 0))
    {
        char c = *(*Str);
        while (c > 0 &&
            (c == '(' || c == ')' || c == '{' || c == '}' ||
			c == '!' || c == '@' || c == '#' || c == '$' ||
			c == '%' || c == '^' || c == '&' || c == '*' ||
			c == '+' || c == '='))
        {
            (*Str)++;
        }
    }
}

void EatSymbols_Backwards(char** Str)
{
    if (!(Str == NULL || *Str == NULL || *(*Str) == 0))
    {
        char c = *(*Str);
        while (c > 0 &&
            (c == '(' || c == ')' || c == '{' || c == '}' ||
			c == '!' || c == '@' || c == '#' || c == '$' ||
			c == '%' || c == '^' || c == '&' || c == '*' ||
			c == '+' || c == '='))
        {
            (*Str)--;
        }
    }
}

/*
#ifdef RIFT_ASAN
// since we are not linking with the standard library, these functions below do not exist for the linker,
// so let's define them here, but they wont do anything
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-prototypes"

int atoi(const char *str)
{
    return 0;
}

long int atol(const char *str)
{
    return 0;
}

void* memchr(const void* str, int c, size_t n)
{
    return nullptr;
}

char* strcat(char* dest, const char* src)
{
    return nullptr;
}

char* strchr(const char* str, int c)
{
    return nullptr;
}

int strcmp(const char* str1, const char* str2)
{
    return 0;
}

char* strcpy(char* dest, const char* src)
{
    return nullptr;
}

size_t strcspn(const char* str1, const char* str2)
{
    return 0;
}

char* strdup(const char* str1)
{
    return nullptr;
}

char* strncat(char* dest, const char* src, size_t n)
{
    return nullptr;
}

int strncmp(const char *str1, const char *str2, size_t n)
{
    return 0;
}

char* strncpy(char* dest, const char* src, size_t n)
{
    return nullptr;
}

size_t strnlen(const char* s, size_t maxlen)
{
    return 0;
}

const char* strpbrk(const char* str1, const char* str2)
{
    return nullptr;
}

char* strrchr(const char* str, int c)
{
    return nullptr;
}

size_t strspn(const char* str1, const char* str2)
{
    return 0;
}

char* strstr(const char* haystack, const char* needle)
{
    return nullptr;
}

char* strtok(char* str, const char* delim)
{
    return nullptr;
}

long int strtol(const char* str, char** endptr, int base)
{
    return 0;
}

size_t wcslen(const wchar_t *ws)
{
    return 0;
}

size_t wcsnlen(const wchar_t *str, size_t numberOfElements)
{
    return 0;
}

int _stricmp(const char* string1, const char* string2)
{
    return 0;
}

#pragma clang diagnostic pop
#endif // RIFT_ASAN
*/
