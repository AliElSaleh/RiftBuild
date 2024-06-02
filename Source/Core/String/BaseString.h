#pragma once

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

#define StringLocal(Name, n) 	            char  MACRO_VAR(CONCAT(Buffer_, Name))[n] = {0}; String   Name = {.Data = MACRO_VAR(CONCAT(Buffer_, Name)), .Length = 0, .Capacity = (n)-1 }
#define String16Local(Name, n) 	            wchar MACRO_VAR(CONCAT(Buffer_, Name))[n] = {0}; String16 Name = {.Data = MACRO_VAR(CONCAT(Buffer_, Name)), .Length = 0, .Capacity = (n)-1 }

#define CStr(s)                             (String)         {.Length = String_GetLength(s),   .Data = (char* )(s), .Capacity = 0}
#define CStrView(s)                         (const String)   {.Length = String_GetLength(s),   .Data = (char* )(s), .Capacity = 0}
#define CStr16(s)                           (String16)       {.Length = String16_GetLength(s), .Data = (wchar*)(s), .Capacity = 0}
#define CStr16View(s)                       (const String16) {.Length = String16_GetLength(s), .Data = (wchar*)(s), .Capacity = 0}

#define S(s)                                (const String)   {.Length = sizeof((s))-1, .Data = (char* )((s)), .Capacity = sizeof((s))-1}
#define S16(s)                              (const String16) {.Length = sizeof((s))-1, .Data = (wchar*)((s)), .Capacity = sizeof((s))-1}

#define StrMake(s)                          (String){.Length = (s).Length, .Data = (s).Data, .Capacity = (s).Capacity}
#define StrView(s)                          (const String){.Length = (s).Length, .Data = (char*)(s).Data, .Capacity = (s).Capacity}
#define StrSlice(s, Len)                    (String){.Length = Len, .Data = (char*)(s), .Capacity = Len}
#define StrShiftF(s, Offset)                (String){.Length = (s).Length-((Offset) < (s).Length ? (Offset) : (s).Length), .Data = (s).Data+((Offset) < (s).Length ? (Offset) : (s).Length), .Capacity = (s).Capacity-((Offset) < (s).Capacity ? (Offset) : (s).Capacity)}
#define StrShiftB(s, Offset)                (String){.Length = (s).Length-(Offset), .Data = (s).Data-(Offset), .Capacity = (s).Capacity}

#define Str16Slice(s, Len)                  (String16){.Length = Len, .Data = (wchar*)(s), .Capacity = Len}

#define StrArray(...)                       (StringArray){.List = ((String[]){__VA_ARGS__}), .Num = SArray_Capacity(((String[]){__VA_ARGS__}))}

#define StrFormat                           "%.*s"
#define StrArg(s)                           (i32)(s).Length, (s).Data
