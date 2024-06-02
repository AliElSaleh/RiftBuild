#ifndef NO_LOG
#include "Log.h"

#include "Platform/Platform.h"
#include "Platform/Filesystem.h"

#include "String/StringUtils.h"

#include "Structures/Array.h"

#include <stdarg.h>

STRUCT(LoggingSystemState)
{
    PlatformHandle ThreadHandle;

    String LogFileName;

    char Buffer[MAX_LOG_MSG_LENGTH];

    bool bDisabled;
    bool bCrashOnFatal;
    bool bAlive;
    bool bReady;
    bool bLogTimestamp;
    bool bLogCategory;
    bool bLogType;

    PlatformCriticalSection CriticalSection;
};

static const String LogTypeString[6] = {StrLit("[INFO] "), StrLit("[SUCCESS] "), StrLit("[WARNING] "), StrLit("[ERROR] "), StrLit("[FATAL] "), StrLit("")};

static LoggingSystemState* GLoggingSystemState = NULL;
static LinearAllocator GLoggingMemoryAllocator = {0};
static void* GLoggingSystemMemory = NULL;

bool Logging_Initialize(void* Memory, bool bOpenFile)
{
    GLoggingSystemMemory = Memory;
    LinearAllocator_Create(Logging_GetMemoryRequirement(), GLoggingSystemMemory, &GLoggingMemoryAllocator);
    
    GLoggingSystemState = LinearAllocator_Allocate(&GLoggingMemoryAllocator, sizeof(LoggingSystemState));
    GLoggingSystemState->bDisabled = false;
    GLoggingSystemState->bLogTimestamp = true;
    GLoggingSystemState->bLogCategory = true;
    GLoggingSystemState->bLogType = true;
    GLoggingSystemState->bCrashOnFatal = true;
    GLoggingSystemState->bAlive = true;

    GLoggingSystemState->CriticalSection = LinearAllocator_Allocate(&GLoggingMemoryAllocator, Platform_GetCriticalSectionMemoryRequirement());
    Platform_InitializeCriticalSection(GLoggingSystemState->CriticalSection);
    
    return true;
}

void Logging_Shutdown(void)
{
    GLoggingSystemState->bAlive = false;
    GLoggingSystemState->bReady = false;
    Platform_WaitForHandle(GLoggingSystemState->ThreadHandle, -1);
    #if PLATFORM_WINDOWS
    GLoggingSystemState->ThreadHandle = NULL;
    #else
    GLoggingSystemState->ThreadHandle = -1;
    #endif

    Platform_DeleteCriticalSection(GLoggingSystemState->CriticalSection);
    
    GLoggingSystemState = nullptr;
}

u64 Logging_GetMemoryRequirement(void)
{
    u64 LogFileNameSize = sizeof(String) + 256;
    u64 LogFormatStringArena = MAX_LOG_MSG_LENGTH*2;
    
    return sizeof(LoggingSystemState) + LogFileNameSize + LogFormatStringArena + Platform_GetCriticalSectionMemoryRequirement();
}

void Logging_Enable(void)
{
    GLoggingSystemState->bDisabled = false;
}

void Logging_Disable(void)
{
    GLoggingSystemState->bDisabled = true;
}

void Logging_ToggleLogTimeStamp(bool bShow)
{
    GLoggingSystemState->bLogTimestamp = bShow;
}

void Logging_ToggleLogCategory(bool bShow)
{
    GLoggingSystemState->bLogCategory = bShow;
}

void Logging_ToggleLogType(bool bShow)
{
    GLoggingSystemState->bLogType = bShow;
}

void Logging_ToggleLogFile(UNUSED bool bLogToFile)
{
}

void Logging_SetCrashOnFatal(bool bShouldCrash)
{
    GLoggingSystemState->bCrashOnFatal = bShouldCrash;
}

bool Logging_ShouldCrashOnFatal(void)
{
    return GLoggingSystemState->bCrashOnFatal;
}

void LogMessage(u8 LogType, const String LogCat, const String Text, ...)
{
    if (UNLIKELY(GLoggingSystemState->bDisabled))
        return;

    Platform_EnterCriticalSection(GLoggingSystemState->CriticalSection);

    SystemTime TimeNow = Platform_GetSystemLocalTime();

    StringLocal(TimeStamp, 64);
    String_Format(&TimeStamp, StrLit("%hu-%.2hu-%.2hu %.2hu:%.2hu:%.2hu "), 64, TimeNow.Year, TimeNow.Month, TimeNow.Day, TimeNow.Hour, TimeNow.Minute, TimeNow.Second);

    va_list Args;
    va_start(Args, Text);
    String Buffer = {.Data = GLoggingSystemState->Buffer, .Length = 0 };
    String_FormatV(&Buffer, Text, MAX_LOG_MSG_LENGTH, Args);
    va_end(Args);

    Buffer.Data[Buffer.Length] = '\n';

    const String FormattedText = { .Length = Buffer.Length+1, .Data = GLoggingSystemState->Buffer };

    LinearAllocator_Scratch Temp = LinearAllocator_GetScratch(&GLoggingMemoryAllocator);

    StringLocal(LogPrefix, 512);

    if (GLoggingSystemState->bLogTimestamp)
        String_Append(&LogPrefix, TimeStamp);

    if (GLoggingSystemState->bLogCategory)
    {
        String_Append(&LogPrefix, StrLit("["));
        String_Append(&LogPrefix, LogCat);
        String_Append(&LogPrefix, StrLit("] "));
    }

    if (GLoggingSystemState->bLogType)
    {
        String_Append(&LogPrefix, LogTypeString[LogType]);
    }

    String FinalMsg = String_Join(Temp.Allocator, StrViewArrayStatic(2, LogPrefix, FormattedText));

    Platform_ConsoleWrite_CustomLength(FinalMsg.Data, FinalMsg.Length, LogType, LogType > LOG_TYPE_WARNING);

    LinearAllocator_ReleaseScratch(&Temp);

    Platform_ExitCriticalSection(GLoggingSystemState->CriticalSection);
}

void LogDirectMessage(u8 LogType, const String Text, ...)
{
    if (UNLIKELY(GLoggingSystemState->bDisabled))
        return;

    Platform_EnterCriticalSection(GLoggingSystemState->CriticalSection);

    va_list Args;
    va_start(Args, Text);
    String Buffer = {.Data = GLoggingSystemState->Buffer, .Length = 0 };
    u32 Len = (u32)String_FormatV(&Buffer, Text, MAX_LOG_MSG_LENGTH, Args);
    va_end(Args);

    GLoggingSystemState->Buffer[Len+1] = 0;
    Platform_ConsoleWrite_CustomLength(GLoggingSystemState->Buffer, Len, LogType, LogType > LOG_TYPE_WARNING);

    Platform_ExitCriticalSection(GLoggingSystemState->CriticalSection);
}

void LogLineBreak(void)
{
    if (UNLIKELY(GLoggingSystemState->bDisabled))
        return;

    Platform_EnterCriticalSection(GLoggingSystemState->CriticalSection);

    Platform_ConsoleWrite_CustomLength("\n", 1, LOG_TYPE_INFO, false);

    Platform_ExitCriticalSection(GLoggingSystemState->CriticalSection);
}

void Logging_PrintStackTrace(void)
{
    TArray(StackTraceData) Frames;
    Platform_CaptureStackTrace(&GLoggingMemoryAllocator, &Frames); // todo: maybe a separate allocator?
    Logging_ToggleLogTimeStamp(false);
    Logging_ToggleLogCategory(false);
    LOG("%S", StrLit("=============== STACK TRACE ==============="));
    u32 i = 0;
    for each_i (i, f, Frames)
    {
        if (i > 0)
            LOG("[%hi] 0x%x | %S", f.Index, f.Address, f.Name);
        //LOG("-------------------------------------------");
    }
    Logging_ToggleLogTimeStamp(true);
    Logging_ToggleLogCategory(true);
}

#endif
