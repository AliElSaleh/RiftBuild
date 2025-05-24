// Copyright (c) Artisan Softworks
// Licensed under the BSD 3-Clause License. See the LICENSE file for details.

#ifndef UNITY_BUILD
#include "Platform.h"
#include "Clock.h"
#include "StringUtils.h"
#include "Filesystem.h"
#endif

read_only FileHandle g_FileHandle = { .Data = &(u8[64]){0}, .Data2 = &(u8[64]){0}, .bBypassSizeCheck = false };

#if CPU_X86 || CPU_X64

#if !COMPILER_MSVC
#include <cpuid.h>
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
    #else
    __cpuid_count(infoType, subtype, info[0], info[1], info[2], info[3]);
    #endif
}

#if PLATFORM_WINDOWS || PLATFORM_LINUX
RIFT_API NO_DISCARD u32 Platform_GetCpuCacheLineSize(void)
{
    // todo: do this dynamically
    return CACHE_LINE_SIZE;
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

    Result.MMX           = edx & BIT(23);
    Result.SSE           = edx & BIT(25);
    Result.SSE2          = edx & BIT(26);
    Result.SSE3          = ecx & BIT(0);
    Result.SSSE3         = ecx & BIT(9);
    Result.SSE4          = ecx & BIT(19);
    Result.SSE41         = ecx & BIT(19);
    Result.SSE42         = ecx & BIT(20);
    Result.AES           = ecx & BIT(25);
    Result.AVX           = ecx & BIT(28);
    Result.F16C          = ecx & BIT(29);
    Result.FMA           = ecx & BIT(12);
    Result.FMA3          = ecx & BIT(12);
    Result.RDRAND        = ecx & BIT(30);
    Result.PCLMULQDQ     = ecx & BIT(1);
    Result.DTES64        = ecx & BIT(2);
    Result.MONITOR       = ecx & BIT(3);
    Result.DSCPL         = ecx & BIT(4);
    Result.VMX           = ecx & BIT(5);
    Result.SMX           = ecx & BIT(6);
    Result.EIST          = ecx & BIT(7);
    Result.TM2           = ecx & BIT(8);
    Result.CNXTID        = ecx & BIT(10);
    Result.SDBG          = ecx & BIT(11);
    Result.CX16          = ecx & BIT(13);
    Result.XTPR          = ecx & BIT(14);
    Result.PDCM          = ecx & BIT(15);
    Result.PCID          = ecx & BIT(17);
    Result.DCA           = ecx & BIT(18);
    Result.X2APIC        = ecx & BIT(21);
    Result.MOVBE         = ecx & BIT(22);
    Result.POPCNT        = ecx & BIT(23);
    Result.TSCDEADLINE   = ecx & BIT(24);
    Result.XSAVE         = ecx & BIT(26);
    Result.OSXSAVE       = ecx & BIT(27);
    Result.HYPERVISOR    = ecx & BIT(31);

    Result.FPU           = edx & BIT(0);
    Result.VME           = edx & BIT(1);
    Result.DE            = edx & BIT(2);
    Result.PSE           = edx & BIT(3);
    Result.TSC           = edx & BIT(4);
    Result.MSR           = edx & BIT(5);
    Result.PAE           = edx & BIT(6);
    Result.MCE           = edx & BIT(7);
    Result.CX8           = edx & BIT(8);
    Result.APIC          = edx & BIT(9);
    Result.SEP           = edx & BIT(11);
    Result.MTRR          = edx & BIT(12);
    Result.PGE           = edx & BIT(13);
    Result.MCA           = edx & BIT(14);
    Result.CMOV          = edx & BIT(15);
    Result.PAT           = edx & BIT(16);
    Result.PSE36         = edx & BIT(17);
    Result.PSN           = edx & BIT(18);
    Result.CLFLUSH       = edx & BIT(19);
    Result.DS            = edx & BIT(21);
    Result.ACPI          = edx & BIT(22);
    Result.FXSR          = edx & BIT(24);
    Result.SS            = edx & BIT(27);
    Result.HTT           = edx & BIT(28);
    Result.TM            = edx & BIT(29);
    Result.PBE           = edx & BIT(31);


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
        Result.AVX2             = ebx & BIT(5);
        Result.BMI1             = ebx & BIT(3);
        Result.TZCNT            = ebx & BIT(3);
        Result.BMI2             = ebx & BIT(8);
        Result.ADX              = ebx & BIT(19);
        Result.MPX              = ebx & BIT(14);
        Result.SHA              = ebx & BIT(29);
        Result.RDSEED           = ebx & BIT(18);
        Result.RDPID            = ebx & BIT(23);
        Result.AVX512F          = ebx & BIT(16);
        Result.AVX512DQ         = ebx & BIT(17);
        Result.AVX512IFMA       = ebx & BIT(21);
        Result.AVX512PF         = ebx & BIT(26);
        Result.AVX512ER         = ebx & BIT(27);
        Result.AVX512CD         = ebx & BIT(28);
        Result.AVX512BW         = ebx & BIT(30);
        Result.AVX512VL         = ebx & BIT(31);
        Result.AVX512           = ebx & BIT(16) || ebx & BIT(17) ||
                                  ebx & BIT(21) || ebx & BIT(26) ||
                                  ebx & BIT(27) || ebx & BIT(28) ||
                                  ebx & BIT(30) || ebx & BIT(31);
        Result.FSGSBASE         = ebx & BIT(0);
        Result.TSCADJUST        = ebx & BIT(1);
        Result.SGX              = ebx & BIT(2);
        Result.HLE              = ebx & BIT(4);
        Result.FDP_EXCEPTN_ONLY = ebx & BIT(6);
        Result.SMEP             = ebx & BIT(7);
        Result.ERMS             = ebx & BIT(9);
        Result.INVPCID          = ebx & BIT(10);
        Result.RTM              = ebx & BIT(11);
        Result.PQM              = ebx & BIT(12);
        Result.FPU_DEPR         = ebx & BIT(13);
        Result.PQE              = ebx & BIT(15);
        Result.SMAP             = ebx & BIT(20);
        Result.PCOMMIT          = ebx & BIT(22);
        Result.CLFLUSHOPT       = ebx & BIT(23);
        Result.CLWB             = ebx & BIT(24);
        Result.INTELPT          = ebx & BIT(25);

        ecx = (u32)info[2];
        Result.PREFETCHWT1      = ecx & BIT(0);
        Result.AVX512VBMI       = ecx & BIT(1);
        Result.AVX512VBMI2      = ecx & BIT(6);
        Result.AVX512VPCLMUL    = ecx & BIT(10);
        Result.AVX512VNNI       = ecx & BIT(11);
        Result.AVX512BITALG     = ecx & BIT(12);
        Result.AVX512VPOPCNTDQ  = ecx & BIT(14);
        Result.GFNI             = ecx & BIT(8);
        Result.VAES             = ecx & BIT(9);
        Result.UMIP             = ecx & BIT(2);
        Result.PKU              = ecx & BIT(3);
        Result.OSPKE            = ecx & BIT(4);
        Result.WAITPKG          = ecx & BIT(5);
        Result.CET_SS           = ecx & BIT(7);
        Result.VPCLMULQDQ       = ecx & BIT(10);
        Result.TME              = ecx & BIT(13);
        Result.LA57             = ecx & BIT(15);
        Result.KL               = ecx & BIT(24);
        Result.CLDEMOTE         = ecx & BIT(25);
        Result.MOVDIRI          = ecx & BIT(26);
        Result.MOVDIR64B        = ecx & BIT(27);
        Result.ENQCMD           = ecx & BIT(28);
        Result.SGXLC            = ecx & BIT(30);
        Result.BUSLOCKDETECT    = ecx & BIT(31);

        edx = (u32)info[3];
        Result.AVX5124VNNIW     = edx & BIT(2);
        Result.AVX5124FMAPS     = edx & BIT(3);

        cpuid(info, 7, 1);
        Result.AVX512BF16       = (u32)info[0] & BIT(5);
        Result.AVX512FP16       = (u32)info[1] & BIT(23);

        PRAGMA_DISABLE_SIGN_CONVERSION_WARNING

        cpuid(info, 0x80000001U, 0);
        Result.LZCNT           = (u32)info[2] & BIT(5);
        
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
    (void)String_IndexOfLastChar(Path, '.', &LastDot);
    (void)String_IndexOfLastPathSlash(Path, &LastSlash);

    bool bSomeCharAfterDot = false;
    if (LastDot+1 < Path.Length)
    {
        u8 C = Path.Data[LastDot+1];
        bSomeCharAfterDot = IsAlphabet(C) || IsDigit(C);
    }

    bool bSuccess = LastDot > LastSlash && bSomeCharAfterDot;

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
