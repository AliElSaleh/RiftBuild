#ifndef _STRINGUTILS_H_
#define _STRINGUTILS_H_

#ifndef UNITY_BUILD
#include "EngineTypes.h"
#endif

ENUM(ECompareResult)
{
    CompareResult_None,
    CompareResult_Equal,
    CompareResult_Less,
    CompareResult_Greater
};

// C String Helpers
// ----------------------------------

RIFT_API u32   CString_Copy(char* Dest, const char* Source);
RIFT_API u32   CString_CopyN(char* Dest, const char* Source, u32 Length);
RIFT_API void  CString_ToBytes(const char* Data, u32 Length, u8* OutBytes);
RIFT_API void  CString_FromBytes(const u8* Data, u32 Length, char* OutCharacters);
RIFT_API bool  CString_IsEqual(const char* StringA, const char* StringB, bool bCaseSensitive);
RIFT_API i32   CString_Format(char* Dest, const char* Format, u32 MaxLength, ...);
RIFT_API i32   CString_FormatV(char* Dest, const char* Format, u32 MaxLength, void* VAList);
RIFT_API void  CString_Zero(char* Str, u32 Length);
RIFT_API void  CString_Fill(char* Str, u32 Length, char N);
RIFT_API char* CString_Empty(char* Str);
RIFT_API void  CString_ToLower(char* Str);
RIFT_API void  CString_ToUpper(char* Str);
RIFT_API void  CString_ToWide(const char* FromString, wchar* ToString);
RIFT_API void  CString_ToNarrow(const wchar* FromString, char* ToString);
RIFT_API u32   CString_ScanUntil(const char* Str, char Char);
RIFT_API void  CString_SubString(char* Dest, const char* Source, u32 Start, u32 Length);
RIFT_API bool  CString_IndexOfChar(const char* Str, char C, u32* OutIndex);

// String Helpers
// ----------------------------------
RIFT_API NO_DISCARD bool String_IsValid(const String Str);
RIFT_API NO_DISCARD bool StringArray_IsValid(const StringArray Str);
RIFT_API NO_DISCARD bool StringList_IsValid(const StringList Str);

RIFT_API NO_DISCARD String String_Create(LinearAllocator* Arena, const String Source); // todo: deprecate
RIFT_API NO_DISCARD String String_Duplicate(LinearAllocator* Arena, const String Source);
RIFT_API NO_DISCARD String String_Reserve(LinearAllocator* Arena, u32 Capacity);
RIFT_API NO_DISCARD String String_ReserveAndCopy(LinearAllocator* Arena, u32 Capacity, const String Source);
RIFT_API NO_DISCARD String String_Join(LinearAllocator* Arena, const StringArray Array);
RIFT_API void   String_ConcatArray(String* Dest, const StringArray Array, u32 MaxSize);

RIFT_API NO_DISCARD StringArray String_CreateArray(LinearAllocator* Arena, const StringArray Array);

RIFT_API NO_DISCARD bool String_IsEqual(const String StringA, const String StringB, bool bCaseSensitive);
RIFT_API NO_DISCARD bool String16_IsEqual(const String16 StringA, const String16 StringB, bool bCaseSensitive);

RIFT_API NO_DISCARD bool String_IsInteger(const String Str);
RIFT_API NO_DISCARD bool String_IsInteger32(const String Str);
RIFT_API NO_DISCARD bool String_IsFloat(const String Str);
RIFT_API NO_DISCARD bool String_IsNumeric(const String Str);

RIFT_API NO_DISCARD bool String_Contains(const String Str, const String SubString, bool bCaseSensitive);
RIFT_API NO_DISCARD bool String_ContainsPathSeparators(const String Str);
RIFT_API NO_DISCARD bool String_ContainsDigits(const String Str);
RIFT_API NO_DISCARD bool String_ContainsNonDigits(const String Str);
RIFT_API NO_DISCARD bool String_StartsWith(const String Str, const String SubString, bool bCaseSensitive);
RIFT_API NO_DISCARD bool String_EndsWith(const String Str, const String SubString, bool bCaseSensitive);

RIFT_API void String_Copy(String* Dest, const String Source);
RIFT_API void String_CopyN(String* Dest, const String Source, u32 Length);

RIFT_API void String_Append(String* Dest, const String Source);
RIFT_API void String_AppendChar(String* Dest, const char Source);
RIFT_API void String_AppendSpace(String* Dest);
RIFT_API void String_AppendTab(String* Dest);
RIFT_API void String_AppendNewline(String* Dest);
RIFT_API void String_AppendPathSeparator(String* Dest);
RIFT_API void String_AppendPathSeparator_Checked(String* Dest);

RIFT_API NO_DISCARD ECompareResult String_CompareVersion(const String VersionA, const String VersionB);

RIFT_API void String_Zero(String* Str);
RIFT_API void String_Fill(String* Str, char C);

#define String_Concat(Dest, ...)               do { String __SArgs__[] = {__VA_ARGS__}; StringArray __TempArray__ = {0}; __TempArray__.List = __SArgs__; __TempArray__.Num = SArray_Capacity(__SArgs__); StringInternal_Concat(Dest, __TempArray__); } while (0)
#define String_BuildSeparator(Dest, Char, ...) do { String __SArgs__[] = {__VA_ARGS__}; StringArray __TempArray__ = {0}; __TempArray__.List = __SArgs__; __TempArray__.Num = SArray_Capacity(__SArgs__); StringInternal_BuildSeparator(Dest, Char, __TempArray__); } while (0)
#define String_BuildPath(Dest, ...)            do { String __SArgs__[] = {__VA_ARGS__}; StringArray __TempArray__ = {0}; __TempArray__.List = __SArgs__; __TempArray__.Num = SArray_Capacity(__SArgs__); StringInternal_BuildPath(Dest, __TempArray__); } while (0)

RIFT_API void StringInternal_Concat(String* Dest, const StringArray Array);
RIFT_API void StringInternal_BuildSeparator(String* Dest, char Separator, const StringArray Array);
RIFT_API void StringInternal_BuildPath(String* Dest, const StringArray Array);

RIFT_API void String_Format(String* Dest, const String Format, u32 Capacity, ...);//todo: remove maxlength
RIFT_API void String_Format_Ex(String* Dest, const String Format, u32 Capacity, ...);
RIFT_API void String_FormatV(String* Dest, const String Format, u32 Capacity, void* VAList);

RIFT_API void String_Empty(String* Str);

RIFT_API void String_ToLower(String* Str);
RIFT_API void String_ToUpper(String* Str);

RIFT_API void String_BackSlashToForwardSlash(String* Str);
RIFT_API void String_ForwardSlashToBackSlash(String* Str);
RIFT_API void String_ConvertSlashToPlatformSlash(String* Str);

RIFT_API void String_ToWide(const String FromString, String16* ToString);
RIFT_API void String_ToNarrow(const String16 FromString, String* ToString);

RIFT_API NO_DISCARD bool String_ReplaceCharInline(String* Str, char Char, char ReplaceChar);
RIFT_API NO_DISCARD bool String_ReplaceNonAlphaNumericCharInline(String* Str, char ReplaceChar);
RIFT_API NO_DISCARD bool String_CollapseMatching(String* Dest, const String A, const String B, bool bCaseSensitive);

RIFT_API NO_DISCARD String String_EatChar(String Str, char Char); // maybe make an s version or single version?
RIFT_API NO_DISCARD String String_EatSpaces(String Str);
RIFT_API NO_DISCARD String String_EatNewLines(String Str);
RIFT_API NO_DISCARD String String_EatPathSeparators(String Str);
RIFT_API NO_DISCARD String String_EatCharFromEnd(String Str, char Char);
RIFT_API NO_DISCARD String String_EatSpacesFromEnd(String Str);
RIFT_API NO_DISCARD String String_EatNewLinesFromEnd(String Str);
RIFT_API NO_DISCARD String String_EatPathSeparatorsFromEnd(String Str);

RIFT_API NO_DISCARD bool String_EatCharInline(String* Str, char Char);
RIFT_API NO_DISCARD bool String_EatCharInline_Single(String* Str, char Char);
RIFT_API NO_DISCARD bool String_EatCharInlineFromEnd(String* Str, char Char);
RIFT_API NO_DISCARD bool String_EatSpacesInline(String* Str);
RIFT_API NO_DISCARD bool String_EatSpacesInlineFromEnd(String* Str);
RIFT_API NO_DISCARD bool String_EatNewLinesInline(String* Str);
RIFT_API NO_DISCARD bool String_EatNewLinesInlineFromEnd(String* Str);
RIFT_API NO_DISCARD bool String_EatPathSeparatorsInline(String* Str);
RIFT_API NO_DISCARD bool String_EatPathSeparatorsInlineFromEnd(String* Str);

RIFT_API NO_DISCARD String String_ScanUntil(const String* Str, char Char);

RIFT_API NO_DISCARD bool String_IndexOfChar(const String Str, char C, u32* OutIndex);
RIFT_API NO_DISCARD bool String_IndexOfLastChar(const String Str, char C, u32* OutIndex);
RIFT_API NO_DISCARD bool String_IndexOfFirstPathSlash(const String Str, u32* OutIndex);
RIFT_API NO_DISCARD bool String_IndexOfLastPathSlash(const String Str, u32* OutIndex);
RIFT_API NO_DISCARD bool String_IndexOfFirstWhitespace(const String Str, u32* OutIndex);
RIFT_API NO_DISCARD bool String_IndexOfLastWhitespace(const String Str, u32* OutIndex);
RIFT_API NO_DISCARD bool String_IndexOfFirstNewline(const String Str, u32* OutIndex);
RIFT_API NO_DISCARD bool String_IndexOfSubstring(const String Str, const String Substring, bool bCaseSensitive, u32* OutIndex);

RIFT_API NO_DISCARD bool String_SanitizePath(String* Dest, const String Source);
RIFT_API NO_DISCARD bool String_SanitizePathAndWrap(String* Dest, const String Source);
RIFT_API NO_DISCARD bool String_SanitizeQuotes(String* Dest, const String Source);

RIFT_API NO_DISCARD bool String_IsFirst(const String Str, char C);
RIFT_API NO_DISCARD bool String_IsLast(const String Str, char C);

RIFT_API NO_DISCARD u32 String_CountChar(const String Str, char C);
RIFT_API NO_DISCARD u32 String_CountSpaces(const String Str);
RIFT_API NO_DISCARD u32 String_CountPathSeparators(const String Str);

RIFT_API void String_StripString(const String Str, const String Substring, String* OutStr);
RIFT_API void String_StripChar(const String Str, char C, String* OutStr);
RIFT_API void String_StripWhitespace(const String Str, String* OutStr);
RIFT_API void String_StripNewline(const String Str, String* OutStr);
RIFT_API void String_StripDigit(const String Str, String* OutStr);
RIFT_API void String_StripSymbol(const String Str, String* OutStr);
RIFT_API void String_StripAlphabet(const String Str, String* OutStr);

RIFT_API NO_DISCARD String* StringArray_Iterate_Next(StringArray* InArray);
RIFT_API NO_DISCARD String* StringArray_Iterate_Begin(StringArray* InArray);

RIFT_API NO_DISCARD StringList StringList_Iterate_Next(StringList InList);
RIFT_API NO_DISCARD StringList StringList_Iterate_Begin(StringList InList);

RIFT_API NO_DISCARD bool StringArray_Contains(const StringArray InArray, const String SubString, bool bCaseSensitive);

RIFT_API NO_DISCARD StringList String_SplitIntoList(LinearAllocator* Arena, const String Value, char Delimiter, bool bHandleQuotes);

RIFT_API NO_DISCARD StringArray String_SplitIntoArray(LinearAllocator* Arena, const String Str, const String Delimiter, u32 StartingIndex, u32 MaxCount);
// rename
RIFT_API NO_DISCARD StringArray String_ParseIntoArray(LinearAllocator* Arena, const String Str, char Delimiter, u32 StartingIndex, u32 MaxCount);
RIFT_API NO_DISCARD StringArray String_ParseIntoArray_IntoExistingBuffer(String* ArrayBuffer, const String Str, char Delimiter, u32 StartingIndex, u32 MaxCount);

RIFT_API NO_DISCARD bool StringArray_Find(StringArray Array, const String Source, u32* FoundIndex);
RIFT_API NO_DISCARD String StringArray_GetStringFromIndex(StringArray Array, u32 Index);

RIFT_API NO_DISCARD bool StringList_Find(StringList List, const String Source, u32* FoundIndex);
RIFT_API NO_DISCARD String StringList_GetStringFromIndex(StringList List, u32 Index);

RIFT_API NO_DISCARD bool String_ToF32(const String Str, f32* OutFloat);
RIFT_API NO_DISCARD bool String_ToF64(const String Str, f64* OutFloat);

RIFT_API NO_DISCARD bool String_ToU8 (const String Str, u8* OutInt);
RIFT_API NO_DISCARD bool String_ToU16(const String Str, u16* OutInt);
RIFT_API NO_DISCARD bool String_ToU32(const String Str, u32* OutInt);
RIFT_API NO_DISCARD bool String_ToU64(const String Str, u64* OutInt);
RIFT_API NO_DISCARD bool String_ToI8 (const String Str, i8* OutInt);
RIFT_API NO_DISCARD bool String_ToI16(const String Str, i16* OutInt);
RIFT_API NO_DISCARD bool String_ToI32(const String Str, i32* OutInt);
RIFT_API NO_DISCARD bool String_ToI64(const String Str, i64* OutInt);

RIFT_API NO_DISCARD bool CString_ToBool(const char* Str);
RIFT_API NO_DISCARD bool String_ToBool(const String Str);

RIFT_API NO_DISCARD u32 String_GetLength(const char* Str);
RIFT_API NO_DISCARD u32 String_GetLength_Ex(const char* Str, u32 MaxLength);
RIFT_API NO_DISCARD u32 String16_GetLength(const wchar* Str);
RIFT_API NO_DISCARD u32 String16_GetLength_Ex(const wchar* Str, u32 MaxLength);

RIFT_API NO_DISCARD bool IsAlphabet(char Char);
RIFT_API NO_DISCARD bool IsAlphabetUpper(char Char);
RIFT_API NO_DISCARD bool IsAlphabetLower(char Char);
RIFT_API NO_DISCARD bool IsDigit(char Char);
RIFT_API NO_DISCARD bool IsWhitespace(char Char);
RIFT_API NO_DISCARD bool IsNewline(char Char);
RIFT_API NO_DISCARD bool IsSymbol(char Char);
RIFT_API NO_DISCARD char ToUpper(char Char);
RIFT_API NO_DISCARD char ToLower(char Char);
RIFT_API NO_DISCARD char ToForwardSlash(char Char);
RIFT_API NO_DISCARD char ToBackSlash(char Char);

// TODO: possibly delete all these functions below
RIFT_API void EatSpaces(char** Str);
RIFT_API void EatSpaces_Backwards(char** Str);
RIFT_API void EatBraces(char** Str);
RIFT_API void EatBraces_Backwards(char** Str);
RIFT_API void EatParenthesis(char** Str);
RIFT_API void EatParenthesis_Backwards(char** Str);
RIFT_API void EatSymbols(char** Str);
RIFT_API void EatSymbols_Backwards(char** Str);


// inline implementations
FORCEINLINE static String StrSlice(const char* Data, u32 Len)
{
    String Result;
    Result.Data     = (char*)Data;
    Result.Length   = Len;
    Result.Capacity = Len;

    return Result;
}

FORCEINLINE static String StrShiftF(const String s, u32 Offset)
{
    const u32 MinLength   = Min(Offset, s.Length);
    const u32 MinCapacity = Min(Offset, s.Capacity);

    String Result;
    Result.Data           = s.Data     + MinLength;
    Result.Length         = s.Length   - MinLength;
    Result.Capacity       = s.Capacity - MinCapacity;

    return Result;
}

#endif // _STRINGUTILS_H_
