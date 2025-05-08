#ifndef ENGINE_TYPES_H
#define ENGINE_TYPES_H

// Unsigned integer types
typedef unsigned char      u8;
typedef unsigned short     u16;
typedef unsigned int       u32;
typedef unsigned long long u64;
typedef unsigned long      ulong;

// Signed integer types
typedef signed char        i8;
typedef signed short       i16;
typedef signed int         i32;
typedef signed long long   i64;
typedef signed long        ilong;

// Floating-point types
typedef float              f32;
typedef double             f64;

// Char types
typedef unsigned char      uchar;
typedef unsigned short     wchar;

// Bool types
typedef u8                 b8;
typedef u16                b16;
typedef u32                b32;
typedef u64                b64;

typedef void VoidFunc(void);

// forward declare
typedef struct LinearAllocator LinearAllocator;

#ifdef __cplusplus
#define LANG_CPP 1
#define C_LINKAGE_BEGIN extern "C" {
#define C_LINKAGE_END }
#define C_LINKAGE extern "C"
#else
#define LANG_C 1
#define C_LINKAGE_BEGIN
#define C_LINKAGE_END
#define C_LINKAGE
#define LANG_C_STD_99 __STDC_VERSION__ >= 199901L
#define LANG_C_STD_11 __STDC_VERSION__ >= 201101L
#define LANG_C_STD_17 __STDC_VERSION__ >= 201701L
#endif

#if LANG_C
    #if LANG_C_STD_99
        typedef _Bool bool;
    #else
        typedef u8 bool;
    #endif

    #define true  1
    #define false 0
#endif

#ifndef NULL
#define NULL ((void*)0)
#endif

#define _Crash_ do { i32* volatile _ = (i32*)1; *_*= 69; } while (0)

#define INT8_MIN         -127
#define INT16_MIN        -32767
#define INT32_MIN        -2147483647
#define INT64_MIN        -9223372036854775807
#define INT8_MAX         127
#define INT16_MAX        32767
#define INT32_MAX        2147483647
#define INT64_MAX        9223372036854775807

#define UINT8_MAX        255U
#define UINT16_MAX       65535U
#define UINT32_MAX       4294967295U
#define UINT64_MAX       0xFFFFFFFFFFFFFFFF //18446744073709551615

// Stolen from <float.h>
#define FLT_MAX          340282346638528859811704183484516925440.0f //3.402823466e+38F
#define FLT_MAX_10_EXP   38  // max decimal exponent
#define FLT_MAX_EXP      128 // max binary exponent

#define DBL_MAX          179769313486231570814527423731704356798070567525844996598917476803157260780028538760589558632766878171540458953514382464234321326889464182768467546703537516986049910576551282076245490090389328944075868508455133942304583236903222948165808559332123348274797826204144723168738177180919299881250404026184124858368.0 //1.7976931348623158e+308 // max value
#define DBL_MAX_10_EXP   308  // max decimal exponent
#define DBL_MAX_EXP      1024 // max binary exponent

#define Clamp(Value, Min, Max) (((Value) < (Min)) ? (Min) : ((Value) < (Max)) ? (Value) : (Max))
#define ClampMin(Value, Min)   (((Value) < (Min)) ? (Min) : (Value))
#define ClampMax(Value, Max)   (((Value) > (Max)) ? (Max) : (Value))

#define Min(A, B) ((A) < (B) ? (A) : (B))
#define Max(A, B) ((A) > (B) ? (A) : (B))

// https://stackoverflow.com/questions/72532179/default-arguments-to-c-macros
// get number of arguments with __NARG__
#define __ARG_N(_1, _2, _3, _4, _5, _6, _7, _8, _9,_10, N, ...) N
#define __RSEQ_N() 11,10,9,8,7,6,5,4,3,2,1,0

#define __NARG_I_(...) __ARG_N(__VA_ARGS__)
#define __NARG__(...)  __NARG_I_(__VA_ARGS__,__RSEQ_N())

#define BITS_PER_LONG (8 * sizeof(u64))
//#define BIT(x)        (1UL << ((x) % BITS_PER_LONG))
#define BIT(x)        (1UL << x##UL)
#define BITX(x)        (1UL << (x))


#define Kilobytes(x) ((x)*(usize)1000)
#define Megabytes(x) (Kilobytes(x)*(usize)1000)
#define Gigabytes(x) (Megabytes(x)*(usize)1000)

#define Kibibytes(x) ((x)*(usize)1024)
#define Mebibytes(x) (Kibibytes(x)*(usize)1024)
#define Gibibytes(x) (Mebibytes(x)*(usize)1024)

#define SArray_Capacity(Array) sizeof((Array)) / sizeof((Array)[0])

#define STRINGIFY_INNER(x) #x
#define STRINGIFY(x) STRINGIFY_INNER(x)

#define CONCAT_INNER(a, b) a##b
#define CONCAT(a, b) CONCAT_INNER(a, b)

#define MACRO_VAR(Name) CONCAT(Name, __LINE__)

#define DEFER(Start, End) for (i32 MACRO_VAR(_i_) = (Start, 0); !MACRO_VAR(_i_); (MACRO_VAR(_i_) += 1), End)

#define STRUCT(Name)            typedef struct Name Name; struct Name
#define UNION(Name)             typedef union Name Name; union Name
#define ENUM(Name)              typedef u8 Name; enum
#define ENUM_TYPED(Name, Type)  typedef Type Name; enum

STRUCT(String)
{
    uchar* Data;
    u32   Length;
    u32   Capacity;
};

STRUCT(String16)
{
    wchar* Data;
    u32    Length;
    u32    Capacity;
};

STRUCT(StringArray)
{
    String* List;
    u32     Num;
    u32     IterIndex;
    void*   IterCurrent;
};

STRUCT(StringList)
{
    String String;
    struct StringList* Next;
};

#define each_str(Element, Array)            (const String* (Element) = StringArray_Iterate_Begin(&(Array)); (Element) != NULL; (Element) = StringArray_Iterate_Next(&(Array)))
#define each_str_i(Index, Element, Array)   (const String* (Element) = StringArray_Iterate_Begin(&(Array)); (Element) != NULL; (Element) = StringArray_Iterate_Next(&(Array)), Index+=1)
#define each_str_list(List)                 (StringList It = List; StringList_Iterate_Check(It); (It) = StringList_Iterate_Next(It))
#define each_str_list_i(Index, List)        (StringList It = List; StringList_Iterate_Check(It); (It) = StringList_Iterate_Next(It), Index+=1)
#define each_str_list_it(Element, List)     (StringList Element = List; StringList_Iterate_Check(Element); Element = StringList_Iterate_Next(Element))
#define each_string_in_list(x)              each_str_list(x)

#define StringN(n)  		                struct { uchar Data[n]; u32 Length; u32 Capacity; }

#define StringLocal(Name, n) 	            u8    MACRO_VAR(CONCAT(Buffer_, Name))[n+1] = {0}; String   Name; Name.Data = (uchar*)MACRO_VAR(CONCAT(Buffer_, Name)); Name.Length = 0; Name.Capacity = (n)
#define String16Local(Name, n) 	            wchar MACRO_VAR(CONCAT(Buffer_, Name))[n+1] = {0}; String16 Name; Name.Data = (wchar*)MACRO_VAR(CONCAT(Buffer_, Name)), Name.Length = 0, Name.Capacity = (n)

#define CStr(s)                             (String)         {.Data = (uchar*)(s),      .Length = String_GetLength(s),             .Capacity = 0}
#define CStrEx(s, n)                        (String)         {.Data = (uchar*)(s),      .Length = String_GetLength_Ex(s, n),       .Capacity = 0}
#define CStrView(s)                         (const String)   {.Data = (uchar*)(s),      .Length = String_GetLength(s),             .Capacity = 0}
#define CStr16(s)                           (String16)       {.Data = (wchar*)(s),      .Length = String16_GetLength((wchar*)(s)), .Capacity = 0}
#define CStr16View(s)                       (const String16) {.Data = (wchar*)(s),      .Length = String16_GetLength(s),           .Capacity = 0}

#define S(s)                                (const String)   {.Data = (uchar*)(s),      .Length = sizeof(s)-1, .Capacity = 0}
#define SC(s)                                                {.Data = (uchar*)(s),      .Length = sizeof(s)-1, .Capacity = 0}
#define S16(s)                              (const String16) {.Data = (wchar*)(s),      .Length = sizeof(s)-1, .Capacity = 0}

#define StrMake(s)                          (String)         {.Data = (s).Data,         .Length = (s).Length, .Capacity = (s).Capacity}
#define StrView(s)                          (const String)   {.Data = (uchar*)(s).Data, .Length = (s).Length, .Capacity = (s).Capacity}
#define Str16Slice(s, Len)                  (String16)       {.Data = (wchar*)(s),      .Length = Len,        .Capacity = Len}

#define StrArray(...)                       (StringArray)    {.List = ((String[]){__VA_ARGS__}), .Num = SArray_Capacity(((String[]){__VA_ARGS__}))}

#define StrFormat                           "%.*s"
#define StrArg(s)                           (i32)(s).Length, (s).Data


#define INVALID_ID UINT32_MAX

// if only microsoft supported typeof() like clang and gcc :(( aaaaarrgghhhhh
//#define each(Element, Array)          (typeof((Array)[0])* CONCAT(Element, _) = &(Array)[0], Element = *CONCAT(Element, _); CONCAT(Element, _) <= &(Array)[Array_Num((Array))-1]; CONCAT(Element, _)++, Element = *CONCAT(Element, _))
//#define each_i(Index, Element, Array) (typeof((Array)[0])* CONCAT(Element, _) = &(Array)[0], Element = *CONCAT(Element, _); CONCAT(Element, _) <= &(Array)[Array_Num((Array))-1]; CONCAT(Element, _)++, Element = *CONCAT(Element, _), ++(Index))

#define each(Type, Element, Array)          (Type* CONCAT(Element, _) = (Type*)&(Array)[0], Element = *CONCAT(Element, _); CONCAT(Element, _) <= &(Array)[Array_Num((Array))-1]; CONCAT(Element, _)+=1, Element = *CONCAT(Element, _))
#define each_i(Index, Type, Element, Array) (Type* CONCAT(Element, _) = (Type*)&(Array)[0], Element = *CONCAT(Element, _); CONCAT(Element, _) <= &(Array)[Array_Num((Array))-1]; CONCAT(Element, _)+=1, Element = *CONCAT(Element, _), (Index)+=1)

#define each_static(Type, Element, Array)        (Type* CONCAT(Element, _) = (Type*)&(Array)[0], Element = *CONCAT(Element, _); CONCAT(Element, _) <= &(Array)[SArray_Capacity((Array))-1]; CONCAT(Element, _)+=1, Element = *CONCAT(Element, _))
#define each_static_i(Index, Type, Element, Array) (Type* CONCAT(Element, _) = (Type*)&(Array)[0], Element = *CONCAT(Element, _); CONCAT(Element, _) <= &(Array)[SArray_Capacity((Array))-1]; CONCAT(Element, _)+=1, Element = *CONCAT(Element, _), (Index)+=1)

#define For(Array) for each (It, Array)
#define ForEach(It, Array) for each (It, Array)

#define EachElement(Index, Array) (u32 Index = 0; Index < SArray_Capacity(Array); Index++)

// this actually kinda works? lol
#define is   ==
#define isnt !=
#define and  &&
#define or   ||
#define not  !

#define TArray(Type) Type*
#define SArray(Type, Name, Count) Type Name[Count]; u32 CONCAT(Name, _Count)
#define TMap(KeyType, ValueType)

#define SLinkedList_Push(List, Entry) \
                        *(List) = Entry; \
                        List = &(*(List))->Next

#define global extern
//#define internal static
#define local_persist static
#define thread_local _Thread_local

#define UNUSED_PARAM(Param) (void)Param

#define FUNCTION_NAME __func__

// Platform detection
// Rift Build only supports the following:
// - Windows (7 and above)
// - Linux (Debian, Fedora, Red Hat and Arch based only)
// - macOS (10.10 and above)
// - BSD (FreeBSD, NetBSD, OpenBSD)

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(_WIN64)
    #define PLATFORM_WINDOWS 1
    #define PLATFORM_STRING "Windows"
#elif defined(__ANDROID__)
    #define PLATFORM_ANDROID 1
    #define PLATFORM_STRING "Android"
#elif defined(__FreeBSD__) || defined(__FreeBSD_kernel__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__bsdi__)
    #define PLATFORM_BSD 1
    #define PLATFORM_UNIX 1

    #if defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
        #define PLATFORM_FREE_BSD 1
        #define PLATFORM_STRING "FreeBSD"
    #elif defined(__NetBSD__)
        #define PLATFORM_NET_BSD 1
        #define PLATFORM_STRING "NetBSD"
    #elif defined(__OpenBSD__)
        #define PLATFORM_OPEN_BSD 1
        #define PLATFORM_STRING "OpenBSD"
    #elif defined(__bsdi__)
        #define PLATFORM_BSDI 1
        #define PLATFORM_STRING "BSD/OS"
    #else
        #error This BSD operating system is not supported
    #endif
#elif defined(__APPLE__)
    #define PLATFORM_APPLE 1
    #define PLATFORM_UNIX 1

    #if defined(__MACH__)
        #define PLATFORM_STRING "macOS"
        #define PLATFORM_MAC 1
    #elif defined(__arm__) || defined(__arm64__) || TARGET_OS_IPHONE
        #define PLATFORM_STRING "iOS"
        #define PLATFORM_IOS 1
        #if TARGET_IPHONE_SIMULATOR
            #define PLATFORM_IOS_SIMULATOR 1
        #endif
    #else
        #define PLATFORM_STRING "Apple"
        #error This Apple operating system is not supported
    #endif
#elif defined(__ORBIS__)
    #define PLATFORM_PLAYSTATION 1
    #define PLATFORM_PS4 1
    #define PLATFORM_STRING "PS4"
#elif defined(_XBOX_ONE)
    #define PLATFORM_XBOX 1
    #define PLATFORM_XBOX_ONE 1
    #define PLATFORM_STRING "XboxOne"
#elif defined(__SWITCH__)
    #define PLATFORM_SWITCH 1
    #define PLATFORM_STRING "Switch"
#elif defined(__unix__)
    #define PLATFORM_UNIX 1
    #if defined(__linux__) || defined(__gnu_linux__)
        #define PLATFORM_LINUX 1
        #define PLATFORM_STRING "Linux"
    
    #if defined(__gnome__)
        #define PLATFORM_LINUX_GNOME 1
    #elif defined(__kde__)
        #define PLATFORM_LINUX_KDE 1
    #elif defined(__cinnamon__)
        #define PLATFORM_LINUX_CINNAMON 1
    #endif
    #else
        #error This UNIX operating system is not supported
    #endif
#else
    #define PLATFORM_STRING "Unknown"
    #error Unknown platform
#endif

// CPU arch detection
// https://github.com/cpredef/predef/blob/master/Architectures.md
#if defined(_M_IX86) || defined(_M_X64) || defined(__i386__) || defined(__x86_64__)
    #define CPU_X86 1
    #define CACHE_LINE_SIZE 64

    #if defined(__x86_64__) || defined(_M_X64)
        #define CPU_X64 1
        #define PLATFORM_64_BIT 1
        #define CPU_ARCHITECTURE_STRING "x64"
        #define CPU_ARCHITECTURE_STRING_EX "x64|x86"
    #else
        #define CPU_ARCHITECTURE_STRING "x86"
        #define CPU_ARCHITECTURE_STRING_EX "x86"
        #define PLATFORM_32_BIT 1
    #endif

#elif defined(_M_PPC) || defined(__powerpc__) || defined(__powerpc64__)
    #define CPU_PPC 1
    #define CACHE_LINE_SIZE 128

    #if defined(__powerpc64__)
        #define CPU_PPC64 1
        #define PLATFORM_64_BIT 1
        #define CPU_ARCHITECTURE_STRING_EX "ppc64"
        #define CPU_ARCHITECTURE_STRING_EX "powerpc|ppc|ppc64"
    #else
        #define PLATFORM_32_BIT 1
        #define CPU_ARCHITECTURE_STRING "ppc"
        #define CPU_ARCHITECTURE_STRING_EX "powerpc|ppc"
    #endif 

#elif defined(__arm__) || defined(__aarch64__) || defined(_M_ARM) || defined(_M_ARM64)
    #define CPU_ARM 1

    #if defined(__aarch64__) || defined(_M_ARM64)
        #define CPU_ARM64 1
        #define CPU_ARCHITECTURE_STRING "arm64"
        #define CPU_ARCHITECTURE_STRING_EX "arm64|arm|aarch|aarch64"
        #define PLATFORM_64_BIT 1
        #define CACHE_LINE_SIZE 128
    #else
        #define CPU_ARCHITECTURE_STRING "arm"
        #define CPU_ARCHITECTURE_STRING_EX "arm|aarch"
        #define PLATFORM_32_BIT 1
        #define CACHE_LINE_SIZE 64
    #endif

#else
    #error Unknown CPU Type
#endif

#if defined(__clang__)
    #define COMPILER_CLANG 1
    #if defined(_MSC_VER)
    #define COMPILER_CLANG_MSVC 1
    #endif
#elif defined(__GNUC__) || defined(__gcc__)
    #define COMPILER_GCC 1
    #if defined(_MSC_VER)
    #define COMPILER_GCC_MSVC 1
    #endif
#elif defined(_MSC_VER)
    #define COMPILER_MSVC 1
#else
    #error Unknown compiler
#endif

// VS Code is retarded
#ifndef __FILE_NAME__
#define __FILE_NAME__ __FILE__
#endif

#define FILELINE __FILE__ " | Line: " STRINGIFY(__LINE__)

// Export and Import
#ifndef RIFT_STATIC
    #ifdef RIFT_EXPORT
        #ifdef PLATFORM_WINDOWS
        #define RIFT_API __declspec(dllexport)
        #else
        #define RIFT_API __attribute__((visibility("default")))
        #endif
    #else
        #ifdef PLATFORM_WINDOWS
        #define RIFT_API __declspec(dllimport)
        #else
        #define RIFT_API
        #endif
    #endif
#else
    #define RIFT_API extern
#endif // RIFT_STATIC

#ifdef _MSC_VER
#define MSVC_SECTION(x)   __declspec(allocate(x))
#define MSVC_ATTRIBUTE(x) __declspec(x)
#define MSVC_PRAGMA(x)    _Pragma(STRINGIFY(x))
#else
#define MSVC_SECTION(x)
#define MSVC_ATTRIBUTE(x)
#define MSVC_PRAGMA(x)
#endif

#ifdef __GNUC__
#define GCC_SECTION(x)   __attribute__((__section__(x)))
#define GCC_ATTRIBUTE(x) __attribute__((x))
#define GCC_PRAGMA(x)    _Pragma(STRINGIFY(x))
#else
#define GCC_SECTION(x)
#define GCC_ATTRIBUTE(x)
#define GCC_PRAGMA(x)
#endif

#define SECTION(x) \
    MSVC_SECTION(x) \
    GCC_SECTION(x)

#ifdef _MSC_VER
#pragma section(".roglob", read)
#endif

// Old: GCC_SECTION(".roglob,\"l\",@progbits#")

#define read_only SECTION(".roglob")

#if COMPILER_CLANG || COMPILER_GCC

    #define PRAGMA_DISABLE_WARNING(x) _Pragma(x)

    #if COMPILER_CLANG
        #define PRAGMA_DISABLE_WARNINGS _Pragma("clang diagnostic push")
        #define PRAGMA_ENABLE_WARNINGS  _Pragma("clang diagnostic pop")

        #define PRAGMA_DISABLE_SIGN_CONVERSION_WARNING \
                PRAGMA_DISABLE_WARNINGS \
                PRAGMA_DISABLE_WARNING("clang diagnostic ignored \"-Wsign-conversion\"")

        #define PRAGMA_DISABLE_MISSING_PROTOTYPES_WARNING \
                PRAGMA_DISABLE_WARNINGS \
                PRAGMA_DISABLE_WARNING("clang diagnostic ignored \"-Wmissing-prototypes\"")

        #define PRAGMA_DISABLE_PADDING_WARNINGS \
                PRAGMA_DISABLE_WARNINGS

    #elif COMPILER_GCC
        #define PRAGMA_DISABLE_WARNINGS _Pragma("GCC diagnostic push")
        #define PRAGMA_ENABLE_WARNINGS  _Pragma("GCC diagnostic pop")

        #define PRAGMA_DISABLE_SIGN_CONVERSION_WARNING \
                PRAGMA_DISABLE_WARNINGS \
                PRAGMA_DISABLE_WARNING("GCC diagnostic ignored \"-Wsign-conversion\"")

        #define PRAGMA_DISABLE_MISSING_PROTOTYPES_WARNING \
                PRAGMA_DISABLE_WARNINGS \
                PRAGMA_DISABLE_WARNING("GCC diagnostic ignored \"-Wmissing-prototypes\"")

        #define PRAGMA_DISABLE_PADDING_WARNINGS \
                PRAGMA_DISABLE_WARNINGS
    #endif

    #define DEPRECATED       __attribute__((__deprecated__))
    #define UNUSED           __attribute__((unused))
    #define CONST_FN         __attribute__((const))
    #define PURE_FN          __attribute__((pure))
    #define NO_DISCARD       __attribute__((warn_unused_result))
    #define FALL_THROUGH     __attribute__((fallthrough))
    #define NO_RETURN        __attribute__((noreturn))
    #define RETURN_NON_NULL  __attribute__((returns_nonnull))
    #define FORCEINLINE      __attribute__((always_inline)) inline
    #define FORCENOINLINE
    #define ASM              __asm__ \

    #define UNLIKELY(Expression) __builtin_expect(!!(Expression), 0)
    #define LIKELY(Expression)   __builtin_expect(!!(Expression), 1)

    #define DEBUG_BREAK()  __builtin_trap()

#elif COMPILER_MSVC
    #define PRAGMA_DISABLE_WARNINGS   __pragma(warning(push))
    #define PRAGMA_ENABLE_WARNINGS    __pragma(warning(pop))
    #define PRAGMA_DISABLE_WARNING(x) __pragma(warning(disable: x))

    #define PRAGMA_DISABLE_SIGN_CONVERSION_WARNING \
        PRAGMA_DISABLE_WARNINGS \

    #define PRAGMA_DISABLE_DEPRECATION_WARNINGS \
        PRAGMA_DISABLE_WARNINGS \
        PRAGMA_DISABLE_WARNING(4995) /* 'function': name was marked as #pragma deprecated */ \
        PRAGMA_DISABLE_WARNING(4996) /* The compiler encountered a deprecated declaration. */

    #define PRAGMA_DISABLE_MISSING_PROTOTYPES_WARNING PRAGMA_DISABLE_WARNINGS

    #define PRAGMA_DISABLE_PADDING_WARNINGS \
        PRAGMA_DISABLE_WARNINGS \
        PRAGMA_DISABLE_WARNING(4820)

    #include <sal.h>

    #define DEPRECATED       __declspec(deprecated)
    #define UNUSED           
    #define CONST_FN     
    #define PURE_FN         
    #define NO_DISCARD       _Check_return_
    #define FALL_THROUGH     
    #define NO_RETURN        __declspec(noreturn)
    #define RETURN_NON_NULL  _Ret_notnull_
    #define ASM              __asm
    #define FORCEINLINE      __forceinline
    #define FORCENOINLINE    __declspec(noinline)

    #define UNLIKELY(Expression) Expression
    #define LIKELY(Expression)   Expression

    extern void __nop(void);
    #define DEBUG_BREAK() (__nop(), __debugbreak())
#endif 

#if DEVELOPER
    #if COMPILER_CLANG
        PRAGMA_DISABLE_WARNINGS
        PRAGMA_DISABLE_WARNING("clang diagnostic ignored \"-Wunused-function\"")
    #elif COMPILER_GCC
        PRAGMA_DISABLE_WARNINGS
        PRAGMA_DISABLE_WARNING("GCC diagnostic ignored \"-Wunused-function\"")
    #endif

    FORCEINLINE static bool __always__(bool bCondition) { if (!bCondition) { DEBUG_BREAK(); } return bCondition; }
    FORCEINLINE static bool __never__(bool bCondition)  { if (bCondition)  { DEBUG_BREAK(); } return bCondition; }

    #if COMPILER_CLANG || COMPILER_GCC
        PRAGMA_ENABLE_WARNINGS
    #endif

    // the || (Expression) is only here for msvc's /analyze flag, as it trips up about "dereferencing null pointers" sometimes
    #define ALWAYS(Expression) (__always__(Expression) || (Expression))
    #define NEVER(Expression)  (__never__(Expression)  || (Expression))
#else
    #define ALWAYS(Expression) Expression
    #define NEVER(Expression)  Expression
#endif

#ifdef NO_ASSERT
    #define ASSERT(Expression)
    #define ENSURE(Expression)
    #define UNREACHABLE()
    #define TODO()
#else
    #define ASSERT(Expression, ...) do { if (Expression) {} else { ##__VA_ARGS__; DEBUG_BREAK(); _Crash_; } } while (0)
    #define ENSURE(Expression, ...) do { if (Expression) {} else { ##__VA_ARGS__; DEBUG_BREAK(); } } while (0)
    #define UNREACHABLE()           do { DEBUG_BREAK(); _Crash_; } while (0)
    #define TODO()                  DEBUG_BREAK()
#endif

#define STATIC_PURE_FN(...) static __VA_ARGS__ PURE_FN; static __VA_ARGS__ 
#define STATIC_CONST_FN(...) static __VA_ARGS__ CONST_FN; static __VA_ARGS__ 

#define STATIC_ASSERT(e, Msg) typedef uchar MACRO_VAR(__C_ASSERT__)[(e) ? 1 : -1]

// drop support for typeof because of MSVC :(
/*
#if COMPILER_CLANG || COMPILER_GCC
    #define typeof  __typeof__
#else
    #define typeof  typeof
#endif

#define SWAP(A, B) do { typeof(A) Temp = (A); (A) = (B); (B) = Temp; } while (0)
*/

// Ensure all types are of the correct size
STATIC_ASSERT(sizeof(bool)  == 1, "Expected size of bool to be 1 byte.");
STATIC_ASSERT(sizeof(char)  == 1, "Expected size of char to be 1 byte.");
STATIC_ASSERT(sizeof(uchar) == 1, "Expected size of uchar to be 1 byte.");
STATIC_ASSERT(sizeof(wchar) == 2, "Expected size of wchar to be 2 bytes.");
STATIC_ASSERT(sizeof(u8)    == 1, "Expected size of u8 to be 1 byte.");
STATIC_ASSERT(sizeof(u16)   == 2, "Expected size of u16 to be 2 bytes.");
STATIC_ASSERT(sizeof(u32)   == 4, "Expected size of u32 to be 4 bytes.");
STATIC_ASSERT(sizeof(u64)   == 8, "Expected size of u64 to be 8 bytes.");
STATIC_ASSERT(sizeof(i8)    == 1, "Expected size of i8 to be 1 byte.");
STATIC_ASSERT(sizeof(i16)   == 2, "Expected size of i16 to be 2 bytes.");
STATIC_ASSERT(sizeof(i32)   == 4, "Expected size of i32 to be 4 bytes.");
STATIC_ASSERT(sizeof(i64)   == 8, "Expected size of i64 to be 8 bytes.");
STATIC_ASSERT(sizeof(f32)   == 4, "Expected size of f32 to be 4 bytes.");
STATIC_ASSERT(sizeof(f64)   == 8, "Expected size of f64 to be 8 bytes.");
STATIC_ASSERT(sizeof(ulong) == 4, "Expected size of ulong to be 4 bytes.");
STATIC_ASSERT(sizeof(ilong) == 4, "Expected size of ilong to be 4 bytes.");

#if PLATFORM_WINDOWS
typedef void* PlatformHandle;
typedef void* PlatformCriticalSection;
typedef void* PlatformPipe[2];
#else
typedef i32 PlatformHandle;
typedef i32 PlatformPipe[2];
typedef void* PlatformCriticalSection;
#endif

#if PLATFORM_64_BIT
STATIC_ASSERT(sizeof(void*) == 8, "Expected size of a pointer to be 8 bytes.");
#else
STATIC_ASSERT(sizeof(void*) == 4, "Expected size of a pointer to be 4 bytes.");
#endif

#if PLATFORM_64_BIT
typedef u64 uptr;
typedef i64 isize;
typedef u64 usize;

#define USIZE_MAX UINT64_MAX
#else
typedef u32 uptr;
typedef i32 isize;
typedef u32 usize;

#define USIZE_MAX UINT32_MAX
#endif // PLATFORM_64_BIT

#if PLATFORM_WINDOWS
    #define MAX_PATH_LENGTH 260
    #define MAX_PATH_LENGTH_EX 32767
    #define PATH_SEPARATOR '\\'
#elif PLATFORM_LINUX
    #define MAX_PATH_LENGTH 4096
    #define MAX_PATH_LENGTH_EX 4096
    #define PATH_SEPARATOR '/'
#elif PLATFORM_APPLE
    #define MAX_PATH_LENGTH 1024
    #define MAX_PATH_LENGTH_EX 1024
    #define PATH_SEPARATOR '/'
#elif PLATFORM_BSD
    #define MAX_PATH_LENGTH 1024
    #define MAX_PATH_LENGTH_EX 1024
    #define PATH_SEPARATOR '/'
#else
    #define MAX_PATH_LENGTH 1024
    #define MAX_PATH_LENGTH_EX 1024
    #define PATH_SEPARATOR '/'
#endif

read_only global String GString_Null;

#endif // ENGINE_TYPES_H
