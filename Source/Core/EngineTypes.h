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

typedef u64*                 rawptr;
typedef u16                  wchar;

typedef void VoidFunc(void);

#ifndef __cplusplus
#define bool  _Bool
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

// https://stackoverflow.com/questions/72532179/default-arguments-to-c-macros
#define z__nargs100__(a00,a01,a02,a03,a04,a05,a06,a07,a08,a09,a0a,a0b,a0c,a0d,a0e,a0f,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a1a,a1b,a1c,a1d,a1e,a1f,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a2a,a2b,a2c,a2d,a2e,a2f,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a3a,a3b,a3c,a3d,a3e,a3f,a40,a41,a42,a43,a44,a45,a46,a47,a48,a49,a4a,a4b,a4c,a4d,a4e,a4f,a50,a51,a52,a53,a54,a55,a56,a57,a58,a59,a5a,a5b,a5c,a5d,a5e,a5f,a60,a61,a62,a63,a64,a65,a66,a67,a68,a69,a6a,a6b,a6c,a6d,a6e,a6f,a70,a71,a72,a73,a74,a75,a76,a77,a78,a79,a7a,a7b,a7c,a7d,a7e,a7f,a80,a81,a82,a83,a84,a85,a86,a87,a88,a89,a8a,a8b,a8c,a8d,a8e,a8f,a90,a91,a92,a93,a94,a95,a96,a97,a98,a99,a9a,a9b,a9c,a9d,a9e,a9f,aa0,aa1,aa2,aa3,aa4,aa5,aa6,aa7,aa8,aa9,aaa,aab,aac,aad,aae,aaf,ab0,ab1,ab2,ab3,ab4,ab5,ab6,ab7,ab8,ab9,aba,abb,abc,abd,abe,abf,ac0,ac1,ac2,ac3,ac4,ac5,ac6,ac7,ac8,ac9,aca,acb,acc,acd,ace,acf,ad0,ad1,ad2,ad3,ad4,ad5,ad6,ad7,ad8,ad9,ada,adb,adc,add,ade,adf,ae0,ae1,ae2,ae3,ae4,ae5,ae6,ae7,ae8,ae9,aea,aeb,aec,aed,aee,aef,af0,af1,af2,af3,af4,af5,af6,af7,af8,af9,afa,afb,afc,afd,afe,aff,a100,...)  a100
#define z__nargs__(...) z__nargs100__(,##__VA_ARGS__, ff,fe,fd,fc,fb,fa,f9,f8,f7,f6,f5,f4,f3,f2,f1,f0,ef,ee,ed,ec,eb,ea,e9,e8,e7,e6,e5,e4,e3,e2,e1,e0,df,de,dd,dc,db,da,d9,d8,d7,d6,d5,d4,d3,d2,d1,d0,cf,ce,cd,cc,cb,ca,c9,c8,c7,c6,c5,c4,c3,c2,c1,c0,bf,be,bd,bc,bb,ba,b9,b8,b7,b6,b5,b4,b3,b2,b1,b0,af,ae,ad,ac,ab,aa,a9,a8,a7,a6,a5,a4,a3,a2,a1,a0,9f,9e,9d,9c,9b,9a,99,98,97,96,95,94,93,92,91,90,8f,8e,8d,8c,8b,8a,89,88,87,86,85,84,83,82,81,80,7f,7e,7d,7c,7b,7a,79,78,77,76,75,74,73,72,71,70,6f,6e,6d,6c,6b,6a,69,68,67,66,65,64,63,62,61,60,5f,5e,5d,5c,5b,5a,59,58,57,56,55,54,53,52,51,50,4f,4e,4d,4c,4b,4a,49,48,47,46,45,44,43,42,41,40,3f,3e,3d,3c,3b,3a,39,38,37,36,35,34,33,32,31,30,2f,2e,2d,2c,2b,2a,29,28,27,26,25,24,23,22,21,20,1f,1e,1d,1c,1b,1a,19,18,17,16,15,14,13,12,11,10,f,e,d,c,b,a,9,8,7,6,5,4,3,2,1,0)
#define z__vfn(name, n) name##n
#define z_vfn(name, n)  z__vfn(name, n)
#define vfn(fn, ...)    z_vfn(fn, z__nargs__(__VA_ARGS__))(__VA_ARGS__)

#define BITS_PER_LONG (8*sizeof(long))
#define OFF(x) ((x)%BITS_PER_LONG)
#define BIT(x) (1UL<<OFF(x))

#define Kilobytes(x) ((x)*(u64)1000)
#define Megabytes(x) (Kilobytes(x)*(u64)1000)
#define Gigabytes(x) (Megabytes(x)*(u64)1000)

#define Kibibytes(x) ((x)*(u64)1024)
#define Mebibytes(x) (Kibibytes(x)*(u64)1024)
#define Gibibytes(x) (Mebibytes(x)*(u64)1024)

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

#define UNUSED           __attribute__((unused))
#define CONST_ATTRIB     __attribute__((const))
#define PURE_ATTRIB      __attribute__((pure))
#define NO_DISCARD       __attribute__((warn_unused_result))
#define FALL_THROUGH     __attribute__((fallthrough))
#define NO_RETURN        __attribute__((noreturn))
#define RETURN_NON_NULL  __attribute__((returns_nonnull))

#define INVALID_ID UINT32_MAX

// move to engine.h?
#define TICK_RATE_30  0.03333333333333333333333333333333
#define TICK_RATE_60  0.01666666666666666666666666666667
#define TICK_RATE_120 0.00833333333333333333333333333333

#define each(Element, Array)          (typeof((Array)[0])* CONCAT(Element, _) = &(Array)[0], Element = *CONCAT(Element, _); CONCAT(Element, _) <= &(Array)[Array_Num((Array))-1]; CONCAT(Element, _)++, Element = *CONCAT(Element, _))
#define each_i(Index, Element, Array) (typeof((Array)[0])* CONCAT(Element, _) = &(Array)[0], Element = *CONCAT(Element, _); CONCAT(Element, _) <= &(Array)[Array_Num((Array))-1]; CONCAT(Element, _)++, Element = *CONCAT(Element, _), ++(Index))

#define For(Array) for each (It, Array)
#define ForEach(It, Array) for each (It, Array)

#define TArray(Type) Type*
#define SArray(Type, Name, Count) Type Name[Count]; u32 CONCAT(Name, _Count)
#define TMap(KeyType, ValueType) Map

#define global extern
#define internal static
#define thread_local _Thread_local

#define CONSOLE_FLOAT(Var, ...)
#define CONSOLE_CMD(Name, ...) void CONCAT(CMD__, Name)(StringArray Arguments)

#if defined(__clang__) || defined(__gcc__)
    #define STATIC_ASSERT _Static_assert
    #define typeof        __typeof__

    //#define STATIC_ASSERT(e) typedef char __C_ASSERT__[(e)?1:-1]
#else
    #define STATIC_ASSERT static_assert
    #define typeof 
#endif

// Ensure all types are of the correct size
STATIC_ASSERT(sizeof(bool) == 1, "Expected size of bool to be 1 byte.");
STATIC_ASSERT(sizeof(char) == 1, "Expected size of char to be 1 byte.");

STATIC_ASSERT(sizeof(u8)   == 1, "Expected size of u8 to be 1 byte.");
STATIC_ASSERT(sizeof(u16)  == 2, "Expected size of u16 to be 2 bytes.");
STATIC_ASSERT(sizeof(u32)  == 4, "Expected size of u32 to be 4 bytes.");
STATIC_ASSERT(sizeof(u64)  == 8, "Expected size of u64 to be 8 bytes.");

STATIC_ASSERT(sizeof(i8)   == 1, "Expected size of i8 to be 1 byte.");
STATIC_ASSERT(sizeof(i16)  == 2, "Expected size of i16 to be 2 bytes.");
STATIC_ASSERT(sizeof(i32)  == 4, "Expected size of i32 to be 4 bytes.");
STATIC_ASSERT(sizeof(i64)  == 8, "Expected size of i64 to be 8 bytes.");

STATIC_ASSERT(sizeof(f32)  == 4, "Expected size of f32 to be 4 bytes.");
STATIC_ASSERT(sizeof(f64)  == 8, "Expected size of f64 to be 8 bytes.");

STATIC_ASSERT(sizeof(void*) == 8, "Expected size of a pointer to be 8 bytes.");

// Platform detection
// Rift Engine only supports the following:
// - Windows (64-bit only)
// - Linux (Debian and Arch based only)
// - MacOS (Darwin)
// - Xbox
// - Playstation
// - BSD is only partially supported for console and utility programs (no graphics)

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(_WIN64)
    #define PLATFORM_WINDOWS 1
    #define PLATFORM_STRING "Windows"

    #ifndef _WIN64
    #error "Rift Engine only supports 64-bit platforms"
    #endif
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
    #else
        #error This UNIX operating system is not supported
    #endif
#else
    #define PLATFORM_STRING "Unknown"
    #error Unknown platform
#endif

// CPU arch detection
#if defined(_M_IX86) || defined(_M_X64) || defined(__i386__) || defined(__x86_64__)
    #define __CPU_X86 1
    #define __CACHE_LINE_SIZE 64

    #if defined(__x86_64__) || defined(_M_X64)
        #define __CPU_X64 1
        #define PLATFORM_64_BIT 1
        #define CPU_ARCHITECTURE_STRING "x64"
    #else
        #define CPU_ARCHITECTURE_STRING "x86"
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
        #define PLATFORM_64_BIT 1
    #else
        #define CPU_ARCHITECTURE_STRING "arm"
        #define PLATFORM_32_BIT 1
    #endif

#elif defined(__MIPSEL__) || defined(__mips_isa_rev)
    #define __CPU_MIPS 1
    #define __CACHE_LINE_SIZE 64
    #define CPU_ARCHITECTURE_STRING "mips"
    #define PLATFORM_32_BIT 1

#else
    #error Unknown CPU Type
#endif

#if defined(_MSC_VER)
    #define COMPILER_MSVC 1
#elif defined(__GNUC__)
    #define COMPILER_GCC 1
#elif defined(__clang__)
    #define COMPILER_CLANG 1
#else
    #error Unknown compiler
#endif

#if PLATFORM_WINDOWS
    #define DEBUG_BREAK __debugbreak
#else
    #define DEBUG_BREAK __builtin_trap
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

#ifdef PLATFORM_WINDOWS
    #define FORCEINLINE    __forceinline
    #define FORCENOINLINE  __declspec(noinline)
    #define read_only      __declspec(allocate(".roglob"))
#else
    #define FORCEINLINE    __attribute__((always_inline))
    #define FORCENOINLINE
    #define read_only 
#endif

#define UNLIKELY(Expression) __builtin_expect(!!(Expression), 0)
#define LIKELY(Expression)   __builtin_expect(!!(Expression), 1)

#ifdef __cplusplus
#define C_LINKAGE_BEGIN extern "C" {
#define C_LINKAGE_END }
#define C_LINKAGE extern "C"
#else
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

#define SWAP(A, B) { typeof(A) Temp = (A); (A) = (B); (B) = Temp; }

#if DEVELOPER
    RIFT_API bool __always__(bool bCondition);
    RIFT_API bool __never__(bool bCondition);

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
