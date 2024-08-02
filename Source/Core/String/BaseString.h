#ifndef _BASE_STRING_H_
#define _BASE_STRING_H_

#include "EngineTypes.h"

STRUCT(String)
{
    char* Data;
    u32 Length;
    u32 Capacity;
};

STRUCT(String16)
{
    wchar* Data;
    u32 Length;
    u32 Capacity;
};

STRUCT(StringArray)
{
    String* List;
    u32 Num;

    u32 IterIndex;
    void* IterCurrent;
};

STRUCT(StringList)
{
    String String;
    struct StringList* Next;
};

RIFT_API bool String_IsValid(const String Str);
RIFT_API bool StringArray_IsValid(const StringArray Str);
RIFT_API bool StringList_IsValid(const StringList Str);

#define each_str(Element, Array)            (const String* (Element) = StringArray_Iterate_Begin(&(Array)); (Element) != NULL; (Element) = StringArray_Iterate_Next(&(Array)))
#define each_str_i(Index, Element, Array)   (const String* (Element) = StringArray_Iterate_Begin(&(Array)); (Element) != NULL; (Element) = StringArray_Iterate_Next(&(Array)), (++Index))
#define each_str_list(List)                 (StringList It = List; (It).String.Data != NULL || (It).Next != NULL; (It) = StringList_Iterate_Next(It))
#define each_str_list_it(Element, List)     (StringList (Element) = List; (Element).String.Data != NULL || (Element).Next != NULL; (Element) = StringList_Iterate_Next(Element))

#define StringN(n)  		                struct { char Data[n]; u32 Length; u32 Capacity; }

#define StringLocal(Name, n) 	            char  MACRO_VAR(CONCAT(Buffer_, Name))[n] = {0}; String   Name; Name.Data = MACRO_VAR(CONCAT(Buffer_, Name)); Name.Length = 0; Name.Capacity = (n)-1
#define String16Local(Name, n) 	            wchar MACRO_VAR(CONCAT(Buffer_, Name))[n] = {0}; String16 Name; Name.Data = MACRO_VAR(CONCAT(Buffer_, Name)), Name.Length = 0, Name.Capacity = (n)-1

#define CStr(s)                             (String)         {.Data = (char* )(s), .Length = String_GetLength(s),       .Capacity = 0}
#define CStrEx(s, n)                        (String)         {.Data = (char* )(s), .Length = String_GetLength_Ex(s, n), .Capacity = 0}
#define CStrView(s)                         (const String)   {.Data = (char* )(s), .Length = String_GetLength(s),       .Capacity = 0}
#define CStr16(s)                           (String16)       {.Data = (wchar*)(s), .Length = String16_GetLength((wchar*)(s)),     .Capacity = 0}
#define CStr16View(s)                       (const String16) {.Data = (wchar*)(s), .Length = String16_GetLength(s),     .Capacity = 0}

#define S(s)                                (const String)   {.Data = (char* )((s)), .Length = sizeof((s))-1, .Capacity = sizeof((s))-1}
#define SC(s)                                                {.Data = (char* )((s)), .Length = sizeof((s))-1, .Capacity = sizeof((s))-1}
#define S16(s)                              (const String16) {.Data = (wchar*)((s)), .Length = sizeof((s))-1, .Capacity = sizeof((s))-1}

#define StrMake(s)                          (String)         {.Data = (s).Data,        .Length = (s).Length, .Capacity = (s).Capacity}
#define StrView(s)                          (const String)   {.Data = (char*)(s).Data, .Length = (s).Length, .Capacity = (s).Capacity}
#define Str16Slice(s, Len)                  (String16)       {.Data = (wchar*)(s),     .Length = Len,        .Capacity = Len}

#define StrArray(...)                       (StringArray)    {.List = ((String[]){__VA_ARGS__}), .Num = SArray_Capacity(((String[]){__VA_ARGS__}))}

#define StrFormat                           "%.*s"
#define StrArg(s)                           (i32)(s).Length, (s).Data


// inline implementations

FORCEINLINE static String StrSlice(const char* Data, u32 Len)
{
    String Result;
    Result.Data     = (char*)Data;
    Result.Length   = Len;
    Result.Capacity = Len;

    return Result;
}

FORCEINLINE static String StrShiftF(String s, u32 Offset)
{
    const u32 MinLength   = Min(Offset, s.Length);
    const u32 MinCapacity = Min(Offset, s.Capacity);

    String Result;
    Result.Data           = s.Data     + MinLength;
    Result.Length         = s.Length   - MinLength;
    Result.Capacity       = s.Capacity - MinCapacity;

    return Result;
}

#endif // _BASE_STRING_H_
