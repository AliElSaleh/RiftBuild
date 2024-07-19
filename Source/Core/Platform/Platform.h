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
    String Name;
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

RIFT_API bool Platform_CreateMutex(const String Name, PlatformMutex* OutMutex);
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
RIFT_API bool Platform_DoesEnvironmentVariableExist(String Name);

RIFT_API u32 Platform_GetNumLogicalProcessors(void);

RIFT_API bool Platform_GetAccountName(String* OutName);
RIFT_API bool Platform_GetUserName(String* OutName);
RIFT_API bool Platform_GetUserDirectory(String* OutDirectory);

RIFT_API bool Platform_GetCurrentProcessName(String* OutName);
RIFT_API u64  Platform_GetCurrentProcessID(void);

RIFT_API bool Platform_IsProgramRunning(const String ProgramName);

RIFT_API bool Platform_GetTerminalDimensions(u32* OutRows, u32* OutColumns);

#endif // _PLATFORM_H_
