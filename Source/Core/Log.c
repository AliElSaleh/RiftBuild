// Copyright (c) 2024 Ali El Saleh 

#ifndef NO_LOG
#include "Log.h"

#include "Platform/Platform.h"
#include "Platform/Filesystem.h"

#include "String/StringUtils.h"

#include "Structures/Array.h"

#include <stdarg.h>

// TODO: option for single threaded logging
// TODO: right now the logging thread does nothing when fast log is disabled this is a no no

#ifdef FAST_LOG
#define MAX_SINK_BUFFER_SIZE Megabytes(1) // maximum amount of data that can be logged at once
#endif

STRUCT(LoggingSystemState)
{
    FileHandle LogFileHandle;
    PlatformHandle ThreadHandle;

    String LogFileName;

    char Buffer[MAX_LOG_MSG_LENGTH];

    #ifdef FAST_LOG
    char SinkBuffer[MAX_SINK_BUFFER_SIZE];
    u32 SinkHead;
    #endif

    bool bDisabled;
    bool bCrashOnFatal;
    bool bEnableOnError;
    bool bAlive;
    bool bReady;
    bool bLogTimestamp;
    bool bLogCategory;
    bool bLogType;
    bool bLogToFile;

    PlatformCriticalSection CriticalSection;
};

static const String LogTypeString[6] = {SC("[INFO] "), SC("[SUCCESS] "), SC("[WARNING] "), SC("[ERROR] "), SC("[FATAL] "), SC("")};

static LoggingSystemState* GLoggingSystemState = NULL;
static LinearAllocator GLoggingMemoryAllocator = {0};
static void* GLoggingSystemMemory = NULL;

internal bool Internal_TryOpenLogFile(void)
{
    if (!GLoggingSystemState->bLogToFile)
    {
        return false;
    }

    if (GLoggingSystemState->LogFileHandle.Data != NULL)
    {
        return true;
    }

    GLoggingSystemState->LogFileName.Data = LinearAllocator_Allocate(&GLoggingMemoryAllocator, sizeof(char) * 256);

    SystemTime TimeNow = Platform_GetSystemLocalTime();
    
    StringLocal(TimeStampBuffer, 128);
    String_Format(&TimeStampBuffer, S("%hu-%.2hu-%.2hu-%.2hu.%.2hu.%.2hu"), 128, TimeNow.Year, TimeNow.Month, TimeNow.Day, TimeNow.Hour, TimeNow.Minute, TimeNow.Second);
    
    StringLocal(LogFileName, 512);
    String_Format(&LogFileName, S("RiftBuild-%S.log"), 512, TimeStampBuffer);

    LinearAllocator_Scratch Temp = LinearAllocator_GetScratch(&GLoggingMemoryAllocator);
    String FullPath = String_Join(Temp.Allocator, StrArray(S("Logs/"), LogFileName));
    String_Copy(&GLoggingSystemState->LogFileName, FullPath);
    LinearAllocator_ReleaseScratch(&Temp);
    
    if (!Filesystem_Open(GLoggingSystemState->LogFileName, FileMode_Write, &GLoggingSystemState->LogFileHandle))
    {
        StringLocal(FormattedMessage, 256);
        String_Format(&FormattedMessage, S("Failed to open %S file for writing\n"), 256, GLoggingSystemState->LogFileName);
        Platform_ConsoleWrite_CustomLength(FormattedMessage.Data, FormattedMessage.Length, LOG_TYPE_ERROR, true);

        return false;
    }

    return true;
}

internal void Internal_WriteToLogFile(char* Text, u32 Length)
{
    //PROFILE_FUNCTION()
    {
        if (!GLoggingSystemState->bLogToFile)
        {
            return;
        }

        if (!Internal_TryOpenLogFile())
        {
            return;
        }

        if (UNLIKELY(Length == 0))
        {
            return;
        }

        u64 Written = 0;

        if (!Filesystem_WriteLine(GLoggingSystemState->LogFileHandle, StrSlice(Text, Length), &Written))
        {
            StringLocal(FormattedMessage, 256);
            u64 Len = (u64)String_Format(&FormattedMessage, S("Failed to write to %S"), 256, GLoggingSystemState->LogFileName);
            Platform_ConsoleWrite_CustomLength(FormattedMessage.Data, Len, LOG_TYPE_ERROR, true);
        }
    }
}

#ifdef FAST_LOG
internal void Internal_LogFlush(void)
{
    GLoggingSystemState->SinkBuffer[Min(GLoggingSystemState->SinkHead, MAX_SINK_BUFFER_SIZE)] = 0;
    Platform_ConsoleWrite_CustomLength(GLoggingSystemState->SinkBuffer, GLoggingSystemState->SinkHead, LOG_TYPE_INFO, false);
    Internal_WriteToLogFile(GLoggingSystemState->SinkBuffer, GLoggingSystemState->SinkHead);
    GLoggingSystemState->SinkHead = 0;
    GLoggingSystemState->bReady = false;
}
#endif

internal u32 Internal_Main_LoggingThread(UNUSED void* lpParameter)
{
#ifdef FAST_LOG
    while (GLoggingSystemState->bAlive)
    {
        if (GLoggingSystemState->bReady && UNLIKELY(GLoggingSystemState->SinkHead > 0))
        {
            PROFILE_SCOPE("Log Flush", GLoggingSystemState->ThreadHandle)
            {
                Internal_LogFlush();
            }
        }
    }

    if (GLoggingSystemState->SinkHead > 0)
    {
        PROFILE_SCOPE("Log Flush", &GLoggingSystemState->ThreadHandle)
        {
            Internal_LogFlush();
        }
    }
#endif
    
    return 0;
}

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
    GLoggingSystemState->bLogToFile = bOpenFile;

    GLoggingSystemState->CriticalSection = LinearAllocator_Allocate(&GLoggingMemoryAllocator, Platform_GetCriticalSectionMemoryRequirement());
    Platform_InitializeCriticalSection(GLoggingSystemState->CriticalSection);

    GLoggingSystemState->ThreadHandle = Platform_CreateThread(S("Logging Thread"), NULL, Internal_Main_LoggingThread, GLoggingSystemState);
    
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
    
    Filesystem_Close(&GLoggingSystemState->LogFileHandle);
    
    GLoggingSystemState = nullptr;
}

usize Logging_GetMemoryRequirement(void)
{
    usize LogFileNameSize = sizeof(String) + 256;
    usize LogFormatStringArena = MAX_LOG_MSG_LENGTH*2;
    
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

void Logging_ToggleLogFile(bool bLogToFile)
{
    GLoggingSystemState->bLogToFile = bLogToFile;
}

void Logging_SetCrashOnFatal(bool bShouldCrash)
{
    GLoggingSystemState->bCrashOnFatal = bShouldCrash;
}

void Logging_ToggleEnableOnError(bool bEnable)
{
    GLoggingSystemState->bEnableOnError = bEnable;
}

bool Logging_ShouldCrashOnFatal(void)
{
    return GLoggingSystemState->bCrashOnFatal;
}

void LogMessage(u8 LogType, const String LogCat, const String Text, ...)
{
    bool bIsErrorMessage = (LogType == LOG_TYPE_ERROR || LogType == LOG_TYPE_FATAL);
    if (UNLIKELY(GLoggingSystemState->bDisabled) && !(GLoggingSystemState->bEnableOnError && bIsErrorMessage))
        return;

    Platform_EnterCriticalSection(GLoggingSystemState->CriticalSection);

#ifdef FAST_LOG
    if (UNLIKELY(GLoggingSystemState->SinkHead >= MAX_SINK_BUFFER_SIZE))
    {
        Internal_LogFlush();
    }
#endif

    SystemTime TimeNow = Platform_GetSystemLocalTime();

    StringLocal(TimeStamp, 64);
    String_Format(&TimeStamp, S("%hu-%.2hu-%.2hu %.2hu:%.2hu:%.2hu "), 64, TimeNow.Year, TimeNow.Month, TimeNow.Day, TimeNow.Hour, TimeNow.Minute, TimeNow.Second);

    va_list Args;
    va_start(Args, Text);
    String Buffer = {.Data = GLoggingSystemState->Buffer, .Length = 0 };
    String_FormatV(&Buffer, Text, MAX_LOG_MSG_LENGTH, Args);
    va_end(Args);

    Buffer.Data[Buffer.Length] = '\n';

    String FormattedText = { .Length = Buffer.Length+1, .Data = Buffer.Data };
    String TrimmedFmt = String_EatChar(FormattedText, '\n');
    String TrimmedNewLines = StrSlice(FormattedText.Data, (u32)(TrimmedFmt.Data - FormattedText.Data));

    LinearAllocator_Scratch Temp = LinearAllocator_GetScratch(&GLoggingMemoryAllocator);

    StringLocal(LogPrefix, 512);

    //if (GLoggingSystemState->bComfyMode)
        //String_Append(&LogPrefix, S(" "));

    if (GLoggingSystemState->bLogTimestamp)
        String_Append(&LogPrefix, TimeStamp);

    if (GLoggingSystemState->bLogCategory)
    {
        String_Append(&LogPrefix, S("["));
        String_Append(&LogPrefix, LogCat);
        String_Append(&LogPrefix, S("] "));
    }

    if (GLoggingSystemState->bLogType)
    {
        String_Append(&LogPrefix, LogTypeString[LogType]);
    }

    String FinalMsg = String_Join(Temp.Allocator, StrArray(TrimmedNewLines, LogPrefix, TrimmedFmt));

#ifdef FAST_LOG
    Platform_MemCopy(&GLoggingSystemState->SinkBuffer[GLoggingSystemState->SinkHead], FinalMsg.Data, FinalMsg.Length);
    GLoggingSystemState->SinkHead += FinalMsg.Length;
#else
    Platform_ConsoleWrite_CustomLength(FinalMsg.Data, FinalMsg.Length, LogType, LogType > LOG_TYPE_WARNING);

    Internal_WriteToLogFile(FinalMsg.Data, FinalMsg.Length);
#endif

    LinearAllocator_ReleaseScratch(&Temp);

    Platform_ExitCriticalSection(GLoggingSystemState->CriticalSection);
}

void LogDirectMessage(u8 LogType, const String Text, ...)
{
    bool bIsErrorMessage = (LogType == LOG_TYPE_ERROR || LogType == LOG_TYPE_FATAL);
    if (UNLIKELY(GLoggingSystemState->bDisabled) && !(GLoggingSystemState->bEnableOnError && bIsErrorMessage))
        return;

    Platform_EnterCriticalSection(GLoggingSystemState->CriticalSection);

    #ifdef FAST_LOG
    if (GLoggingSystemState->SinkHead >= MAX_SINK_BUFFER_SIZE)
    {
        Internal_LogFlush();
    }
    #endif

#ifdef FAST_LOG
    va_list Args;
    va_start(Args, Text);
    String Buffer = {.Data = &GLoggingSystemState->SinkBuffer[GLoggingSystemState->SinkHead], .Length = 0 };
    u32 Len = (u32)String_FormatV(&Buffer, Text, MAX_LOG_MSG_LENGTH, Args);
    va_end(Args);

    GLoggingSystemState->SinkHead += Len;
#else
    va_list Args;
    va_start(Args, Text);
    String Buffer = {.Data = GLoggingSystemState->Buffer, .Length = 0 };
    u32 Len = (u32)String_FormatV(&Buffer, Text, MAX_LOG_MSG_LENGTH, Args);
    va_end(Args);

    GLoggingSystemState->Buffer[Len+1] = 0;
    Platform_ConsoleWrite_CustomLength(GLoggingSystemState->Buffer, Len, LogType, LogType > LOG_TYPE_WARNING);

    Internal_WriteToLogFile(GLoggingSystemState->Buffer, Len);
#endif

    Platform_ExitCriticalSection(GLoggingSystemState->CriticalSection);
}

void LogLineBreak(void)
{
    if (UNLIKELY(GLoggingSystemState->bDisabled))
        return;

    Platform_EnterCriticalSection(GLoggingSystemState->CriticalSection);

    #ifdef FAST_LOG
    if (GLoggingSystemState->SinkHead >= MAX_SINK_BUFFER_SIZE)
    {
        Internal_LogFlush();
    }
    #endif

#ifdef FAST_LOG
    GLoggingSystemState->SinkBuffer[GLoggingSystemState->SinkHead] = '\n';
    GLoggingSystemState->SinkHead += 1;
#else
    Platform_ConsoleWrite_CustomLength("\n", 1, LOG_TYPE_INFO, false);
    Internal_WriteToLogFile("\n", 1);
#endif

    Platform_ExitCriticalSection(GLoggingSystemState->CriticalSection);
}

#ifdef FAST_LOG
void Logging_Flush(void)
{
    if (!GLoggingSystemState->bReady && UNLIKELY(GLoggingSystemState->SinkHead > 0))
        GLoggingSystemState->bReady = true;

    return;
    
    /*
    // Only allowed on main thread!
    ASSERT(LIKELY(Platform_GetCurrentThreadID() == Platform_GetMainThreadID()))

    if (UNLIKELY(GLoggingSystemState->SinkHead > 0))
    {
        Internal_LogFlush();
    }
    */
}
#endif


void Logging_PrintStackTrace(void)
{
    TArray(StackTraceData) Frames;
    Platform_CaptureStackTrace(&GLoggingMemoryAllocator, &Frames); // todo: maybe a separate allocator?
    Logging_ToggleLogTimeStamp(false);
    Logging_ToggleLogCategory(false);
    LOG("%S", S("=============== STACK TRACE ==============="));
    u32 i = 0;
    for each_i (i, StackTraceData, f, Frames)
    {
        if (i > 0)
            LOG("[%hi] 0x%x | %S", f.Index, f.Address, f.Name);
        //LOG("-------------------------------------------");
    }
    Logging_ToggleLogTimeStamp(true);
    Logging_ToggleLogCategory(true);
}

#endif
