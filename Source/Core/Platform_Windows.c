// Copyright (c) Artisan Softworks
// Licensed under the BSD 3-Clause License. See the LICENSE file for details.

#ifndef UNITY_BUILD
#include "Platform.h"
#endif

#if PLATFORM_WINDOWS

#ifndef UNITY_BUILD
#include "Uuid.h"
#include "Filesystem.h"
#include "StringUtils.h"
#include "Array.h"
#include <stdarg.h>
#endif

#include "Win32Types.h"

//#include <gs_support.c>

//#include <Windows.h>
//#include <strsafe.h>
//#include <Shlwapi.h>
//#include <DbgHelp.h>
//#include <shellapi.h>
//#include <psapi.h>
//#include <Shlobj.h>

/*
PRAGMA_DISABLE_MISSING_PROTOTYPES_WARNING

#if defined(_M_IX86)
    #define GSAPI __fastcall
#else
    #define GSAPI __cdecl
#endif

uptr __security_cookie = 0;
uptr __security_cookie_complement = 0;
void GSAPI __security_check_cookie(_In_ uintptr_t _StackCookie)
{
    if (_StackCookie != __security_cookie)
    {
        DEBUG_BREAK();
    }
}

PRAGMA_ENABLE_WARNINGS
*/

ENUM(WinConsoleForegroundColors)
{
    FG_BLACK = 0,
    FG_BLUE = 1,
    FG_GREEN = 2,
    FG_CYAN = 3,
    FG_RED = 4,
    FG_MAGENTA = 5,
    FG_BROWN = 6,
    FG_LIGHTGRAY = 7,
    FG_GRAY = 8,
    FG_LIGHTBLUE = 9,
    FG_LIGHTGREEN = 10,
    FG_LIGHTCYAN = 11,
    FG_LIGHTRED = 12,
    FG_LIGHTMAGENTA = 13,
    FG_YELLOW = 14,
    FG_WHITE = 15,
    BG_NAVYBLUE = 16,
    BG_GREEN = 32,
    BG_TEAL = 48,
    BG_MAROON = 64,
    BG_PURPLE = 80,
    BG_OLIVE = 96,
    BG_SILVER = 112,
    BG_GRAY = 128,
    BG_BLUE = 144,
    BG_LIME = 160,
    BG_CYAN = 176,
    BG_RED = 192,
    BG_MAGENTA = 208,
    BG_YELLOW = 224,
    BG_WHITE = 240
};

C_LINKAGE_BEGIN
int _fltused = 0;
C_LINKAGE_END

#define CONSOLE_MUTE_COLOR (FOREGROUND_INTENSITY)
#define CONSOLE_INFO_COLOR (FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE)
#define CONSOLE_SUCCESS_COLOR (FOREGROUND_INTENSITY | FOREGROUND_GREEN)
#define CONSOLE_WARNING_COLOR 14
#define CONSOLE_ERROR_COLOR (FOREGROUND_INTENSITY | FOREGROUND_RED)
#define CONSOLE_FATAL_COLOR (FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | BACKGROUND_RED)

static u8 ArgumentBuffer[128][512] = {0};

static String GArgV[128] = {0};
static i32 GArgC = 0;

#ifndef NO_LOG 
static void LogLastError(const String Prefix)
{
    TCHAR Message[4096] = {0};
    DWORD Code = GetLastError();
    u32 Len = FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, Code,
                            MAKELANGID(0, 0x01), // SUBLANG_DEFAULT
                            (LPTSTR)&Message, sizeof(TCHAR)*4095,
                            NULL);

    StringLocal(FormattedMessage, 4096);
    String_Format(&FormattedMessage, S("%S\n        Error Code: %i\n        Reason: %S"), Prefix, Code, StrSlice((uchar*)Message, Len));
    Platform_ConsoleWrite_CustomLength((char*)FormattedMessage.Data, FormattedMessage.Length, 3, true);
}
#else
#define LogLastError(...)
#endif

void Platform_PreInitialize(void)
{
    //__security_init_cookie();

    ULONG Stack = Mebibytes(4);
    bool bStackGuarantee = SetThreadStackGuarantee(&Stack);
    if (!bStackGuarantee)
    {
        String Msg = S("Failed to guarantee stack size of 4MiB\n");
        Platform_ConsoleWrite_CustomLength((const char*)Msg.Data, Msg.Length, 4, true);
        _Crash_;
    }

    i32 NumArgs = 0;
    wchar** ArgsW = CommandLineToArgvW(GetCommandLineW(), &NumArgs);

    GArgC = NumArgs;

    for (u16 i = 0; i < 128; i++)
    {
        GArgV[i].Data = ArgumentBuffer[i];
        GArgV[i].Length = 0;
        GArgV[i].Capacity = 511;
    }

    for (i32 i = 1; i < NumArgs; i++)
    {
        u8* Buffer = ArgumentBuffer[(i-1)]; // &ArgumentBuffer[(i-1)*1024];

        register u32 Len = 0;
        while (Len < 512 && ArgsW[i][Len] != 0) // arbitrary max length of 512
        {
            Buffer[Len] = (uchar)ArgsW[i][Len];
            Len++; 
        }

        GArgV[i-1].Length = Len;
        GArgV[i-1].Capacity = Len;
    }

    (void)LocalFree(ArgsW);
}

NO_DISCARD bool Platform_CreateMutex(PlatformMutex* OutMutex)
{
    bool bSuccess = false;

    if (OutMutex != NULL)
    {
        HANDLE M = CreateMutexA(NULL, TRUE, NULL);
        if (M)
        {
            OutMutex->Handle = M;
            OutMutex->Name = String_Null();

            bool bError = GetLastError() == ERROR_ALREADY_EXISTS;
            bSuccess = !bError;
        }
    }

    return bSuccess;
}

NO_DISCARD bool Platform_CreateNamedMutex(const String Name, PlatformMutex* OutMutex)
{
    bool bSuccess = false;

    if (ALWAYS(Name.Length > 0) && OutMutex != NULL)
    {
        u32 Diff = Name.Length > 255 ? Name.Length - 255 : 0; // clamp to 255 characters
        String ClampedName = StrShiftF(Name, Diff);

        HANDLE M = CreateMutexA(NULL, TRUE, ClampedName.Length == 0 ? NULL : (char*)ClampedName.Data);
        if (M)
        {
            OutMutex->Handle = M;
            OutMutex->Name = ClampedName;

            bool bError = GetLastError() == ERROR_ALREADY_EXISTS;
            bSuccess = !bError;
        }
    }

    return bSuccess;
}

NO_DISCARD bool Platform_ReleaseMutex(PlatformMutex* Mutex)
{
    bool bResult = false;

    if (Mutex != NULL)
    {
        bResult = ReleaseMutex(Mutex->Handle);
        if (bResult)
        {
            bResult = CloseHandle(Mutex->Handle);
        }
    }

    return bResult;
}

NO_DISCARD u32 Platform_GetConsoleProcessCount(void)
{
    DWORD Processes[4] = {0};
    DWORD Count = GetConsoleProcessList(Processes, 4);
    return Count;
}

void Platform_Abort(u32 ExitCode)
{
    ExitProcess(ExitCode);
}

NO_DISCARD StringArray Platform_GetCommandLineArgs(void)
{
    StringArray Args = {0};
    Args.Num = (u32)(GArgC-1 <= 0 ? 0 : (GArgC-1 < 128 ? GArgC-1 : 128));
    Args.List = GArgV;
    return Args;
}

NO_DISCARD f64 Platform_GetClockFrequency(void)
{
    LARGE_INTEGER Frequency = {0};
    BOOL bSuccess = QueryPerformanceFrequency(&Frequency);
    const f64 ClockFrequency = bSuccess ? 1.0/(f64)Frequency.QuadPart : 0.0;
    return ClockFrequency;
}

PRAGMA_DISABLE_WARNINGS

#if COMPILER_CLANG
#pragma clang diagnostic ignored "-Wincompatible-library-redeclaration"
#pragma clang diagnostic ignored "-Wsign-conversion"
#pragma clang diagnostic ignored "-Wunused-function"
#pragma clang diagnostic ignored "-Wmissing-prototypes"
#elif COMPILER_GCC
#pragma GCC diagnostic ignored "-Wbuiltin-declaration-mismatch"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wmissing-prototypes"
#endif

#if COMPILER_MSVC
#pragma intrinsic(memset, memcpy, memmove, memcmp)
#pragma function(memset, memcpy, memmove, memcmp)
#endif

ASAN_NO_SANITIZE_ADDRESS void* memset(void *dst, int c, SIZE_T len)
{
    register volatile u8* dp = dst;
    register SIZE_T length = len;

    while (length--)
    {
        *dp++ = (u8)c;
    }

    return dst;
}

ASAN_NO_SANITIZE_ADDRESS void* memcpy(void* restrict dst, const void* restrict src, SIZE_T len)
{
    register volatile u8* dp = dst;
    register const u8* sp = src;
    register SIZE_T length = len;

    while (length--)
    {
        *dp++ = *sp++;
    }

    return dst;
}

ASAN_NO_SANITIZE_ADDRESS void* memmove(void* dst, const void* src, SIZE_T len)
{
    register volatile u8* dp = dst;
    register const u8* sp = src;

    if (sp < dp)
    {
        for (dp += len, sp += len; len--;)
        {
            *--dp = *--sp;
        }
    }
    else
    {
        while (len--)
        {
            *dp++ = *sp++;
        }
    }

    return dst;
}

ASAN_NO_SANITIZE_ADDRESS i32 memcmp(const void* s1, const void* s2, SIZE_T len)
{
    register const u8* p1 = (const u8*)s1;
    register const u8* p2 = (const u8*)s2;

    i32 Result = 0;
    for (register usize i = 0; i < len; i++)
    {
        if (p1[i] < p2[i])
        {
            Result = -1;
        }
        else if (p1[i] > p2[i]) 
        {
            Result = 1;
        }
        else
        {
            // no action required
        }

        if (Result != 0)
        {
            break;
        }
    }

    return Result;
}

PRAGMA_ENABLE_WARNINGS

NO_DISCARD void* Platform_MemAlloc(usize Size)
{
    DWORD dwFlags = HEAP_CREATE_ALIGN_16;
    void* Block = HeapAlloc(GetProcessHeap(), dwFlags, Size);
    return Block;
}

NO_DISCARD void* Platform_MemAllocZero(usize Size)
{
    DWORD dwFlags = HEAP_ZERO_MEMORY | HEAP_CREATE_ALIGN_16;
    void* Block = HeapAlloc(GetProcessHeap(), dwFlags, Size);
    return Block;
}

NO_DISCARD void* Platform_MemReAlloc(void* Block, usize Size)
{
    void* Result;

    if (!Block)
    {
        Result = Platform_MemAlloc(Size);
    }
    else
    {
        DWORD dwFlags = HEAP_ZERO_MEMORY;
        Result = HeapReAlloc(GetProcessHeap(), dwFlags, Block, Size);
    }

    return Result;
}

void Platform_MemFree(void* Block)
{
    (void)HeapFree(GetProcessHeap(), 0, Block);
}

void Platform_MemZero(void* Block, usize Size)
{
    (void)RtlZeroMemory(Block, Size);
}

void Platform_MemCopy(void* Dest, const void* Source, usize Size)
{
    (void)RtlCopyMemory(Dest, Source, Size);
}

void Platform_MemMove(void* Dest, const void* Source, usize Size)
{
    (void)RtlMoveMemory(Dest, Source, Size);
}

void Platform_MemSet(void* Dest, i32 Value, usize Size)
{
    (void)RtlFillMemory(Dest, Size, (BYTE)Value);
}

NO_DISCARD bool Platform_SetWorkingDirectory(const String Path)
{
    StringLocal(Copy, MAX_PATH_LENGTH);
    String_Copy(&Copy, Path);

    BOOL bResult = SetCurrentDirectoryA((char*)Copy.Data);
    return bResult;
}

void Platform_ConsoleWrite(const char* Message, u8 Color, bool bIsError)
{
    Platform_ConsoleWrite_CustomLength(Message, String_GetLength_Fast(Message), Color, bIsError);
}

void Platform_ConsoleWrite_CustomLength(const char* Message, u32 Length, u8 Color, bool bIsError)
{
    UNUSED_PARAM(bIsError);

    DWORD OutputHandle = STD_ERROR_HANDLE;// bIsError ? STD_ERROR_HANDLE : STD_OUTPUT_HANDLE;
    HANDLE ConsoleHandle = GetStdHandle(OutputHandle);

    static u8 GConsoleColorLevels[7] = { CONSOLE_INFO_COLOR, CONSOLE_SUCCESS_COLOR, CONSOLE_WARNING_COLOR, CONSOLE_ERROR_COLOR, CONSOLE_FATAL_COLOR, CONSOLE_INFO_COLOR, CONSOLE_MUTE_COLOR };
    const u8 ConsoleColor = GConsoleColorLevels[Color];

    // SetConsoleTextAttribute is slow, so only call it when the color changes
    if (ConsoleColor != CONSOLE_INFO_COLOR)
    {
        xx SetConsoleTextAttribute(ConsoleHandle, ConsoleColor);
    }

    bool bIgnoreNewLine = Color == 4 && Message[Length-1] == '\n';
    if (UNLIKELY(bIgnoreNewLine))
    {
        Length--;
    }

    #if _DEBUG
    {
        StringLocal(Temp, 32768);
        String_Copy(&Temp, StrSlice((uchar*)Message, Length));
        OutputDebugString((char*)Temp.Data);
    }
    #endif

    xx WriteConsole(ConsoleHandle, Message, (DWORD)Length, NULL, 0);

    // Reset back to white
    if (ConsoleColor != CONSOLE_INFO_COLOR)
    {
        xx SetConsoleTextAttribute(ConsoleHandle, CONSOLE_INFO_COLOR);
    }

    if (UNLIKELY(bIgnoreNewLine))
    {
        #if _DEBUG
        OutputDebugString("\n");
        #endif

        xx WriteConsole(ConsoleHandle, "\n", 1, NULL, 0);
    }
}

NO_DISCARD f64 Platform_GetAbsoluteTime(void)
{
    LARGE_INTEGER Frequency = {0}, Now = {0};

    BOOL bSuccess = QueryPerformanceFrequency(&Frequency);
    if (bSuccess)
    {
        bSuccess = QueryPerformanceCounter(&Now);
    }

    f64 Result = bSuccess ? (f64)Now.QuadPart * (1.0/(f64)Frequency.QuadPart) : 0.0;
    return Result;
}

NO_DISCARD SystemTime Platform_GetSystemLocalTime(void)
{
    SYSTEMTIME SysTime = {0};
    GetLocalTime(&SysTime);

    u16 DayOfYear = Platform_GetDayOfYear(SysTime.wDay, SysTime.wMonth, SysTime.wYear);

    SystemTime Result  = {0};
    Result.Year        = SysTime.wYear;
    Result.Month       = SysTime.wMonth;
    Result.Week        = DayOfYear / 7;
    Result.DayOfYear   = DayOfYear;
    Result.DayOfWeek   = SysTime.wDayOfWeek;
    Result.Day         = SysTime.wDay;
    Result.Hour        = SysTime.wHour;
    Result.Minute      = SysTime.wMinute;
    Result.Second      = SysTime.wSecond;
    Result.Millisecond = SysTime.wMilliseconds;

    return Result;
}

NO_DISCARD bool Platform_GetTimeZone(String* OutTimeZone)
{
    TIME_ZONE_INFORMATION TimeZoneInfo = {0};
    DWORD Result = GetTimeZoneInformation(&TimeZoneInfo);

    if (Result == 1) // TIME_ZONE_ID_STANDARD
    {
        String16 StandardNameWide = CStr16(TimeZoneInfo.StandardName);
        String_ToNarrow(StandardNameWide, OutTimeZone);
    }
    else if (Result == 2) // TIME_ZONE_ID_DAYLIGHT
    {
        String16 DaylightNameWide = CStr16(TimeZoneInfo.DaylightName);
        String_ToNarrow(DaylightNameWide, OutTimeZone);
    }
    else
    {
        // no action required
    }
    
    return Result > 0;
}

void Platform_Wait(f64 ms)
{
    if (ms > 0)
    {
        LARGE_INTEGER Frequency = {0}, Now = {0};
        BOOL bSuccess = QueryPerformanceCounter(&Now);
        if (bSuccess)
        {
            bSuccess = QueryPerformanceFrequency(&Frequency);
        }

        if (bSuccess)
        {
            f64 Start = (f64)Now.QuadPart * (1.0/(f64)Frequency.QuadPart);
            f64 Target = ms/1000.0;

            while (1)
            {
                (void)QueryPerformanceCounter(&Now);
                if ((((f64)Now.QuadPart * (1.0/(f64)Frequency.QuadPart)) - Start) >= Target)
                {
                    break;
                }
            }
        }
    }
}

void Platform_Sleep(u32 ms)
{
    Sleep(ms);
}

NO_DISCARD u64 Platform_GetCurrentThreadID(void)
{
    return GetCurrentThreadId();
}

NO_DISCARD u64 Platform_GetMainThreadID(void)
{
    return GetCurrentThreadId();
}

void Platform_GetComputerName(String* OutName)
{
    constant { MAX_NAME_LENGTH = 256 };
	local_persist char Result[MAX_NAME_LENGTH] = {0};

    BOOL bSuccess = Result[0] != 0;
	if (!bSuccess)
	{
		DWORD Size = MAX_NAME_LENGTH;
		bSuccess = GetComputerName(Result, &Size);
	}

    if (bSuccess && OutName)
    {
        String_Copy(OutName, CStrEx(Result, MAX_NAME_LENGTH));
    }
}

void Platform_GetFriendlyComputerName(String* OutName)
{
    constant { MAX_NAME_LENGTH = 256 };
	local_persist char Result[MAX_NAME_LENGTH] = {0};

    BOOL bSuccess = Result[0] != 0;
	if (!bSuccess)
	{
		DWORD Size = MAX_NAME_LENGTH;
		bSuccess = GetComputerNameEx(ComputerNamePhysicalDnsHostname, Result, &Size);
	}

    if (bSuccess && OutName)
    {
        String_Copy(OutName, CStrEx(Result, MAX_NAME_LENGTH));
    }
}

NO_DISCARD bool Platform_GetAccountName(String* OutName)
{
    u8 UserName[256] = {0};
    DWORD Size = 255;
    BOOL bResult = GetUserName((char*)UserName, &Size);
    if (bResult)
    {
        String_Copy(OutName, StrSlice(UserName, Size-1));
    }
    else
    {
        LogLastError(S("Failed to get the current user name"));
    }

    return bResult;
}

NO_DISCARD bool Platform_GetUserName(String* OutName)
{
    bool bSuccess = false;

    const i32 id = 0x0028; // USERPROFILE  CSIDL_PROFILE

    char Path[MAX_PATH] = {0};
    if (SUCCEEDED(SHGetFolderPath(NULL, id, NULL, 0, Path)))
    {
        String Name = CStrEx(Path, MAX_PATH);

        u32 LastSlash = 0;
        if (String_IndexOfLastPathSlash(Name, &LastSlash))
        {
            String_Copy(OutName, StrShiftF(Name, LastSlash+1));
        }
        else
        {
            String_Copy(OutName, Name);
        }

        bSuccess = true;
    }

    return bSuccess;
}

NO_DISCARD bool Platform_GetUserDirectory(String* OutDirectory)
{
    bool bSuccess = false;

    const i32 id = 0x0028; // USERPROFILE  CSIDL_PROFILE

    char Path[MAX_PATH] = {0};
    if (SUCCEEDED(SHGetFolderPath(NULL, id, NULL, 0, Path)))
    {
        String_Copy(OutDirectory, CStr(Path));

        bSuccess = true;
    }

    return bSuccess;
}

void Platform_GetHomeDirectory(String* OutDirectory)
{
    (void)Platform_GetUserDirectory(OutDirectory);
}

NO_DISCARD bool Platform_GetCurrentProcessName(String* OutName)
{
    TCHAR FileName[MAX_PATH] = {0};
    u32 Len = GetModuleFileName(NULL, FileName, MAX_PATH);

    bool bSuccess = false;

    for (u32 i = Len; i > 0; i--)
    {
        if (FileName[i] == '\\')
        {
            String_Copy(OutName, StrSlice((uchar*)&FileName[i+1], Len-i-1));
            bSuccess = true;
            break;
        }
    }

    return bSuccess;
}

NO_DISCARD u64 Platform_GetCurrentProcessID(void)
{
    return GetCurrentProcessId();
}

void Platform_GetWorkingDirectory(String* OutPath)
{
    u32 Len = GetCurrentDirectory(OutPath->Capacity, (char*)OutPath->Data);
    OutPath->Length = Len;
}

NO_DISCARD bool Platform_GetEnvironmentVariableValue(String Name, String* OutVariable)
{
#ifdef UNICODE
    String16Local(NameWide, 2048);
    String_ToWide(Name, &NameWide);

    String16Local(TempBuffer, 2048);

    DWORD Len = GetEnvironmentVariable(NameWide.Data, TempBuffer.Data, TempBuffer.Capacity*2);
    TempBuffer.Length = Len;

    String_ToNarrow(TempBuffer, OutVariable);

    return Len != 0;
#else
    StringLocal(NameCopy, 128); // we copy the name because the passed in Name could have had its length altered but not the data, so create a copy with a null terminator at the length so windows gets the correct string
    String_Copy(&NameCopy, Name);

    DWORD Len = GetEnvironmentVariable((char*)NameCopy.Data, (char*)OutVariable->Data, OutVariable->Capacity);
    OutVariable->Length = Len;

    return Len != 0;
#endif
}

NO_DISCARD bool Platform_SetEnvironmentVariableValue(String Name, String Value)
{
    StringLocal(NameCopy, 128); // we copy the name because the passed in Name could have had its length altered but not the data, so create a copy with a null terminator at the length so windows gets the correct string
    String_Copy(&NameCopy, Name);

    StringLocal(ValueCopy, INT16_MAX);
    String_Copy(&ValueCopy, Value);

    BOOL bSuccess = SetEnvironmentVariable((char*)NameCopy.Data, (char*)ValueCopy.Data);
    return bSuccess;
}

NO_DISCARD bool Platform_DoesEnvironmentVariableExist(String Name)
{
    StringLocal(NameCopy, 128); // we copy the name because the passed in Name could have had its length altered but not the data, so create a copy with a null terminator at the length so windows gets the correct string
    String_Copy(&NameCopy, Name);

    DWORD Len = GetEnvironmentVariable((char*)NameCopy.Data, NULL, 0);
    return Len != 0;
}

NO_DISCARD u32 Platform_GetNumLogicalProcessors(void)
{
    SYSTEM_INFO info = {0};
    GetSystemInfo(&info);
    return info.dwNumberOfProcessors;
}

NO_DISCARD bool Filesystem_Open(const String FilePath, EFileMode Mode, FileHandle* OutHandle)
{
    bool bSuccess = false;

    DWORD OpenStyle = 0;
    DWORD ShareStyle = 0;
    DWORD Disposition = 0;

    if ((Mode & FileMode_Read) != 0 && (Mode & FileMode_Write) != 0)
    {
        OpenStyle = GENERIC_READ | GENERIC_WRITE;
        ShareStyle = FILE_SHARE_READ | FILE_SHARE_WRITE;
        Disposition = OPEN_ALWAYS;
        bSuccess = true;
    }
    else if ((Mode & FileMode_Read) != 0 && (Mode & FileMode_Write) == 0)
    {
        OpenStyle = GENERIC_READ;
        ShareStyle = FILE_SHARE_READ;
        Disposition = OPEN_EXISTING;
        bSuccess = true;
    }
    else if ((Mode & FileMode_Read) == 0 && (Mode & FileMode_Write) != 0)
    {
        OpenStyle = GENERIC_WRITE;
        ShareStyle = FILE_SHARE_WRITE;
        Disposition = CREATE_ALWAYS;
        bSuccess = true;
    }
    else
    {
        // Invalid mode passed
        ENSURE(0);
        bSuccess = false;
    }

    // make the directory if we are not in read only mode
    if (bSuccess && Mode != FileMode_Read)
    {
        bool bFoundPathSeparator = false;
        u32 NextSlashIndex = 0;

        do
        {
            bFoundPathSeparator = false;

            for (u32 i = NextSlashIndex; i < FilePath.Length; i++)
            {
                if (FilePath.Data[i] == '/' || FilePath.Data[i] == '\\')
                {
                    bFoundPathSeparator = true;

                    StringLocal(BaseDirectory, MAX_PATH);
                    String_Copy(&BaseDirectory, StrSlice(FilePath.Data, i));

                    NextSlashIndex = i+1;

                    BOOL bDirectoryCreated = Filesystem_DoesDirectoryExist(BaseDirectory);
                    bSuccess = bDirectoryCreated;

                    if (!bDirectoryCreated)
                    {
                        bDirectoryCreated = CreateDirectory((char*)BaseDirectory.Data, NULL);
                        bSuccess = bDirectoryCreated;

                        if (!bDirectoryCreated)
                        {
                            StringLocal(Prefix, 512);
                            String_Format(&Prefix, S("Failed to create directory \"%S\""), BaseDirectory);
                            LogLastError(Prefix);

                            bFoundPathSeparator = false;
                        }
                    }

                    break;
                }
            }
        }
        while (bFoundPathSeparator);
    }

    if (bSuccess)
    {
        StringLocal(PathCopy, MAX_PATH);
        String_Copy(&PathCopy, FilePath);
        
        HANDLE File = CreateFile((char*)PathCopy.Data, OpenStyle, ShareStyle, NULL, Disposition, FILE_ATTRIBUTE_NORMAL, NULL);
        if (File == INVALID_HANDLE_VALUE)
        {
            //StringLocal(Prefix, 512);
            //String_Format(&Prefix, S("Failed to open file \"%S\""), PathCopy);
            //LogLastError(Prefix);

            bSuccess = false;
        }
        else
        {
            if (OutHandle)
            {
                OutHandle->Data = File;
                OutHandle->Data2 = NULL;
            }
            
            bSuccess = true;
        }
    }

    return bSuccess;
}

NO_DISCARD bool Filesystem_NewFile(const String FilePath)
{
    FileHandle f = {0};
    bool bSuccess = Filesystem_Open(FilePath, FileMode_Write, &f);
    if (bSuccess)
    {
        Filesystem_Close(&f);
    }

    return bSuccess;
}

NO_DISCARD bool Filesystem_DeleteFile(String FilePath)
{
    StringLocal(Copy, MAX_PATH);
    String_Copy(&Copy, FilePath);
    
    i32 Result = DeleteFile((char*)Copy.Data) != 0;

    return Result != 0;
}

NO_DISCARD bool Filesystem_Open_MemoryMapped(const String FilePath, EFileMode Mode, FileHandle* OutHandle, u8** OutData, usize* OutSize)
{
    bool bSuccess = false;

    if (OutSize)
    {
        *OutSize = 0;
    }

    if (OutData)
    {
        *OutData = NULL;
    }

    if (OutHandle && Filesystem_Open(FilePath, Mode, OutHandle))
    {
        bSuccess = IsValidFileHandle(*OutHandle);
    }

    DWORD ProtectFlag = 0;
    if (bSuccess)
    {
        if ((Mode & FileMode_Read) != 0 && (Mode & FileMode_Write) != 0)
        {
            ProtectFlag = PAGE_READWRITE;
        }
        else if ((Mode & FileMode_Read) != 0 && (Mode & FileMode_Write) == 0)
        {
            ProtectFlag = PAGE_READONLY;
        }
        else if ((Mode & FileMode_Read) == 0 && (Mode & FileMode_Write) != 0)
        {
            ProtectFlag = PAGE_WRITECOPY;
        }
        else
        {
            ENSURE(0);
            bSuccess = false;
        }
    }

    HANDLE fm = NULL;
    if (bSuccess)
    {
        fm = CreateFileMapping(OutHandle->Data, NULL, ProtectFlag, 0, 0, NULL);
        if (fm == NULL || fm == INVALID_HANDLE_VALUE)
        {
            StringLocal(Prefix, 512);
            String_Format(&Prefix, S("Failed to create file mapping for \"%S\""), FilePath);
            LogLastError(Prefix);

            bSuccess = false;
        }
    }

    DWORD OpenStyle = 0;
    if (bSuccess)
    {
        OutHandle->Data2 = fm;

        if ((Mode & FileMode_Read) != 0 && (Mode & FileMode_Write) != 0)
        {
            OpenStyle = FILE_MAP_READ | FILE_MAP_WRITE;
        }
        else if ((Mode & FileMode_Read) != 0 && (Mode & FileMode_Write) == 0)
        {
            OpenStyle = FILE_MAP_READ;
        }
        else if ((Mode & FileMode_Read) == 0 && (Mode & FileMode_Write) != 0)
        {
            OpenStyle = FILE_MAP_WRITE;
        }
        else
        {
            ENSURE(0);
            bSuccess = false;
        }
    }

    if (bSuccess)
    {
        LARGE_INTEGER FileSize = {0};
        (void)GetFileSizeEx(OutHandle->Data, &FileSize);

        if (OutSize)
        {
            *OutSize = (usize)FileSize.QuadPart;
        }

        if (OutData)
        {
            *OutData = MapViewOfFile(fm, OpenStyle, 0, 0, (SIZE_T)FileSize.QuadPart);
        }
    }

    if (!bSuccess)
    {
        Filesystem_Close(OutHandle);
    }

    return bSuccess;
}

NO_DISCARD bool Filesystem_OpenDirectory(const String FilePath)
{
    bool bAnySuccess = Filesystem_DoesDirectoryExist(FilePath);
    
    if (!bAnySuccess)
    {
        bool bFoundPathSeparator = false;
        u32 NextSlashIndex = 0;

        do
        {
            bFoundPathSeparator = false;

            for (u32 i = NextSlashIndex; i < FilePath.Length; i++)
            {
                if (FilePath.Data[i] == '/' || FilePath.Data[i] == '\\' || FilePath.Length-1 == i)
                {
                    bFoundPathSeparator = true;

                    StringLocal(BaseDirectory, MAX_PATH);
                    if (FilePath.Length-1 == i)
                    {
                        String_Copy(&BaseDirectory, StrSlice(FilePath.Data, i+1));
                    }
                    else
                    {
                        String_Copy(&BaseDirectory, StrSlice(FilePath.Data, i));
                    }

                    NextSlashIndex = i+1;

                    BOOL bDirectoryCreated = Filesystem_DoesDirectoryExist(BaseDirectory);
                    bAnySuccess = bDirectoryCreated;

                    if (!bDirectoryCreated)
                    {
                        bDirectoryCreated = CreateDirectory((char*)BaseDirectory.Data, NULL);
                        bAnySuccess = bDirectoryCreated;

                        if (!bDirectoryCreated)
                        {
                            StringLocal(Prefix, 512);
                            String_Format(&Prefix, S("Failed to create directory \"%S\""), BaseDirectory);
                            LogLastError(Prefix);

                            bFoundPathSeparator = false;
                        }
                    }

                    break;
                }
            }
        }
        while (bFoundPathSeparator);
    }

    return bAnySuccess;
}

NO_DISCARD bool Filesystem_OpenDirectory_Ex(const String FilePath, FileHandle* OutHandle)
{
    bool bSuccess = false;

    if (Filesystem_OpenDirectory(FilePath))
    {
        bSuccess = true;
    }

    if (bSuccess)
    {
        StringLocal(PathCopy, MAX_PATH);
        String_Copy(&PathCopy, FilePath);
        
        HANDLE File = CreateFile((char*)PathCopy.Data, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_FLAG_BACKUP_SEMANTICS, NULL);

        if (File == INVALID_HANDLE_VALUE)
        {
            StringLocal(Prefix, 512);
            String_Format(&Prefix, S("Failed to open directory \"%S\""), PathCopy);
            LogLastError(Prefix);

            bSuccess = false;
        }
        else
        {
            if (OutHandle)
            {
                OutHandle->Data = File;
            }
        }
    }

    return bSuccess;
}

void Filesystem_Close(FileHandle* Handle)
{
    if (Handle)
    {
        if (Handle->Data2)
        {
            CloseHandle(Handle->Data2);
        }

        if (IsValidFileHandle(*Handle))
        {
            CloseHandle(Handle->Data);
            *Handle = FileHandle_Null();
        }
    }
}

NO_DISCARD bool Filesystem_Seek(const FileHandle Handle, isize Offset)
{
    DWORD Result = SetFilePointer(Handle.Data, (long)Offset, NULL, FILE_CURRENT);
    return Result != INVALID_SET_FILE_POINTER;
}

NO_DISCARD bool Filesystem_SeekFromBeginning(const FileHandle Handle, usize Offset)
{
    DWORD Result = SetFilePointer(Handle.Data, (long)Offset, NULL, FILE_BEGIN);
    return Result != INVALID_SET_FILE_POINTER;
}

NO_DISCARD bool Filesystem_SeekFromEnd(const FileHandle Handle, usize Offset)
{
    DWORD Result = SetFilePointer(Handle.Data, (long)Offset, NULL, FILE_END);
    return Result != INVALID_SET_FILE_POINTER;
}

NO_DISCARD bool Filesystem_SeekToBeginning(const FileHandle Handle)
{
    DWORD Result = SetFilePointer(Handle.Data, 0, NULL, FILE_BEGIN);
    return Result != INVALID_SET_FILE_POINTER;
}

NO_DISCARD bool Filesystem_SeekToEnd(const FileHandle Handle)
{
    DWORD Result = SetFilePointer(Handle.Data, 0, NULL, FILE_END);
    return Result != INVALID_SET_FILE_POINTER;
}

NO_DISCARD usize Filesystem_GetCurrentFilePosition(const FileHandle Handle)
{
    return SetFilePointer(Handle.Data, 0, NULL, FILE_CURRENT);
}

NO_DISCARD FileHandle Filesystem_GetStdInputHandle(void)
{
    FileHandle Handle = {0};
    Handle.Data = GetStdHandle(STD_INPUT_HANDLE);
    return Handle;
}

NO_DISCARD usize Filesystem_GetLastWriteTime(const String FilePath)
{
    FILETIME FileTimeStamp = {0};

    if (Filesystem_DoesFileExist(FilePath))
    {
        FileHandle f = {0};
        if (Filesystem_Open(FilePath, FileMode_Read, &f))
        {
            (void)GetFileTime(f.Data, NULL, NULL, &FileTimeStamp);
            Filesystem_Close(&f);
        }
    }

    ULONGLONG a = (((ULONGLONG)FileTimeStamp.dwHighDateTime) << 32) + FileTimeStamp.dwLowDateTime;
    return (usize)a;
}

NO_DISCARD usize Filesystem_GetLastAccessTime(const String FilePath)
{
    FILETIME FileTimeStamp = {0};

    if (Filesystem_DoesFileExist(FilePath))
    {
        FileHandle f = {0};
        if (Filesystem_Open(FilePath, FileMode_Read, &f))
        {
            (void)GetFileTime(f.Data, NULL, &FileTimeStamp, NULL);
            Filesystem_Close(&f);
        }
    }

    ULONGLONG a = (((ULONGLONG)FileTimeStamp.dwHighDateTime) << 32) + FileTimeStamp.dwLowDateTime;
    return (usize)a;
}

NO_DISCARD usize Filesystem_GetCreationTime(const String FilePath)
{
    FILETIME FileTimeStamp = {0};

    if (Filesystem_DoesFileExist(FilePath))
    {
        FileHandle f = {0};
        if (Filesystem_Open(FilePath, FileMode_Read, &f))
        {
            (void)GetFileTime(f.Data, &FileTimeStamp, NULL, NULL);
            Filesystem_Close(&f);
        }
    }

    ULONGLONG a = (((ULONGLONG)FileTimeStamp.dwHighDateTime) << 32) + FileTimeStamp.dwLowDateTime;
    return (usize)a;
}

NO_DISCARD FileTimeData Filesystem_GetFileTime(const String FilePath)
{
    FileTimeData Time = {0};

    if (Filesystem_DoesFileExist(FilePath))
    {
        FileHandle f = {0};
        if (Filesystem_Open(FilePath, FileMode_Read, &f))
        {
            FILETIME CreationTime = {0};
            FILETIME LastAccessTime = {0};
            FILETIME LastWriteTime = {0};

            (void)GetFileTime(f.Data, &CreationTime, &LastAccessTime, &LastWriteTime);
            Filesystem_Close(&f);

            Time.CreationTime   = (((ULONGLONG)CreationTime.dwHighDateTime)   << 32) + CreationTime.dwLowDateTime;
            Time.LastAccessTime = (((ULONGLONG)LastAccessTime.dwHighDateTime) << 32) + LastAccessTime.dwLowDateTime;
            Time.LastWriteTime  = (((ULONGLONG)LastWriteTime.dwHighDateTime)  << 32) + LastWriteTime.dwLowDateTime;
        }
    }

    return Time;
}

NO_DISCARD usize Filesystem_GetLastWriteTimeH(const FileHandle Handle)
{
    FILETIME FileTimeStamp = {0};
    (void)GetFileTime(Handle.Data, NULL, NULL, &FileTimeStamp);

    ULONGLONG a = (((ULONGLONG)FileTimeStamp.dwHighDateTime) << 32) + FileTimeStamp.dwLowDateTime;
    return (usize)a;
}

NO_DISCARD usize Filesystem_GetLastAccessTimeH(const FileHandle Handle)
{
    FILETIME FileTimeStamp = {0};
    (void)GetFileTime(Handle.Data, NULL, &FileTimeStamp, NULL);

    ULONGLONG a = (((ULONGLONG)FileTimeStamp.dwHighDateTime) << 32) + FileTimeStamp.dwLowDateTime;
    return (usize)a;
}

NO_DISCARD usize Filesystem_GetCreationTimeH(const FileHandle Handle)
{
    FILETIME FileTimeStamp = {0};
    (void)GetFileTime(Handle.Data, &FileTimeStamp, NULL, NULL);

    ULONGLONG a = (((ULONGLONG)FileTimeStamp.dwHighDateTime) << 32) + FileTimeStamp.dwLowDateTime;
    return (usize)a;
}

NO_DISCARD FileTimeData Filesystem_GetFileTimeH(const FileHandle Handle)
{
    FileTimeData Time = {0};

    FILETIME CreationTime = {0};
    FILETIME LastAccessTime = {0};
    FILETIME LastWriteTime = {0};
    (void)GetFileTime(Handle.Data, &CreationTime, &LastAccessTime, &LastWriteTime);

    Time.CreationTime   = (((ULONGLONG)CreationTime.dwHighDateTime)   << 32) + CreationTime.dwLowDateTime;
    Time.LastAccessTime = (((ULONGLONG)LastAccessTime.dwHighDateTime) << 32) + LastAccessTime.dwLowDateTime;
    Time.LastWriteTime  = (((ULONGLONG)LastWriteTime.dwHighDateTime)  << 32) + LastWriteTime.dwLowDateTime;

    return Time;
}

NO_DISCARD bool Filesystem_ReadPipe(PlatformPipe Handle, usize DataSize, void* OutData, usize* OutBytesRead)
{
    bool bSuccess = Handle[0] && Handle[1];

    DWORD BytesRead = 0;
    if (bSuccess)
    {
        bSuccess = ReadFile(Handle[0], OutData, (DWORD)DataSize, &BytesRead, NULL);
    }
    
    if (OutBytesRead)
    {
        *OutBytesRead = BytesRead;
    }

    return bSuccess;
}

NO_DISCARD bool Filesystem_Read(const FileHandle Handle, usize DataSize, void* OutData, usize* OutBytesRead)
{
    bool bSuccess = IsValidFileHandle(Handle);

    DWORD BytesRead = 0;
    if (bSuccess)
    {
        bSuccess = ReadFile(Handle.Data, OutData, (DWORD)DataSize, &BytesRead, NULL);
    }

    if (OutBytesRead)
    {
        *OutBytesRead = BytesRead;
    }

    return bSuccess;
}

NO_DISCARD bool Filesystem_ReadEntireFile(const FileHandle Handle, void* OutData, usize* OutBytesRead)
{
    bool bSuccess = IsValidFileHandle(Handle);
    
    usize Size = 0;
    if (bSuccess)
    {
        bSuccess = Filesystem_GetFileSize(Handle, &Size);
    }

    if (bSuccess)
    {
        bSuccess = Filesystem_SeekToBeginning(Handle);
    }

    if (bSuccess)
    {
        DWORD BytesRead = 0;
        bSuccess = ReadFile(Handle.Data, OutData, (DWORD)Size, &BytesRead, NULL);

        if (OutBytesRead)
        {
            *OutBytesRead = BytesRead;
        }
    }

    return bSuccess;
}

NO_DISCARD bool Filesystem_ReadLine(const FileHandle Handle, String* LineBuffer)
{
    bool bSuccess = IsValidFileHandle(Handle) && LineBuffer && String_IsDataValid(*LineBuffer);
    
    usize Size = 0;
    if (bSuccess)
    {
        LARGE_INTEGER FileSize = {0};
        (void)GetFileSizeEx(Handle.Data, &FileSize);
        Size = (usize)FileSize.QuadPart;
        bSuccess = Size > 0;
    }

    DWORD CurrentPosition = 0;
    if (bSuccess)
    {
        CurrentPosition = SetFilePointer(Handle.Data, 0, NULL, FILE_CURRENT);

        if (CurrentPosition >= Size)
        {
            (void)Filesystem_SeekToBeginning(Handle);
            bSuccess = false;
        }
    }

    if (bSuccess)
    {
        u8 TempBuffer[8192] = {0};
        DWORD BytesRead = 0;
        
        u32 Counter = 0;
        u32 FilePointerOffset = 0;

        if (ReadFile(Handle.Data, TempBuffer, 8191, &BytesRead, NULL))
        {
            for (u32 i = 0; i < BytesRead; i++)
            {
                if (TempBuffer[i] == '\0' || TempBuffer[i] == '\n' || TempBuffer[i] == '\r')
                {
                    FilePointerOffset = Counter;

                    if (TempBuffer[i] == '\r' && TempBuffer[i+1] == '\n') // todo: bounds check
                    {
                        FilePointerOffset += 2;
                    }
                    else
                    {
                        FilePointerOffset += 1;
                    }

                    TempBuffer[i] = 0;

                    break;
                }

                Counter++;
            }

            if (FilePointerOffset == 0) // did not find a new line char, possibly at end of file
            {
                FilePointerOffset = Counter;
            }

            if (FilePointerOffset == 0)
            {
                bSuccess = false;
            }
        }

        u32 MaxLength = Min(LineBuffer->Capacity, 8192);
        u32 LineLength = Min(MaxLength-1, Counter);

        if (LineLength > 0)
        {
            String_Copy(LineBuffer, StrSlice(TempBuffer, LineLength));
        }
        else
        {
            String_Empty(LineBuffer);
        }

        if (FilePointerOffset > 0)
        {
            (void)SetFilePointer(Handle.Data, (i32)(CurrentPosition + FilePointerOffset), NULL, FILE_BEGIN);
        }
    }

    return bSuccess;
}

bool Filesystem_Write(const FileHandle Handle, usize DataSize, const void* Data, usize* OutBytesWritten)
{
    bool bSuccess = IsValidFileHandle(Handle) && DataSize > 0;

    if (bSuccess)
    {
        bSuccess = Filesystem_SeekToBeginning(Handle);
    }

    if (bSuccess)
    {
        DWORD BytesWritten = 0;
        bSuccess = WriteFile(Handle.Data, Data, (DWORD)DataSize, &BytesWritten, NULL);

        if (OutBytesWritten)
        {
            *OutBytesWritten = BytesWritten;
        }
    }

    return bSuccess;
}

bool Filesystem_WriteLine(const FileHandle Handle, const String Text, usize* OutBytesWritten)
{
    bool bSuccess = IsValidFileHandle(Handle) && Text.Length > 0;

    if (bSuccess)
    {
        bSuccess = Filesystem_SeekToEnd(Handle);
    }

    if (bSuccess)
    {
        DWORD BytesWritten = 0;
        bSuccess = WriteFile(Handle.Data, Text.Data, (DWORD)Text.Length, &BytesWritten, NULL);

        if (OutBytesWritten)
        {
            *OutBytesWritten = BytesWritten;
        }
    }

    return bSuccess;
}

bool Filesystem_WriteLineFormatted(const FileHandle Handle, const String Text, usize* OutBytesWritten, ...)
{
    bool bSuccess = IsValidFileHandle(Handle) && Text.Length > 0;

    if (bSuccess)
    {
        bSuccess = Filesystem_SeekToEnd(Handle);
    }

    if (bSuccess)
    {
        va_list Args = {0};
        va_start(Args, OutBytesWritten);
        StringLocal(Buffer, 32768);
        String_FormatV(&Buffer, Text, Args);
        va_end(Args);

        DWORD BytesWritten = 0;
        bSuccess = WriteFile(Handle.Data, Buffer.Data, (DWORD)Buffer.Length, &BytesWritten, NULL);

        if (OutBytesWritten)
        {
            *OutBytesWritten = BytesWritten;
        }
    }

    return bSuccess;
}

NO_DISCARD bool Filesystem_DoesFileExist(const String FilePath)
{
    bool bExists = false;

    if (FilePath.Length > 0)
    {
        StringLocal(Copy, MAX_PATH);
        String_Copy(&Copy, FilePath);

        bExists = PathFileExists((char*)Copy.Data);

        if (bExists)
        {
            bExists = Filesystem_IsFile(FilePath);
        }
    }

    return bExists;
}

NO_DISCARD bool Filesystem_DoesDirectoryExist(const String FilePath)
{
    bool bExists = false;

    if (FilePath.Length > 0)
    {
        StringLocal(Copy, MAX_PATH);
        String_Copy(&Copy, FilePath);

        DWORD Attrib = GetFileAttributes((char*)Copy.Data);
        bExists = (Attrib != INVALID_FILE_ATTRIBUTES && (Attrib & FILE_ATTRIBUTE_DIRECTORY));
    }

    return bExists;
}

NO_DISCARD bool Filesystem_GetFilePath(const FileHandle File, String* OutPath)
{
    bool bSuccess = IsValidFileHandle(File);
    if (bSuccess)
    {
        const u32 MaxCap = Min(OutPath->Capacity, MAX_PATH);
        const u32 Length = GetFinalPathNameByHandle(File.Data, (char*)OutPath->Data, MaxCap, FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
        bSuccess = Length > 0;

        if (bSuccess)
        {
            OutPath->Length = Length;
            *OutPath = StrShiftF(*OutPath, 4); // ignore //?/
        }
    }

    return bSuccess;
}

NO_DISCARD bool Filesystem_GetFileSize(const FileHandle File, usize* OutSize)
{
    bool bSuccess = IsValidFileHandle(File);

    if (bSuccess)
    {
        LARGE_INTEGER FileSize = {0};
        bSuccess = GetFileSizeEx(File.Data, &FileSize);

        if (OutSize)
        {
            *OutSize = (usize)FileSize.QuadPart;
        }
    }

    return bSuccess;
}

NO_DISCARD bool Filesystem_IsFile(const String Path)
{
    DWORD Attrib = GetFileAttributes((char*)Path.Data);
    bool bIsFile = (Attrib != INVALID_FILE_ATTRIBUTES && (Attrib != FILE_ATTRIBUTE_DIRECTORY));
    return bIsFile;
}

NO_DISCARD bool Filesystem_IsDirectory(const String Path)
{
    DWORD Attrib = GetFileAttributes((char*)Path.Data);
    bool bIsDir = (Attrib != INVALID_FILE_ATTRIBUTES && (Attrib & FILE_ATTRIBUTE_DIRECTORY));
    return bIsDir;
}

NO_DISCARD bool Filesystem_IsHidden(const String Path)
{
    DWORD Attrib = GetFileAttributes((char*)Path.Data);
    bool bIsHidden = (Attrib != INVALID_FILE_ATTRIBUTES && (Attrib & FILE_ATTRIBUTE_HIDDEN));
    return bIsHidden;
}

NO_DISCARD bool Filesystem_IsPathRelative(const String Path)
{
    bool bDriveSymbol = String_IndexOfChar(Path, ':', NULL);

    bool bRelative = !bDriveSymbol;

    return bRelative;
}

NO_DISCARD bool Filesystem_ConvertRelativeToAbsolutePath(String* OutFullPath)
{
    StringLocal(Copy, MAX_PATH);
    String_Copy(&Copy, *OutFullPath);

    bool bResult = PathCanonicalize((char*)OutFullPath->Data, (char*)Copy.Data);
    OutFullPath->Length = String_GetLength_N((char*)OutFullPath->Data, MAX_PATH);
    return bResult;
}

/*

// Using the low level Nt version of FindFirstFile. tested on debug and release mode.
// there is literally no noticable difference.

UNUSED NO_DISCARD static bool _Internal_IterateDirectory(const String RootPath, const String DirectoryPath, DirectoryIterator Callback, bool bRecursive, void* UserData)
{
    bool bSuccess = true;

    StringLocal(Test, MAX_PATH);
    String_Copy(&Test, DirectoryPath.Length == 0 ? S(".") : DirectoryPath);

    // Open the directory with backup semantics
    HANDLE hDir = CreateFile(
        (char*)Test.Data,
        FILE_LIST_DIRECTORY | SYNCHRONIZE,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS, // Required to open directories
        NULL
    );

    BYTE buffer[64 * 1024]; // 64KB buffer for many entries
    IO_STATUS_BLOCK iosb = {0};

    NTSTATUS status = {0};
    BOOL restart = TRUE;

    do
    {
        status = NtQueryDirectoryFile(
            hDir,
            NULL,
            NULL,
            NULL,
            &iosb,
            buffer,
            sizeof(buffer),
            FileDirectoryInformation,
            FALSE,
            NULL,
            restart
        );

        if (status == STATUS_NO_MORE_FILES)
        {
            break;
        }

        if (status != 0)
        {
            break;
        }

        restart = FALSE;

        bool bShouldBreak = false;

        // Walk all entries in this buffer
        BYTE* ptr = buffer;
        do
        {
            FILE_DIRECTORY_INFORMATION* info = (FILE_DIRECTORY_INFORMATION*)ptr;

            String16 FileNameWide = {0};
            FileNameWide.Data = info->FileName;
            FileNameWide.Length = ClampMax(info->FileNameLength / sizeof(WCHAR), MAX_PATH - 1);

            StringLocal(FileName, MAX_PATH);
            String_ToNarrow(FileNameWide, &FileName);

            if (String_IsEqual(FileName, S("."), false) ||
                String_IsEqual(FileName, S(".."), false))
            {
            }
            else
            {
                StringLocal(FilePath, MAX_PATH);
                String_BuildPath(&FilePath, Test, FileName);

                String FullPath = FilePath;
                xx String_EatPathSeparatorsInline(&FullPath);

                String RelativePath = StrShiftF(FilePath, RootPath.Length);
                xx String_EatPathSeparatorsInline(&RelativePath);

                if (info->FileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                {
                    bool bResult = Callback(FullPath, RelativePath, FileName, 0, true, UserData);
                    if (bResult)
                    {
                        if (bRecursive)
                        {
                            if (!_Internal_IterateDirectory(RootPath, FullPath, Callback, true, UserData))
                            {
                                bSuccess = false;
                                bShouldBreak = true;
                            }
                        }
                    }
                    else
                    {
                        bSuccess = false;
                        bShouldBreak = true;
                    }
                }
                else
                {
                    u64 FileSize = (u64)info->EndOfFile.QuadPart;
                    bool bResult = Callback(FullPath, RelativePath, FileName, FileSize, false, UserData);

                    if (!bResult) // the user wants to end the iteration
                    {
                        bSuccess = false;
                        bShouldBreak = true;
                    }
                }

            }

            if (bShouldBreak)
            {
                break;
            }

            if (info->NextEntryOffset == 0)
            {
                break;
            }

            ptr += info->NextEntryOffset;

        }
        while (1);

        if (bShouldBreak)
        {
            break;
        }
    }
    while (1);

    return bSuccess;
}
*/

/*

// Iterative approach instead of recursive. tested on debug and release mode. there is literally no noticable difference

NO_DISCARD static bool Internal_IterateDirectory(const String RootPath, const String DirectoryPath, DirectoryIterator Callback, bool bRecursive, void* UserData)
{
    bool bSuccess = true;

    STRUCT(DirectoryStackFrameData)
    {
        WIN32_FIND_DATA ffd;
        HANDLE FindHandle;
        uchar PathBuffer[MAX_PATH+4]; // +4 for alignment stuff
        String Path;
    };

    StackLocal(DirectoryStackFrameData, Stack, 32); // 32 = max directory depth we will support

    // push the initial frame
    {
        Stack_PushZero(Stack);

        DirectoryStackFrameData* Frame = &Stack[0];
        Frame->Path.Data = Frame->PathBuffer;
        Frame->Path.Capacity = MAX_PATH;

        String_Copy(&Frame->Path, DirectoryPath.Length == 0 ? S(".") : DirectoryPath);
        String_Append(&Frame->Path, S("\\*"));

        Frame->FindHandle = FindFirstFile((char*)Frame->Path.Data, &Frame->ffd);
        Frame->Path.Length -= 2; // ignore \*
    }

    bool bShouldBreak = false;

    while (Stack_Count)
    {
        DirectoryStackFrameData Current = {0};
        Stack_Pop(Stack, Current);

        if (Current.FindHandle != INVALID_HANDLE_VALUE)
        {
            do
            {
                String FileName = CStrEx(Current.ffd.cFileName, 259);

                if (String_IsEqual(FileName, S("."), false) ||
                    String_IsEqual(FileName, S(".."), false))
                {
                    continue;
                }

                StringLocal(FilePath, MAX_PATH);
                String_BuildPath(&FilePath, Current.Path, FileName);

                String FullPath = FilePath;
                xx String_EatPathSeparatorsInline(&FullPath);

                String RelativePath = StrShiftF(FilePath, RootPath.Length);
                xx String_EatPathSeparatorsInline(&RelativePath);

                if (Current.ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                {
                    bool bResult = Callback(FullPath, RelativePath, FileName, 0, true, UserData);
                    if (bResult)
                    {
                        if (bRecursive)
                        {
                            Stack_PushZero(Stack);

                            DirectoryStackFrameData* Frame = &Stack[Stack_Count-1];
                            Frame->Path.Data = Frame->PathBuffer;
                            Frame->Path.Capacity = MAX_PATH;

                            String_Copy(&Frame->Path, FullPath);
                            String_Append(&Frame->Path, S("\\*"));

                            Frame->FindHandle = FindFirstFile((char*)Frame->Path.Data, &Frame->ffd);
                            Frame->Path.Length -= 2; // ignore \*
                        }
                    }
                    else
                    {
                        bSuccess = false;
                        bShouldBreak = true;
                    }
                }
                else
                {
                    DWORD FileSize = (Current.ffd.nFileSizeHigh * (MAXDWORD+1)) + Current.ffd.nFileSizeLow;
                    bool bResult = Callback(FullPath, RelativePath, FileName, FileSize, false, UserData);
                    if (!bResult) // the user wants to end the iteration
                    {
                        bSuccess = false;
                        bShouldBreak = true;
                    }
                }

                if (bShouldBreak)
                {
                    break;
                }
            }
            while (FindNextFile(Current.FindHandle, &Current.ffd) != 0);

            FindClose(Current.FindHandle);

            if (bShouldBreak)
            {
                break;
            }
        }
    }

    return bSuccess;
}
*/


NO_DISCARD static bool Internal_IterateDirectory(const String RootPath, const String DirectoryPath, DirectoryIterator Callback, bool bRecursive, void* UserData)
{
    const String RealDirectoryPath = DirectoryPath.Length == 0 ? S(".") : DirectoryPath;

    StringLocal(Temp, MAX_PATH);
    String_Copy(&Temp, RealDirectoryPath);
    String_Append(&Temp, S("\\*"));

    WIN32_FIND_DATA ffd = {0};
    HANDLE Find = FindFirstFile((char*)Temp.Data, &ffd);

    bool bSuccess = false;

    if (Find != INVALID_HANDLE_VALUE)
    {
        bSuccess = true;

        do
        {
            String FileName = CStrEx(ffd.cFileName, 259);

            if (String_IsEqual(FileName, S("."), false) ||
                String_IsEqual(FileName, S(".."), false))
            {
                continue;
            }

            StringLocal(FilePath, MAX_PATH);
            String_BuildPath(&FilePath, RealDirectoryPath, FileName);

            String FullPath = FilePath;
            xx String_EatPathSeparatorsInline(&FullPath);

            String RelativePath = StrShiftF(FilePath, RootPath.Length);
            xx String_EatPathSeparatorsInline(&RelativePath);

            bool bShouldBreak = false;

            if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            {
                bool bResult = Callback(FullPath, RelativePath, FileName, 0, true, UserData);
                if (bResult)
                {
                    if (bRecursive)
                    {
                        if (!Internal_IterateDirectory(RootPath, FullPath, Callback, true, UserData))
                        {
                            bSuccess = false;
                            bShouldBreak = true;
                        }
                    }
                }
                else
                {
                    bSuccess = false;
                    bShouldBreak = true;
                }
            }
            else
            {
                DWORD FileSize = (ffd.nFileSizeHigh * (MAXDWORD+1)) + ffd.nFileSizeLow;
                bool bResult = Callback(FullPath, RelativePath, FileName, FileSize, false, UserData);
                if (!bResult) // the user wants to end the iteration
                {
                    bSuccess = false;
                    bShouldBreak = true;
                }
            }

            if (bShouldBreak)
            {
                break;
            }
        }
        while (FindNextFile(Find, &ffd) != 0);

        FindClose(Find);
    }

    return bSuccess;
}

void Filesystem_IterateDirectory(const String BasePath, DirectoryIterator Callback, bool bRecursive)
{
    xx Internal_IterateDirectory(BasePath, BasePath, Callback, bRecursive, NULL);
}

void Filesystem_IterateDirectory_Ex(const String BasePath, DirectoryIterator Callback, bool bRecursive, void* UserData)
{
    xx Internal_IterateDirectory(BasePath, BasePath, Callback, bRecursive, UserData);
}

NO_DISCARD bool Filesystem_DeleteFiles(const String FilePath, const String Wildcard, bool bRecursive)
{
    WIN32_FIND_DATA fd = {0};

    StringLocal(WildcardPath, MAX_PATH_LENGTH);
    String_BuildPath(&WildcardPath, FilePath, Wildcard);

    HANDLE hFind = FindFirstFile((char*)WildcardPath.Data, &fd);

    bool bAnyFilesDeleted = false;

    if (hFind != INVALID_HANDLE_VALUE)
    {
        do
        {
            String FileName = CStr(fd.cFileName);

            if (String_IsEqual(FileName, S("."), false) ||
                String_IsEqual(FileName, S(".."), false))
            {
                continue;
            }

            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            {
                StringLocal(SubPath, MAX_PATH_LENGTH);
                String_BuildPath(&SubPath, FilePath, FileName);

                if (bRecursive)
                {
                    (void)Filesystem_DeleteFiles(SubPath, Wildcard, true);
                }
            }
            else
            {
                StringLocal(FullPath, MAX_PATH_LENGTH);
                String_BuildPath(&FullPath, FilePath, FileName);

                i32 Result = DeleteFile((char*)FullPath.Data);
                if (Result != 0)
                {
                    bAnyFilesDeleted = true;
                }
            }
        }
        while (FindNextFile(hFind, &fd));

        FindClose(hFind);
    }

    return bAnyFilesDeleted;
}

NO_DISCARD bool Filesystem_DeleteDirectory(const String DirectoryPath)
{
    WIN32_FIND_DATA fd = {0};

    StringLocal(WildcardPath, MAX_PATH_LENGTH);
    String_BuildPath(&WildcardPath, DirectoryPath, S("*"));

    HANDLE hFind = FindFirstFile((char*)WildcardPath.Data, &fd);

    if (hFind != INVALID_HANDLE_VALUE)
    {
        do
        {
            String FileName = CStr(fd.cFileName);

            if (String_IsEqual(FileName, S("."), false) ||
                String_IsEqual(FileName, S(".."), false))
            {
                continue;
            }

            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            {
                StringLocal(SubPath, MAX_PATH_LENGTH);
                String_BuildPath(&SubPath, DirectoryPath, FileName);

                (void)Filesystem_DeleteDirectory(SubPath);
            }
            else
            {
                StringLocal(FullPath, MAX_PATH_LENGTH);
                String_BuildPath(&FullPath, DirectoryPath, FileName);

                (void)DeleteFile((char*)FullPath.Data);
            }
        }
        while (FindNextFile(hFind, &fd));

        FindClose(hFind);
    }

    bool bResult = RemoveDirectory((char*)DirectoryPath.Data);

    return bResult;
}

STRUCT(CopyDirectoryMetadata)
{
    String DestinationDirectory;
    b64 bSuccess;
};

static bool Internal_CopyFilesToDirectory_Recursive(const String FullPath, const String RelativePath, const String FileName, u64 FileSize, bool bIsDirectory, void* UserData)
{
    bool bResult = true;

    CopyDirectoryMetadata* Meta = UserData;

    if (bIsDirectory)
    {
        StringLocal(Destination, MAX_PATH);
        String_BuildPath(&Destination, Meta->DestinationDirectory, RelativePath);
        
        xx Filesystem_OpenDirectory(Destination);
    }
    else
    {
        StringLocal(Source, MAX_PATH);
        String_BuildPath(&Source, FullPath);

        StringLocal(Destination, MAX_PATH);
        String_BuildPath(&Destination, Meta->DestinationDirectory, RelativePath);

        bResult = CopyFileEx((char*)Source.Data, (char*)Destination.Data, NULL, NULL, NULL, COPY_FILE_NO_BUFFERING);
        if (bResult == false)
        {
            StringLocal(Msg, 512);
            String_Format(&Msg, S("Failed to copy \"%S\" to \"%S\""), Source, Destination);
            LogLastError(Msg);
        }

        Meta->bSuccess = bResult;
    }

    return bResult;
}

NO_DISCARD bool Filesystem_Copy(const String Source, const String Destination)
{
    StringLocal(SourceCopy, MAX_PATH);
    StringLocal(DestinationCopy, MAX_PATH);
    String_Copy(&SourceCopy, Source);
    String_Copy(&DestinationCopy, Destination);

    String_ConvertSlashToPlatformSlash(&SourceCopy);
    String_ConvertSlashToPlatformSlash(&DestinationCopy);

    // the source file must exist
    if (!(Filesystem_IsFile(SourceCopy) || Filesystem_IsDirectory(SourceCopy)))
    {
        StringLocal(Msg, 512);
        String_Format(&Msg, S("\n    Source path \"%S\" does not exist.\n"), SourceCopy);
        Platform_ConsoleWrite_CustomLength((const char*)Msg.Data, Msg.Length, 3, true);
        return false;
    }

    // handles a case where we dont specify the file/directory on the Destination string,
    // so we handle that for them here.
    {
        u32 LastSlash = 0;
        xx String_IndexOfLastPathSlash(SourceCopy, &LastSlash);

        String LastPart = StrShiftF(SourceCopy, LastSlash == 0 ? 0 : LastSlash+1);

        if (!String_EndsWith(DestinationCopy, LastPart, false))
        {
            String_BuildPath(&DestinationCopy, LastPart);
        }
    }

    // now that the destination string is fully built, open the directories
    {
        u32 LastSlash = 0;
        if (String_IndexOfLastPathSlash(DestinationCopy, &LastSlash))
        {
            xx Filesystem_OpenDirectory(StrSlice(DestinationCopy.Data, LastSlash));
        }
    }

    // it is an error to try to copy a directory into a file
    if (Filesystem_IsFile(DestinationCopy) && Filesystem_IsDirectory(SourceCopy))
    {
        StringLocal(Msg, 1024);
        String_Format(&Msg, S("Destination \"%S\" can not be copied into \"%S\". You likely have the two mixed up."), DestinationCopy, SourceCopy);

        Platform_ConsoleWrite_CustomLength((const char*)Msg.Data, Msg.Length, 3, true);
        return false;
    }

    bool bResult = false;

    // recursively copy all files within the source directory to the destination directory
    if (Filesystem_IsDirectory(SourceCopy))
    {
        if (Filesystem_IsFile(DestinationCopy))
        {
            StringLocal(Msg, 1024);
            String_Format(&Msg, S("Source \"%S\" can not be copied into destination \"%S\" because the destination is a file. You likely have the two mixed up."), SourceCopy, DestinationCopy);
            Platform_ConsoleWrite_CustomLength((const char*)Msg.Data, Msg.Length, 3, true);
            bResult = false;
        }
        else
        {
            bResult = true;
            xx Filesystem_OpenDirectory(DestinationCopy);
        }

        if (bResult)
        {
            CopyDirectoryMetadata Meta = {DestinationCopy, true};
            Filesystem_IterateDirectory_Ex(SourceCopy, Internal_CopyFilesToDirectory_Recursive, true, &Meta);

            bResult = (bool)Meta.bSuccess;
        }
    }
    // copy single file to the destination directory
    else
    {
        // remove the read only attribute if we're copying from a source which had a readonly attribute set on it,
        // otherwise the copy will fail if the file already exists at the destination
        // maybe we shouldnt care...
        if (Filesystem_IsFile(DestinationCopy) && Filesystem_DoesFileExist(DestinationCopy))
        {
            xx SetFileAttributes((char*)DestinationCopy.Data, (u32)GetFileAttributes((char*)DestinationCopy.Data) & (u32)~FILE_ATTRIBUTE_READONLY);
        }

        bResult = CopyFileEx((char*)SourceCopy.Data, (char*)DestinationCopy.Data, NULL, NULL, NULL, COPY_FILE_NO_BUFFERING);
        if (bResult == 0)
        {
            StringLocal(Msg, 512);
            String_Format(&Msg, S("Failed to copy \"%S\" to \"%S\""), Source, Destination);
            LogLastError(Msg);
        }
    }

    return bResult;
}

NO_DISCARD bool Filesystem_Move(const String Source, const String Destination, bool bRename)
{
    StringLocal(SourceCopy, MAX_PATH);
    StringLocal(DestinationCopy, MAX_PATH);
    String_Copy(&SourceCopy, Source);
    String_Copy(&DestinationCopy, Destination);

    String_ConvertSlashToPlatformSlash(&SourceCopy);
    String_ConvertSlashToPlatformSlash(&DestinationCopy);

    if (!bRename)
    {
        u32 LastSlash = 0;
        (void)String_IndexOfLastPathSlash(SourceCopy, &LastSlash);

        const String FileName = StrShiftF(SourceCopy, LastSlash);
        if (!String_EndsWith(DestinationCopy, FileName, false))
        {
            String_BuildPath(&DestinationCopy, FileName);
        }

        // TODO: allow source to be a direcotry and copy everything from there

        // try to create the directory if it doesn't exist
        LastSlash = 0;
        bool bHasSlash = String_IndexOfLastPathSlash(DestinationCopy, &LastSlash);
        (void)Filesystem_OpenDirectory(bHasSlash ? StrSlice(DestinationCopy.Data, LastSlash) : DestinationCopy);
    }

    BOOL bResult = MoveFileEx((char*)SourceCopy.Data, (char*)DestinationCopy.Data, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
    if (bResult == 0)
    {
        StringLocal(Msg, 512);
        String_Format(&Msg, S("Failed to %S \"%S\" to \"%S\""), bRename ? S("rename") : S("move"), Source, Destination);
        LogLastError(Msg);
    }

    return bResult;
}

NO_DISCARD bool Filesystem_ArePathsCommon(String PathA, String PathB)
{
    StringLocal(CommonPath, MAX_PATH);
    i32 Len = PathCommonPrefix((char*)PathA.Data, (char*)PathB.Data, (char*)CommonPath.Data);
    CommonPath.Length = (u32)Len;

    return String_IsEqual(CommonPath, PathA, false);
}

NO_DISCARD PlatformHandle Platform_RunCommand(const String CmdLine, const String WorkingDirectory, const String EnvBlock)
{
    PROCESS_INFORMATION ProcessInfo = {0};
    STARTUPINFO StartupInfo = {0};
    StartupInfo.cb = sizeof(StartupInfo);

    // @verify: verify parent env gets included
    StringLocal(Copy, MAX_PATH_LENGTH);
    String_Copy(&Copy, WorkingDirectory);

    const char* Dir = Copy.Length > 0 ? (char*)Copy.Data : NULL;
    void* Env = EnvBlock.Length == 0 ? NULL : (char*)EnvBlock.Data;

    PlatformHandle ProcessHandle = INVALID_HANDLE_VALUE;
    if (CreateProcess(NULL, (char*)CmdLine.Data, NULL, NULL, TRUE, 0, Env, Dir, &StartupInfo, &ProcessInfo))
    {
        ProcessHandle = ProcessInfo.hProcess;

        SetPriorityClass(ProcessInfo.hProcess, HIGH_PRIORITY_CLASS);
        CloseHandle(ProcessInfo.hThread);
    }
    else
    {
        StringLocal(Prefix, Kibibytes(8));
        String_Format(&Prefix, S("Failed to run command: \"%S\""), CmdLine);
        LogLastError(Prefix);
    }

    return ProcessHandle;
}

NO_DISCARD PlatformHandle Platform_RunProcess(const String ProcessExePath, const String Parameters, const String WorkingDirectory, const String EnvBlock)
{
    PROCESS_INFORMATION ProcessInfo = {0};
    STARTUPINFO StartupInfo = {0};
    StartupInfo.cb = sizeof(StartupInfo);

    // @verify: verify parent env gets included
    StringLocal(Copy, MAX_PATH_LENGTH);
    String_Copy(&Copy, WorkingDirectory);

    const char* Dir = Copy.Length > 0 ? (char*)Copy.Data : NULL;
    void* Env = EnvBlock.Length == 0 ? NULL : (char*)EnvBlock.Data;

    PlatformHandle ProcessHandle = INVALID_HANDLE_VALUE;
    if (CreateProcess((char*)ProcessExePath.Data, (char*)Parameters.Data, NULL, NULL, TRUE, 0, Env, Dir, &StartupInfo, &ProcessInfo))
    {
        ProcessHandle = ProcessInfo.hProcess;

        SetPriorityClass(ProcessInfo.hProcess, HIGH_PRIORITY_CLASS);
        CloseHandle(ProcessInfo.hThread);
    }
    else
    {
        StringLocal(Prefix, Kibibytes(8));
        String_Format(&Prefix, S("Failed to run process: \"%S %S\""), ProcessExePath, Parameters);
        LogLastError(Prefix);
    }

    return ProcessHandle;
}

NO_DISCARD PlatformHandle Platform_RunProcess_Ex(const String ProcessExePath, const String Parameters, const String WorkingDirectory, PlatformPipe* StdOutPipe)
{
    PROCESS_INFORMATION ProcessInfo = {0};
    SECURITY_ATTRIBUTES saAttr = {0}; 
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES); 
    saAttr.bInheritHandle = TRUE; 
    saAttr.lpSecurityDescriptor = NULL; 

    PlatformHandle ProcessHandle = INVALID_HANDLE_VALUE;

    HANDLE r = 0, w = 0;

    bool bSuccess = CreatePipe(&r, &w, &saAttr, 0);

    if (bSuccess)
    {
        bSuccess = SetHandleInformation(r, HANDLE_FLAG_INHERIT, 0);
    }

    if (bSuccess)
    {
        if (StdOutPipe)
        {
            (*StdOutPipe)[0] = r;
            (*StdOutPipe)[1] = w;
        }

        STARTUPINFO StartupInfo = {0};
        StartupInfo.cb = sizeof(STARTUPINFO);
        StartupInfo.hStdError = w;
        StartupInfo.hStdOutput = w;
        StartupInfo.hStdInput = NULL;
        StartupInfo.dwFlags |= STARTF_USESTDHANDLES;

        StringLocal(Copy, MAX_PATH_LENGTH);
        String_Copy(&Copy, WorkingDirectory);
        const char* Dir = Copy.Length > 0 ? (char*)Copy.Data : NULL;

        if (CreateProcess((char*)ProcessExePath.Data, (char*)Parameters.Data, NULL, NULL, TRUE, 0, NULL, Dir, &StartupInfo, &ProcessInfo))
        {
            ProcessHandle = ProcessInfo.hProcess;

            SetPriorityClass(ProcessInfo.hProcess, HIGH_PRIORITY_CLASS);
            CloseHandle(ProcessInfo.hThread);
        }
        else
        {
            StringLocal(Prefix, Kibibytes(8));
            String_Format(&Prefix, S("Failed to run command: \"%S\""), Parameters);
            LogLastError(Prefix);
        }
    }

    return ProcessHandle;
}

NO_DISCARD PlatformHandle Platform_RunCommand_Ex(const String CmdLine, const String WorkingDirectory, PlatformPipe* StdOutPipe)
{
    PROCESS_INFORMATION ProcessInfo = {0};
    SECURITY_ATTRIBUTES saAttr = {0}; 
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES); 
    saAttr.bInheritHandle = TRUE; 
    saAttr.lpSecurityDescriptor = NULL; 

    PlatformHandle ProcessHandle = INVALID_HANDLE_VALUE;

    HANDLE r = 0, w = 0;

    bool bSuccess = CreatePipe(&r, &w, &saAttr, 0);

    if (bSuccess)
    {
        bSuccess = SetHandleInformation(r, HANDLE_FLAG_INHERIT, 0);
    }

    if (bSuccess)
    {
        if (StdOutPipe)
        {
            (*StdOutPipe)[0] = r;
            (*StdOutPipe)[1] = w;
        }

        STARTUPINFO StartupInfo = {0};
        StartupInfo.cb = sizeof(STARTUPINFO);
        StartupInfo.hStdError = w;
        StartupInfo.hStdOutput = w;
        StartupInfo.hStdInput = NULL;
        StartupInfo.dwFlags |= STARTF_USESTDHANDLES;

        StringLocal(Copy, MAX_PATH_LENGTH);
        String_Copy(&Copy, WorkingDirectory);
        const char* Dir = Copy.Length > 0 ? (char*)Copy.Data : NULL;

        if (CreateProcess(NULL, (char*)CmdLine.Data, NULL, NULL, TRUE, 0, NULL, Dir, &StartupInfo, &ProcessInfo))
        {
            ProcessHandle = ProcessInfo.hProcess;

            SetPriorityClass(ProcessInfo.hProcess, HIGH_PRIORITY_CLASS);
            CloseHandle(ProcessInfo.hThread);
        }
        else
        {
            StringLocal(Prefix, Kibibytes(8));
            String_Format(&Prefix, S("Failed to run command: \"%S\""), CmdLine);
            LogLastError(Prefix);
        }
    }

    return ProcessHandle;
}

NO_DISCARD bool Platform_TerminateProcess(PlatformHandle Handle, u32 ExitCode)
{
    bool bResult = TerminateProcess(Handle, ExitCode);
    CloseHandle(Handle);
    return bResult;
}

NO_DISCARD bool Platform_FindProgram(String ProgramName)
{
    return Platform_FindFile_Ex(ProgramName, S(".exe"), NULL) ||
           Platform_FindFile_Ex(ProgramName, S(".com"), NULL);
}

NO_DISCARD bool Platform_FindProgram_Ex(String ProgramName, String* OutProgramPath)
{
    return Platform_FindFile_Ex(ProgramName, S(".exe"), OutProgramPath) ||
           Platform_FindFile_Ex(ProgramName, S(".com"), OutProgramPath);
}

NO_DISCARD bool Platform_FindFile(String FileName, String ExtensionWithDot)
{
    return Platform_FindFile_Ex(FileName, ExtensionWithDot, NULL);
}

NO_DISCARD bool Platform_FindFile_Ex(String FileName, String ExtensionWithDot, String* OutFilePath)
{
    bool bSuccess = String_IsValid(FileName);

    if (bSuccess)
    {
        StringLocal(FileNameCopy, MAX_PATH_LENGTH);
        String_Copy(&FileNameCopy, FileName);

        const char* Ext = NULL;
        if (ExtensionWithDot.Length > 1)
        {
            Ext = (char*)ExtensionWithDot.Data;
        }

        u8 FullPath[MAX_PATH] = {0};
        DWORD Len = SearchPath(NULL, (char*)FileNameCopy.Data, Ext, MAX_PATH, (char*)FullPath, NULL);
        bSuccess = Len > 0;

        if (bSuccess && OutFilePath)
        {
            //String_Empty(OutFilePath);
            //String_AppendChar(OutFilePath, '"');
            String_Copy(OutFilePath, StrSlice(FullPath, Len));
            //String_AppendChar(OutFilePath, '"');
        }
    }

    return bSuccess;
}

NO_DISCARD bool Platform_IsValidHandle(const PlatformHandle Handle)
{
    return Handle != NULL && Handle != INVALID_HANDLE_VALUE;
}

NO_DISCARD usize Platform_GetCriticalSectionMemoryRequirement(void)
{
    return sizeof(CRITICAL_SECTION);
}

void Platform_InitializeCriticalSection(PlatformCriticalSection OutCriticalSection)
{
    InitializeCriticalSectionAndSpinCount(OutCriticalSection, 0);
}

void Platform_DeleteCriticalSection(PlatformCriticalSection CriticalSection)
{
    DeleteCriticalSection(CriticalSection);
}

void Platform_EnterCriticalSection(PlatformCriticalSection CriticalSection)
{
    EnterCriticalSection(CriticalSection);
}

void Platform_ExitCriticalSection(PlatformCriticalSection CriticalSection)
{
    LeaveCriticalSection(CriticalSection);
}

NO_DISCARD bool Platform_AnyKeyPressed(void)
{
    bool bHit = false;

    // Run through all key scancodes from 7 to 255
    // start from 0x07. the first 6 are mouse keys
    for (i32 i = 7; i < 255; i++)
    {
        i32 State = GetAsyncKeyState(i);
        if (State & 0x01)
        {
            // a key has been pressed
            bHit = true;
            break;
        }
    }

    return bHit;
}

void Platform_BeginNonBlockingMode(void)
{
}

void Platform_EndNonBlockingMode(void)
{
}

NO_DISCARD bool Platform_GetFullCpuName(String* OutName)
{
    HKEY Key = NULL;
    LSTATUS Status = RegOpenKeyExA(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0\\",
                       0, KEY_QUERY_VALUE | KEY_WOW64_32KEY | KEY_ENUMERATE_SUB_KEYS, &Key);

    bool bSuccess = Status == S_OK;

    if (bSuccess)
    {
        DWORD Length = 511;
        StringLocal(CpuName, 512);

        Status = RegQueryValueEx(Key, "ProcessorNameString", NULL, NULL, (LPBYTE)CpuName.Data, &Length);

        bSuccess = Status == S_OK && Length > 0;

        if (bSuccess)
        {
            CpuName.Length = Length-1;
            String_Copy(OutName, CpuName);
        }

        xx RegCloseKey(Key);
    }

    return bSuccess;
}

NO_DISCARD u32 Platform_GetExitCodeForProcess(PlatformHandle Handle)
{
    u32 Code = 0;

    if (Platform_IsValidHandle(Handle))
    {
        DWORD ExitCode = 0;
        if (GetExitCodeProcess(Handle, &ExitCode))
        {
            Code = ExitCode;
        }
        else
        {
            Code = UINT32_MAX;
        }

        if (ExitCode == 259) // STILL_ACTIVE
        {
            Code = UINT32_MAX;
        }
    }

    return Code;
}

NO_DISCARD u32 Platform_WaitForProcessAndGetExitCode(PlatformHandle Handle)
{
    u32 Code = 0;

        //Platform_ConsoleWrite("waiting", 0, false);


    if (Platform_IsValidHandle(Handle))
    {
        (void)WaitForSingleObject(Handle, INFINITE);

        DWORD ExitCode = 0;
        if (GetExitCodeProcess(Handle, &ExitCode))
        {
            Code = ExitCode;
        }
        else
        {
            Code = UINT32_MAX;
        }
    }

    return Code;
}

NO_DISCARD u32 Platform_WaitForMultipleHandles(PlatformHandle* Handles, u32 NumHandles, i32 Milliseconds, bool bWaitAll)
{
    i32 Time = Milliseconds <= 0 ? (i32)INFINITE : Milliseconds;
    return WaitForMultipleObjects(NumHandles, Handles, bWaitAll, (u32)Time);
}

void Platform_WaitForHandle(PlatformHandle Handle, i32 Milliseconds)
{
    if (Platform_IsValidHandle(Handle))
    {
        i32 Time = Milliseconds <= 0 ? (i32)INFINITE : Milliseconds;
        (void)WaitForSingleObject(Handle, (u32)Time);
    }
}

void Platform_CloseHandle(PlatformHandle Handle)
{
    if (Platform_IsValidHandle(Handle))
    {
        CloseHandle(Handle);
    }
}

NO_DISCARD bool Platform_GetTerminalDimensions(u32* OutRows, u32* OutColumns)
{
    bool bSuccess = false;

    CONSOLE_SCREEN_BUFFER_INFO csbi = {0};
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi))
    {
        bSuccess = true;

        *OutColumns = (u32)(csbi.srWindow.Right - csbi.srWindow.Left + 1);
        *OutRows    = (u32)(csbi.srWindow.Bottom - csbi.srWindow.Top + 1);
    }

    return bSuccess;
}

NO_DISCARD PlatformVersion Platform_GetVersion(void)
{
    PlatformVersion Result = {0};
    
    RTL_OSVERSIONINFOW Info = {0};
    Info.dwOSVersionInfoSize = sizeof(Info);
    RtlGetVersion(&Info);

    Result.Major = Info.dwMajorVersion;
    Result.Minor = Info.dwMinorVersion;
    Result.Patch = Info.dwBuildNumber;

    return Result;
}

NO_DISCARD bool Platform_IsConsoleFocused(void)
{
    bool bIsFocused = false;

    HWND ForegroundWindow = GetConsoleWindow();
    DWORD ThisProcessID = GetCurrentProcessId();
    DWORD ForegroundProcessID = 0;
    DWORD Result = GetWindowThreadProcessId(ForegroundWindow, &ForegroundProcessID);
    
    if (Result > 0 && ForegroundProcessID > 0)
    {
        bIsFocused = ThisProcessID == ForegroundProcessID;
    }

    return bIsFocused;
}

NO_DISCARD bool Platform_IsWindowFocused(void)
{
    bool bIsFocused = false;

    HWND ForegroundWindow = GetForegroundWindow();
    DWORD ThisProcessID = GetCurrentProcessId();
    DWORD ForegroundProcessID = 0;
    DWORD Result = GetWindowThreadProcessId(ForegroundWindow, &ForegroundProcessID);
    
    if (Result > 0 && ForegroundProcessID > 0)
    {
        bIsFocused = ThisProcessID == ForegroundProcessID;
    }

    return bIsFocused;
}

NO_DISCARD u32 Platform_GetPosixVersion(void)
{
    return 0;
}

void Platform_DetectDesktopEnvironment(String* DesktopEnv)
{
    PlatformVersion Version = Platform_GetVersion();

    String Name = S("Unknown");

    if      (Version.Major == 5  && Version.Minor == 1) { Name = S("WindowsXP"); }
    else if (Version.Major == 6  && Version.Minor == 0) { Name = S("WindowsVista"); }
    else if (Version.Major == 6  && Version.Minor == 1) { Name = S("Windows7"); }
    else if (Version.Major == 6  && Version.Minor == 2) { Name = S("Windows8"); }
    else if (Version.Major == 6  && Version.Minor == 3) { Name = S("Windows8.1"); }
    else if (Version.Major == 10 && Version.Minor == 0)
    {
        if (Version.Patch < 22000) { Name = S("Windows10"); }
        else                       { Name = S("Windows11"); }
    }

    String_Copy(DesktopEnv, Name);
}

NO_DISCARD i32 Rand(void)
{
    i32 Buffer = 0;

    BCRYPT_ALG_HANDLE Prov = {0};
    xx BCryptOpenAlgorithmProvider(&Prov, BCRYPT_RNG_ALGORITHM, NULL, 0);
    xx BCryptGenRandom(Prov, (PUCHAR)(&Buffer), sizeof(Buffer), 0);
    xx BCryptCloseAlgorithmProvider(Prov, 0);

    return Buffer;
}

/*
PRAGMA_DISABLE_MISSING_PROTOTYPES_WARNING

FORCENOINLINE BOOL __cdecl _DllMainCRTStartup(HANDLE hDllHandle, DWORD dwReason, LPVOID lpreserved)
{
    return true;
}

void* __stack_chk_guard = (void*)((usize)0xdeadbeef);

void __stack_chk_fail(void)
{
    ExitProcess(1);
}

void __chkstk(void)
{
    return;
}

void ___chkstk_ms(void)
{
    return;
}

PRAGMA_ENABLE_WARNINGS
*/

#endif // PLATFORM_WINDOWS
