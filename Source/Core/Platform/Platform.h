#ifndef _PLATFORM_H_
#define _PLATFORM_H_

#include "EngineTypes.h"
#include "String/BaseString.h"
#include "Memory/Allocators.h"

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
    i32 ID;
    String Name;
};

STRUCT(CpuInfo)
{
    // Vendor
    bool Intel           : 1;
    bool AMD             : 1;
    bool Apple           : 1;

    // Architecture
    bool x86             : 1;
    bool x64             : 1;
    bool ARM             : 1;
    bool ARM64           : 1;
    bool PPC             : 1;
    bool PPC64           : 1;

    // x86 Instruction Set Extensions
    bool MMX             : 1;
    bool SSE             : 1;
    bool SSE2            : 1;
    bool SSE3            : 1;
    bool SSSE3           : 1;
    bool SSE4            : 1;
    bool SSE41           : 1;
    bool SSE42           : 1;
    bool AVX             : 1;
    bool AVX2            : 1;
    bool FMA3            : 1;
    bool RDRAND          : 1;
    bool AES             : 1;
    bool SHA             : 1;
    bool ADX             : 1;
    bool MPX             : 1;
    bool BMI1            : 1;
    bool BMI2            : 1;
    bool RDSEED          : 1;
    bool RDPID           : 1;
    bool PREFETCHWT1     : 1;
    bool AVX512          : 1;
    bool AVX512F         : 1;
    bool AVX512DQ        : 1;
    bool AVX512IFMA      : 1;
    bool AVX512PF        : 1;
    bool AVX512ER        : 1;
    bool AVX512CD        : 1;
    bool AVX512BW        : 1;
    bool AVX512VL        : 1;
    bool AVX512VBMI      : 1;
    bool AVX512VBMI2     : 1;
    bool AVX512VPCLMUL   : 1;
    bool AVX512VNNI      : 1;
    bool AVX512BITALG    : 1;
    bool AVX512VPOPCNTDQ : 1;
    bool AVX5124VNNIW    : 1;
    bool AVX5124FMAPS    : 1;
    bool AVX512BF16      : 1;
    bool AVX512FP16      : 1;
    bool GFNI            : 1;
    bool VAES            : 1;

    // Arm Instruction Set Extensions
    bool NEON            : 1;
    bool NEON_HPFP       : 1;
    bool NEON_FP16       : 1;
    bool ARMV8_1_ATOMICS : 1;
    bool ARMV8_2_FHM     : 1;
    bool ARMV8_2_SHA512  : 1;
    bool ARMV8_2_SHA3    : 1;
    bool ARMV8_3_COMPNUM : 1;
    bool ARMV8_CRC32     : 1;
    bool ARMV8_GPI       : 1;
    bool AdvSIMD         : 1;
    bool AdvSIMD_HPFPCVT : 1;
    bool UCNORMAL_MEM    : 1;
    bool FLAGM           : 1;
    bool FLAGM2          : 1;
    bool FLAGM3          : 1;
    bool FLAGM4          : 1;
    bool FHM             : 1;
    bool DOTPROD         : 1;
    bool SHA3            : 1;
    bool RDM             : 1;
    bool LSE             : 1;
    bool SHA256          : 1;
    bool SHA512          : 1;
    bool SHA1            : 1;
    bool PMULL           : 1;
    bool SPECRES         : 1;
    bool SB              : 1;
    bool FRINTTS         : 1;
    bool LRCPC           : 1;
    bool LRCPC2          : 1;
    bool FCMA            : 1;
    bool JSCVT           : 1;
    bool PAUTH           : 1;
    bool PAUTH2          : 1;
    bool FPAC            : 1;
    bool DPB             : 1;
    bool DPB2            : 1;
    bool BF16            : 1;
    bool I8MM            : 1;
    bool ECV             : 1;
    bool LSE2            : 1;
    bool CSV2            : 1;
    bool CSV3            : 1;
    bool DIT             : 1;
    bool FP16            : 1;
    bool SSBS            : 1;
    bool BTI             : 1;
};

RIFT_API void Platform_PreInitialize(void);
RIFT_API f64 Platform_GetClockFrequency(void);

RIFT_API void Platform_Abort(u32 ExitCode);

RIFT_API StringArray Platform_GetCommandLineArgs(void);

RIFT_API void* Platform_MemAlloc(usize Size);
RIFT_API void* Platform_MemAllocZero(usize Size);
RIFT_API void* Platform_MemReAlloc(const void* Block, usize Size);
RIFT_API void  Platform_MemFree(const void* Block);
RIFT_API void* Platform_MemZero(void* Block, usize Size);
RIFT_API void* Platform_MemCopy(void* restrict Dest, const void* restrict Source, usize Size);
RIFT_API void* Platform_MemMove(void* restrict Dest, const void* restrict Source, usize Size);
RIFT_API void* Platform_MemSet(void* Dest, i32 Value, usize Size);
RIFT_API bool Platform_MemEqual(const void* Block1, const void* Block2, usize Size);

RIFT_API void Platform_ConsoleWrite(const char* Message, u8 Color, bool bIsError);
RIFT_API void Platform_ConsoleWrite_CustomLength(const char* Message, u32 Length, u8 Color, bool bIsError);

RIFT_API void Platform_BeginNonBlockingMode(void);
RIFT_API void Platform_EndNonBlockingMode(void);

RIFT_API PlatformHandle Platform_RunCommand(const String CmdLine, const String WorkingDirectory, const String EnvBlock);
RIFT_API PlatformHandle Platform_RunCommand_Ex(const String CmdLine, const String WorkingDirectory, PlatformPipe* StdOutPipe);

RIFT_API bool Platform_TerminateProcess(PlatformHandle Handle, u32 ExitCode);

RIFT_API bool Platform_FindProgram(String ProgramName);
RIFT_API bool Platform_FindProgram_Ex(String ProgramName, String* OutProgramPath);
RIFT_API bool Platform_FindFile(String FileName, String ExtensionWithDot);
RIFT_API bool Platform_FindFile_Ex(String FileName, String ExtensionWithDot, String* OutFilePath);

RIFT_API u32 Platform_GetExitCodeForProcess(PlatformHandle Handle);
RIFT_API u32 Platform_WaitForProcessAndGetExitCode(PlatformHandle Handle);
RIFT_API void Platform_WaitForHandle(PlatformHandle Handle, i32 Milliseconds);
RIFT_API u32 Platform_WaitForMultipleHandles(PlatformHandle* Handles, u32 NumHandles, i32 Milliseconds, bool bWaitAll);
RIFT_API void Platform_CloseHandle(PlatformHandle Handle);
RIFT_API bool Platform_IsValidHandle(const PlatformHandle Handle);

RIFT_API usize Platform_GetCriticalSectionMemoryRequirement(void);
RIFT_API void Platform_InitializeCriticalSection(PlatformCriticalSection OutCriticalSection);
RIFT_API void Platform_DeleteCriticalSection(PlatformCriticalSection CriticalSection);
RIFT_API void Platform_EnterCriticalSection(PlatformCriticalSection CriticalSection);
RIFT_API void Platform_ExitCriticalSection(PlatformCriticalSection CriticalSection);

RIFT_API bool Platform_AnyKeyPressed(void);

RIFT_API bool Platform_CreateMutex(PlatformMutex* OutMutex);
RIFT_API bool Platform_CreateNamedMutex(const String Name, PlatformMutex* OutMutex);
RIFT_API bool Platform_ReleaseMutex(PlatformMutex* Mutex);

RIFT_API u32  Platform_GetConsoleProcessCount(void);

RIFT_API f64 Platform_GetAbsoluteTime(void);
RIFT_API SystemTime Platform_GetSystemLocalTime(void);
RIFT_API bool Platform_GetTimeZone(String* OutTimeZone);

RIFT_API void Platform_Sleep(f64 ms);

RIFT_API u64 Platform_GetCurrentThreadID(void);
RIFT_API u64 Platform_GetMainThreadID(void);

RIFT_API void Platform_GetWorkingDirectory(String* OutPath);

RIFT_API bool Platform_GetEnvironmentVariableValue(String Name, String* OutVariable);
RIFT_API bool Platform_SetEnvironmentVariableValue(String Name, String Value);
RIFT_API bool Platform_DoesEnvironmentVariableExist(String Name);

RIFT_API u32 Platform_GetNumLogicalProcessors(void);

RIFT_API bool Platform_GetAccountName(String* OutName);
RIFT_API bool Platform_GetUserName(String* OutName);
RIFT_API bool Platform_GetUserDirectory(String* OutDirectory);

RIFT_API bool Platform_GetCurrentProcessName(String* OutName);
RIFT_API u64  Platform_GetCurrentProcessID(void);

RIFT_API bool Platform_IsProgramRunning(const String ProgramName);

RIFT_API bool Platform_GetTerminalDimensions(u32* OutRows, u32* OutColumns);

RIFT_API u32 Platform_GetCpuCacheLineSize(void);
RIFT_API bool Platform_GetCpuBrandName(String* OutName);
RIFT_API bool Platform_GetFullCpuName(String* OutName);
RIFT_API CpuInfo Platform_QueryCPUInfo(void);

#endif // _PLATFORM_H_
