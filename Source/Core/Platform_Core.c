// Copyright (c) Artisan Softworks
// Licensed under the BSD 3-Clause License. See the LICENSE file for details.

#ifndef UNITY_BUILD
#include "Platform.h"
#include "Clock.h"
#include "StringUtils.h"
#include "Filesystem.h"
#include "Uuid.h"
#endif

#if PLATFORM_LINUX
#include <features.h> // defines __GLIBC__ when building against glibc (absent on musl)
#endif

read_only FileHandle   g_FileHandle = { .Data = &(u8[64]){0} };
read_only PlatformPipe g_PipeNil    = { PLATFORM_PIPE_INVALID, PLATFORM_PIPE_INVALID };

#if CPU_X86 || CPU_X64

#if COMPILER_CLANG || COMPILER_GCC
#include <cpuid.h>
#endif

#if COMPILER_TCC
u64 __fixunsdfdi(f64 x)
{
    return x < 0 ? 0ULL : (u64)x;
}

f32 __floatundisf(u64 x)
{
    return (float)x;
}
#endif

/*
static read_only String GX86ExtensionsTable_ECX_Lvl1[] =
{
    S("SSE3"), S("PCLMULQDQ"), S("DTES64"),  S("MONITOR"), S("DS-CPL"), S("VMX"),    S("SMX"),       S("EIST"),
    S("TM2"),  S("SSSE3"),     S("CNXT-ID"), S("SDBG"),    S("FMA"),    S("CX16"),   S("XTPR"),      S("PDCM"), S(""),
    S("PCID"), S("DCA"),       S("SSE4.1"),  S("SSE4.2"),  S("X2APIC"), S("MOVBE"),  S("POPCNT"),    S("TSC-Deadline"),
    S("AES"),  S("XSAVE"),     S("OSXSAVE"), S("AVX"),     S("F16C"),   S("RDRAND"), S("HyperVisor")
};

static read_only String GX86ExtensionsTable_EDX_Lvl1[] =
{
    S("FPU"),    S("VME"),    S("DE"),   S("PSE"),     S("TSC"),  S("MSR"), S("PAE"),  S("MCE"),
    S("CX8"),    S("APIC"),   S(""),     S("SEP"),     S("MTRR"), S("PGE"), S("MCA"),  S("CMOV"),
    S("PAT"),    S("PSE-36"), S("PSN"),  S("CLFLUSH"), S(""),     S("DS"),  S("ACPI"), S("MMX"),
    S("FXSR"),   S("SSE"),    S("SSE2"), S("SS"),      S("HTT"),  S("TM"),  S(""),     S("PBE")
};

static read_only String GX86ExtensionsTable_EBX_Lvl7[] =
{
    S("FSGSBASE"), S("TSC-ADJUST"), S("SGX"),      S("BMI1"),     S("HLE"),      S("AVX2"),       S("FDP-Exceptn-Only"), S("SMEP"),
    S("BMI2"),     S("ERMS"),       S("INVPCID"),  S("RTM"),      S("PQM"),      S("FPU-DEPR"),   S("MPX"),              S("PQE"),
    S("AVX512F"),  S("AVX512DQ"),   S("RDSEED"),   S("ADX"),      S("SMAP"),     S("AVX512IFMA"), S("PCOMMIT"),          S("CLFLUSHOPT"),
    S("CLWB"),     S("IntelPT"),    S("AVX512PF"), S("AVX512ER"), S("AVX512CD"), S("SHA"),        S("AVX512BW"),         S("AVX512VL")
};

static read_only String GX86ExtensionsTable_ECX_Lvl7[] =
{
    S("PREFETCHWT1"), S("AVX512-VBMI"), S("UMIP"),        S("PKU"),           S("OSPKE"),            S("WAITPKG"),     S("AVX512VBMI2"),      S("CET_SS"),
    S("GFNI"),        S("VAES"),        S("VPCLMULQDQ"),  S("AVX512-VNNI"),   S("AVX512-BITALG"),    S("TME"),         S("AVX512-VPOPCNTDQ"), S("LA57"),
    S(""),            S(""),            S(""),            S(""),              S(""),                 S(""),            S(""),                 S("RDPID"),
    S("KL"),          S("CLDEMOTE"),    S("MOVDIRI"),     S("MOVDIR64B"),     S("ENQCMD"),           S("SGX-LC"),      S("PKS"),              S("BUSLOCKDETECT")
};

static read_only String GX86ExtensionsTable_EDX_Lvl7[] =
{
    S(""), S(""), S("AVX512_4VNNIW"), S("AVX512_4FMAPS")
};
*/

#if COMPILER_MSVC
extern void __cpuidex(i32 cpuInfo[4], i32 function_id, i32 subfunction_id);
#endif

static void cpuid(i32 info[4], i32 infoType, i32 subtype)
{
    #if COMPILER_MSVC
    __cpuidex(info, infoType, subtype);
    #elif COMPILER_CLANG || COMPILER_GCC
    __cpuid_count(infoType, subtype, info[0], info[1], info[2], info[3]);
    #else
    __asm__("cpuid"
            : "=a"(info[0]), "=b"(info[1]), "=c"(info[2]), "=d"(info[3])
            : "a"(infoType), "c"(subtype));
    #endif
}

#if PLATFORM_WINDOWS || PLATFORM_LINUX
RIFT_API NO_DISCARD u32 Platform_GetCpuCacheLineSize(void)
{
    i32 info[4] = {0};
    cpuid(info, 1, 0);

    // EBX bits [15:8] = CLFLUSH line size in 8-byte units
    u32 LineSize = ((info[1] >> 8) & 0xFF) * 8;

    return LineSize;
}

NO_DISCARD bool Platform_GetCpuBrandName(String* OutName)
{
    i32 info[4] = {0};
    cpuid(info, 0, 0);

    char vendor[13] = {0};
    ((i32*)vendor)[0] = info[1];
    ((i32*)vendor)[1] = info[3];
    ((i32*)vendor)[2] = info[2];
    vendor[12] = '\0';

    String_Copy(OutName, CStrEx(vendor, 12));
    return true;
}
#endif

NO_DISCARD CpuInfo Platform_QueryCPUInfo(void)
{
    CpuInfo Result = {0};

    i32 info[4] = {0};

    // basic CPUID information
    cpuid(info, 0, 0);
    
    i32 MaxSupportedIDs = info[0];

    // vendor string
    char vendor[13] = {0};
    ((i32*)vendor)[0] = info[1];
    ((i32*)vendor)[1] = info[3];
    ((i32*)vendor)[2] = info[2];
    vendor[12] = '\0';

    const String CpuVendor = CStrEx(vendor, 32);

    Result.Intel = String_Contains(CpuVendor, S("Intel"), false);
    Result.AMD   = String_Contains(CpuVendor, S("AMD"), false);

    // architecture
    #if CPU_X64
        Result.x64 = 1;
        Result.x86 = 1;
    #elif CPU_X86
        Result.x86 = 1;
    #endif

    // check for specific instruction sets
    cpuid(info, 1, 0);
    // EAX: Processor Info and Feature Bits
    // EBX: Additional Information (e.g., Brand Index, CLFLUSH line size)
    // ECX: Feature Flags
    // EDX: Feature Flags
    u32 ecx = (u32)info[2];
    u32 edx = (u32)info[3];

    Result.MMX           = BIT_TEST(edx, 23);
    Result.SSE           = BIT_TEST(edx, 25);
    Result.SSE2          = BIT_TEST(edx, 26);
    Result.SSE3          = BIT_TEST(ecx, 0);
    Result.SSSE3         = BIT_TEST(ecx, 9);
    Result.SSE4          = BIT_TEST(ecx, 19);
    Result.SSE41         = BIT_TEST(ecx, 19);
    Result.SSE42         = BIT_TEST(ecx, 20);
    Result.AES           = BIT_TEST(ecx, 25);
    Result.AVX           = BIT_TEST(ecx, 28);
    Result.F16C          = BIT_TEST(ecx, 29);
    Result.FMA           = BIT_TEST(ecx, 12);
    Result.FMA3          = BIT_TEST(ecx, 12);
    Result.RDRAND        = BIT_TEST(ecx, 30);
    Result.PCLMULQDQ     = BIT_TEST(ecx, 1);
    Result.DTES64        = BIT_TEST(ecx, 2);
    Result.MONITOR       = BIT_TEST(ecx, 3);
    Result.DSCPL         = BIT_TEST(ecx, 4);
    Result.VMX           = BIT_TEST(ecx, 5);
    Result.SMX           = BIT_TEST(ecx, 6);
    Result.EIST          = BIT_TEST(ecx, 7);
    Result.TM2           = BIT_TEST(ecx, 8);
    Result.CNXTID        = BIT_TEST(ecx, 10);
    Result.SDBG          = BIT_TEST(ecx, 11);
    Result.CX16          = BIT_TEST(ecx, 13);
    Result.XTPR          = BIT_TEST(ecx, 14);
    Result.PDCM          = BIT_TEST(ecx, 15);
    Result.PCID          = BIT_TEST(ecx, 17);
    Result.DCA           = BIT_TEST(ecx, 18);
    Result.X2APIC        = BIT_TEST(ecx, 21);
    Result.MOVBE         = BIT_TEST(ecx, 22);
    Result.POPCNT        = BIT_TEST(ecx, 23);
    Result.TSCDEADLINE   = BIT_TEST(ecx, 24);
    Result.XSAVE         = BIT_TEST(ecx, 26);
    Result.OSXSAVE       = BIT_TEST(ecx, 27);
    Result.HYPERVISOR    = BIT_TEST(ecx, 31);

    Result.FPU           = BIT_TEST(edx, 0);
    Result.VME           = BIT_TEST(edx, 1);
    Result.DE            = BIT_TEST(edx, 2);
    Result.PSE           = BIT_TEST(edx, 3);
    Result.TSC           = BIT_TEST(edx, 4);
    Result.MSR           = BIT_TEST(edx, 5);
    Result.PAE           = BIT_TEST(edx, 6);
    Result.MCE           = BIT_TEST(edx, 7);
    Result.CX8           = BIT_TEST(edx, 8);
    Result.APIC          = BIT_TEST(edx, 9);
    Result.SEP           = BIT_TEST(edx, 11);
    Result.MTRR          = BIT_TEST(edx, 12);
    Result.PGE           = BIT_TEST(edx, 13);
    Result.MCA           = BIT_TEST(edx, 14);
    Result.CMOV          = BIT_TEST(edx, 15);
    Result.PAT           = BIT_TEST(edx, 16);
    Result.PSE36         = BIT_TEST(edx, 17);
    Result.PSN           = BIT_TEST(edx, 18);
    Result.CLFLUSH       = BIT_TEST(edx, 19);
    Result.DS            = BIT_TEST(edx, 21);
    Result.ACPI          = BIT_TEST(edx, 22);
    Result.FXSR          = BIT_TEST(edx, 24);
    Result.SS            = BIT_TEST(edx, 27);
    Result.HTT           = BIT_TEST(edx, 28);
    Result.TM            = BIT_TEST(edx, 29);
    Result.PBE           = BIT_TEST(edx, 31);


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

        const u32 ebx = (u32)info[1];
        Result.AVX2             = BIT_TEST(ebx, 5);
        Result.BMI1             = BIT_TEST(ebx, 3);
        Result.TZCNT            = BIT_TEST(ebx, 3);
        Result.BMI2             = BIT_TEST(ebx, 8);
        Result.ADX              = BIT_TEST(ebx, 19);
        Result.MPX              = BIT_TEST(ebx, 14);
        Result.SHA              = BIT_TEST(ebx, 29);
        Result.RDSEED           = BIT_TEST(ebx, 18);
        Result.RDPID            = BIT_TEST(ebx, 23);
        Result.AVX512F          = BIT_TEST(ebx, 16);
        Result.AVX512DQ         = BIT_TEST(ebx, 17);
        Result.AVX512IFMA       = BIT_TEST(ebx, 21);
        Result.AVX512PF         = BIT_TEST(ebx, 26);
        Result.AVX512ER         = BIT_TEST(ebx, 27);
        Result.AVX512CD         = BIT_TEST(ebx, 28);
        Result.AVX512BW         = BIT_TEST(ebx, 30);
        Result.AVX512VL         = BIT_TEST(ebx, 31);
        Result.AVX512           = BIT_TEST(ebx, 16) || BIT_TEST(ebx, 17) ||
                                  BIT_TEST(ebx, 21) || BIT_TEST(ebx, 26) ||
                                  BIT_TEST(ebx, 27) || BIT_TEST(ebx, 28) ||
                                  BIT_TEST(ebx, 30) || BIT_TEST(ebx, 31);
        Result.FSGSBASE         = BIT_TEST(ebx, 0);
        Result.TSCADJUST        = BIT_TEST(ebx, 1);
        Result.SGX              = BIT_TEST(ebx, 2);
        Result.HLE              = BIT_TEST(ebx, 4);
        Result.FDP_EXCEPTN_ONLY = BIT_TEST(ebx, 6);
        Result.SMEP             = BIT_TEST(ebx, 7);
        Result.ERMS             = BIT_TEST(ebx, 9);
        Result.INVPCID          = BIT_TEST(ebx, 10);
        Result.RTM              = BIT_TEST(ebx, 11);
        Result.PQM              = BIT_TEST(ebx, 12);
        Result.FPU_DEPR         = BIT_TEST(ebx, 13);
        Result.PQE              = BIT_TEST(ebx, 15);
        Result.SMAP             = BIT_TEST(ebx, 20);
        Result.PCOMMIT          = BIT_TEST(ebx, 22);
        Result.CLFLUSHOPT       = BIT_TEST(ebx, 23);
        Result.CLWB             = BIT_TEST(ebx, 24);
        Result.INTELPT          = BIT_TEST(ebx, 25);

        ecx = (u32)info[2];
        Result.PREFETCHWT1      = BIT_TEST(ecx, 0);
        Result.AVX512VBMI       = BIT_TEST(ecx, 1);
        Result.AVX512VBMI2      = BIT_TEST(ecx, 6);
        Result.AVX512VPCLMUL    = BIT_TEST(ecx, 10);
        Result.AVX512VNNI       = BIT_TEST(ecx, 11);
        Result.AVX512BITALG     = BIT_TEST(ecx, 12);
        Result.AVX512VPOPCNTDQ  = BIT_TEST(ecx, 14);
        Result.GFNI             = BIT_TEST(ecx, 8);
        Result.VAES             = BIT_TEST(ecx, 9);
        Result.UMIP             = BIT_TEST(ecx, 2);
        Result.PKU              = BIT_TEST(ecx, 3);
        Result.OSPKE            = BIT_TEST(ecx, 4);
        Result.WAITPKG          = BIT_TEST(ecx, 5);
        Result.CET_SS           = BIT_TEST(ecx, 7);
        Result.VPCLMULQDQ       = BIT_TEST(ecx, 10);
        Result.TME              = BIT_TEST(ecx, 13);
        Result.LA57             = BIT_TEST(ecx, 15);
        Result.KL               = BIT_TEST(ecx, 24);
        Result.CLDEMOTE         = BIT_TEST(ecx, 25);
        Result.MOVDIRI          = BIT_TEST(ecx, 26);
        Result.MOVDIR64B        = BIT_TEST(ecx, 27);
        Result.ENQCMD           = BIT_TEST(ecx, 28);
        Result.SGXLC            = BIT_TEST(ecx, 30);
        Result.BUSLOCKDETECT    = BIT_TEST(ecx, 31);

        edx = (u32)info[3];
        Result.AVX5124VNNIW     = BIT_TEST(edx, 2);
        Result.AVX5124FMAPS     = BIT_TEST(edx, 3);

        cpuid(info, 7, 1);
        Result.AVX512BF16       = BIT_TEST(info[0], 5);
        Result.AVX512FP16       = BIT_TEST(info[1], 23);

        PRAGMA_DISABLE_SIGN_CONVERSION_WARNING

        cpuid(info, 0x80000001U, 0);
        Result.SVM             = BIT_TEST(info[2], 2);   // AMD-V (Secure Virtual Machine)
        Result.LZCNT           = BIT_TEST(info[2], 5);
        Result.SSE4A           = BIT_TEST(info[2], 6);

        PRAGMA_ENABLE_WARNINGS
    }

    return Result;
}

#elif CPU_ARM || CPU_ARM64

// TODO: linux and windows?

#if PLATFORM_APPLE || PLATFORM_BSD
#include <sys/types.h>
#include <sys/sysctl.h>

static inline bool IsSysAttributeSet(const char* Name)
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
    #if CPU_ARM64
    Result.ARM64 = 1;
    Result.ARM   = 1;
    #elif CPU_ARM
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

#elif CPU_PPC || CPU_PPC64

CpuInfo Platform_QueryCPUInfo(void)
{
    CpuInfo Result = {0};

    return Result;
}

#endif

#if PLATFORM_WINDOWS && PLATFORM_32_BIT
PRAGMA_DISABLE_MISSING_PROTOTYPES_WARNING
/*
u64 _aulldiv(u64 Numerator, u64 Denominator)
{
    if (Numerator == 0) return 0;
    
    if (Denominator == 0)
    {
        return Numerator; // Indicate error (undefined behavior)
    }

    u64 Result = 0;
    u64 Remainder = 0;

    // Loop over each bit of the numerator
    for (i8 i = 63; i >= 0; i--)
    {
        // Shift remainder to the left and add the next bit of the numerator
        Remainder = (Remainder << 1) | ((Numerator >> i) & 1);

        // If the remainder is greater than or equal to the denominator, we can subtract
        if (Remainder >= Denominator)
        {
            Remainder -= Denominator;
            Result |= (1 << i);
        }
    }

    return Result;
}

i64 _alldiv(i64 Numerator, i64 Denominator)
{
    if (Numerator == 0) return 0;

    if (Denominator == 0)
    {
        return Numerator; // Division by zero is undefined
    }

    i64 Quotient = 0;
    i64 Remainder = 0;

    // We need to handle the case where the numerator is negative or positive.
    // We also need to handle cases where the denominator is negative or positive.
    i64 AbsNumerator = (Numerator < 0) ? -Numerator : Numerator;
    i64 AbsDenominator = (Denominator < 0) ? -Denominator : Denominator;
    i64 Sign = ((Numerator < 0) ^ (Denominator < 0)) ? -1 : 1;

    // Loop over each bit of the numerator
    for (i8 i = 63; i >= 0; i--)
    {
        // Shift remainder to the left and add the next bit of the numerator
        Remainder = (Remainder << 1) | ((AbsNumerator >> i) & 1);

        // If the remainder is greater than or equal to the denominator, we can subtract
        if (Remainder >= AbsDenominator)
        {
            Remainder -= AbsDenominator;
            Quotient |= (1LL << i);
        }
    }

    return Quotient * Sign;
}

i64 _allrem(i64 Numerator, i64 Denominator)
{
    if (Numerator == 0) return 0;

    if (Denominator == 0)
    {
        return Numerator; // Division by zero is undefined
    }

    i64 Remainder = 0;

    // We need to handle the case where the numerator is negative or positive.
    // We also need to handle cases where the denominator is negative or positive.
    i64 AbsNumerator = (Numerator < 0) ? -Numerator : Numerator;
    i64 AbsDenominator = (Denominator < 0) ? -Denominator : Denominator;
    i64 Sign = (Numerator < 0) ? -1 : 1;

    // Loop over each bit of the numerator
    for (i8 i = 63; i >= 0; i--)
    {
        // Shift remainder to the left and add the next bit of the numerator
        Remainder = (Remainder << 1) | ((AbsNumerator >> i) & 1);

        // If the remainder is greater than or equal to the denominator, we can subtract
        if (Remainder >= AbsDenominator)
        {
            Remainder -= AbsDenominator;
        }
    }

    if (Sign < 0)
    {
        return -Remainder;
    }

    return Remainder;
}

u64 _aullrem(u64 Numerator, u64 Denominator)
{
    if (Numerator == 0) return 0;
    if (Denominator == 0)
    {
        // Handle division by zero as appropriate for your use case
        return Numerator;
    }

    u64 Remainder = 0;

    // Perform bit-by-bit division
    for (i32 i = 63; i >= 0; i--)
    {
        Remainder = (Remainder << 1) | ((Numerator >> i) & 1);

        if (Remainder >= Denominator)
        {
            Remainder -= Denominator;
        }

        //LOG("Bit: %d, Remainder: %llu\n", i, Remainder);
    }

    return Remainder;
}
*/

PRAGMA_ENABLE_WARNINGS
#endif

///////////// Clock /////////////

void Clock_Start(Clock* C)
{
    C->StartTime = Platform_GetAbsoluteTime();
    C->ElapsedTime = 0.0;
}

void Clock_Stop(Clock* C)
{
    C->StartTime = 0;
}

void Clock_Tick(Clock* C)
{
    C->ElapsedTime = Platform_GetAbsoluteTime() - C->StartTime;
}

void Clock_TickAndPrint(Clock* C)
{
    C->ElapsedTime = Platform_GetAbsoluteTime() - C->StartTime;

    Clock_PrintElapsedTime(C, true);
}

NO_DISCARD f64 Clock_GetElapsedTime(const Clock* C, bool bAutoConvertTimeUnit)
{
    f64 ElapsedTime;
    if (bAutoConvertTimeUnit)
    {
        ElapsedTime = Time_AutoConvert(C->ElapsedTime);
    }
    else
    {
        ElapsedTime = C->ElapsedTime;
    }
    
    return ElapsedTime;
}

void Clock_GetElapsedTime_ToString(const Clock* C, bool bAutoConvertTimeUnit, String* OutString)
{
    Time_ToString(C->ElapsedTime, bAutoConvertTimeUnit, OutString);
}

void Clock_GetElapsedTime_ToStringEx(const Clock* C, bool bAutoConvertTimeUnit, String* OutString, const String Format)
{
    Time_ToStringEx(C->ElapsedTime, bAutoConvertTimeUnit, OutString, Format);
}

NO_DISCARD f64 Clock_GetElapsedTime_Milliseconds(const Clock* C)
{
    return C->ElapsedTime * 1000.0;
}

NO_DISCARD f64 Clock_GetElapsedTime_Microseconds(const Clock* C)
{
    return C->ElapsedTime * 1000000.0;
}

NO_DISCARD f64 Clock_GetElapsedTime_Nanoseconds(const Clock* C)
{
    return C->ElapsedTime * 1000000000.0;
}

NO_DISCARD f64 Time_AutoConvert(f64 Seconds)
{
    f64 FinalTime = 0.0;

    // Nanosecond detection
    // less than 1us and greater than 1ns
    if (Seconds >= 0.000000001 && Seconds < 0.000001)
    {
        FinalTime = Seconds * 1000000000.0;
    }

    // Microsecond detection
    // less than 1ms and greater than 1us
    if (Seconds >= 0.000001 && Seconds < 0.001)
    {
        FinalTime = Seconds * 1000000.0;
    }

    // Millisecond detection
    // greater than 1ms and less than 1s
    if (Seconds >= 0.001 && Seconds < 1.0)
    {
        FinalTime = Seconds * 1000.0;
    }
    
    return FinalTime;
}

void Time_ToString(f64 Seconds, bool bAutoConvertTimeUnit, String* OutString)
{
    f64 TimeAdjusted;
    u8 TimeUnit[4] = {0};

    TimeAdjusted = Seconds;
    TimeUnit[0] = 's';
    u8 Len = 1;

    if (bAutoConvertTimeUnit)
    {
        // Nanosecond detection
        // less than 1us and greater than 1ns
        if (Seconds >= 0.000000001 && Seconds < 0.000001)
        {
            TimeAdjusted = Seconds * 1000000000.0;

            TimeUnit[0] = 'n';
            TimeUnit[1] = 's';
            Len = 2;
        }

        // Microsecond detection
        // less than 1ms and greater than 1us
        if (Seconds >= 0.000001 && Seconds < 0.001)
        {
            TimeAdjusted = Seconds * 1000000.0;

            TimeUnit[0] = 'u';
            TimeUnit[1] = 's';
            Len = 2;
        }

        // Millisecond detection
        // greater than 1ms and less than 1s
        if (Seconds >= 0.001 && Seconds < 1.0)
        {
            TimeAdjusted = Seconds * 1000.0;

            TimeUnit[0] = 'm';
            TimeUnit[1] = 's';
            Len = 2;
        }
    }

    String_Format(OutString, S("%f%S"), TimeAdjusted, StrSlice(TimeUnit, Len));
}

void Time_ToStringEx(f64 Seconds, bool bAutoConvertTimeUnit, String* OutString, const String Format)
{
    f64 TimeAdjusted;
    u8 TimeUnit[4] = {0};

    TimeAdjusted = Seconds;
    TimeUnit[0] = 's';
    u8 Len = 1;

    if (bAutoConvertTimeUnit)
    {
        // Nanosecond detection
        // less than 1us and greater than 1ns
        if (Seconds >= 0.000000001 && Seconds < 0.000001)
        {
            TimeAdjusted = Seconds * 1000000000.0;

            TimeUnit[0] = 'n';
            TimeUnit[1] = 's';
            Len = 2;
        }

        // Microsecond detection
        // less than 1ms and greater than 1us
        if (Seconds >= 0.000001 && Seconds < 0.001)
        {
            TimeAdjusted = Seconds * 1000000.0;

            TimeUnit[0] = 'u';
            TimeUnit[1] = 's';
            Len = 2;
        }

        // Millisecond detection
        // greater than 1ms and less than 1s
        if (Seconds >= 0.001 && Seconds < 1.0)
        {
            TimeAdjusted = Seconds * 1000.0;

            TimeUnit[0] = 'm';
            TimeUnit[1] = 's';
            Len = 2;
        }
    }

    StringLocal(TimeFormat, 32);
    String a = S("%S");
    String_Concat(&TimeFormat, Format, a);
    String_Format(OutString, TimeFormat, TimeAdjusted, StrSlice(TimeUnit, Len));
}

void Clock_PrintElapsedTime(const Clock* C, bool bAutoConvertTimeUnit)
{
    StringLocal(Time, 64);
    Clock_GetElapsedTime_ToString(C, bAutoConvertTimeUnit, &Time);

    Platform_ConsoleWrite_CustomLength((char*)Time.Data, Time.Length, 0, false);
    Platform_ConsoleWrite_CustomLength("\n", 1, 0, false);
}

///////////// Filesystem /////////////

NO_DISCARD bool IsValidFileHandle(const FileHandle Handle)
{
    bool bValid = Handle.Data != NULL;

    if (Handle.Data == g_FileHandle.Data)
    {
        bValid = false;
    }
    
    return bValid;
}

NO_DISCARD bool Filesystem_IsNewer(const String PathA, const String PathB)
{
    usize a = Filesystem_GetLastWriteTime(PathA);
    usize b = Filesystem_GetLastWriteTime(PathB);
    return a > b;
}

NO_DISCARD bool Filesystem_IsOlder(const String PathA, const String PathB)
{
    usize a = Filesystem_GetLastWriteTime(PathA);
    usize b = Filesystem_GetLastWriteTime(PathB);
    return a < b;
}

NO_DISCARD bool Filesystem_DoesPathHaveFileExtension(const String Path)
{
    u32 LastDot = 0, LastSlash = 0;
    bool bHasDot = String_IndexOfLastChar(Path, '.', &LastDot);
    bool bHasSlash = String_IndexOfLastPathSlash(Path, &LastSlash);

    bool bSomeCharAfterDot = false;
    if (bHasDot && LastDot+1 < Path.Length)
    {
        u8 C = Path.Data[LastDot+1];
        bSomeCharAfterDot = IsAlphabet(C) || IsDigit(C);
    }

    bool bDotAfterSlash = bHasDot && (!bHasSlash || LastDot > LastSlash);
    bool bSuccess = bDotAfterSlash && bSomeCharAfterDot;

    return bSuccess;
}

// Returns the path of where this file lives
// with the option of including the last slash or not
// Example:
//  Case 1
//   Input:  path/to/some/file.txt
//   Output: path/to/some
//  Case 2
//   Input:  hello.txt
//   Output: <empty> since there is no path slashes present in the input string

NO_DISCARD String Filesystem_ExtractFilePath(const String Path, bool bIncludeSlash)
{
    String Final = String_Null();

    u32 LastSlash = 0;
    if (String_IndexOfLastPathSlash(Path, &LastSlash))
    {
        // edge case for something like:
        //   /somefile.txt
        // where "/" is the first one and bIncludeSlash is false
        // we want to return "/" and not an empty string, so as to not confuse the user
        // that no "path" was extracted
        if (LastSlash == 0 && !bIncludeSlash)
        {
            LastSlash++;
        }
        
        Final = StrSlice(Path.Data, bIncludeSlash ? LastSlash+1 : LastSlash);
    }

    return Final;
}

// Returns the file name of the given path
// Example:
//   Input:  path/to/some/file.txt
//   Output: file or file.txt
//   with the option of including the extension or not
NO_DISCARD String Filesystem_ExtractFileName(const String Path, bool bIncludeExtension)
{
    String FileName = Path;

    u32 LastSlash = 0;
    xx String_IndexOfLastPathSlash(Path, &LastSlash);
    if (LastSlash)
    {
        FileName = StrShiftF(Path, LastSlash+1);
    }

    if (!bIncludeExtension)
    {
        u32 LastDot = 0;
        if (String_IndexOfLastChar(FileName, '.', &LastDot))
        {
            FileName = StrSlice(FileName.Data, LastDot);
        }
    }

    return FileName;
}

NO_DISCARD String Filesystem_StripFileExtension(const String FilePath)
{
    String Final = FilePath;

    u32 LastDot = 0;
    if (String_IndexOfLastChar(FilePath, '.', &LastDot))
    {
        Final = StrSlice(FilePath.Data, LastDot);
    }

    return Final;
}

NO_DISCARD String Filesystem_ExtractFileExtension(const String FilePath, bool bIncludeDot)
{
    String Final = String_Null();

    u32 LastDot = 0;
    if (String_IndexOfLastChar(FilePath, '.', &LastDot))
    {
        Final = StrShiftF(FilePath, bIncludeDot ? LastDot : LastDot+1);
    }

    return Final;
}

void Filesystem_AppendExeExtension(String* FilePathNoExt)
{
    #if PLATFORM_WINDOWS
    if (!String_EndsWith(*FilePathNoExt, S(".exe"), false))
    {
        String_Append(FilePathNoExt, S(".exe"));
    }
    #endif
}

// True when any component of Path is exactly ".."
NO_DISCARD bool Filesystem_HasDotDotComponent(const String Path)
{
    bool bFound = false;

    u32 Start = 0;
    for (u32 i = 0; i <= Path.Length && !bFound; i++)
    {
        bool bAtSeparator = i == Path.Length || Path.Data[i] == '/' || Path.Data[i] == '\\';
        if (bAtSeparator)
        {
            if (i - Start == 2 && Path.Data[Start] == '.' && Path.Data[Start+1] == '.')
            {
                bFound = true;
            }

            Start = i+1;
        }
    }

    return bFound;
}

// A path that names a filesystem root: "/" (or any run of separators) and
// drive designators like "C:", "C:/", "C:\".
NO_DISCARD bool Filesystem_IsRootPath(const String Path)
{
    String Trimmed = Path;
    xx String_EatPathSeparatorsInlineFromEnd(&Trimmed);

    bool bRoot = false;

    if (Path.Length > 0 && Trimmed.Length == 0)
    {
        bRoot = true;
    }
    else if (Trimmed.Length == 2 && Trimmed.Data[1] == ':' && IsAlphabet(Trimmed.Data[0]))
    {
        bRoot = true;
    }

    return bRoot;
}

// True when Child names the same directory as Parent or anything nested below it.
// The comparison is lexical: both paths should already be absolute with any ".."
// resolved. Component-aware ("C:/foo" does not contain "C:/foobar"), and slash
// style and letter case are ignored, consistent with file name handling elsewhere.
NO_DISCARD bool Filesystem_IsPathInside(const String Parent, const String Child)
{
    String P = Parent;
    String C = Child;
    xx String_EatPathSeparatorsInlineFromEnd(&P);
    xx String_EatPathSeparatorsInlineFromEnd(&C);

    bool bInside = false;

    if (Parent.Length > 0 && P.Length == 0)
    {
        // Parent is nothing but separators (a filesystem root): every absolute path is inside it
        bInside = C.Length == 0 || C.Data[0] == '/' || C.Data[0] == '\\';
    }
    else if (P.Length > 0 && C.Length >= P.Length)
    {
        bInside = true;

        for (u32 i = 0; i < P.Length && bInside; i++)
        {
            i32 A = (i32)P.Data[i];
            i32 B = (i32)C.Data[i];

            if (A == '\\') { A = '/'; }
            if (B == '\\') { B = '/'; }

            if (A >= 'A' && A <= 'Z') { A += 32; }
            if (B >= 'A' && B <= 'Z') { B += 32; }

            if (A != B)
            {
                bInside = false;
            }
        }

        // a longer child must continue with a new path component, otherwise
        // "C:/foo" would be treated as containing "C:/foobar"
        if (bInside && C.Length > P.Length)
        {
            u8 Next = C.Data[P.Length];
            if (Next != '/' && Next != '\\')
            {
                bInside = false;
            }
        }
    }

    return bInside;
}

// Wildcard path expansion. A pattern path is walked one component at a time: literal components
// descend directly, components containing '*'/'?' are matched against every entry of the directory
// reached so far, and a component that is exactly "**" matches zero or more directories in between.
// Matching is case-insensitive, consistent with how file names are compared everywhere else.

STRUCT(WildcardExpandState)
{
    WildcardIterator Callback;
    void*            UserData;
    u32              MatchCount;
    b32              bStopped;
};

STRUCT(WildcardExpandLevel)
{
    WildcardExpandState* State;
    String               Component;  // pattern component being matched at this directory level
    String               Remaining;  // pattern after this component (empty when Component is the last one)
};

static void Internal_ExpandWildcards(const String Directory, const String Pattern, WildcardExpandState* State);
static void Internal_ExpandWildcards_Recursive(const String Directory, const String Remaining, WildcardExpandState* State);

static void Internal_ReportWildcardMatch(WildcardExpandState* State, const String FullPath, const String FileName, bool bIsDirectory)
{
    State->MatchCount++;

    if (!State->Callback(FullPath, FileName, bIsDirectory, State->UserData))
    {
        State->bStopped = true;
    }
}

static bool Internal_WildcardLevelIterator(const String FullPath, const String RelativePath, const String FileName, u64 FileSize, bool bIsDirectory, void* UserData)
{
    UNUSED_PARAM(RelativePath);
    UNUSED_PARAM(FileSize);

    WildcardExpandLevel* Level = UserData;
    WildcardExpandState* State = Level->State;

    bool bContinue = true;

    if (String_MatchesWildcard(FileName, Level->Component, false))
    {
        if (Level->Remaining.Length == 0)
        {
            Internal_ReportWildcardMatch(State, FullPath, FileName, bIsDirectory);
        }
        else if (bIsDirectory)
        {
            Internal_ExpandWildcards(FullPath, Level->Remaining, State);
        }
    }

    if (State->bStopped)
    {
        bContinue = false;
    }

    return bContinue;
}

static bool Internal_WildcardDescendIterator(const String FullPath, const String RelativePath, const String FileName, u64 FileSize, bool bIsDirectory, void* UserData)
{
    UNUSED_PARAM(RelativePath);
    UNUSED_PARAM(FileName);
    UNUSED_PARAM(FileSize);

    WildcardExpandLevel* Level = UserData;
    WildcardExpandState* State = Level->State;

    bool bContinue = true;

    if (bIsDirectory)
    {
        Internal_ExpandWildcards_Recursive(FullPath, Level->Remaining, State);
    }

    if (State->bStopped)
    {
        bContinue = false;
    }

    return bContinue;
}

// handles a "**" component: match the remaining pattern in this directory (the zero-directories
// case) and keep doing so in every subdirectory below it
static void Internal_ExpandWildcards_Recursive(const String Directory, const String Remaining, WildcardExpandState* State)
{
    Internal_ExpandWildcards(Directory, Remaining, State);

    if (!State->bStopped)
    {
        WildcardExpandLevel Level = { State, String_Null(), Remaining };
        Filesystem_IterateDirectory_Ex(Directory, &Internal_WildcardDescendIterator, false, &Level);
    }
}

static void Internal_ExpandWildcards(const String Directory, const String Pattern, WildcardExpandState* State)
{
    // split the pattern into its first component and the rest, skipping over redundant separators
    String Remaining = Pattern;
    while (Remaining.Length > 0 && (Remaining.Data[0] == '/' || Remaining.Data[0] == '\\'))
    {
        Remaining = StrShiftF(Remaining, 1);
    }

    String Component = Remaining;
    String Rest = String_Null();

    u32 SlashIndex = 0;
    if (String_IndexOfFirstPathSlash(Remaining, &SlashIndex))
    {
        Component = StrSlice(Remaining.Data, SlashIndex);
        Rest = StrShiftF(Remaining, SlashIndex+1);
    }

    if (Component.Length > 0 && !State->bStopped)
    {
        bool bRecursiveComponent = String_IsEqual(Component, S("**"), false);

        // a trailing "**" would report both a directory and everything inside it, making the
        // caller operate on the same file twice - treat it as a plain "*" instead
        if (bRecursiveComponent && Rest.Length == 0)
        {
            bRecursiveComponent = false;
            Component = S("*");
        }

        if (bRecursiveComponent)
        {
            Internal_ExpandWildcards_Recursive(Directory, Rest, State);
        }
        else if (String_ContainsChars(Component, S("*?")))
        {
            WildcardExpandLevel Level = { State, Component, Rest };
            Filesystem_IterateDirectory_Ex(Directory, &Internal_WildcardLevelIterator, false, &Level);
        }
        else
        {
            StringLocal(Path, MAX_PATH_LENGTH);
            String_BuildPath(&Path, Directory, Component);

            if (Rest.Length == 0)
            {
                if (Filesystem_DoesFileExist(Path))
                {
                    Internal_ReportWildcardMatch(State, Path, Component, false);
                }
                else if (Filesystem_DoesDirectoryExist(Path))
                {
                    Internal_ReportWildcardMatch(State, Path, Component, true);
                }
            }
            else if (Filesystem_DoesDirectoryExist(Path))
            {
                Internal_ExpandWildcards(Path, Rest, State);
            }
        }
    }
}

u32 Filesystem_ExpandWildcards(const String PathPattern, WildcardIterator Callback, void* UserData)
{
    WildcardExpandState State = { Callback, UserData, 0, false };

    String Pattern = PathPattern;
    xx String_EatPathSeparatorsInlineFromEnd(&Pattern);

    // everything before the last separator preceding the first wildcard is a literal
    // base directory we can start walking from
    u32 FirstWildcard = 0;
    for (u32 i = 0; i < Pattern.Length; i++)
    {
        if (Pattern.Data[i] == '*' || Pattern.Data[i] == '?')
        {
            break;
        }

        FirstWildcard++;
    }

    StringLocal(BaseDirectory, MAX_PATH_LENGTH);

    u32 LastSlash = 0;
    if (String_IndexOfLastPathSlash(StrSlice(Pattern.Data, FirstWildcard), &LastSlash))
    {
        String_Copy(&BaseDirectory, StrSlice(Pattern.Data, LastSlash));
        Pattern = StrShiftF(Pattern, LastSlash+1);
    }
    else
    {
        String_Copy(&BaseDirectory, S("."));
    }

    // "C:" alone is a drive-relative path, not the drive's root - keep the separator
    if (String_IsLast(BaseDirectory, ':') || BaseDirectory.Length == 0)
    {
        String_AppendChar(&BaseDirectory, '/');
    }

    if (Filesystem_DoesDirectoryExist(BaseDirectory))
    {
        Internal_ExpandWildcards(BaseDirectory, Pattern, &State);
    }

    return State.MatchCount;
}

// UUID Version 4 - Random based
// https://datatracker.ietf.org/doc/html/rfc4122#section-4.4
NO_DISCARD Uuid UUID_Generate(void)
{
    Uuid Result = {0};

    Result.TimeLow               = (u32)Rand();
    Result.TimeMid               = (u16)Rand();
    
    Result.TimeHiAndVersion      = (u16)Rand();
    Result.TimeHiAndVersion      &= 0x0FFF;
    Result.TimeHiAndVersion      |= 0x4000;

    Result.ClockSeqHiAndReserved = (u8)Rand();
    Result.ClockSeqHiAndReserved &= 0x3F;
    Result.ClockSeqHiAndReserved |= 0x80;

    Result.ClockSeqLow           = (u8)Rand();

    for (u8 i = 0; i < 6; i++)
    {
        Result.Node[i] = (u8)Rand();
    }

    return Result;
}

NO_DISCARD bool UUID_IsEqual(Uuid First, Uuid Second)
{
    bool bMatch = Platform_MemEqual(&First, &Second, sizeof(struct Uuid));
    return bMatch;
}

NO_DISCARD static u8 HexCharToU8(uchar c)
{
    uchar Char = 0xFF;

    if (c >= '0' && c <= '9')
    {
        Char = (u8)(c - '0');
    }
    else if (c >= 'a' && c <= 'f')
    {
        Char = (u8)(c - 'a' + 10);
    }
    else if (c >= 'A' && c <= 'F')
    {
        Char = (u8)(c - 'A' + 10);
    }
    else
    {
    }

    return Char;
}

NO_DISCARD Uuid UUID_FromString(const String IDString)
{
    Uuid Result = {0};

    if (IDString.Length >= 36)
    {
        u8 b = 0;
        u8 Bytes[16] = {0};

        for (u32 i = 0; i < IDString.Length;)
        {
            if (i >= 36)
            {
                // just in case a string longer than this was given. ignore the rest...
                break;
            }

            if (IDString.Data[i] == '-') // skip the dashes
            {
                i++;
                continue;
            }

            u8 High = HexCharToU8(IDString.Data[i]);
            u8 Low  = HexCharToU8(IDString.Data[i+1]);

            // stop if we get invalid hex conversions
            if (High == 0xFF || Low == 0xFF)
            {
                break;
            }

            Bytes[b++] = (u8)((High << 4) | Low);

            i += 2;
        }

        // only do this if we converted everything
        if (b == 16)
        {
            Result.TimeLow = (u32)(((u32)Bytes[0] << 24) |
                                   ((u32)Bytes[1] << 16) |
                                   ((u32)Bytes[2] << 8)  |
                                   ((u32)Bytes[3]));

            Result.TimeMid = (u16)(((u16)Bytes[4] << 8) | ((u16)Bytes[5]));

            Result.TimeHiAndVersion = (u16)(((u16)Bytes[6] << 8) | ((u16)Bytes[7]));

            Result.ClockSeqHiAndReserved = Bytes[8];
            Result.ClockSeqLow           = Bytes[9];

            for (u8 i = 0; i < 6; ++i)
            {
                Result.Node[i] = Bytes[10 + i];
            }
        }
    }

    return Result;
}

NO_DISCARD static uchar IntToHexChar(u32 Val)
{
    uchar Char = Val < 10 ? (uchar)('0' + Val) : (uchar)('a' + (Val - 10));
    return Char;
}

void UUID_ToString(Uuid ID, String* OutString)
{
    String_Format(OutString, S("%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x"),
                ID.TimeLow, ID.TimeMid, ID.TimeHiAndVersion, ID.ClockSeqHiAndReserved, ID.ClockSeqLow,
                ID.Node[0], ID.Node[1], ID.Node[2], ID.Node[3], ID.Node[4], ID.Node[5]);
}

void UUID_ToStringFast(Uuid ID, String* OutString)
{
    ENSURE(OutString->Capacity >= 36);

    OutString->Data[0]  = IntToHexChar((ID.TimeLow >> 28) & 0x0F);
    OutString->Data[1]  = IntToHexChar((ID.TimeLow >> 24) & 0x0F);
    OutString->Data[2]  = IntToHexChar((ID.TimeLow >> 20) & 0x0F);
    OutString->Data[3]  = IntToHexChar((ID.TimeLow >> 16) & 0x0F);
    OutString->Data[4]  = IntToHexChar((ID.TimeLow >> 12) & 0x0F);
    OutString->Data[5]  = IntToHexChar((ID.TimeLow >> 8 ) & 0x0F);
    OutString->Data[6]  = IntToHexChar((ID.TimeLow >> 4 ) & 0x0F);
    OutString->Data[7]  = IntToHexChar((ID.TimeLow >> 0 ) & 0x0F);

    OutString->Data[8]  = '-';

    OutString->Data[9]  = IntToHexChar((ID.TimeMid >> 12) & 0x0F);
    OutString->Data[10] = IntToHexChar((ID.TimeMid >> 8 ) & 0x0F);
    OutString->Data[11] = IntToHexChar((ID.TimeMid >> 4 ) & 0x0F);
    OutString->Data[12] = IntToHexChar((ID.TimeMid >> 0 ) & 0x0F);

    OutString->Data[13] = '-';

    OutString->Data[14] = IntToHexChar((ID.TimeHiAndVersion >> 12) & 0x0F);
    OutString->Data[15] = IntToHexChar((ID.TimeHiAndVersion >> 8 ) & 0x0F);
    OutString->Data[16] = IntToHexChar((ID.TimeHiAndVersion >> 4 ) & 0x0F);
    OutString->Data[17] = IntToHexChar((ID.TimeHiAndVersion >> 0 ) & 0x0F);

    OutString->Data[18] = '-';

    OutString->Data[19] = IntToHexChar((ID.ClockSeqHiAndReserved >> 4) & 0x0F);
    OutString->Data[20] = IntToHexChar((ID.ClockSeqHiAndReserved >> 0) & 0x0F);
    OutString->Data[21] = IntToHexChar((ID.ClockSeqLow           >> 4) & 0x0F);
    OutString->Data[22] = IntToHexChar((ID.ClockSeqLow           >> 0) & 0x0F);

    OutString->Data[23] = '-';

    for (u8 i = 0; i < 6; i++)
    {
        OutString->Data[24 + i*2]     = IntToHexChar((ID.Node[i] >> 4) & 0x0F);
        OutString->Data[24 + i*2 + 1] = IntToHexChar((ID.Node[i] >> 0) & 0x0F);
    }

    OutString->Length = 36;
}

// https://stackoverflow.com/questions/4768180/rand-implementation

NO_DISCARD i32 RandFast(void)
{
    u64 Seed = (u64)Platform_GetAbsoluteTime();
    Seed *= 1103515245 + 12345;

    return (i32)(Seed/65536) % 32768;
}

NO_DISCARD f32 FRand(void)
{
    // inline Absi32 function
    i32 Value = Rand();
	i32 Temp = Value >> 31;
	Value ^= Temp;
	Value += Temp & 1;

	return (f32)Value / (f32)INT32_MAX;
}

NO_DISCARD f32 FRandFast(void)
{
    constant { RAND_MAX = 0x7fff };

    u64 Seed = (u64)Platform_GetAbsoluteTime();
    Seed *= 1103515245 + 12345;
    
    f32 RandFastResult = (f32)((Seed/65536) % 32768);
	return RandFastResult / (f32)RAND_MAX;
}

// https://stackoverflow.com/questions/19377396/c-get-day-of-year-from-date
NO_DISCARD bool IsLeapYear(u16 Year)
{
    return (Year % 4 == 0 && Year % 100 != 0) || (Year % 400 == 0);
}

NO_DISCARD u16 Platform_GetDayOfYear(u16 Day, u16 Month, u16 Year)
{
    static const u16 Days[2][13] =
    {
        {0, 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334},
        {0, 0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335}
    };

    bool Leap = IsLeapYear(Year);

    return Days[Leap][Month] + Day;
}

static String DayNames[7] =
{
    SC("Sunday"),
    SC("Monday"),
    SC("Tuesday"),
    SC("Wednesday"),
    SC("Thursday"),
    SC("Friday"),
    SC("Saturday")
};

static String MonthNames[13] =
{
    SC(""),
    SC("January"),
    SC("Februrary"),
    SC("March"),
    SC("April"),
    SC("May"),
    SC("June"),
    SC("July"),
    SC("August"),
    SC("September"),
    SC("October"),
    SC("November"),
    SC("December")
};

NO_DISCARD String Platform_GetDayName(u16 DayOfWeek)
{
    u16 Clamped = ClampU16_Max(DayOfWeek, 6);
    String DayName = DayNames[Clamped];
    return DayName;
}

NO_DISCARD String Platform_GetMonthName(u16 Month)
{
    u16 Clamped = ClampU16_Max(Month, 12);
    String MonthName = MonthNames[Clamped];
    return MonthName;
}

ASAN_NO_SANITIZE("float-cast-overflow")
NO_DISCARD i32 FloatRoundToInt(f64 x)
{
    // Split into integer part and fractional part
    i32 i = (i32)x;
    f64 frac = x - (f64)i;

    if (x >= 0.0)
    {
        if (frac > 0.5)
        {
            i += 1; // round up
        }
        else if (frac == 0.5)
        {
            // tie: round to even
            if (i & 1)
            {
                i += 1;
            }
        }
    }
    else
    {
        if (frac < -0.5)
        {
            i -= 1; // round down
        }
        else if (frac == -0.5)
        {
            // tie: round to even
            if (i & 1)
            {
                i -= 1;
            }
        }
    }

    return i;
}

NO_DISCARD bool Platform_IsBigEndian(void)
{
    u32 a = 1;
    uchar* c = (uchar*)&a;
    bool bBig = c[0] == 0;
    return bBig;
}

NO_DISCARD bool Platform_IsLittleEndian(void)
{
    u32 a = 1;
    uchar* c = (uchar*)&a;
    bool bLittle = c[0] == 1;
    return bLittle;
}

NO_DISCARD String Platform_GetCLibraryName(void)
{
    String Name;

    #if PLATFORM_WINDOWS
    Name = S("msvcrt");
    #elif PLATFORM_MAC
    Name = S("macos");
    #elif PLATFORM_LINUX
    #if defined(__GLIBC__)
    Name = S("glibc");
    #elif defined(__UCLIBC__)
    Name = S("uclibc");
    #elif defined(__BIONIC__)
    Name = S("bionic");
    #endif
    #elif PLATFORM_BSD
    Name = S("bsd");
    #else
    Name = String_Null();
    #endif

    return Name;
}

NO_DISCARD ECpuClipBehaviour Platform_GetCpuClippingBehaviour(void)
{
    bool bClipsPositive = false;
    {
        f64 FVal = 1.0 * 0x7FFFFFFF;

        for (u8 i = 0; i < 100; i++)
        {
            i32 IVal = FloatRoundToInt(FVal) >> 24;
            if (IVal != 127)
            {
                bClipsPositive = true;
                break;
            }

            FVal *= 1.2499999;
        }
    }

    bool bClipsNegative = false;
    {
        f64 FVal = -8.0 * 0x10000000;
        
        for (u8 i = 0; i < 100; i++)
        {
            i32 IVal = FloatRoundToInt(FVal) >> 24;
            if (IVal != -128)
            {
                bClipsNegative = true;
                break;
            }

            FVal *= 1.2499999;
        }
    }

    ECpuClipBehaviour Result = CpuClip_None;

    if (bClipsPositive && bClipsNegative)
    {
        Result = CpuClip_Both;
    }
    else if (bClipsPositive)
    {
        Result = CpuClip_Positive;
    }
    else if (bClipsNegative)
    {
        Result = CpuClip_Negative;
    }
    else
    {
    }

    return Result;
}

// a zero-initialized pipe is NOT invalid on unix (but on windows it is)
void Platform_PipeInit(PlatformPipe Pipe)
{
    Pipe[0] = g_PipeNil[0];
    Pipe[1] = g_PipeNil[1];
}

void Platform_ClosePipeEnd(PlatformHandle* PipeEnd)
{
    if (Platform_IsValidHandle(*PipeEnd))
    {
        Platform_CloseHandle(*PipeEnd);
        *PipeEnd = PLATFORM_PIPE_INVALID;
    }
}

void Platform_ClosePipe(PlatformPipe Pipe)
{
    Platform_ClosePipeEnd(&Pipe[0]);
    Platform_ClosePipeEnd(&Pipe[1]);
}
