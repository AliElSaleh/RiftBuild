#ifndef PLATFORM_H
#define PLATFORM_H

#ifndef UNITY_BUILD
#include "EngineTypes.h"
#endif

STRUCT(SystemTime)
{
    u16 Year;
    u16 Month;
    u16 DayOfWeek;
    u16 Day;
    u16 Hour;
    u16 Minute;
    u16 Second;
    u16 Millisecond;
};

STRUCT(PlatformMutex)
{
    void* Handle;
    String Name;
    i32 ID;
    i32 Padding;
};

STRUCT(CpuInfo)
{
    // Vendor
    u32 Intel            : 1;
    u32 AMD              : 1;
    u32 Apple            : 1;

    // Architecture
    u32 x86              : 1;
    u32 x64              : 1;
    u32 ARM              : 1;
    u32 ARM64            : 1;
    u32 PPC              : 1;
    u32 PPC64            : 1;

    // x86 Instruction Set Extensions
    u32 MMX              : 1;
    u32 SSE              : 1;
    u32 SSE2             : 1;
    u32 SSE3             : 1;
    u32 SSSE3            : 1;
    u32 SSE4             : 1;
    u32 SSE41            : 1;
    u32 SSE42            : 1;
    u32 AVX              : 1;
    u32 AVX2             : 1;
    u32 F16C             : 1;
    u32 FMA              : 1;
    u32 FMA3             : 1;
    u32 RDRAND           : 1;
    u32 PCLMULQDQ        : 1;
    u32 DTES64           : 1;
    u32 MONITOR          : 1;
    u32 DSCPL            : 1;
    u32 VMX              : 1;
    u32 SMX              : 1;
    u32 EIST             : 1;
    u32 TM2              : 1;
    u32 CNXTID           : 1;
    u32 SDBG             : 1;
    u32 CX16             : 1;
    u32 XTPR             : 1;
    u32 PDCM             : 1;
    u32 PCID             : 1;
    u32 DCA              : 1;
    u32 X2APIC           : 1;
    u32 MOVBE            : 1;
    u32 POPCNT           : 1;
    u32 TSCDEADLINE      : 1;
    u32 XSAVE            : 1;
    u32 OSXSAVE          : 1;
    u32 HYPERVISOR       : 1;
    u32 FPU              : 1;
    u32 VME              : 1;
    u32 DE               : 1;
    u32 PSE              : 1;
    u32 TSC              : 1;
    u32 MSR              : 1;
    u32 PAE              : 1;
    u32 MCE              : 1;
    u32 CX8              : 1;
    u32 APIC             : 1;
    u32 SEP              : 1;
    u32 MTRR             : 1;
    u32 PGE              : 1;
    u32 MCA              : 1;
    u32 CMOV             : 1;
    u32 PAT              : 1;
    u32 PSE36            : 1;
    u32 PSN              : 1;
    u32 CLFLUSH          : 1;
    u32 DS               : 1;
    u32 ACPI             : 1;
    u32 FXSR             : 1;
    u32 SS               : 1;
    u32 HTT              : 1;
    u32 TM               : 1;
    u32 PBE              : 1;
    u32 AES              : 1;
    u32 SHA              : 1;
    u32 ADX              : 1;
    u32 MPX              : 1;
    u32 BMI1             : 1;
    u32 LZCNT            : 1;
    u32 TZCNT            : 1;
    u32 BMI2             : 1;
    u32 RDSEED           : 1;
    u32 RDPID            : 1;
    u32 PREFETCHWT1      : 1;
    u32 AVX512           : 1;
    u32 AVX512F          : 1;
    u32 AVX512DQ         : 1;
    u32 AVX512IFMA       : 1;
    u32 AVX512PF         : 1;
    u32 AVX512ER         : 1;
    u32 AVX512CD         : 1;
    u32 AVX512BW         : 1;
    u32 AVX512VL         : 1;
    u32 AVX512VBMI       : 1;
    u32 AVX512VBMI2      : 1;
    u32 AVX512VPCLMUL    : 1;
    u32 AVX512VNNI       : 1;
    u32 AVX512BITALG     : 1;
    u32 AVX512VPOPCNTDQ  : 1;
    u32 AVX5124VNNIW     : 1;
    u32 AVX5124FMAPS     : 1;
    u32 AVX512BF16       : 1;
    u32 AVX512FP16       : 1;
    u32 GFNI             : 1;
    u32 VAES             : 1;

    u32 FSGSBASE         : 1;
    u32 TSCADJUST        : 1;
    u32 SGX              : 1;
    u32 HLE              : 1;
    u32 FDP_EXCEPTN_ONLY : 1;
    u32 SMEP             : 1;
    u32 ERMS             : 1;
    u32 INVPCID          : 1;
    u32 RTM              : 1;
    u32 PQM              : 1;
    u32 FPU_DEPR         : 1;
    u32 PQE              : 1;
    u32 SMAP             : 1;
    u32 PCOMMIT          : 1;
    u32 CLFLUSHOPT       : 1;
    u32 CLWB             : 1;
    u32 INTELPT          : 1;
    u32 UMIP             : 1;
    u32 PKU              : 1;
    u32 OSPKE            : 1;
    u32 WAITPKG          : 1;
    u32 CET_SS           : 1;
    u32 VPCLMULQDQ       : 1;
    u32 TME              : 1;
    u32 LA57             : 1;
    u32 KL               : 1;
    u32 CLDEMOTE         : 1;
    u32 MOVDIRI          : 1;
    u32 MOVDIR64B        : 1;
    u32 ENQCMD           : 1;
    u32 SGXLC            : 1;
    u32 BUSLOCKDETECT    : 1;

    // Arm Instruction Set Extensions
    u32 NEON             : 1;
    u32 NEON_HPFP        : 1;
    u32 NEON_FP16        : 1;
    u32 ARMV8_1_ATOMICS  : 1;
    u32 ARMV8_2_FHM      : 1;
    u32 ARMV8_2_SHA512   : 1;
    u32 ARMV8_2_SHA3     : 1;
    u32 ARMV8_3_COMPNUM  : 1;
    u32 ARMV8_CRC32      : 1;
    u32 ARMV8_GPI        : 1;
    u32 AdvSIMD          : 1;
    u32 AdvSIMD_HPFPCVT  : 1;
    u32 UCNORMAL_MEM     : 1;
    u32 FLAGM            : 1;
    u32 FLAGM2           : 1;
    u32 FLAGM3           : 1;
    u32 FLAGM4           : 1;
    u32 FHM              : 1;
    u32 DOTPROD          : 1;
    u32 SHA3             : 1;
    u32 RDM              : 1;
    u32 LSE              : 1;
    u32 SHA256           : 1;
    u32 SHA512           : 1;
    u32 SHA1             : 1;
    u32 PMULL            : 1;
    u32 SPECRES          : 1;
    u32 SB               : 1;
    u32 FRINTTS          : 1;
    u32 LRCPC            : 1;
    u32 LRCPC2           : 1;
    u32 FCMA             : 1;
    u32 JSCVT            : 1;
    u32 PAUTH            : 1;
    u32 PAUTH2           : 1;
    u32 FPAC             : 1;
    u32 DPB              : 1;
    u32 DPB2             : 1;
    u32 BF16             : 1;
    u32 I8MM             : 1;
    u32 ECV              : 1;
    u32 LSE2             : 1;
    u32 CSV2             : 1;
    u32 CSV3             : 1;
    u32 DIT              : 1;
    u32 FP16             : 1;
    u32 SSBS             : 1;
    u32 BTI              : 1;
};

STRUCT(PlatformVersion)
{
    u32 Major;
    u32 Minor;
    u32 Patch;
};

RIFT_API void Platform_PreInitialize(void);
RIFT_API NO_DISCARD f64 Platform_GetClockFrequency(void);

RIFT_API void Platform_Abort(u32 ExitCode);

RIFT_API NO_DISCARD StringArray Platform_GetCommandLineArgs(void);

RIFT_API NO_DISCARD void* Platform_MemAlloc(usize Size);
RIFT_API NO_DISCARD void* Platform_MemAllocZero(usize Size);
RIFT_API NO_DISCARD void* Platform_MemReAlloc(void* Block, usize Size);
RIFT_API            void  Platform_MemFree(void* Block);
RIFT_API            void  Platform_MemZero(void* Block, usize Size);
RIFT_API            void  Platform_MemCopy(void* Dest, const void* Source, usize Size);
RIFT_API            void  Platform_MemMove(void* Dest, const void* Source, usize Size);
RIFT_API            void  Platform_MemSet(void* Dest, i32 Value, usize Size);
RIFT_API NO_DISCARD bool  Platform_MemEqual(const void* Block1, const void* Block2, usize Size);

RIFT_API NO_DISCARD bool Platform_SetWorkingDirectory(const String Path);

RIFT_API void Platform_ConsoleWrite(const char* Message, u8 Color, bool bIsError);
RIFT_API void Platform_ConsoleWrite_CustomLength(const char* Message, u32 Length, u8 Color, bool bIsError);

RIFT_API void Platform_BeginNonBlockingMode(void);
RIFT_API void Platform_EndNonBlockingMode(void);

RIFT_API NO_DISCARD PlatformHandle Platform_RunCommand(const String CmdLine, const String WorkingDirectory, const String EnvBlock);
RIFT_API NO_DISCARD PlatformHandle Platform_RunCommand_Ex(const String CmdLine, const String WorkingDirectory, PlatformPipe* StdOutPipe);
RIFT_API NO_DISCARD PlatformHandle Platform_RunProcess(const String ProcessExePath, const String Parameters, const String WorkingDirectory, const String EnvBlock);
RIFT_API NO_DISCARD bool Platform_TerminateProcess(PlatformHandle Handle, u32 ExitCode);
RIFT_API NO_DISCARD bool Platform_FindProgram(String ProgramName);
RIFT_API NO_DISCARD bool Platform_FindProgram_Ex(String ProgramName, String* OutProgramPath);
RIFT_API NO_DISCARD bool Platform_FindFile(String FileName, String ExtensionWithDot);
RIFT_API NO_DISCARD bool Platform_FindFile_Ex(String FileName, String ExtensionWithDot, String* OutFilePath);

RIFT_API NO_DISCARD u32 Platform_GetExitCodeForProcess(PlatformHandle Handle);
RIFT_API NO_DISCARD u32 Platform_WaitForProcessAndGetExitCode(PlatformHandle Handle);
RIFT_API            void Platform_WaitForHandle(PlatformHandle Handle, i32 Milliseconds);
RIFT_API NO_DISCARD u32 Platform_WaitForMultipleHandles(PlatformHandle* Handles, u32 NumHandles, i32 Milliseconds, bool bWaitAll);
RIFT_API            void Platform_CloseHandle(PlatformHandle Handle);
RIFT_API NO_DISCARD bool Platform_IsValidHandle(const PlatformHandle Handle);

RIFT_API NO_DISCARD usize Platform_GetCriticalSectionMemoryRequirement(void);
RIFT_API void Platform_InitializeCriticalSection(PlatformCriticalSection OutCriticalSection);
RIFT_API void Platform_DeleteCriticalSection(PlatformCriticalSection CriticalSection);
RIFT_API void Platform_EnterCriticalSection(PlatformCriticalSection CriticalSection);
RIFT_API void Platform_ExitCriticalSection(PlatformCriticalSection CriticalSection);

RIFT_API NO_DISCARD bool Platform_AnyKeyPressed(void);

RIFT_API NO_DISCARD bool Platform_CreateMutex(PlatformMutex* OutMutex);
RIFT_API NO_DISCARD bool Platform_CreateNamedMutex(const String Name, PlatformMutex* OutMutex);
RIFT_API NO_DISCARD bool Platform_ReleaseMutex(PlatformMutex* Mutex);

RIFT_API NO_DISCARD bool Platform_IsRunningAsAdmin(void);

RIFT_API NO_DISCARD u32  Platform_GetConsoleProcessCount(void);

RIFT_API NO_DISCARD f64 Platform_GetAbsoluteTime(void);
RIFT_API NO_DISCARD SystemTime Platform_GetSystemLocalTime(void);
RIFT_API NO_DISCARD bool Platform_GetTimeZone(String* OutTimeZone);

RIFT_API void Platform_Sleep(f64 ms);

RIFT_API NO_DISCARD u64 Platform_GetCurrentThreadID(void);
RIFT_API NO_DISCARD u64 Platform_GetMainThreadID(void);

RIFT_API void Platform_GetWorkingDirectory(String* OutPath);

RIFT_API NO_DISCARD bool Platform_GetEnvironmentVariableValue(String Name, String* OutVariable);
RIFT_API NO_DISCARD bool Platform_SetEnvironmentVariableValue(String Name, String Value);
RIFT_API NO_DISCARD bool Platform_DoesEnvironmentVariableExist(String Name);

RIFT_API NO_DISCARD u32 Platform_GetNumLogicalProcessors(void);

RIFT_API NO_DISCARD bool Platform_GetAccountName(String* OutName);
RIFT_API NO_DISCARD bool Platform_GetUserName(String* OutName);
RIFT_API NO_DISCARD bool Platform_GetUserDirectory(String* OutDirectory);

RIFT_API NO_DISCARD bool Platform_GetCurrentProcessName(String* OutName);
RIFT_API NO_DISCARD u64  Platform_GetCurrentProcessID(void);
RIFT_API NO_DISCARD bool Platform_GetTerminalDimensions(u32* OutRows, u32* OutColumns);

RIFT_API NO_DISCARD u32 Platform_GetCpuCacheLineSize(void);
RIFT_API NO_DISCARD bool Platform_GetCpuBrandName(String* OutName);
RIFT_API NO_DISCARD bool Platform_GetFullCpuName(String* OutName);
RIFT_API NO_DISCARD CpuInfo Platform_QueryCPUInfo(void);

// TODO: kernel version
RIFT_API NO_DISCARD PlatformVersion Platform_GetVersion(void);

RIFT_API NO_DISCARD bool Platform_IsWindowFocused(void);

#endif // PLATFORM_H
