// Copyright (c) 2024 Ali El Saleh

#ifndef UNITY_BUILD
#include "Platform.h"
#endif

#if PLATFORM_WINDOWS

#ifndef UNITY_BUILD
#include "Globals.h"

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
    FG_WHITE = 15
};

ENUM(WinConsoleForegroundColors)
{
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

#define WM_FILE_WATCHER (WM_USER+1)

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

    //Platform_GetClockFrequency();

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

bool Platform_CreateMutex(PlatformMutex* OutMutex)
{
    if (NEVER(OutMutex == NULL)) return false;

    HANDLE M = CreateMutexA(NULL, TRUE, NULL);
    if (M == NULL) return false;

    OutMutex->Handle = M;
    OutMutex->Name = String_Null();

    bool bError = GetLastError() == ERROR_ALREADY_EXISTS;
    return !bError;
}

bool Platform_CreateNamedMutex(const String Name, PlatformMutex* OutMutex)
{
    if ((NEVER(Name.Length == 0)) || (NEVER(OutMutex == NULL)))
    {
        // a name and mutex ref must be provided
        return false;
    }

    u32 Diff = Name.Length > 255 ? Name.Length - 255 : 0; // clamp to 255 characters
    String ClampedName = StrShiftF(Name, Diff);

    HANDLE M = CreateMutexA(NULL, TRUE, ClampedName.Length == 0 ? NULL : (char*)ClampedName.Data);
    if (M == NULL) return false;

    OutMutex->Handle = M;
    OutMutex->Name = ClampedName;

    bool bError = GetLastError() == ERROR_ALREADY_EXISTS;
    return !bError;
}

bool Platform_ReleaseMutex(PlatformMutex* Mutex)
{
    if (NEVER(Mutex == NULL)) return false;

    BOOL bResult = ReleaseMutex(Mutex->Handle);
    if (bResult)
    {
        bResult = CloseHandle(Mutex->Handle);
    }

    return bResult;
}

u32 Platform_GetConsoleProcessCount(void)
{
    DWORD Processes[4] = {0};
    DWORD Count = GetConsoleProcessList(Processes, 4);
    return Count;
}

void Platform_Abort(u32 ExitCode)
{
    ExitProcess(ExitCode);
}

StringArray Platform_GetCommandLineArgs(void)
{
    StringArray Args = {0};
    Args.Num = (u32)(GArgC-1 <= 0 ? 0 : (GArgC-1 < 128 ? GArgC-1 : 128));
    Args.List = GArgV;
    return Args;
}

f64 Platform_GetClockFrequency(void)
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

void* memset(void *dst, int c, SIZE_T len)
{
    register volatile u8* dp = dst;

    while (len--)
    {
        *dp++ = (u8)c;
    }

    return dst;
}

void* memcpy(void* restrict dst, const void* restrict src, SIZE_T len)
{
    register volatile u8* dp = dst;
    register const u8* sp = src;

    while (len--)
    {
        *dp++ = *sp++;
    }

    return dst;
}

void* memmove(void* dst, const void* src, SIZE_T len)
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

int memcmp(const void* s1, const void* s2, SIZE_T len)
{
    register const u8* p1 = (const u8*)s1;
    register const u8* p2 = (const u8*)s2;

    for (register usize i = 0; i < len; i++)
    {
        if (p1[i] < p2[i])
        {
            return -1;
        }
        else if (p1[i] > p2[i]) 
        {
            return 1;
        }
    }

    return 0;
}

PRAGMA_ENABLE_WARNINGS

void* Platform_MemAlloc(usize Size)
{
    DWORD dwFlags = HEAP_CREATE_ALIGN_16;
    return HeapAlloc(GetProcessHeap(), dwFlags, Size);
}

void* Platform_MemAllocZero(usize Size)
{
    DWORD dwFlags = HEAP_ZERO_MEMORY | HEAP_CREATE_ALIGN_16;
    return HeapAlloc(GetProcessHeap(), dwFlags, Size);
}

void* Platform_MemReAlloc(const void* Block, usize Size)
{
    if (!Block)
        return Platform_MemAlloc(Size);

    DWORD dwFlags = HEAP_ZERO_MEMORY;
    return HeapReAlloc(GetProcessHeap(), dwFlags, (void*)Block, Size);
}

void Platform_MemFree(const void* Block)
{
    (void)HeapFree(GetProcessHeap(), 0, (void*)Block);
}

void Platform_MemZero(void* Block, usize Size)
{
    RtlZeroMemory(Block, Size);
}

void Platform_MemCopy(void* restrict Dest, const void* restrict Source, usize Size)
{
    RtlCopyMemory(Dest, Source, Size);
}

void Platform_MemMove(void* restrict Dest, const void* restrict Source, usize Size)
{
    RtlMoveMemory(Dest, Source, Size);
}

void Platform_MemSet(void* Dest, i32 Value, usize Size)
{
    RtlFillMemory(Dest, Size, (BYTE)Value);
}

bool Platform_SetWorkingDirectory(const String Path)
{
    StringLocal(Copy, MAX_PATH_LENGTH);
    String_Copy(&Copy, Path);

    BOOL bResult = SetCurrentDirectoryA((char*)Copy.Data);
    return bResult;
}

void Platform_ConsoleWrite(const char* Message, u8 Color, bool bIsError)
{
    Platform_ConsoleWrite_CustomLength(Message, String_GetLength(Message), Color, bIsError);
}

void Platform_ConsoleWrite_CustomLength(const char* Message, u32 Length, u8 Color, bool bIsError)
{
    DWORD OutputHandle = STD_ERROR_HANDLE;// bIsError ? STD_ERROR_HANDLE : STD_OUTPUT_HANDLE;
    HANDLE ConsoleHandle = GetStdHandle(OutputHandle);

    static u8 GConsoleColorLevels[7] = { CONSOLE_INFO_COLOR, CONSOLE_SUCCESS_COLOR, CONSOLE_WARNING_COLOR, CONSOLE_ERROR_COLOR, CONSOLE_FATAL_COLOR, CONSOLE_INFO_COLOR, CONSOLE_MUTE_COLOR };

    // SetConsoleTextAttribute is slow, so only call it when the color changes
    const u8 ConsoleColor = GConsoleColorLevels[Color];
    if (ConsoleColor != CONSOLE_INFO_COLOR) // Regular white color
    {
        (void)SetConsoleTextAttribute(ConsoleHandle, ConsoleColor);
    }

    bool bIgnoreNewLine = Color == 4 && Message[Length-1] == '\n';
    if (UNLIKELY(bIgnoreNewLine))
        Length--;

    #if _DEBUG
    OutputDebugString(Message);
    #endif

    (void)WriteConsole(ConsoleHandle, Message, (DWORD)Length, NULL, 0);

    // SetConsoleTextAttribute is slow, so only call it when the color changes
    if (ConsoleColor != CONSOLE_INFO_COLOR) // Regular white color
    {
        // Reset back to white
        (void)SetConsoleTextAttribute(ConsoleHandle, CONSOLE_INFO_COLOR);
    }

    if (UNLIKELY(bIgnoreNewLine))
    {
        (void)WriteConsole(ConsoleHandle, "\n", 1, NULL, 0);
    }
}

f64 Platform_GetAbsoluteTime(void)
{
    LARGE_INTEGER Frequency = {0}, Now = {0};

    BOOL bSuccess = QueryPerformanceFrequency(&Frequency);
    if (bSuccess)
        bSuccess = QueryPerformanceCounter(&Now);

    f64 Result = bSuccess ? (f64)Now.QuadPart * (1.0/(f64)Frequency.QuadPart) : 0.0;
    return Result;
}

SystemTime Platform_GetSystemLocalTime(void)
{
    SYSTEMTIME SysTime = {0};
    GetLocalTime(&SysTime);

    SystemTime Result  = {0};
    Result.Year        = SysTime.wYear;
    Result.Month       = SysTime.wMonth;
    Result.DayOfWeek   = SysTime.wDayOfWeek;
    Result.Day         = SysTime.wDay;
    Result.Hour        = SysTime.wHour;
    Result.Minute      = SysTime.wMinute;
    Result.Second      = SysTime.wSecond;
    Result.Millisecond = SysTime.wMilliseconds;

    return Result;
}

bool Platform_GetTimeZone(String* OutTimeZone)
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
    
    return Result > 0;
}

void Platform_Sleep(f64 ms)
{
    if (ms > 0)
    {
        LARGE_INTEGER Frequency = {0}, Now = {0};
        BOOL bSuccess = QueryPerformanceCounter(&Now);
        if (bSuccess)
            bSuccess = QueryPerformanceFrequency(&Frequency);

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

u64 Platform_GetCurrentThreadID(void)
{
    return GetCurrentThreadId();
}

u64 Platform_GetMainThreadID(void)
{
    return GetCurrentThreadId();
}

bool Platform_GetAccountName(String* OutName)
{
    u8 UserName[256] = {0};
    DWORD Size = 255;
    BOOL bResult = GetUserName((char*)UserName, &Size);
    if (!bResult)
    {
        LogLastError(S("Failed to get the current user name"));
        return false;
    }

    String_Copy(OutName, StrSlice(UserName, Size-1));
    return true;
}

bool Platform_GetUserName(String* OutName)
{
    const i32 id = 0x0028; // USERPROFILE  CSIDL_PROFILE

    char Path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPath(NULL, id, NULL, 0, Path)))
    {
        String Name = CStr(Path);

        u32 LastSlash = 0;
        if (String_IndexOfLastPathSlash(Name, &LastSlash))
        {
            String_Copy(OutName, StrShiftF(Name, LastSlash+1));
        }
        else
        {
            String_Copy(OutName, Name);
        }

        return true;
    }

    return false;
}

bool Platform_GetUserDirectory(String* OutDirectory)
{
    const i32 id = 0x0028; // USERPROFILE  CSIDL_PROFILE

    char Path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPath(NULL, id, NULL, 0, Path)))
    {
        String_Copy(OutDirectory, CStr(Path));

        return true;
    }

    return false;
}

bool Platform_GetCurrentProcessName(String* OutName)
{
    TCHAR FileName[MAX_PATH] = {0};
    u32 Len = GetModuleFileName(NULL, FileName, MAX_PATH);

    for (u32 i = Len; i > 0; i--)
    {
        if (FileName[i] == '\\')
        {
            String_Copy(OutName, StrSlice((uchar*)&FileName[i+1], Len-i-1));
            break;
        }
    }

    return true;
}

u64 Platform_GetCurrentProcessID(void)
{
    return GetCurrentProcessId();
}

void Platform_GetWorkingDirectory(String* OutPath)
{
    u32 Len = GetCurrentDirectory(OutPath->Capacity, (char*)OutPath->Data);
    OutPath->Length = Len;
}

bool Platform_GetEnvironmentVariableValue(String Name, String* OutVariable)
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

bool Platform_SetEnvironmentVariableValue(String Name, String Value)
{
    StringLocal(NameCopy, 128); // we copy the name because the passed in Name could have had its length altered but not the data, so create a copy with a null terminator at the length so windows gets the correct string
    String_Copy(&NameCopy, Name);

    StringLocal(ValueCopy, 4096);
    String_Copy(&ValueCopy, Value);

    BOOL bSuccess = SetEnvironmentVariable((char*)NameCopy.Data, (char*)ValueCopy.Data);
    return bSuccess;
}

bool Platform_DoesEnvironmentVariableExist(String Name)
{
    StringLocal(NameCopy, 128); // we copy the name because the passed in Name could have had its length altered but not the data, so create a copy with a null terminator at the length so windows gets the correct string
    String_Copy(&NameCopy, Name);

    DWORD Len = GetEnvironmentVariable((char*)NameCopy.Data, NULL, 0);
    return Len != 0;
}

u32 Platform_GetNumLogicalProcessors(void)
{
    SYSTEM_INFO info = {0};
    GetSystemInfo(&info);
    return info.dwNumberOfProcessors;
}

Uuid UUID_Generate(void)
{
    uuid_t id = {0};
    (void)UuidCreate(&id);

    return *(Uuid*)&id;
}

bool UUID_IsEqual(Uuid First, Uuid Second)
{
    return Platform_MemEqual(&First, &Second, sizeof(Uuid));
}

void UUID_ToString(Uuid ID, String* OutString)
{
    // @Speed: Make our own uuid to string converter and not use windows heap allocating string

    RPC_CSTR str = {0};
    RPC_STATUS Status = UuidToString((uuid_t*)&ID, &str);
    if (Status == RPC_S_OK)
    {
        String_Copy(OutString, StrSlice((uchar*)str, GUID_LENGTH-1));
        (void)RpcStringFree(&str);
    }
}

Uuid UUID_FromString(const String IDString)
{
    uuid_t id = {0};
    (void)UuidFromString((const RPC_CSTR)IDString.Data, &id);

    return *(Uuid*)&id;
}

bool Filesystem_Open(const String FilePath, u32 Mode, FileHandle* OutHandle)
{
    DWORD OpenStyle;
    DWORD ShareStyle;
    DWORD Disposition;
    if ((Mode & FileMode_Read) != 0 && (Mode & FileMode_Write) != 0)
    {
        OpenStyle = GENERIC_READ | GENERIC_WRITE;
        ShareStyle = FILE_SHARE_READ | FILE_SHARE_WRITE;
        Disposition = OPEN_ALWAYS;
    }
    else if ((Mode & FileMode_Read) != 0 && (Mode & FileMode_Write) == 0)
    {
        OpenStyle = GENERIC_READ;
        ShareStyle = FILE_SHARE_READ;
        Disposition = OPEN_EXISTING;
    }
    else if ((Mode & FileMode_Read) == 0 && (Mode & FileMode_Write) != 0)
    {
        OpenStyle = GENERIC_WRITE;
        ShareStyle = FILE_SHARE_WRITE;
        Disposition = CREATE_ALWAYS;
    }
    else
    {
        // Invalid mode passed (%u) while trying to open file: %S", Mode, FilePath
        ENSURE(0);
        return false;
    }

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
                if (!bDirectoryCreated)
                {
                    bDirectoryCreated = CreateDirectory((char*)BaseDirectory.Data, NULL);

                    if (!bDirectoryCreated)
                    {
                        StringLocal(Prefix, 512);
                        String_Format(&Prefix, S("Failed to create directory \"%S\""), BaseDirectory);
                        LogLastError(Prefix);

                        return false;
                    }
                }

                break;
            }
        }
    }
    while (bFoundPathSeparator);

    HANDLE File = CreateFile((char*)FilePath.Data, OpenStyle, ShareStyle, NULL, Disposition, FILE_ATTRIBUTE_NORMAL, NULL);
    if (File == INVALID_HANDLE_VALUE)
    {
        StringLocal(Prefix, 512);
        String_Format(&Prefix, S("Failed to open file \"%S\""), FilePath);
        LogLastError(Prefix);

        return false;
    }

    if (OutHandle)
    {
        OutHandle->Data = File;
        OutHandle->Data2 = NULL;
    }

    return true;
}

bool Filesystem_NewFile(const String FilePath)
{
    FileHandle f = {0};
    bool bSuccess = Filesystem_Open(FilePath, FileMode_Write, &f);
    Filesystem_Close(&f);

    return bSuccess;
}

bool Filesystem_DeleteFile(String FilePath)
{
    StringLocal(Copy, MAX_PATH);
    String_Copy(&Copy, FilePath);
    
    i32 Result = DeleteFile((char*)Copy.Data) != 0;

    return Result != 0;
}

bool Filesystem_Open_MemoryMapped(const String FilePath, u32 Mode, FileHandle* OutHandle, u8** OutData, usize* OutSize)
{
    if (OutSize)
        *OutSize = 0;

    if (OutData)
        *OutData = NULL;

    if (NEVER(OutHandle == NULL))
        return false;

    if (!IsValidFileHandle(*OutHandle))
    {
        if (!Filesystem_Open(FilePath, Mode, OutHandle))
        {
            return false;
        }
    }

    if (IsValidFileHandle(*OutHandle))
    {
        DWORD ProtectFlag;
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
            Filesystem_Close(OutHandle);
            //LOG_ERROR("Invalid mode passed (%u) while trying to map view file: %S", Mode, FilePath);
            ENSURE(0);
            return false;
        }

        HANDLE fm = CreateFileMapping(OutHandle->Data, NULL, ProtectFlag, 0, 0, NULL);
        if (fm == NULL || fm == INVALID_HANDLE_VALUE)
        {
            StringLocal(Prefix, 512);
            String_Format(&Prefix, S("Failed to create file mapping for \"%S\""), FilePath);
            LogLastError(Prefix);
            Filesystem_Close(OutHandle);
            return false;
        }

        OutHandle->Data2 = fm;

        DWORD OpenStyle;
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
            //LOG_ERROR("Invalid mode passed (%u) while trying to map view file: %S", Mode, FilePath);
            Filesystem_Close(OutHandle);
            ENSURE(0);
            return false;
        }

        LARGE_INTEGER FileSize = {0};
        (void)GetFileSizeEx(OutHandle->Data, &FileSize);

        if (OutSize)
            *OutSize = (usize)FileSize.QuadPart;

        if (OutData)
            *OutData = MapViewOfFile(fm, OpenStyle, 0, 0, (SIZE_T)FileSize.QuadPart);

        return true;
    }

    return false;
}

bool Filesystem_OpenDirectory(const String FilePath)
{
    if (Filesystem_DoesDirectoryExist(FilePath))
        return true;

    bool bAnySuccess = false;
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
                    String_Copy(&BaseDirectory, StrSlice(FilePath.Data, i+1));
                else
                    String_Copy(&BaseDirectory, StrSlice(FilePath.Data, i));

                NextSlashIndex = i+1;

                BOOL bDirectoryCreated = Filesystem_DoesDirectoryExist(BaseDirectory);
                if (!bDirectoryCreated)
                {
                    bDirectoryCreated = CreateDirectory((char*)BaseDirectory.Data, NULL);

                    if (!bDirectoryCreated)
                    {
                        StringLocal(Prefix, 512);
                        String_Format(&Prefix, S("Failed to create directory \"%S\""), BaseDirectory);
                        LogLastError(Prefix);

                        return false;
                    }

                    bAnySuccess = true;
                }

                break;
            }
        }
    }
    while (bFoundPathSeparator);

    return bAnySuccess;
}

bool Filesystem_OpenDirectory_Ex(const String FilePath, FileHandle* OutHandle)
{
    if (!Filesystem_OpenDirectory(FilePath))
    {
        return false;
    }

    if (NEVER(OutHandle == NULL)) return false;

    HANDLE File = CreateFile((char*)FilePath.Data, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_FLAG_BACKUP_SEMANTICS, NULL);

    if (File == INVALID_HANDLE_VALUE)
    {
        StringLocal(Prefix, 512);
        String_Format(&Prefix, S("Failed to open directory \"%S\""), FilePath);
        LogLastError(Prefix);

        return false;
    }

    OutHandle->Data = File;

    return true;
}

void Filesystem_Close(FileHandle* Handle)
{
    if (NEVER(Handle == NULL))
        return;

    if (Handle->Data2)
        CloseHandle(Handle->Data2);

    if (IsValidFileHandle(*Handle))
    {
        CloseHandle(Handle->Data);
        *Handle = FileHandle_Null();
    }
}

bool Filesystem_Seek(const FileHandle Handle, isize Offset)
{
    DWORD Result = SetFilePointer(Handle.Data, (long)Offset, NULL, FILE_CURRENT);
    return Result != INVALID_SET_FILE_POINTER;
}

bool Filesystem_SeekFromBeginning(const FileHandle Handle, usize Offset)
{
    DWORD Result = SetFilePointer(Handle.Data, (long)Offset, NULL, FILE_BEGIN);
    return Result != INVALID_SET_FILE_POINTER;
}

bool Filesystem_SeekFromEnd(const FileHandle Handle, usize Offset)
{
    DWORD Result = SetFilePointer(Handle.Data, (long)Offset, NULL, FILE_END);
    return Result != INVALID_SET_FILE_POINTER;
}

bool Filesystem_SeekToBeginning(const FileHandle Handle)
{
    DWORD Result = SetFilePointer(Handle.Data, 0, NULL, FILE_BEGIN);
    return Result != INVALID_SET_FILE_POINTER;
}

bool Filesystem_SeekToEnd(const FileHandle Handle)
{
    DWORD Result = SetFilePointer(Handle.Data, 0, NULL, FILE_END);
    return Result != INVALID_SET_FILE_POINTER;
}

usize Filesystem_GetCurrentFilePosition(const FileHandle Handle)
{
    return SetFilePointer(Handle.Data, 0, NULL, FILE_CURRENT);
}

FileHandle Filesystem_GetStdInputHandle(void)
{
    FileHandle Handle = {0};
    Handle.Data = GetStdHandle(STD_INPUT_HANDLE);
    return Handle;
}

usize Filesystem_GetLastWriteTime(const String FilePath)
{
    if (!Filesystem_DoesFileExist(FilePath))
        return 0;

    FileHandle f = {0};
    FILETIME FileTimeStamp = {0};
    if (Filesystem_Open(FilePath, FileMode_Read, &f))
    {
        (void)GetFileTime(f.Data, NULL, NULL, &FileTimeStamp);
        Filesystem_Close(&f);
    }

    ULONGLONG a = (((ULONGLONG)FileTimeStamp.dwHighDateTime) << 32) + FileTimeStamp.dwLowDateTime;
    return (usize)a;
}

usize Filesystem_GetLastAccessTime(const String FilePath)
{
    if (!Filesystem_DoesFileExist(FilePath))
        return 0;

    FileHandle f = {0};
    FILETIME FileTimeStamp = {0};
    if (Filesystem_Open(FilePath, FileMode_Read, &f))
    {
        (void)GetFileTime(f.Data, NULL, &FileTimeStamp, NULL);
        Filesystem_Close(&f);
    }

    ULONGLONG a = (((ULONGLONG)FileTimeStamp.dwHighDateTime) << 32) + FileTimeStamp.dwLowDateTime;
    return (usize)a;
}

usize Filesystem_GetCreationTime(const String FilePath)
{
    if (!Filesystem_DoesFileExist(FilePath))
        return 0;

    FileHandle f = {0};
    FILETIME FileTimeStamp = {0};
    if (Filesystem_Open(FilePath, FileMode_Read, &f))
    {
        (void)GetFileTime(f.Data, &FileTimeStamp, NULL, NULL);
        Filesystem_Close(&f);
    }

    ULONGLONG a = (((ULONGLONG)FileTimeStamp.dwHighDateTime) << 32) + FileTimeStamp.dwLowDateTime;
    return (usize)a;
}

FileTimeData Filesystem_GetFileTime(const String FilePath)
{
    FileTimeData Time = {0};

    if (!Filesystem_DoesFileExist(FilePath))
        return Time;

    FileHandle f = {0};
    if (Filesystem_Open(FilePath, FileMode_Read, &f))
    {
        FILETIME CreationTime = {0};
        FILETIME LastAccessTime = {0};
        FILETIME LastWriteTime = {0};

        (void)GetFileTime(f.Data, &CreationTime, &LastAccessTime, &LastWriteTime);
        Filesystem_Close(&f);

        Time.CreationTime = (((ULONGLONG)CreationTime.dwHighDateTime) << 32) + CreationTime.dwLowDateTime;
        Time.LastAccessTime = (((ULONGLONG)LastAccessTime.dwHighDateTime) << 32) + LastAccessTime.dwLowDateTime;
        Time.LastWriteTime = (((ULONGLONG)LastWriteTime.dwHighDateTime) << 32) + LastWriteTime.dwLowDateTime;
    }

    return Time;
}

usize Filesystem_GetLastWriteTimeH(const FileHandle Handle)
{
    FILETIME FileTimeStamp = {0};
    (void)GetFileTime(Handle.Data, NULL, NULL, &FileTimeStamp);

    ULONGLONG a = (((ULONGLONG)FileTimeStamp.dwHighDateTime) << 32) + FileTimeStamp.dwLowDateTime;
    return (usize)a;
}

usize Filesystem_GetLastAccessTimeH(const FileHandle Handle)
{
    FILETIME FileTimeStamp = {0};
    (void)GetFileTime(Handle.Data, NULL, &FileTimeStamp, NULL);

    ULONGLONG a = (((ULONGLONG)FileTimeStamp.dwHighDateTime) << 32) + FileTimeStamp.dwLowDateTime;
    return (usize)a;
}

usize Filesystem_GetCreationTimeH(const FileHandle Handle)
{
    FILETIME FileTimeStamp = {0};
    (void)GetFileTime(Handle.Data, &FileTimeStamp, NULL, NULL);

    ULONGLONG a = (((ULONGLONG)FileTimeStamp.dwHighDateTime) << 32) + FileTimeStamp.dwLowDateTime;
    return (usize)a;
}

FileTimeData Filesystem_GetFileTimeH(const FileHandle Handle)
{
    FileTimeData Time = {0};

    FILETIME CreationTime = {0};
    FILETIME LastAccessTime = {0};
    FILETIME LastWriteTime = {0};
    (void)GetFileTime(Handle.Data, &CreationTime, &LastAccessTime, &LastWriteTime);

    Time.CreationTime = (((ULONGLONG)CreationTime.dwHighDateTime) << 32) + CreationTime.dwLowDateTime;
    Time.LastAccessTime = (((ULONGLONG)LastAccessTime.dwHighDateTime) << 32) + LastAccessTime.dwLowDateTime;
    Time.LastWriteTime = (((ULONGLONG)LastWriteTime.dwHighDateTime) << 32) + LastWriteTime.dwLowDateTime;

    return Time;
}

bool Filesystem_ReadPipe(PlatformPipe Handle, usize DataSize, void* OutData, usize* OutBytesRead)
{
    if (NEVER(Handle[0] == NULL)) return false;
    if (NEVER(Handle[1] == NULL)) return false;

    DWORD BytesRead = 0;
    BOOL Result = ReadFile(Handle[0], OutData, (DWORD)DataSize, &BytesRead, NULL);

    if (OutBytesRead)
        *OutBytesRead = BytesRead;

    return Result;
}

bool Filesystem_Read(const FileHandle Handle, usize DataSize, void* OutData, usize* OutBytesRead)
{
    if (NEVER(!IsValidFileHandle(Handle)))
        return false;

    DWORD BytesRead = 0;
    BOOL Result = ReadFile(Handle.Data, OutData, (DWORD)DataSize, &BytesRead, NULL);

    if (OutBytesRead)
        *OutBytesRead = BytesRead;

    return Result;
}

bool Filesystem_ReadEntireFile(const FileHandle Handle, void* OutData, usize* OutBytesRead)
{
    if (NEVER(!IsValidFileHandle(Handle)))
        return false;

    usize Size = 0;
    if (!Filesystem_GetFileSize(Handle, &Size))
        return false;

    if (!Filesystem_SeekToBeginning(Handle))
        return false;

    DWORD BytesRead = 0;
    BOOL Result = ReadFile(Handle.Data, OutData, (DWORD)Size, &BytesRead, NULL);

    if (OutBytesRead)
        *OutBytesRead = BytesRead;

    return Result;
}

bool Filesystem_ReadLine(const FileHandle Handle, String* LineBuffer)
{
    if (NEVER(!IsValidFileHandle(Handle))) return false;
    if (NEVER(LineBuffer == NULL)) return false;
    if (NEVER(LineBuffer->Data == NULL || LineBuffer->Data == String_Null().Data)) return false;

    DWORD CurrentPosition = SetFilePointer(Handle.Data, 0, NULL, FILE_CURRENT);

    LARGE_INTEGER FileSize = {0};
    if (!GetFileSizeEx(Handle.Data, &FileSize))
    {
        return false;
    }

    usize Size = (usize)FileSize.QuadPart;
    if (Size == 0)
    {
        return false;
    }

    if (CurrentPosition >= Size)
    {
        (void)Filesystem_SeekToBeginning(Handle);
        return false;
    }

    u8 TempBuffer[8192] = {0};
    DWORD BytesRead = 0;
    if (!ReadFile(Handle.Data, TempBuffer, 8192, &BytesRead, NULL))
    {
        LogLastError(S("Filesystem_ReadLine | ReadFile() failed"));

        return false;
    }

    u32 Counter = 0;
    u32 FilePointerOffset = 0;
    for (u32 i = 0; i < BytesRead; i++)
    {
        if (TempBuffer[i] == '\0' || TempBuffer[i] == '\n' || TempBuffer[i] == '\r')
        {
            FilePointerOffset = Counter;

            if (TempBuffer[i] == '\r' && TempBuffer[i+1] == '\n') // todo: bounds check
                FilePointerOffset += 2;
            else
                FilePointerOffset++;

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
        return false;
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

    (void)SetFilePointer(Handle.Data, (i32)(CurrentPosition + FilePointerOffset), NULL, FILE_BEGIN);

    return true;
}

// todo: make internal function, code duplication
/*
bool Filesystem_ReadLine_Backwards(const FileHandle Handle, String* LineBuffer)
{
    if (NEVER(!IsValidFileHandle(Handle))) return false;
    if (NEVER(LineBuffer == NULL)) return false;

    {
        DWORD CurrentPosition = SetFilePointer(Handle.Data, 0, NULL, FILE_CURRENT);

        if (CurrentPosition == 0)
        {
            return false;
        }

        DWORD BytesRead = 0;
        char Char[2] = {0};

        SetFilePointer(Handle.Data, -2, NULL, FILE_CURRENT);

        bool bFirstNewLineFound = false;

        while (ReadFile(Handle.Data, Char, 1, &BytesRead, NULL))
        {
            if (Char[0] == '\0' || Char[0] == '\n' || Char[0] == '\r')
            {
                if (!bFirstNewLineFound)
                {
                    bFirstNewLineFound = true;
                }
                else
                {
                    break;
                }
            }
            else if (SetFilePointer(Handle.Data, 0, NULL, FILE_CURRENT) == 0)
            {
                break;
            }

            SetFilePointer(Handle.Data, -2, NULL, FILE_CURRENT);
        }

        char TempBuffer[8192] = {0};
        if (!ReadFile(Handle.Data, TempBuffer, 8192, &BytesRead, NULL))
        {
            LogLastError(S("Filesystem_ReadLine_Backwards | ReadFile() failed"));

            return false;
        }

        u32 Counter = 0;
        u32 FilePointerOffset = 0;
        for (u32 i = 0; i < BytesRead; i++)
        {
            if (TempBuffer[i] == '\0' || TempBuffer[i] == '\n' || TempBuffer[i] == '\r')
            {
                FilePointerOffset = Counter;

                char* p = &TempBuffer[i];

                // eat new lines and returns
                while (*p == '\n' || *p == '\r')
                {
                    p++;
                    FilePointerOffset++;
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
            return false;
        }

        u32 MaxLength = Min(LineBuffer->Capacity, 8192);
        u32 LineLength = Min(MaxLength-1, Counter);

        if (LineLength > 0)
        {
            String_Copy(LineBuffer, StrSlice(TempBuffer, LineLength));
        }

        return Counter > 0;
    }
}
*/

bool Filesystem_Write(const FileHandle Handle, usize DataSize, const void* Data, usize* OutBytesWritten)
{
    if (NEVER(!IsValidFileHandle(Handle))) return false;
    if (DataSize == 0) return false;

    if (!Filesystem_SeekToBeginning(Handle))
        return false;

    DWORD BytesWritten = 0;
    BOOL bResult = WriteFile(Handle.Data, Data, (DWORD)DataSize, &BytesWritten, NULL);

    if (OutBytesWritten)
    {
        *OutBytesWritten = BytesWritten;
    }

    return bResult;
}

bool Filesystem_WriteLine(const FileHandle Handle, const String Text, usize* OutBytesWritten)
{
    if (NEVER(!IsValidFileHandle(Handle))) return false;

    bool bResult = Filesystem_SeekToEnd(Handle);

    if (bResult)
    {
        DWORD BytesWritten = 0;
        bResult = WriteFile(Handle.Data, Text.Data, (DWORD)Text.Length, &BytesWritten, NULL);

        if (OutBytesWritten)
        {
            *OutBytesWritten = BytesWritten;
        }
    }

    return bResult;
}

bool Filesystem_WriteLineFormatted(const FileHandle Handle, const String Text, usize* OutBytesWritten, ...)
{
    if (NEVER(!IsValidFileHandle(Handle))) return false;

    bool bResult = Filesystem_SeekToEnd(Handle);
    if (bResult)
    {
        va_list Args;
        va_start(Args, OutBytesWritten);
        StringLocal(Buffer, 32768);
        String_FormatV(&Buffer, Text, Buffer.Capacity, Args);
        va_end(Args);

        DWORD BytesWritten = 0;
        bResult = WriteFile(Handle.Data, Buffer.Data, (DWORD)Buffer.Length, &BytesWritten, NULL);

        if (OutBytesWritten)
        {
            *OutBytesWritten = BytesWritten;
        }
    }

    return bResult;
}

bool Filesystem_DoesFileExist(const String FilePath)
{
    if (FilePath.Length == 0) return false;

    StringLocal(Copy, MAX_PATH);
    String_Copy(&Copy, FilePath);

    return PathFileExists((char*)Copy.Data);
}

bool Filesystem_DoesDirectoryExist(const String FilePath)
{
    if (FilePath.Length == 0) return false;

    StringLocal(Copy, MAX_PATH);
    String_Copy(&Copy, FilePath);

    DWORD Attrib = GetFileAttributes((char*)Copy.Data);
    return (Attrib != INVALID_FILE_ATTRIBUTES && (Attrib & FILE_ATTRIBUTE_DIRECTORY));
}

bool Filesystem_GetFilePath(const FileHandle File, String* OutPath)
{
    if (!IsValidFileHandle(File)) return false;

    const u32 MaxCap = Min(OutPath->Capacity, MAX_PATH);
    const u32 Length = GetFinalPathNameByHandle(File.Data, (char*)OutPath->Data, MaxCap, FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
    const bool bSuccess = Length > 0;

    if (bSuccess)
    {
        OutPath->Length = Length;
        *OutPath = StrShiftF(*OutPath, 4); // ignore //?/
    }

    return bSuccess;
}

bool Filesystem_GetFileSize(const FileHandle File, usize* OutSize)
{
    if (!IsValidFileHandle(File)) return false;

    LARGE_INTEGER FileSize = {0};
    BOOL Result = GetFileSizeEx(File.Data, &FileSize);
    *OutSize = (usize)FileSize.QuadPart;
    return Result;
}

bool Filesystem_IsFile(const String Path)
{
    DWORD Attrib = GetFileAttributes((char*)Path.Data);
    return (Attrib != INVALID_FILE_ATTRIBUTES && (Attrib & FILE_ATTRIBUTE_NORMAL));
}

bool Filesystem_IsDirectory(const String Path)
{
    DWORD Attrib = GetFileAttributes((char*)Path.Data);
    return (Attrib != INVALID_FILE_ATTRIBUTES && (Attrib & FILE_ATTRIBUTE_DIRECTORY));
}

bool Filesystem_IsNewer(const String PathA, const String PathB)
{
    usize a = Filesystem_GetLastWriteTime(PathA);
    usize b = Filesystem_GetLastWriteTime(PathB);
    return a > b;
}

bool Filesystem_IsOlder(const String PathA, const String PathB)
{
    usize a = Filesystem_GetLastWriteTime(PathA);
    usize b = Filesystem_GetLastWriteTime(PathB);
    return a < b;
}

bool Filesystem_IsPathRelative(const String Path)
{
    bool bDriveSymbol = String_IndexOfChar(Path, ':', NULL);

    bool bRelative = !bDriveSymbol;

    return bRelative;
}

bool Filesystem_ConvertRelativeToAbsolutePath(String* OutFullPath)
{
    StringLocal(Copy, MAX_PATH);
    String_Copy(&Copy, *OutFullPath);

    bool bResult = PathCanonicalize((char*)OutFullPath->Data, (char*)Copy.Data);
    OutFullPath->Length = String_GetLength_Ex((char*)OutFullPath->Data, MAX_PATH);
    return bResult;
}

static bool Internal_IterateDirectory(const String RootPath, const String DirectoryPath, DirectoryIterator Callback, bool bRecursive, void* UserData)
{
    const String RealDirectoryPath = DirectoryPath.Length == 0 ? S(".") : DirectoryPath;

    StringLocal(Temp, MAX_PATH);
    String_Copy(&Temp, RealDirectoryPath);
    String_Append(&Temp, S("\\*"));

    WIN32_FIND_DATA ffd = {0};
    HANDLE Find = FindFirstFile((char*)Temp.Data, &ffd);

    if (Find != INVALID_HANDLE_VALUE)
    {
        bool bSuccess = true;

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
            (void)String_EatPathSeparatorsInline(&FullPath);

            String RelativePath = StrShiftF(FilePath, RootPath.Length);
            (void)String_EatPathSeparatorsInline(&RelativePath);

            if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            {
                bool bResult = Callback(FullPath, RelativePath, FileName, 0, true, UserData);
                if (!bResult)
                {
                    bSuccess = false;
                    break;
                }

                if (bRecursive)
                {
                    if (!Internal_IterateDirectory(RootPath, FullPath, Callback, true, UserData))
                    {
                        bSuccess = false;
                        break;
                    }
                }
            }
            else
            {
                DWORD FileSize = (ffd.nFileSizeHigh * (MAXDWORD+1)) + ffd.nFileSizeLow;
                bool bResult = Callback(FullPath, RelativePath, FileName, FileSize, false, UserData);
                if (!bResult) // the user wants to end the iteration
                {
                    bSuccess = false;
                    break;
                }
            }
        }
        while (FindNextFile(Find, &ffd) != 0);

        FindClose(Find);
        return bSuccess;
    }

    return false;
}

void Filesystem_IterateDirectory(const String BasePath, DirectoryIterator Callback, bool bRecursive)
{
    Internal_IterateDirectory(BasePath, BasePath, Callback, bRecursive, NULL);
}

void Filesystem_IterateDirectory_Ex(const String BasePath, DirectoryIterator Callback, bool bRecursive, void* UserData)
{
    Internal_IterateDirectory(BasePath, BasePath, Callback, bRecursive, UserData);
}

bool Filesystem_DeleteFiles(const String FilePath, const String Wildcard, bool bRecursive)
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

bool Filesystem_DeleteDirectory(const String DirectoryPath)
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

bool Filesystem_Copy(const String Source, const String Destination)
{
    StringLocal(SourceCopy, MAX_PATH);
    StringLocal(DestinationCopy, MAX_PATH);
    String_Copy(&SourceCopy, Source);
    String_Copy(&DestinationCopy, Destination);

    String_ConvertSlashToPlatformSlash(&SourceCopy);
    String_ConvertSlashToPlatformSlash(&DestinationCopy);

    u32 LastSlash = 0;
    if (String_IndexOfLastPathSlash(DestinationCopy, &LastSlash))
    {
        (void)Filesystem_OpenDirectory(StrSlice(DestinationCopy.Data, LastSlash));
    }

    /*
    u32 LastSlash = 0;
    String_IndexOfLastPathSlash(SourceCopy, &LastSlash);

    const String FileName = StrShiftF(SourceCopy, LastSlash);
    if (!String_EndsWith(DestinationCopy, FileName, false))
    {
        String_BuildPath(&DestinationCopy, FileName);
    }

    // TODO: allow source to be a direcotry and copy everything from there

    // try to create the directory if it doesn't exist
    String_IndexOfLastPathSlash(DestinationCopy, &LastSlash);
    Filesystem_OpenDirectory(StrSlice(DestinationCopy.Data, LastSlash));
    */

    // remove the read only attribute if we're copying from a source which had a readonly attribute set on it,
    // otherwise the copy will fail if the file already exists at the destination
    if (Filesystem_DoesFileExist(DestinationCopy))
    {
        (void)SetFileAttributes((char*)DestinationCopy.Data, (u32)GetFileAttributes((char*)DestinationCopy.Data) & (u32)~FILE_ATTRIBUTE_READONLY);
    }

    BOOL bResult = CopyFileEx((char*)SourceCopy.Data, (char*)DestinationCopy.Data, NULL, NULL, NULL, COPY_FILE_NO_BUFFERING);
    if (bResult == 0)
    {
        StringLocal(Msg, 512);
        String_Format(&Msg, S("Failed to copy \"%S\" to \"%S\""), Source, Destination);
        LogLastError(Msg);
        return false;
    }

    return true;
}

bool Filesystem_Move(const String Source, const String Destination, bool bRename)
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
        return false;
    }

    return true;
}

bool Filesystem_ArePathsCommon(String PathA, String PathB)
{
    StringLocal(CommonPath, MAX_PATH);
    i32 Len = PathCommonPrefix((char*)PathA.Data, (char*)PathB.Data, (char*)CommonPath.Data);
    CommonPath.Length = (u32)Len;

    return String_IsEqual(CommonPath, PathA, false);
}

PlatformHandle Platform_RunCommand(const String CmdLine, const String WorkingDirectory, const String EnvBlock)
{
    PROCESS_INFORMATION ProcessInfo = {0};
    STARTUPINFO StartupInfo = {0};
    StartupInfo.cb = sizeof(StartupInfo);

    // todo: verify parent env gets included
    StringLocal(Copy, MAX_PATH_LENGTH);
    String_Copy(&Copy, WorkingDirectory);

    const char* Dir = Copy.Length > 0 ? (char*)Copy.Data : NULL;
    void* Env = EnvBlock.Length == 0 ? NULL : (char*)EnvBlock.Data;
    if (!CreateProcess(NULL, (char*)CmdLine.Data, NULL, NULL, TRUE, 0, Env, Dir, &StartupInfo, &ProcessInfo))
    {
        StringLocal(Prefix, Kibibytes(8));
        String_Format(&Prefix, S("Failed to run command: \"%S\""), CmdLine);
        LogLastError(Prefix);

        return INVALID_HANDLE_VALUE;
    }

    SetPriorityClass(ProcessInfo.hProcess, ABOVE_NORMAL_PRIORITY_CLASS);
    CloseHandle(ProcessInfo.hThread);

    return ProcessInfo.hProcess;
}

PlatformHandle Platform_RunCommand_Ex(const String CmdLine, const String WorkingDirectory, PlatformPipe* StdOutPipe)
{
    PROCESS_INFORMATION ProcessInfo = {0};
    SECURITY_ATTRIBUTES saAttr = {0}; 
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES); 
    saAttr.bInheritHandle = TRUE; 
    saAttr.lpSecurityDescriptor = NULL; 

    HANDLE r = 0, w = 0;
    if (!CreatePipe(&r, &w, &saAttr, 0))
    {
        LogLastError(S("Failed to create pipe"));

        return INVALID_HANDLE_VALUE;
    }

    if (!SetHandleInformation(r, HANDLE_FLAG_INHERIT, 0))
    {
        LogLastError(S("Failed to set pipe information"));

        return INVALID_HANDLE_VALUE;
    }

    (*StdOutPipe)[0] = r;
    (*StdOutPipe)[1] = w;

    STARTUPINFO StartupInfo = {0};
    StartupInfo.cb = sizeof(STARTUPINFO);
    StartupInfo.hStdError = w;
    StartupInfo.hStdOutput = w;
    StartupInfo.hStdInput = NULL;
    StartupInfo.dwFlags |= STARTF_USESTDHANDLES;

    StringLocal(Copy, MAX_PATH_LENGTH);
    String_Copy(&Copy, WorkingDirectory);
    const char* Dir = Copy.Length > 0 ? (char*)Copy.Data : NULL;

    if (!CreateProcess(NULL, (char*)CmdLine.Data, NULL, NULL, TRUE, 0, NULL, Dir, &StartupInfo, &ProcessInfo))
    {
        StringLocal(Prefix, Kibibytes(8));
        String_Format(&Prefix, S("Failed to run command: \"%S\""), CmdLine);
        LogLastError(Prefix);

        return INVALID_HANDLE_VALUE;
    }

    SetPriorityClass(ProcessInfo.hProcess, ABOVE_NORMAL_PRIORITY_CLASS);
    CloseHandle(ProcessInfo.hThread);

    return ProcessInfo.hProcess;
}

bool Platform_TerminateProcess(PlatformHandle Handle, u32 ExitCode)
{
    bool bResult = TerminateProcess(Handle, ExitCode);
    CloseHandle(Handle);
    return bResult;
}

bool Platform_FindProgram(String ProgramName)
{
    // todo: .com executable as well
    return Platform_FindFile_Ex(ProgramName, S(".exe"), NULL);
}

bool Platform_FindProgram_Ex(String ProgramName, String* OutProgramPath)
{
    return Platform_FindFile_Ex(ProgramName, S(".exe"), OutProgramPath);
}

bool Platform_FindFile(String FileName, String ExtensionWithDot)
{
    return Platform_FindFile_Ex(FileName, ExtensionWithDot, NULL);
}

bool Platform_FindFile_Ex(String FileName, String ExtensionWithDot, String* OutFilePath)
{
    if (!String_IsValid(FileName))
    {
        return false;
    }

    StringLocal(FileNameCopy, MAX_PATH_LENGTH);
    String_Copy(&FileNameCopy, FileName);

    const char* Ext = NULL;
    if (ExtensionWithDot.Length > 1)
        Ext = (char*)ExtensionWithDot.Data;

    u8 FullPath[MAX_PATH] = {0};
    DWORD Len = SearchPath(NULL, (char*)FileNameCopy.Data, Ext, MAX_PATH, (char*)FullPath, NULL);
    if (Len == 0)
        return false;

    if (OutFilePath)
    {
        String_Copy(OutFilePath, StrSlice(FullPath, Len));
    }

    return true;
}

bool Platform_IsValidHandle(const PlatformHandle Handle)
{
    return Handle != NULL && Handle != INVALID_HANDLE_VALUE;// && Handle != nullptr;
}

usize Platform_GetCriticalSectionMemoryRequirement(void)
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

// implement our own kbhit since we're not linking against the standard library
static bool kbhit(void)
{
    // Run through all key scancodes from 7 to 255
    // start from 0x07. the first 6 are mouse keys
    for (i32 i = 7; i < 255; i++)
    {
        i32 State = GetAsyncKeyState(i);
        if (State & 0x01)
        {
            // a key has been pressed
            return true;
        }
    }

    return false;
}

bool Platform_AnyKeyPressed(void)
{
    return kbhit();
}

void Platform_BeginNonBlockingMode(void)
{
}

void Platform_EndNonBlockingMode(void)
{
}

bool Platform_GetFullCpuName(String* OutName)
{
    HKEY Key = NULL;
    LSTATUS Status = RegOpenKeyExA(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0\\",
                       0, KEY_QUERY_VALUE | KEY_WOW64_32KEY | KEY_ENUMERATE_SUB_KEYS, &Key);

    if (Status != S_OK) return false;

    DWORD Length = 511;
    StringLocal(CpuName, 512);
    Status = RegQueryValueEx(Key, "ProcessorNameString", NULL, NULL, (LPBYTE)CpuName.Data, &Length);
    if (Status != S_OK) return false;

    if (Length > 0)
    {
        CpuName.Length = Length-1;
        String_Copy(OutName, CpuName);
        return true;
    }

    return false;
}

u32 Platform_GetExitCodeForProcess(PlatformHandle Handle)
{
    if (NEVER(!Platform_IsValidHandle(Handle))) return 0;

    DWORD ExitCode = 0;
    if (!GetExitCodeProcess(Handle, &ExitCode))
    {
        return UINT32_MAX;
    }

    if (ExitCode == 259) // STILL_ACTIVE
    {
        return UINT32_MAX;
    }

    return ExitCode;
}

u32 Platform_WaitForProcessAndGetExitCode(PlatformHandle Handle)
{
    if (NEVER(!Platform_IsValidHandle(Handle))) return 0;

    (void)WaitForSingleObject(Handle, INFINITE);

    DWORD ExitCode = 0;
    if (!GetExitCodeProcess(Handle, &ExitCode))
    {
        return UINT32_MAX;
    }

    return ExitCode;
}

u32 Platform_WaitForMultipleHandles(PlatformHandle* Handles, u32 NumHandles, i32 Milliseconds, bool bWaitAll)
{
    i32 Time = Milliseconds <= 0 ? (i32)INFINITE : Milliseconds;
    return WaitForMultipleObjects(NumHandles, Handles, bWaitAll, (u32)Time);
}

void Platform_WaitForHandle(PlatformHandle Handle, i32 Milliseconds)
{
    if (NEVER(!Platform_IsValidHandle(Handle))) return;

    i32 Time = Milliseconds <= 0 ? (i32)INFINITE : Milliseconds;
    (void)WaitForSingleObject(Handle, (u32)Time);
}

void Platform_CloseHandle(PlatformHandle Handle)
{
    if (NEVER(!Platform_IsValidHandle(Handle)))
    {
        return;
    }

    CloseHandle(Handle);
}

bool Platform_GetTerminalDimensions(u32* OutRows, u32* OutColumns)
{
    CONSOLE_SCREEN_BUFFER_INFO csbi = {0};
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi) == 0)
    {
        return false;
    }

    *OutColumns = (u32)(csbi.srWindow.Right - csbi.srWindow.Left + 1);
    *OutRows    = (u32)(csbi.srWindow.Bottom - csbi.srWindow.Top + 1);

    return true;
}

PlatformVersion Platform_GetVersion(void)
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

bool Platform_IsWindowFocused(void)
{
    HWND ForegroundWindow = GetForegroundWindow();
    if (ForegroundWindow == NULL)
    {
        return false;
    }

    DWORD ThisProcessID = GetCurrentProcessId();
    DWORD ForegroundProcessID = 0;
    DWORD Result = GetWindowThreadProcessId(ForegroundWindow, &ForegroundProcessID);
    if (Result == 0 || ForegroundProcessID == 0)
    {
        return false;
    }
    
    bool bSame = ThisProcessID == ForegroundProcessID;
    return bSame;
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
