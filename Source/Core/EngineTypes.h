#pragma once

// Unsigned integer types
typedef unsigned char        u8;
typedef unsigned short       u16;
typedef unsigned int         u32;
typedef unsigned long long   u64;

// Signed integer types
typedef signed char          i8;
typedef signed short         i16;
typedef signed int           i32;
typedef signed long long     i64;

// Floating-point types
typedef float                f32;
typedef double               f64;

typedef u16                  wchar;

typedef void VoidFunc(void);

#ifndef __cplusplus
    #if (__STDC_VERSION__ >= 199901L)
        #define bool  _Bool
    #else
        #define bool  u8
    #endif

    #define true  1
    #define false 0
#endif

#ifndef NULL
#define NULL ((void*)0)
#endif

#define _Crash_ do { int* volatile _nptr_ = (int*)1; *_nptr_ = 69; } while (0)

#define INT8_MIN         -127
#define INT16_MIN        -32767
#define INT32_MIN        -2147483647
#define INT64_MIN        -9223372036854775807
#define INT8_MAX         127
#define INT16_MAX        32767
#define INT32_MAX        2147483647
#define INT64_MAX        9223372036854775807

#define UINT8_MAX        255
#define UINT16_MAX       65535
#define UINT32_MAX       4294967295
#define UINT64_MAX       0xFFFFFFFFFFFFFFFF //18446744073709551615

// Stolen from <float.h>
#define FLT_MAX          340282346638528859811704183484516925440.0f //3.402823466e+38F
#define FLT_MAX_10_EXP   38                      // max decimal exponent
#define FLT_MAX_EXP      128                     // max binary exponent

#define DBL_MAX          179769313486231570814527423731704356798070567525844996598917476803157260780028538760589558632766878171540458953514382464234321326889464182768467546703537516986049910576551282076245490090389328944075868508455133942304583236903222948165808559332123348274797826204144723168738177180919299881250404026184124858368.0 //1.7976931348623158e+308 // max value
#define DBL_MAX_10_EXP   308                     // max decimal exponent
#define DBL_MAX_EXP      1024                    // max binary exponent

#define Clamp(Value, Min, Max) ((Value) < (Min)) ? (Min) : ((Value) < (Max)) ? (Value) : (Max)
#define ClampMin(Value, Min) ((Value) < (Min)) ? (Min) : (Value)
#define ClampMax(Value, Max) ((Value) > (Max)) ? (Max) : (Value)

#define Min(A, B) ((A) < (B) ? (A) : (B))
#define Max(A, B) ((A) > (B) ? (A) : (B))

// https://stackoverflow.com/questions/72532179/default-arguments-to-c-macros
// get number of arguments with __NARG__
#define __ARG_N(_1, _2, _3, _4, _5, _6, _7, _8, _9,_10, N, ...) N
#define __RSEQ_N() 11,10,9,8,7,6,5,4,3,2,1,0

#define __NARG_I_(...) __ARG_N(__VA_ARGS__)
#define __NARG__(...)  __NARG_I_(__VA_ARGS__,__RSEQ_N())

#define BITS_PER_LONG (8*sizeof(long))
#define OFF(x) ((x)%BITS_PER_LONG)
#define BIT(x) (1UL<<OFF(x))

#define Kilobytes(x) ((x)*(usize)1000)
#define Megabytes(x) (Kilobytes(x)*(usize)1000)
#define Gigabytes(x) (Megabytes(x)*(usize)1000)

#define Kibibytes(x) ((x)*(usize)1024)
#define Mebibytes(x) (Kibibytes(x)*(usize)1024)
#define Gibibytes(x) (Mebibytes(x)*(usize)1024)

#define SArray_Capacity(Array) sizeof((Array)) / sizeof((Array)[0])

#define STRINGIZE_DETAIL(x) #x
#define STRINGIZE(x) STRINGIZE_DETAIL(x)

#define CONCAT_INNER(a, b) a##b
#define CONCAT(a, b) CONCAT_INNER(a, b)

#define MACRO_VAR(Name) CONCAT(Name, __LINE__)

#define DEFER(Start, End) for (i32 MACRO_VAR(_i_) = (Start, 0); !MACRO_VAR(_i_); (MACRO_VAR(_i_) += 1), End)

#define STRUCT(Name)            typedef struct Name Name; struct Name
#define UNION(Name)             typedef union Name Name; union Name
#define ENUM(Name)              typedef u8 Name; enum
#define ENUM_TYPED(Name, Type)  typedef Type Name; enum

#define INVALID_ID UINT32_MAX

// move to engine.h?
#define TICK_RATE_30  0.03333333333333333333333333333333
#define TICK_RATE_60  0.01666666666666666666666666666667
#define TICK_RATE_120 0.00833333333333333333333333333333

// if only microsoft supported this like clang and gcc :((
//#define each(Element, Array)          (typeof((Array)[0])* CONCAT(Element, _) = &(Array)[0], Element = *CONCAT(Element, _); CONCAT(Element, _) <= &(Array)[Array_Num((Array))-1]; CONCAT(Element, _)++, Element = *CONCAT(Element, _))
//#define each_i(Index, Element, Array) (typeof((Array)[0])* CONCAT(Element, _) = &(Array)[0], Element = *CONCAT(Element, _); CONCAT(Element, _) <= &(Array)[Array_Num((Array))-1]; CONCAT(Element, _)++, Element = *CONCAT(Element, _), ++(Index))

#define each(Type, Element, Array)          (Type* CONCAT(Element, _) = &(Array)[0], Element = *CONCAT(Element, _); CONCAT(Element, _) <= &(Array)[Array_Num((Array))-1]; CONCAT(Element, _)++, Element = *CONCAT(Element, _))
#define each_i(Index, Type, Element, Array) (Type* CONCAT(Element, _) = &(Array)[0], Element = *CONCAT(Element, _); CONCAT(Element, _) <= &(Array)[Array_Num((Array))-1]; CONCAT(Element, _)++, Element = *CONCAT(Element, _), ++(Index))

#define For(Array) for each (It, Array)
#define ForEach(It, Array) for each (It, Array)

#define TArray(Type) Type*
#define SArray(Type, Name, Count) Type Name[Count]; u32 CONCAT(Name, _Count)
#define TMap(KeyType, ValueType) Map

#define global extern
#define internal static
#define thread_local _Thread_local

#define FUNCTION_NAME __func__

// Platform detection
// Rift Build only supports the following:
// - Windows (XP and above)
// - Linux (Debian, Fedora, Red Hat and Arch based only)
// - MacOS (10.5 and above)
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
        #define PLATFORM_STRING "Apple MacOS"
        #define PLATFORM_MAC 1
    #elif defined(__arm__) || defined(__arm64__) || TARGET_OS_IPHONE
        #define PLATFORM_STRING "Apple iOS"
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
    #define __CPU_X86 1
    #define __CACHE_LINE_SIZE 64

    #if defined(__x86_64__) || defined(_M_X64)
        #define __CPU_X64 1
        #define PLATFORM_64_BIT 1
        #define CPU_ARCHITECTURE_STRING "x64"
        #define CPU_ARCHITECTURE_STRING_EX "x64|x86"
    #else
        #define CPU_ARCHITECTURE_STRING "x86"
        #define CPU_ARCHITECTURE_STRING_EX "x86"
        #define PLATFORM_32_BIT 1
    #endif

#elif defined(_M_PPC) || defined(__powerpc__) || defined(__powerpc64__)
    #define __CPU_PPC 1
    #define __CACHE_LINE_SIZE 128
    #define CPU_ARCHITECTURE_STRING "powerpc"
    #define PLATFORM_32_BIT 1

#elif defined(__arm__) || defined(__aarch64__) || defined(_M_ARM) || defined(_M_ARM64)
    #define __CPU_ARM 1
    #define __CACHE_LINE_SIZE 64
    #if defined(__aarch64__) || defined(_M_ARM64)
        #define __CPU_ARM64 1
        #define CPU_ARCHITECTURE_STRING "arm64"
        #define CPU_ARCHITECTURE_STRING_EX "arm64|arm|aarch|aarch64"
        #define PLATFORM_64_BIT 1
    #else
        #define CPU_ARCHITECTURE_STRING "arm"
        #define CPU_ARCHITECTURE_STRING_EX "arm|aarch"
        #define PLATFORM_32_BIT 1
    #endif

#elif defined(__MIPSEL__) || defined(__mips_isa_rev)
    #define __CPU_MIPS 1
    #define __CACHE_LINE_SIZE 64
    #define CPU_ARCHITECTURE_STRING "mips"
    // todo: 64 bit
    #define CPU_ARCHITECTURE_STRING_EX "mips|mipsel|mips64|mips64el"
    #define PLATFORM_32_BIT 1

#elif defined(__loongarch__)
    #define __CPU_LOONGARCH 1
    #define __CACHE_LINE_SIZE 64
    #define PLATFORM_32_BIT 1
    #define CPU_ARCHITECTURE_STRING "loongarch"
    #define CPU_ARCHITECTURE_STRING_EX "loongarch"

#else
    #error Unknown CPU Type
#endif

#if defined(__clang__)
    #define COMPILER_CLANG 1
#elif defined(__GNUC__) || defined(__gcc__)
    #define COMPILER_GCC 1
#elif defined(_MSC_VER)
    #define COMPILER_MSVC 1
#else
    #error Unknown compiler
#endif

// VS Code is retarded
#ifndef __FILE_NAME__
#define __FILE_NAME__ ""
#endif

#define FILELINE __FILE__ " | Line: " STRINGIZE(__LINE__)

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
    #define RIFT_API
#endif // RIFT_STATIC

        
#if COMPILER_CLANG || COMPILER_GCC

    #define PRAGMA_DISABLE_WARNING(x) _Pragma(x)

    #if COMPILER_CLANG
        #define PRAGMA_DISABLE_WARNINGS _Pragma("clang diagnostic push")
        #define PRAGMA_ENABLE_WARNINGS  _Pragma("clang diagnostic pop")

        #define PRAGMA_DISABLE_MISSING_PROTOTYPES_WARNING \
                PRAGMA_DISABLE_WARNINGS \
                PRAGMA_DISABLE_WARNING("clang diagnostic ignored \"-Wmissing-prototypes\"")

    #elif COMPILER_GCC
        #define PRAGMA_DISABLE_WARNINGS _Pragma("GCC diagnostic push")
        #define PRAGMA_ENABLE_WARNINGS  _Pragma("GCC diagnostic pop")

        #define PRAGMA_DISABLE_MISSING_PROTOTYPES_WARNING \
                PRAGMA_DISABLE_WARNINGS \
                PRAGMA_DISABLE_WARNING("GCC diagnostic ignored \"-Wmissing-prototypes\"")
    #endif

    #define DEPRECATED       __attribute__((__deprecated__))
    #define UNUSED           __attribute__((unused))
    #define CONST_ATTRIB     __attribute__((const))
    #define PURE_ATTRIB      __attribute__((pure))
    #define NO_DISCARD       __attribute__((warn_unused_result))
    #define FALL_THROUGH     __attribute__((fallthrough))
    #define NO_RETURN        __attribute__((noreturn))
    #define RETURN_NON_NULL  __attribute__((returns_nonnull))
    #define ASM              __asm__ \

    #define UNLIKELY(Expression) __builtin_expect(!!(Expression), 0)
    #define LIKELY(Expression)   __builtin_expect(!!(Expression), 1)

    #define FORCEINLINE    __attribute__((always_inline)) inline
    #define FORCENOINLINE
    #define read_only 

    #define DEBUG_BREAK __builtin_trap

#elif COMPILER_MSVC
    #define PRAGMA_DISABLE_WARNINGS   __pragma(warning(push))
    #define PRAGMA_ENABLE_WARNINGS    __pragma(warning(pop))
    #define PRAGMA_DISABLE_WARNING(x) __pragma(warning(disable: x))

    #define PRAGMA_DISABLE_DEPRECATION_WARNINGS \
        PRAGMA_DISABLE_WARNINGS \
        PRAGMA_DISABLE_WARNING(4995) /* 'function': name was marked as #pragma deprecated */ \
        PRAGMA_DISABLE_WARNING(4996) /* The compiler encountered a deprecated declaration. */

    #define PRAGMA_DISABLE_MISSING_PROTOTYPES_WARNING PRAGMA_DISABLE_WARNINGS

    #define DEPRECATED       __declspec(deprecated)
    #define UNUSED           
    #define CONST_ATTRIB     
    #define PURE_ATTRIB      
    #define NO_DISCARD       
    #define FALL_THROUGH     
    #define NO_RETURN        
    #define RETURN_NON_NULL  
    #define ASM              __asm

    #define UNLIKELY(Expression) Expression
    #define LIKELY(Expression)   Expression

    #define FORCEINLINE    __forceinline 
    #define FORCENOINLINE  __declspec(noinline)
    #define read_only      __declspec(allocate(".roglob"))

    #define DEBUG_BREAK __debugbreak
#endif

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
#endif

RIFT_API void* MemoryDump(void);

#ifndef ENGINE_GLOBALS
#define ENGINE_GLOBALS
global void* nullptr_z; // points to the engine memory dump
#define nullptr nullptr_z
#endif

#if DEVELOPER

    #if COMPILER_CLANG
    PRAGMA_DISABLE_WARNINGS
    PRAGMA_DISABLE_WARNING("clang diagnostic ignored \"-Wunused-function\"")
    #elif COMPILER_GCC
    PRAGMA_DISABLE_WARNINGS
    PRAGMA_DISABLE_WARNING("GCC diagnostic ignored \"-Wunused-function\"")
    #endif

    FORCEINLINE internal bool __always__(bool bCondition) { if (!bCondition) { DEBUG_BREAK(); } return bCondition; }
    FORCEINLINE internal bool __never__(bool bCondition)  { if (bCondition)  { DEBUG_BREAK(); } return bCondition; }

    #if COMPILER_CLANG || COMPILER_GCC
    PRAGMA_ENABLE_WARNINGS
    #endif

    #define ALWAYS(Expression) __always__(Expression)
    #define NEVER(Expression)  __never__(Expression)
#else
    #define ALWAYS(Expression) Expression
    #define NEVER(Expression)  Expression
#endif

#if PLATFORM_WINDOWS
typedef void* PlatformHandle;
typedef void* PlatformCriticalSection;
typedef void* PlatformPipe[2];
#else
typedef i32 PlatformHandle;
typedef i32 PlatformPipe[2];
typedef void* PlatformCriticalSection;
#endif

#define STATIC_ASSERT(e, Msg) typedef char MACRO_VAR(__C_ASSERT__)[(e) ? 1 : -1]

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

#if PLATFORM_64_BIT
STATIC_ASSERT(sizeof(void*) == 8, "Expected size of a pointer to be 8 bytes.");
#else
STATIC_ASSERT(sizeof(void*) == 4, "Expected size of a pointer to be 4 bytes.");
#endif

#if PLATFORM_64_BIT
typedef u64 uptr;
typedef u64 usize;

#define USIZE_MAX UINT64_MAX
#else
typedef u32 uptr;
typedef u32 usize;

#define USIZE_MAX UINT32_MAX
#endif
