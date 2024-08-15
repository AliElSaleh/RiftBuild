// Copyright (c) 2024 Ali El Saleh

#include "Platform.h"
#include "String/StringUtils.h"
#include "Log.h"

#if __CPU_X86 || __CPU_X64
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

internal void cpuid(int info[4], int infoType, int subtype)
{
    #if COMPILER_MSVC
    __cpuidex(info, infoType, subtype);
    #else
    __cpuid_count(infoType, subtype, info[0], info[1], info[2], info[3]);
    #endif
}

#if PLATFORM_WINDOWS || PLATFORM_LINUX
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
#endif

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
    // EAX: Processor Info and Feature Bits
    // EBX: Additional Information (e.g., Brand Index, CLFLUSH line size)
    // ECX: Feature Flags
    // EDX: Feature Flags
    int ecx = info[2];
    int edx = info[3];

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
    Result.F16C          = (ecx & (1 << 29)) ? 1 : 0;
    Result.FMA           = (ecx & (1 << 12)) ? 1 : 0;
    Result.FMA3          = (ecx & (1 << 12)) ? 1 : 0;
    Result.RDRAND        = (ecx & (1 << 30)) ? 1 : 0;
    Result.PCLMULQDQ     = (ecx & (1 << 1))  ? 1 : 0;
    Result.DTES64        = (ecx & (1 << 2))  ? 1 : 0;
    Result.MONITOR       = (ecx & (1 << 3))  ? 1 : 0;
    Result.DSCPL         = (ecx & (1 << 4))  ? 1 : 0;
    Result.VMX           = (ecx & (1 << 5))  ? 1 : 0;
    Result.SMX           = (ecx & (1 << 6))  ? 1 : 0;
    Result.EIST          = (ecx & (1 << 7))  ? 1 : 0;
    Result.TM2           = (ecx & (1 << 8))  ? 1 : 0;
    Result.CNXTID        = (ecx & (1 << 10)) ? 1 : 0;
    Result.SDBG          = (ecx & (1 << 11)) ? 1 : 0;
    Result.CX16          = (ecx & (1 << 13)) ? 1 : 0;
    Result.XTPR          = (ecx & (1 << 14)) ? 1 : 0;
    Result.PDCM          = (ecx & (1 << 15)) ? 1 : 0;
    Result.PCID          = (ecx & (1 << 17)) ? 1 : 0;
    Result.DCA           = (ecx & (1 << 18)) ? 1 : 0;
    Result.X2APIC        = (ecx & (1 << 21)) ? 1 : 0;
    Result.MOVBE         = (ecx & (1 << 22)) ? 1 : 0;
    Result.POPCNT        = (ecx & (1 << 23)) ? 1 : 0;
    Result.TSCDEADLINE   = (ecx & (1 << 24)) ? 1 : 0;
    Result.XSAVE         = (ecx & (1 << 26)) ? 1 : 0;
    Result.OSXSAVE       = (ecx & (1 << 27)) ? 1 : 0;
    Result.HYPERVISOR    = (ecx & (1 << 31)) ? 1 : 0;

    Result.FPU           = (edx & (1 << 0))  ? 1 : 0;
    Result.VME           = (edx & (1 << 1))  ? 1 : 0;
    Result.DE            = (edx & (1 << 2))  ? 1 : 0;
    Result.PSE           = (edx & (1 << 3))  ? 1 : 0;
    Result.TSC           = (edx & (1 << 4))  ? 1 : 0;
    Result.MSR           = (edx & (1 << 5))  ? 1 : 0;
    Result.PAE           = (edx & (1 << 6))  ? 1 : 0;
    Result.MCE           = (edx & (1 << 7))  ? 1 : 0;
    Result.CX8           = (edx & (1 << 8))  ? 1 : 0;
    Result.APIC          = (edx & (1 << 9))  ? 1 : 0;
    Result.SEP           = (edx & (1 << 11)) ? 1 : 0;
    Result.MTRR          = (edx & (1 << 12)) ? 1 : 0;
    Result.PGE           = (edx & (1 << 13)) ? 1 : 0;
    Result.MCA           = (edx & (1 << 14)) ? 1 : 0;
    Result.CMOV          = (edx & (1 << 15)) ? 1 : 0;
    Result.PAT           = (edx & (1 << 16)) ? 1 : 0;
    Result.PSE36         = (edx & (1 << 17)) ? 1 : 0;
    Result.PSN           = (edx & (1 << 18)) ? 1 : 0;
    Result.CLFLUSH       = (edx & (1 << 19)) ? 1 : 0;
    Result.DS            = (edx & (1 << 21)) ? 1 : 0;
    Result.ACPI          = (edx & (1 << 22)) ? 1 : 0;
    Result.FXSR          = (edx & (1 << 24)) ? 1 : 0;
    Result.SS            = (edx & (1 << 27)) ? 1 : 0;
    Result.HTT           = (edx & (1 << 28)) ? 1 : 0;
    Result.TM            = (edx & (1 << 29)) ? 1 : 0;
    Result.PBE           = (edx & (1 << 31)) ? 1 : 0;


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
        Result.AVX2             = (ebx & (1 << 5))  ? 1 : 0;
        Result.BMI1             = (ebx & (1 << 3))  ? 1 : 0;
        Result.TZCNT            = (ebx & (1 << 3))  ? 1 : 0;
        Result.BMI2             = (ebx & (1 << 8))  ? 1 : 0;
        Result.ADX              = (ebx & (1 << 19)) ? 1 : 0;
        Result.MPX              = (ebx & (1 << 14)) ? 1 : 0;
        Result.SHA              = (ebx & (1 << 29)) ? 1 : 0;
        Result.RDSEED           = (ebx & (1 << 18)) ? 1 : 0;
        Result.RDPID            = (ebx & (1 << 23)) ? 1 : 0;
        Result.AVX512F          = (ebx & (1 << 16)) ? 1 : 0;
        Result.AVX512DQ         = (ebx & (1 << 17)) ? 1 : 0;
        Result.AVX512IFMA       = (ebx & (1 << 21)) ? 1 : 0;
        Result.AVX512PF         = (ebx & (1 << 26)) ? 1 : 0;
        Result.AVX512ER         = (ebx & (1 << 27)) ? 1 : 0;
        Result.AVX512CD         = (ebx & (1 << 28)) ? 1 : 0;
        Result.AVX512BW         = (ebx & (1 << 30)) ? 1 : 0;
        Result.AVX512VL         = (ebx & (1 << 31)) ? 1 : 0;
        Result.AVX512           = (ebx & (1 << 16)) || (ebx & (1 << 17)) ||
                                  (ebx & (1 << 21)) || (ebx & (1 << 26)) ||
                                  (ebx & (1 << 27)) || (ebx & (1 << 28)) ||
                                  (ebx & (1 << 30)) || (ebx & (1 << 31)) ? 1 : 0;
        Result.FSGSBASE         = (ebx & (1 << 0))  ? 1 : 0;
        Result.TSCADJUST        = (ebx & (1 << 1))  ? 1 : 0;
        Result.SGX              = (ebx & (1 << 2))  ? 1 : 0;
        Result.HLE              = (ebx & (1 << 4))  ? 1 : 0;
        Result.FDP_EXCEPTN_ONLY = (ebx & (1 << 6))  ? 1 : 0;
        Result.SMEP             = (ebx & (1 << 7))  ? 1 : 0;
        Result.ERMS             = (ebx & (1 << 9))  ? 1 : 0;
        Result.INVPCID          = (ebx & (1 << 10)) ? 1 : 0;
        Result.RTM              = (ebx & (1 << 11)) ? 1 : 0;
        Result.PQM              = (ebx & (1 << 12)) ? 1 : 0;
        Result.FPU_DEPR         = (ebx & (1 << 13)) ? 1 : 0;
        Result.PQE              = (ebx & (1 << 15)) ? 1 : 0;
        Result.SMAP             = (ebx & (1 << 20)) ? 1 : 0;
        Result.PCOMMIT          = (ebx & (1 << 22)) ? 1 : 0;
        Result.CLFLUSHOPT       = (ebx & (1 << 23)) ? 1 : 0;
        Result.CLWB             = (ebx & (1 << 24)) ? 1 : 0;
        Result.INTELPT          = (ebx & (1 << 25)) ? 1 : 0;

        ecx = info[2];
        Result.PREFETCHWT1      = (ecx & (1 << 0))  ? 1 : 0;
        Result.AVX512VBMI       = (ecx & (1 << 1))  ? 1 : 0;
        Result.AVX512VBMI2      = (ecx & (1 << 6))  ? 1 : 0;
        Result.AVX512VPCLMUL    = (ecx & (1 << 10)) ? 1 : 0;
        Result.AVX512VNNI       = (ecx & (1 << 11)) ? 1 : 0;
        Result.AVX512BITALG     = (ecx & (1 << 12)) ? 1 : 0;
        Result.AVX512VPOPCNTDQ  = (ecx & (1 << 14)) ? 1 : 0;
        Result.GFNI             = (ecx & (1 << 8))  ? 1 : 0;
        Result.VAES             = (ecx & (1 << 9))  ? 1 : 0;
        Result.UMIP             = (ecx & (1 << 2))  ? 1 : 0;
        Result.PKU              = (ecx & (1 << 3))  ? 1 : 0;
        Result.OSPKE            = (ecx & (1 << 4))  ? 1 : 0;
        Result.WAITPKG          = (ecx & (1 << 5))  ? 1 : 0;
        Result.CET_SS           = (ecx & (1 << 7))  ? 1 : 0;
        Result.VPCLMULQDQ       = (ecx & (1 << 10)) ? 1 : 0;
        Result.TME              = (ecx & (1 << 13)) ? 1 : 0;
        Result.LA57             = (ecx & (1 << 15)) ? 1 : 0;
        Result.KL               = (ecx & (1 << 24)) ? 1 : 0;
        Result.CLDEMOTE         = (ecx & (1 << 25)) ? 1 : 0;
        Result.MOVDIRI          = (ecx & (1 << 26)) ? 1 : 0;
        Result.MOVDIR64B        = (ecx & (1 << 27)) ? 1 : 0;
        Result.ENQCMD           = (ecx & (1 << 28)) ? 1 : 0;
        Result.SGXLC            = (ecx & (1 << 30)) ? 1 : 0;
        Result.BUSLOCKDETECT    = (ecx & (1 << 31)) ? 1 : 0;

        edx = info[3];
        Result.AVX5124VNNIW     = (edx & (1 << 2))  ? 1 : 0;
        Result.AVX5124FMAPS     = (edx & (1 << 3))  ? 1 : 0;

        cpuid(info, 7, 1);
        Result.AVX512BF16       = (info[0] & (1 << 5))  ? 1 : 0;
        Result.AVX512FP16       = (info[1] & (1 << 23)) ? 1 : 0;

        PRAGMA_DISABLE_SIGN_CONVERSION_WARNING

        cpuid(info, 0x80000001, 0);
        Result.LZCNT           = (info[2] & (1 << 5))  ? 1 : 0;
        
        PRAGMA_ENABLE_WARNINGS
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
