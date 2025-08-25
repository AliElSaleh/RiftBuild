// Copyright (c) Artisan Softworks 
// Licensed under the BSD 3-Clause License. See the LICENSE file for details.

#ifndef NO_LOG

#ifndef UNITY_BUILD
#include "Log.h"

#include "Allocators.h"

#include "Platform.h"
#include "Filesystem.h"

#include "StringUtils.h"

#include "Array.h"

#include <stdarg.h>
#endif

STRUCT(LoggingSystemState)
{
    FileHandle LogFileHandle;

    String LogFileName;

    u8 Buffer[MAX_LOG_MSG_LENGTH];

    bool bDisabled;
    bool bCrashOnFatal;
    bool bEnableOnError;
    bool bLogTimestamp;
    bool bLogCategory;
    bool bLogType;
    bool bLogToFile;
    bool bPadding1;

    PlatformCriticalSection CriticalSection;
};

static LoggingSystemState* GLoggingSystemState = NULL;
static LinearAllocator GLoggingMemoryAllocator = {0};

static bool Internal_TryOpenLogFile(void)
{
    bool bSuccess = GLoggingSystemState->bLogToFile;

    if (bSuccess)
    {
        if (GLoggingSystemState->LogFileHandle.Data == NULL)
        {
            GLoggingSystemState->LogFileName = String_Reserve(&GLoggingMemoryAllocator, 255);

            SystemTime TimeNow = Platform_GetSystemLocalTime();
            
            StringLocal(TimeStampBuffer, 128);
            String_Format(&TimeStampBuffer, S("%hu-%.2hu-%.2hu-%.2hu.%.2hu.%.2hu"), TimeNow.Year, TimeNow.Month, TimeNow.Day, TimeNow.Hour, TimeNow.Minute, TimeNow.Second);
            
            StringLocal(LogFileName, 512);
            String_Format(&LogFileName, S("RiftBuild-%S.log"), TimeStampBuffer);

            LinearAllocator Scratch = GLoggingMemoryAllocator;
            {
                String FullPath = String_Join(&Scratch, StrArray(S("Logs/"), LogFileName));
                String_Copy(&GLoggingSystemState->LogFileName, FullPath);
                
                if (!Filesystem_Open(GLoggingSystemState->LogFileName, FileMode_Write, &GLoggingSystemState->LogFileHandle))
                {
                    bSuccess = false;

                    StringLocal(FormattedMessage, 256);
                    String_Format(&FormattedMessage, S("Failed to open \"%S\" file for writing\n"), GLoggingSystemState->LogFileName);
                    Platform_ConsoleWrite_CustomLength((char*)FormattedMessage.Data, FormattedMessage.Length, LOG_TYPE_ERROR, true);
                }
            }
        }
    }

    return bSuccess;
}

static void Internal_WriteToLogFile(const String Text)
{
    if (Internal_TryOpenLogFile())
    {
        usize Written = 0;
        if (!Filesystem_WriteLine(GLoggingSystemState->LogFileHandle, Text, &Written))
        {
            StringLocal(FormattedMessage, 256);
            String_Format(&FormattedMessage, S("Failed to write to %S"), GLoggingSystemState->LogFileName);
            Platform_ConsoleWrite_CustomLength((char*)FormattedMessage.Data, FormattedMessage.Length, LOG_TYPE_ERROR, true);
        }
    }
}

NO_DISCARD bool Logging_Initialize(void* Memory, bool bOpenFile)
{
    usize Amount = Logging_GetMemoryRequirement();
    LinearAllocator_Create(Amount, Memory, &GLoggingMemoryAllocator);
    
    GLoggingSystemState = LinearAllocator_Allocate(&GLoggingMemoryAllocator, sizeof(LoggingSystemState));
    GLoggingSystemState->bDisabled = false;
    GLoggingSystemState->bLogTimestamp = true;
    GLoggingSystemState->bLogCategory = true;
    GLoggingSystemState->bLogType = true;
    GLoggingSystemState->bCrashOnFatal = true;
    GLoggingSystemState->bLogToFile = bOpenFile;

    GLoggingSystemState->CriticalSection = LinearAllocator_Allocate(&GLoggingMemoryAllocator, Platform_GetCriticalSectionMemoryRequirement());
    Platform_InitializeCriticalSection(GLoggingSystemState->CriticalSection);

    return true;
}

void Logging_Shutdown(void)
{
    Platform_DeleteCriticalSection(GLoggingSystemState->CriticalSection);
    
    Filesystem_Close(&GLoggingSystemState->LogFileHandle);

    LinearAllocator_Destroy(&GLoggingMemoryAllocator);
    
    GLoggingSystemState = NULL;
}

NO_DISCARD usize Logging_GetMemoryRequirement(void)
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
    bool bDisabled = UNLIKELY(GLoggingSystemState->bDisabled) && !(GLoggingSystemState->bEnableOnError && bIsErrorMessage);
    if (!bDisabled)
    {
        va_list Args = {0};
        va_start(Args, Text);
        String Buffer = {.Data = GLoggingSystemState->Buffer, .Length = 0, .Capacity = MAX_LOG_MSG_LENGTH };
        String_FormatV(&Buffer, Text, Args);
        va_end(Args);

        String_AppendChar(&Buffer, '\n');

        String TrimmedFmt = String_EatChar(Buffer, '\n');
        String TrimmedNewLines = StrSlice(Buffer.Data, Buffer.Length - TrimmedFmt.Length);

        // -------------------

        StringLocal(LogPrefix, 128);
        {
            //if (GLoggingSystemState->bComfyMode)
            //{
                //String_Append(&LogPrefix, S(" "));
            //}

            if (GLoggingSystemState->bLogTimestamp)
            {
                SystemTime TimeNow = Platform_GetSystemLocalTime();
                StringLocal(TimeStamp, 64);
                String_Format(&TimeStamp, S("%hu-%.2hu-%.2hu %.2hu:%.2hu:%.2hu "), TimeNow.Year, TimeNow.Month, TimeNow.Day, TimeNow.Hour, TimeNow.Minute, TimeNow.Second);
                String_Append(&LogPrefix, TimeStamp);
            }

            if (GLoggingSystemState->bLogCategory)
            {
                String_Append(&LogPrefix, S("["));
                String_Append(&LogPrefix, LogCat);
                String_Append(&LogPrefix, S("] "));
            }

            if (GLoggingSystemState->bLogType)
            {
                static const String LogTypeString[7] = {SC("[INFO] "), SC("[SUCCESS] "), SC("[WARNING] "), SC("[ERROR] "), SC("[FATAL] "), SC(""), SC("")};
                String_Append(&LogPrefix, LogTypeString[LogType]);
            }
        }

        // -------------------

        LinearAllocator Scratch = GLoggingMemoryAllocator;
        {
            String FinalMsg = String_Join(&Scratch, StrArray(TrimmedNewLines, LogPrefix, TrimmedFmt));

            Platform_ConsoleWrite_CustomLength((char*)FinalMsg.Data, FinalMsg.Length, LogType, LogType > LOG_TYPE_WARNING);
            Internal_WriteToLogFile(FinalMsg);
        }
    }
}

void LogDirectMessage(u8 LogType, const String Text, ...)
{
    bool bIsErrorMessage = (LogType == LOG_TYPE_ERROR || LogType == LOG_TYPE_FATAL);
    bool bDisabled = UNLIKELY(GLoggingSystemState->bDisabled) && !(GLoggingSystemState->bEnableOnError && bIsErrorMessage);
    if (!bDisabled)
    {
        va_list Args = {0};
        va_start(Args, Text);
        String Buffer = {.Data = GLoggingSystemState->Buffer, .Length = 0, .Capacity = MAX_LOG_MSG_LENGTH };
        String_FormatV(&Buffer, Text, Args);
        va_end(Args);

        GLoggingSystemState->Buffer[Buffer.Length+1] = 0;
        Platform_ConsoleWrite_CustomLength((char*)GLoggingSystemState->Buffer, Buffer.Length, LogType, LogType > LOG_TYPE_WARNING);

        Internal_WriteToLogFile(StrSlice(GLoggingSystemState->Buffer, Buffer.Length));
    }
}

void LogLineBreak(void)
{
    bool bDisabled = UNLIKELY(GLoggingSystemState->bDisabled);
    if (!bDisabled)
    {
        const String NewLine = S("\n");

        Platform_ConsoleWrite_CustomLength((char*)NewLine.Data, NewLine.Length, LOG_TYPE_INFO, false);
        Internal_WriteToLogFile(NewLine);
    }
}
#endif
