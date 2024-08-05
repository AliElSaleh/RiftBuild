// Copyright (c) 2024 Ali El Saleh

#include "Platform.h"
#include "String/StringUtils.h"

#if !COMPILER_MSVC
    #if (__x86_64__ || __i386__)
    #include <cpuid.h>
    #endif
#endif

internal void cpuid(int info[4], int infoType, int subtype)
{
#if __CPU_X86 || __CPU_X64
    #if COMPILER_MSVC
    __cpuidex(info, infoType, subtype);
    #else
    __cpuid_count(infoType, subtype, info[0], info[1], info[2], info[3]);
    #endif
#endif
}

CpuInfo Platform_QueryCPUInfo(void)
{
    CpuInfo Result = {0};

    int info[4] = {0};

    // basic CPUID information
    cpuid(info, 0, 0);
    
    int MaxSupportedIDs = info[0];

    // vendor string
    char vendor[13];
    ((int*)vendor)[0] = info[1];
    ((int*)vendor)[1] = info[3];
    ((int*)vendor)[2] = info[2];
    vendor[12] = '\0';

    const String CpuVendor = CStrEx(vendor, 32);

    Result.Intel = String_Contains(CpuVendor, S("Intel"), false);
    Result.AMD   = String_Contains(CpuVendor, S("AMD"), false);

    // architecture
    #if __CPU_X64
        Result.x64 = 1;
        Result.x86 = 1;
    #elif __CPU_X86
        Result.x86 = 1;
    #elif __CPU_ARM64
        Result.ARM64 = 1;
        Result.ARM   = 1;
    #elif __CPU_ARM
        Result.ARM = 1;
    #elif __CPU_PPC64
        Result.PPC64 = 1;
        Result.PPC   = 1;
    #elif __CPU_PPC
        Result.PPC = 1;
    #endif

    // check for specific instruction sets
    cpuid(info, 1, 0);
    int edx = info[3];
    int ecx = info[2];
    // todo: neon??

    Result.MMX           = (edx & (1 << 23)) ? 1 : 0;
    Result.SSE           = (edx & (1 << 25)) ? 1 : 0;
    Result.SSE2          = (edx & (1 << 26)) ? 1 : 0;
    Result.SSE3          = (ecx & (1 << 0))  ? 1 : 0;
    Result.SSSE3         = (ecx & (1 << 9))  ? 1 : 0;
    Result.SSE4          = (ecx & (1 << 19)) ? 1 : 0;
    Result.SSE41         = (ecx & (1 << 19)) ? 1 : 0;
    Result.SSE42         = (ecx & (1 << 20)) ? 1 : 0;
    Result.AES           = (ecx & (1 << 25)) ? 1 : 0;
    Result.AVX           = (ecx & (1 << 28)) ? 1 : 0;
    Result.FMA3          = (ecx & (1 << 12)) ? 1 : 0;

    // extended features
    if (MaxSupportedIDs >= 7)
    {
        cpuid(info, 7, 0);
        const int ebx = info[1];
        ecx = info[2];
        edx = info[3];

        Result.AVX2            = (ebx & (1 << 5))  ? 1 : 0;
        Result.BMI1            = (ebx & (1 << 3))  ? 1 : 0;
        Result.BMI2            = (ebx & (1 << 8))  ? 1 : 0;
        Result.ADX             = (ebx & (1 << 19)) ? 1 : 0;
        Result.MPX             = (ebx & (1 << 14)) ? 1 : 0;
        Result.SHA             = (ebx & (1 << 29)) ? 1 : 0;
        Result.RDSEED          = (ebx & (1 << 18)) ? 1 : 0;
        Result.PREFETCHWT1     = (ebx & (1 << 0))  ? 1 : 0;
        Result.RDPID           = (ebx & (1 << 22)) ? 1 : 0;
        Result.AVX512F         = (ebx & (1 << 16)) ? 1 : 0;
        Result.AVX512DQ        = (ebx & (1 << 17)) ? 1 : 0;
        Result.AVX512IFMA      = (ebx & (1 << 21)) ? 1 : 0;
        Result.AVX512PF        = (ebx & (1 << 26)) ? 1 : 0;
        Result.AVX512ER        = (ebx & (1 << 27)) ? 1 : 0;
        Result.AVX512CD        = (ebx & (1 << 28)) ? 1 : 0;
        Result.AVX512BW        = (ebx & (1 << 30)) ? 1 : 0;
        Result.AVX512VL        = (ebx & (1 << 31)) ? 1 : 0;
        Result.AVX512          = (ebx & (1 << 16)) || (ebx & (1 << 17)) ||
                                 (ebx & (1 << 21)) || (ebx & (1 << 26)) ||
                                 (ebx & (1 << 27)) || (ebx & (1 << 28)) ||
                                 (ebx & (1 << 30)) || (ebx & (1 << 31)) ? 1 : 0;

        Result.AVX512VBMI      = (ecx & (1 << 1))  ? 1 : 0;
        Result.AVX512VBMI2     = (ecx & (1 << 6))  ? 1 : 0;
        Result.AVX512VPCLMUL   = (ecx & (1 << 10)) ? 1 : 0;
        Result.AVX512VNNI      = (ecx & (1 << 11)) ? 1 : 0;
        Result.AVX512BITALG    = (ecx & (1 << 12)) ? 1 : 0;
        Result.AVX512VPOPCNTDQ = (ecx & (1 << 14)) ? 1 : 0;
        Result.AVX5124VNNIW    = (edx & (1 << 2))  ? 1 : 0;
        Result.AVX5124FMAPS    = (edx & (1 << 3))  ? 1 : 0;
        Result.GFNI            = (ecx & (1 << 8))  ? 1 : 0;
        Result.VAES            = (ecx & (1 << 9))  ? 1 : 0;

        cpuid(info, 7, 1);
        Result.AVX512BF16      = (info[0] & (1 << 5))  ? 1 : 0;
        Result.AVX512FP16      = (info[1] & (1 << 23)) ? 1 : 0;
    }

    return Result;
}
