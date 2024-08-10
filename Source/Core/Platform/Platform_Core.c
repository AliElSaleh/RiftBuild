// Copyright (c) 2024 Ali El Saleh

#include "Platform.h"
#include "String/StringUtils.h"
#include "Log.h"

#if __CPU_X86 || __CPU_X64
#if !COMPILER_MSVC
#include <cpuid.h>
#endif

internal void cpuid(int info[4], int infoType, int subtype)
{
    #if COMPILER_MSVC
    __cpuidex(info, infoType, subtype);
    #else
    __cpuid_count(infoType, subtype, info[0], info[1], info[2], info[3]);
    #endif
}

RIFT_API u32 Platform_GetCpuCacheLineSize(void)
{
    // todo: do this dynamically
    return __CACHE_LINE_SIZE;
}

bool Platform_GetCpuBrandName(String* OutName)
{
    int info[4] = {0};
    cpuid(info, 0, 0);

    char vendor[13] = {0};
    ((int*)vendor)[0] = info[1];
    ((int*)vendor)[1] = info[3];
    ((int*)vendor)[2] = info[2];
    vendor[12] = '\0';

    String_Copy(OutName, CStrEx(vendor, 12));
    return true;
}

CpuInfo Platform_QueryCPUInfo(void)
{
    CpuInfo Result = {0};

    int info[4] = {0};

    // basic CPUID information
    cpuid(info, 0, 0);
    
    int MaxSupportedIDs = info[0];

    // vendor string
    char vendor[13] = {0};
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
    #endif

    // check for specific instruction sets
    cpuid(info, 1, 0);
    int edx = info[3];
    int ecx = info[2];

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
    Result.RDRAND        = (ecx & (1 << 30)) ? 1 : 0;

    // TODO: test all these holy moly
    /*
    flags           : fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm pbe syscall nx 
                      pdpe1gb rdtscp lm constant_tsc art arch_perfmon pebs bts rep_good nopl xtopology nonstop_tsc cpuid aperfmperf pni pclmulqdq dtes64 
                      monitor ds_cpl vmx smx est tm2 ssse3 sdbg fma cx16 xtpr pdcm pcid sse4_1 sse4_2 x2apic movbe popcnt tsc_deadline_timer aes xsave avx 
                      f16c rdrand lahf_lm abm 3dnowprefetch cpuid_fault epb ssbd ibrs ibpb stibp ibrs_enhanced tpr_shadow flexpriority ept vpid ept_ad 
                      fsgsbase tsc_adjust sgx bmi1 avx2 smep bmi2 erms invpcid mpx rdseed adx smap clflushopt intel_pt xsaveopt xsavec xgetbv1 xsaves dtherm 
                      ida arat pln pts hwp hwp_notify hwp_act_window hwp_epp vnmi md_clear flush_l1d arch_capabilities
    */

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

#elif __CPU_ARM || __CPU_ARM64

#if PLATFORM_APPLE || PLATFORM_BSD
#include <sys/types.h>
#include <sys/sysctl.h>

internal inline bool IsSysAttributeSet(const char* Name)
{
    i64 Ret = 0;
    size_t Size = sizeof(i64);
    i32 Result = sysctlbyname(Name, &Ret, &Size, NULL, 0);
    if (Result == -1)
    {
        return false;
    }

    return Ret != 0;
}
#endif

CpuInfo Platform_QueryCPUInfo(void)
{
    CpuInfo Result = {0};

    // architecture
    #if __CPU_ARM64
    Result.ARM64 = 1;
    Result.ARM   = 1;
    #elif __CPU_ARM
    Result.ARM = 1;
    #endif

    #if PLATFORM_APPLE
    Result.Apple = 1;
    #else
    #error "TODO: Intel/AMD"
    #endif

    #if PLATFORM_APPLE || PLATFORM_BSD
    Result.NEON               = IsSysAttributeSet("hw.optional.neon");
    Result.NEON_HPFP          = IsSysAttributeSet("hw.optional.neon_hpfp");
    Result.NEON_FP16          = IsSysAttributeSet("hw.optional.neon_fp16");
    Result.ARMV8_1_ATOMICS    = IsSysAttributeSet("hw.optional.armv8_1_atomics");
    Result.ARMV8_2_FHM        = IsSysAttributeSet("hw.optional.armv8_2_fhm");
    Result.ARMV8_2_SHA512     = IsSysAttributeSet("hw.optional.armv8_2_sha512");
    Result.ARMV8_2_SHA3       = IsSysAttributeSet("hw.optional.armv8_2_sha3");
    Result.ARMV8_3_COMPNUM    = IsSysAttributeSet("hw.optional.armv8_3_compnum");
    Result.ARMV8_CRC32        = IsSysAttributeSet("hw.optional.armv8_crc32");
    Result.ARMV8_GPI          = IsSysAttributeSet("hw.optional.armv8_gpi");
    Result.AdvSIMD            = IsSysAttributeSet("hw.optional.AdvSIMD");
    Result.AdvSIMD_HPFPCVT    = IsSysAttributeSet("hw.optional.AdvSIMD_HPFPCvt");
    Result.UCNORMAL_MEM       = IsSysAttributeSet("hw.optional.ucnormal_mem");
    Result.FLAGM              = IsSysAttributeSet("hw.optional.arm.FEAT_FlagM");
    Result.FLAGM2             = IsSysAttributeSet("hw.optional.arm.FEAT_FlagM2");
    Result.FLAGM3             = IsSysAttributeSet("hw.optional.arm.FEAT_FlagM3");
    Result.FLAGM4             = IsSysAttributeSet("hw.optional.arm.FEAT_FlagM4");
    Result.FHM                = IsSysAttributeSet("hw.optional.arm.FEAT_FHM");
    Result.DOTPROD            = IsSysAttributeSet("hw.optional.arm.FEAT_DotProd");
    Result.SHA3               = IsSysAttributeSet("hw.optional.arm.FEAT_SHA3");
    Result.RDM                = IsSysAttributeSet("hw.optional.arm.FEAT_RDM");
    Result.LSE                = IsSysAttributeSet("hw.optional.arm.FEAT_LSE");
    Result.SHA256             = IsSysAttributeSet("hw.optional.arm.FEAT_SHA256");
    Result.SHA512             = IsSysAttributeSet("hw.optional.arm.FEAT_SHA512");
    Result.SHA1               = IsSysAttributeSet("hw.optional.arm.FEAT_SHA1");
    Result.AES                = IsSysAttributeSet("hw.optional.arm.FEAT_AES");
    Result.PMULL              = IsSysAttributeSet("hw.optional.arm.FEAT_PMULL");
    Result.SPECRES            = IsSysAttributeSet("hw.optional.arm.FEAT_SPECRES");
    Result.SB                 = IsSysAttributeSet("hw.optional.arm.FEAT_SB");
    Result.FRINTTS            = IsSysAttributeSet("hw.optional.arm.FEAT_FRINTTS");
    Result.LRCPC              = IsSysAttributeSet("hw.optional.arm.FEAT_LRCPC");
    Result.LRCPC2             = IsSysAttributeSet("hw.optional.arm.FEAT_LRCPC2");
    Result.FCMA               = IsSysAttributeSet("hw.optional.arm.FEAT_FCMA");
    Result.JSCVT              = IsSysAttributeSet("hw.optional.arm.FEAT_JSCVT");
    Result.PAUTH              = IsSysAttributeSet("hw.optional.arm.FEAT_PAuth");
    Result.PAUTH2             = IsSysAttributeSet("hw.optional.arm.FEAT_PAuth2");
    Result.FPAC               = IsSysAttributeSet("hw.optional.arm.FEAT_FPAC");
    Result.DPB                = IsSysAttributeSet("hw.optional.arm.FEAT_DPB");
    Result.DPB2               = IsSysAttributeSet("hw.optional.arm.FEAT_DPB2");
    Result.BF16               = IsSysAttributeSet("hw.optional.arm.FEAT_BF16");
    Result.I8MM               = IsSysAttributeSet("hw.optional.arm.FEAT_I8MM");
    Result.ECV                = IsSysAttributeSet("hw.optional.arm.FEAT_ECV");
    Result.LSE2               = IsSysAttributeSet("hw.optional.arm.FEAT_LSE2");
    Result.CSV2               = IsSysAttributeSet("hw.optional.arm.FEAT_CSV2");
    Result.CSV3               = IsSysAttributeSet("hw.optional.arm.FEAT_CSV3");
    Result.DIT                = IsSysAttributeSet("hw.optional.arm.FEAT_DIT");
    Result.FP16               = IsSysAttributeSet("hw.optional.arm.FEAT_FP16");
    Result.SSBS               = IsSysAttributeSet("hw.optional.arm.FEAT_SSBS");
    Result.BTI                = IsSysAttributeSet("hw.optional.arm.FEAT_BTI");
    #endif

    return Result;
}

#elif __CPU_PPC || __CPU_PPC64

CpuInfo Platform_QueryCPUInfo(void)
{
    CpuInfo Result = {0};

    return Result;
}

#endif
