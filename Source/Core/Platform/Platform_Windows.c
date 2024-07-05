// Copyright (c) 2024 Ali El Saleh

#include "Platform.h"

#if PLATFORM_WINDOWS

#include "Log.h"

#include "Uuid.h"
#include "Filesystem.h"
#include "String/BaseString.h"
#include "String/StringUtils.h"
#include "Structures/Array.h"

#include "Win32Types.h"

//#include <Windows.h>
//#include <strsafe.h>
//#include <Shlwapi.h>
//#include <DbgHelp.h>
//#include <shellapi.h>
//#include <psapi.h>
//#include <Shlobj.h>

#include <stdarg.h>

#include <gs_support.c>

#if COMPILER_CLANG
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-prototypes"
#endif

u64 __security_cookie = 0;
u64 __security_cookie_complement = 0;
void __fastcall __security_check_cookie(u64 cookie)
{
    if (cookie != __security_cookie)
        __debugbreak();
}

#if COMPILER_CLANG
#pragma clang diagnostic pop
#endif

typedef enum WinConsoleForegroundColors
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
} WinConsoleForegroundColors;

typedef enum WinConsoleBackgroundColor
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
} WinConsoleBackgroundColor;

C_LINKAGE_BEGIN
int _fltused = 0;
C_LINKAGE_END

#define CONSOLE_INFO_COLOR (FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE)
#define CONSOLE_SUCCESS_COLOR (FOREGROUND_INTENSITY | FOREGROUND_GREEN)
#define CONSOLE_WARNING_COLOR 14
#define CONSOLE_ERROR_COLOR (FOREGROUND_INTENSITY | FOREGROUND_RED)
#define CONSOLE_FATAL_COLOR (FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | BACKGROUND_RED)

#define WM_FILE_WATCHER (WM_USER+1)

// INFO, SUCCESS, WARNING, ERROR, FATAL, NONE
static u8 GConsoleColorLevels[6] = { CONSOLE_INFO_COLOR, CONSOLE_SUCCESS_COLOR, CONSOLE_WARNING_COLOR, CONSOLE_ERROR_COLOR, CONSOLE_FATAL_COLOR, CONSOLE_INFO_COLOR };

static char ArgumentBuffer[128][512] = {0};

static String GArgV[128] = {0};
static i32 GArgC = 0;

#ifndef NO_LOG 
internal void LogLastError(const String Prefix)
{
    TCHAR Message[4096] = {0};
    DWORD Code = GetLastError();
    u32 Len = FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, Code,
                            MAKELANGID(0, 0x01), // SUBLANG_DEFAULT
                            (LPTSTR)&Message, sizeof(TCHAR)*4095,
                            NULL);

    LOG_ERROR("%S\n        Error Code: %i\n        Reason: %S", Prefix, Code, StrSlice(Message, Len));
}
#else
#define LogLastError(...)
#endif

void Platform_PreInitialize(void)
{
    Platform_GetClockFrequency();

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
        char* Buffer = ArgumentBuffer[(i-1)]; // &ArgumentBuffer[(i-1)*1024];

        register u32 Len = 0;
        while (Len < 512 && ArgsW[i][Len] != 0) // arbitrary max length of 512
        {
            Buffer[Len] = (char)ArgsW[i][Len];
            Len++; 
        }

        GArgV[i-1].Length = Len;
        GArgV[i-1].Capacity = Len;
    }

    LocalFree(ArgsW);
}

bool Platform_CreateMutex(const String Name, PlatformMutex* OutMutex)
{
    u32 Diff = Name.Length > 255 ? Name.Length - 255 : 0; // clamp to 255 characters
    String ClampedName = StrShiftF(Name, Diff);

    HANDLE M = CreateMutexA(NULL, TRUE, ClampedName.Data);
    if (M == NULL)
    {
        return false;
    }

    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        OutMutex->Handle = M;
        OutMutex->Name = ClampedName;
        return false;
    }

    OutMutex->Handle = M;
    OutMutex->Name = ClampedName;
    return true;
}

bool Platform_ReleaseMutex(PlatformMutex* Mutex)
{
    BOOL bResult = ReleaseMutex(Mutex->Handle);
    if (!bResult)
    {
        //LogLastError(S("Failed to release mutex"));
        return false;
    }

    CloseHandle(Mutex->Handle);
    return true;
}

u32 Platform_GetConsoleProcessCount(void)
{
    DWORD Processes[4] = {0};
    DWORD Count = GetConsoleProcessList(Processes, 4);
    return Count;
}

void* Platform_GetDeviceContext(void)
{
    return NULL;
}

bool Platform_Startup(void* State, const String ApplicationName, i32 X, i32 Y, u32 Width, u32 Height)
{
    return true;
}

void Platform_ShowWindow(void)
{
}

void Platform_Shutdown(void)
{
}

u64 Platform_GetMemoryRequirement(void)
{
    return 4;
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
    LARGE_INTEGER Frequency;
    QueryPerformanceFrequency(&Frequency);
    const f64 ClockFrequency = 1.0/(f64)Frequency.QuadPart;
    return ClockFrequency;
}

bool Platform_PushMessages(void)
{
    MSG Message;
    while (PeekMessage(&Message, NULL, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&Message);
        DispatchMessage(&Message);
    }

    return true;
}

void* Platform_GetWindowHandle(void)
{
    return NULL;
}

void* Platform_MemAlloc(u64 Size)
{
    DWORD dwFlags = HEAP_CREATE_ALIGN_16;
    return HeapAlloc(GetProcessHeap(), dwFlags, Size);
}

void* Platform_MemAllocZero(u64 Size)
{
    DWORD dwFlags = HEAP_ZERO_MEMORY | HEAP_CREATE_ALIGN_16;
    return HeapAlloc(GetProcessHeap(), dwFlags, Size);
}

void* Platform_MemReAlloc(const void* Block, u64 Size)
{
    if (!Block)
        return Platform_MemAlloc(Size);

    DWORD dwFlags = HEAP_ZERO_MEMORY;
    return HeapReAlloc(GetProcessHeap(), dwFlags, (void*)Block, Size);
}

void Platform_MemFree(const void* Block)
{
    HeapFree(GetProcessHeap(), 0, (void*)Block);
}

void* Platform_MemZero(void* Block, u64 Size)
{
    RtlZeroMemory(Block, Size);
    return Block;
}

void* Platform_MemCopy(void* restrict Dest, const void* restrict Source, u64 Size)
{
    RtlCopyMemory(Dest, Source, Size);
    return Dest;
}

void* Platform_MemMove(void* restrict Dest, const void* restrict Source, u64 Size)
{
    RtlMoveMemory(Dest, Source, Size);
    return Dest;
}

void* Platform_MemSet(void* Dest, i32 Value, u64 Size)
{
    RtlFillMemory(Dest, Size, (BYTE)Value);
    return Dest;
}

/*
internal COORD GetConsoleCursorPosition(HANDLE hConsoleOutput)
{
    CONSOLE_SCREEN_BUFFER_INFO cbsi;
    if (GetConsoleScreenBufferInfo(hConsoleOutput, &cbsi))
    {
        return cbsi.dwCursorPosition;
    }
    else
    {
        // The function failed. Call GetLastError() for details.
        COORD invalid = { 0, 0 };
        return invalid;
    }
}
*/

void Platform_ConsoleWrite(const char* Message, u8 Color, bool bIsError)
{
    Platform_ConsoleWrite_CustomLength(Message, String_GetLength(Message), Color, bIsError);
}

void Platform_ConsoleWrite_CustomLength(const char* Message, u64 Length, u8 Color, bool bIsError)
{
    //PROFILE_SCOPE("Platform_ConsoleWrite") // @todo: make it thread local
    {
        DWORD OutputHandle = STD_ERROR_HANDLE;// bIsError ? STD_ERROR_HANDLE : STD_OUTPUT_HANDLE;
        HANDLE ConsoleHandle = GetStdHandle(OutputHandle);

        SetConsoleTextAttribute(ConsoleHandle, GConsoleColorLevels[Color]);

        bool bIgnoreNewLine = Color == 4 && Message[Length-1] == '\n';
        if (UNLIKELY(bIgnoreNewLine))
            Length--;

        OutputDebugString(Message);
        WriteConsole(ConsoleHandle, Message, (DWORD)Length, NULL, 0);

        // Reset back to white
        SetConsoleTextAttribute(ConsoleHandle, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);

        if (UNLIKELY(bIgnoreNewLine))
            WriteConsole(ConsoleHandle, "\n", 1, NULL, 0);
    }
}

f64 Platform_GetAbsoluteTime(void)
{
    LARGE_INTEGER Frequency, Now;

    QueryPerformanceFrequency(&Frequency);
    QueryPerformanceCounter(&Now);

    return (f64)Now.QuadPart * (1.0/(f64)Frequency.QuadPart);
}

SystemTime Platform_GetSystemLocalTime(void)
{
    SYSTEMTIME SysTime = {0};
    GetLocalTime(&SysTime);

    SystemTime EngineTime = {0};
    EngineTime.Year = SysTime.wYear;
    EngineTime.Month = SysTime.wMonth;
    EngineTime.DayOfWeek = SysTime.wDayOfWeek;
    EngineTime.Day = SysTime.wDay;
    EngineTime.Hour = SysTime.wHour;
    EngineTime.Minute = SysTime.wMinute;
    EngineTime.Second = SysTime.wSecond;
    EngineTime.Millisecond = SysTime.wMilliseconds;

    return EngineTime;
}

bool Platform_GetTimeZone(String* OutTimeZone)
{
    TIME_ZONE_INFORMATION TimeZoneInfo = {0};
    DWORD Result = GetTimeZoneInformation(&TimeZoneInfo);

    if (Result == 1) // TIME_ZONE_ID_STANDARD
    {
        String16 StandardNameWide = CStr16(TimeZoneInfo.StandardName);
        if (NEVER(StandardNameWide.Length > OutTimeZone->Capacity)) return false;
        String_ToNarrow(StandardNameWide, OutTimeZone);
    }
    else if (Result == 2) // TIME_ZONE_ID_DAYLIGHT
    {
        String16 DaylightNameWide = CStr16(TimeZoneInfo.DaylightName);
        if (NEVER(DaylightNameWide.Length > OutTimeZone->Capacity)) return false;
        String_ToNarrow(DaylightNameWide, OutTimeZone);
    }
    
    return Result > 0;
}

void Platform_Sleep(f64 ms)
{
    if (ms > 0)
    {
        LARGE_INTEGER Frequency, Now;
        QueryPerformanceCounter(&Now);
        QueryPerformanceFrequency(&Frequency);

        f64 Start = (f64)Now.QuadPart * (1.0/(f64)Frequency.QuadPart);
        f64 Target = ms/1000.0;

        while (1)
        {
            QueryPerformanceCounter(&Now);
            if ((((f64)Now.QuadPart * (1.0/(f64)Frequency.QuadPart)) - Start) >= Target)
                break;
        }
    }
}

void Platform_ShowCursor(bool bShow)
{
    static bool bShown = true;
    if (bShown)
    {
        if (!bShow)
        {
            SetCursor(NULL);
            bShown = false;
        }
    }
    else
    {
        if (bShow)
        {
            SetCursor(LoadCursor(NULL, IDC_ARROW));
            bShown = true;
        }
    }
}

void Platform_GetMousePosition(f32* X, f32* Y)
{
    POINT p;
    GetCursorPos(&p);

    *X = (f32)p.x;
    *Y = (f32)p.y;
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
    char UserName[256] = {0};
    DWORD Size = 255;
    BOOL bResult = GetUserName(UserName, &Size);
    if (!bResult)
    {
        LogLastError(S("Failed to get the current user name"));
        return false;
    }

    String_Copy(OutName, CStr(UserName));
    return true;
}

bool Platform_GetUserName(String* OutName)
{
    const u32 id = 0x0028; // USERPROFILE  CSIDL_PROFILE

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
    const u32 id = 0x0028; // USERPROFILE  CSIDL_PROFILE

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
            String_Copy(OutName, StrSlice(&FileName[i+1], Len-i-1));
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
    u32 Len = GetCurrentDirectory(OutPath->Capacity, OutPath->Data);
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

    DWORD Len = GetEnvironmentVariable(NameCopy.Data, OutVariable->Data, OutVariable->Capacity);
    OutVariable->Length = Len;

    return Len != 0;
#endif
}

bool Platform_DoesEnvironmentVariableExist(String Name)
{
    StringLocal(NameCopy, 128); // we copy the name because the passed in Name could have had its length altered but not the data, so create a copy with a null terminator at the length so windows gets the correct string
    String_Copy(&NameCopy, Name);

    DWORD Len = GetEnvironmentVariable(NameCopy.Data, NULL, 0);
    return Len != 0;
}

bool Platform_CaptureStackTrace(LinearAllocator* Arena, TArray(StackTraceData)* OutInfo)
{
    HANDLE ProcessHandle = GetCurrentProcess();
    SymInitialize(ProcessHandle, NULL, true);

    LinearAllocator_Scratch Temp = Memory_GetScratch();

    void* StackAddresses = LinearAllocator_Allocate(Temp.Allocator, sizeof(void*) * 1024);
    const u16 NumFramesCaptured = CaptureStackBackTrace(0, 1024, StackAddresses, NULL);

    SYMBOL_INFO* Symbol = LinearAllocator_Allocate(Temp.Allocator, sizeof(SYMBOL_INFO) + 256 * sizeof(char));
    Symbol->MaxNameLen = 255;
    Symbol->SizeOfStruct = sizeof(SYMBOL_INFO);

    u64 ArrayMemoryAmount = _ArrayCalculateMemRequirement(NumFramesCaptured, sizeof(StackTraceData));
    u8* ArrayMemory = LinearAllocator_Allocate(Arena, ArrayMemoryAmount);

    TArray(StackTraceData) StackTraceCache = Array_CreateStatic(StackTraceData, NumFramesCaptured, ArrayMemory);

    for (u16 i = 1; i < NumFramesCaptured; i++)
    {
        SymFromAddr(ProcessHandle, (u64)(((u64*)StackAddresses)[i]), 0, Symbol);

        StackTraceData d;
        d.Name = String_Create(Arena, StrSlice(Symbol->Name, Symbol->NameLen));
        d.Address = Symbol->Address;
        d.Index = NumFramesCaptured - i - 1;

        Array_Add(StackTraceCache, d);
    }

    *OutInfo = StackTraceCache;

    Memory_ReleaseScratch(&Temp);

    return true;
}

u32 Platform_GetNumLogicalProcessors(void)
{
    SYSTEM_INFO info = {0};
    GetSystemInfo(&info);

    return info.dwNumberOfProcessors;
    //return GetCurrentProcessorNumber();
}

static StringN(64) GThreadNameBuffer = {0};

bool Platform_GetThreadName(void* ThreadHandle, String* OutName)
{
    if (!ThreadHandle)
    {
        StringN_Copy(GThreadNameBuffer, S("Main Thread"));

        String s;
        s.Data = GThreadNameBuffer.Data;
        s.Length = GThreadNameBuffer.Length;
        s.Capacity = GThreadNameBuffer.Capacity;

        *OutName = s;

        return false;
    }

    wchar_t a[64] = {0};
    wchar_t* str = &a[0];

    HANDLE* Handle = (HANDLE*)ThreadHandle;
    if ((u64)Handle == 0xfffffffffffffffe)
    {
        StringN_Copy(GThreadNameBuffer, S(""));

        String s;
        s.Data = GThreadNameBuffer.Data;
        s.Length = GThreadNameBuffer.Length;
        s.Capacity = GThreadNameBuffer.Capacity;

        *OutName = s;

        return false;
    }

    HRESULT Result = GetThreadDescription(Handle, &str);

    if (HRESULT_CODE(Result) == S_OK)
    {
        String ConversionBuffer = {.Data = GThreadNameBuffer.Data, .Length = 0};

        String_ToNarrow(CStr16View(str), &ConversionBuffer);

        GThreadNameBuffer.Length = ConversionBuffer.Length;

        String s;
        s.Data = GThreadNameBuffer.Data;
        s.Length = GThreadNameBuffer.Length;
        s.Capacity = GThreadNameBuffer.Capacity;

        *OutName = s;

        return GThreadNameBuffer.Length > 0;
    }

    return false;
}

Uuid UUID_Generate(void)
{
    uuid_t id = {0};
    UuidCreate(&id);

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
    UuidToString((uuid_t*)&ID, &str);
    String_Copy(OutString, StrSlice(str, GUID_LENGTH-1));
    RpcStringFree(&str);
}

Uuid UUID_FromString(const String IDString)
{
    uuid_t id = {0};
    UuidFromString((const RPC_CSTR)IDString.Data, &id);

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
        Disposition = OPEN_ALWAYS;
    }
    else if ((Mode & FileMode_Read) == 0 && (Mode & FileMode_Write) != 0)
    {
        OpenStyle = GENERIC_WRITE;
        ShareStyle = FILE_SHARE_WRITE;
        Disposition = CREATE_ALWAYS;
    }
    else
    {
        LOG_ERROR("Invalid mode passed (%u) while trying to open file: %S", Mode, FilePath);
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
                    bDirectoryCreated = CreateDirectory(BaseDirectory.Data, NULL);

                    if (!bDirectoryCreated)
                    {
                        StringLocal(Prefix, 512);
                        String_Format(&Prefix, S("Failed to create directory \"%S\""), Prefix.Capacity, BaseDirectory);
                        LogLastError(Prefix);

                        return false;
                    }
                }

                break;
            }
        }
    }
    while (bFoundPathSeparator);

    HANDLE File = CreateFile(FilePath.Data, OpenStyle, ShareStyle, NULL, Disposition, FILE_ATTRIBUTE_NORMAL, NULL);
    if (File == INVALID_HANDLE_VALUE)
    {
        StringLocal(Prefix, 512);
        String_Format(&Prefix, S("Failed to open file \"%S\""), Prefix.Capacity, FilePath);
        LogLastError(Prefix);

        return false;
    }

    if (OutHandle)
    {
        OutHandle->Data = File;
        OutHandle->Data2 = NULL;

        //LARGE_INTEGER FileSize;
        //GetFileSizeEx(File, &FileSize);
        //OutHandle->Size = (u64)FileSize.QuadPart;

        //StringN_Copy(OutHandle->Path, FilePath);
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
    //StringLocal(Copy, MAX_PATH);
    //String_Copy(&Copy, FilePath);
    // figured it be better to just overwrite one character to 0 instead of copying the entire string

    // sigh... if only windows used length delimited strings...
    //char Temp = FilePath.Data[FilePath.Length];
    //FilePath.Data[FilePath.Length] = 0;
    i32 Result = DeleteFile(FilePath.Data) != 0;
    //FilePath.Data[FilePath.Length] = Temp;

    return Result != 0;
}

bool Filesystem_Open_MemoryMapped(const String FilePath, u32 Mode, FileHandle* OutHandle, u8** OutData, u64* OutSize)
{
    if (OutSize)
        *OutSize = 0;

    if (OutData)
        *OutData = NULL;

    if (NEVER(OutHandle == NULL))
        return false;

    if (!IsValidFileHandle(*OutHandle))
    {
        Filesystem_Open(FilePath, Mode, OutHandle);
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
            LOG_ERROR("Invalid mode passed (%u) while trying to map view file: %S", Mode, FilePath);
            return false;
        }

        HANDLE fm = CreateFileMapping(OutHandle->Data, NULL, ProtectFlag, 0, 0, NULL);
        if (fm == NULL || fm == INVALID_HANDLE_VALUE)
        {
            StringLocal(Prefix, 512);
            String_Format(&Prefix, S("Failed to create file mapping for \"%S\""), Prefix.Capacity, FilePath);
            LogLastError(Prefix);
            Filesystem_Close(OutHandle);
            return false;
        }

        OutHandle->Data2 = fm;

        LARGE_INTEGER FileSize;
        GetFileSizeEx(OutHandle->Data, &FileSize);

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
            LOG_ERROR("Invalid mode passed (%u) while trying to map view file: %S", Mode, FilePath);
            Filesystem_Close(OutHandle);
            return false;
        }

        if (OutSize)
            *OutSize = (u64)FileSize.QuadPart;

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
                    bDirectoryCreated = CreateDirectory(BaseDirectory.Data, NULL);

                    if (!bDirectoryCreated)
                    {
                        StringLocal(Prefix, 512);
                        String_Format(&Prefix, S("Failed to create directory \"%S\""), Prefix.Capacity, BaseDirectory);
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

    HANDLE File = CreateFile(FilePath.Data, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_FLAG_BACKUP_SEMANTICS, NULL);

    if (File == INVALID_HANDLE_VALUE)
    {
        StringLocal(Prefix, 512);
        String_Format(&Prefix, S("Failed to open directory \"%S\""), Prefix.Capacity, FilePath);
        LogLastError(Prefix);

        return false;
    }

    if (OutHandle)
    {
        OutHandle->Data = File;

        /*
        LARGE_INTEGER FileSize;
        GetFileSizeEx(File, &FileSize);
        OutHandle->Size = (u64)FileSize.QuadPart;

        StringN_Copy(OutHandle->Path, FilePath);
        */
    }

    return true;
}

bool Filesystem_Close(FileHandle* Handle)
{
    if (NEVER(Handle == NULL))
        return false;

    if (Handle->Data2)
        CloseHandle(Handle->Data2);

    if (IsValidFileHandle(*Handle))
    {
        CloseHandle(Handle->Data);
        *Handle = FileHandle_Null();
        return true;
    }

    return false;
}

bool Filesystem_Seek(const FileHandle Handle, i64 Offset)
{
    DWORD Result = SetFilePointer(Handle.Data, (long)Offset, NULL, FILE_CURRENT);
    return Result != INVALID_SET_FILE_POINTER;
}

bool Filesystem_SeekFromBeginning(const FileHandle Handle, u64 Offset)
{
    DWORD Result = SetFilePointer(Handle.Data, (long)Offset, NULL, FILE_BEGIN);
    return Result != INVALID_SET_FILE_POINTER;
}

bool Filesystem_SeekFromEnd(const FileHandle Handle, u64 Offset)
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

u64 Filesystem_GetCurrentFilePosition(const FileHandle Handle)
{
    return SetFilePointer(Handle.Data, 0, NULL, FILE_CURRENT);
}

u64 Filesystem_GetLastWriteTime(const String FilePath)
{
    if (!Filesystem_DoesFileExist(FilePath))
        return 0;

    FileHandle f = {0};
    FILETIME FileTimeStamp = {0};
    if (Filesystem_Open(FilePath, FileMode_Read, &f))
    {
        GetFileTime(f.Data, NULL, NULL, &FileTimeStamp);
        Filesystem_Close(&f);
    }

    ULONGLONG a = (((ULONGLONG)FileTimeStamp.dwHighDateTime) << 32) + FileTimeStamp.dwLowDateTime;
    return a;
}

u64 Filesystem_GetLastAccessTime(const String FilePath)
{
    if (!Filesystem_DoesFileExist(FilePath))
        return 0;

    FileHandle f = {0};
    FILETIME FileTimeStamp = {0};
    if (Filesystem_Open(FilePath, FileMode_Read, &f))
    {
        GetFileTime(f.Data, NULL, &FileTimeStamp, NULL);
        Filesystem_Close(&f);
    }

    ULONGLONG a = (((ULONGLONG)FileTimeStamp.dwHighDateTime) << 32) + FileTimeStamp.dwLowDateTime;
    return a;
}

u64 Filesystem_GetCreationTime(const String FilePath)
{
    if (!Filesystem_DoesFileExist(FilePath))
        return 0;

    FileHandle f = {0};
    FILETIME FileTimeStamp = {0};
    if (Filesystem_Open(FilePath, FileMode_Read, &f))
    {
        GetFileTime(f.Data, &FileTimeStamp, NULL, NULL);
        Filesystem_Close(&f);
    }

    ULONGLONG a = (((ULONGLONG)FileTimeStamp.dwHighDateTime) << 32) + FileTimeStamp.dwLowDateTime;
    return a;
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

        GetFileTime(f.Data, &CreationTime, &LastAccessTime, &LastWriteTime);
        Filesystem_Close(&f);

        Time.CreationTime = (((ULONGLONG)CreationTime.dwHighDateTime) << 32) + CreationTime.dwLowDateTime;
        Time.LastAccessTime = (((ULONGLONG)LastAccessTime.dwHighDateTime) << 32) + LastAccessTime.dwLowDateTime;
        Time.LastWriteTime = (((ULONGLONG)LastWriteTime.dwHighDateTime) << 32) + LastWriteTime.dwLowDateTime;
    }

    return Time;
}

u64 Filesystem_GetLastWriteTimeH(const FileHandle Handle)
{
    FILETIME FileTimeStamp = {0};
    GetFileTime(Handle.Data, NULL, NULL, &FileTimeStamp);

    ULONGLONG a = (((ULONGLONG)FileTimeStamp.dwHighDateTime) << 32) + FileTimeStamp.dwLowDateTime;
    return a;
}

u64 Filesystem_GetLastAccessTimeH(const FileHandle Handle)
{
    FILETIME FileTimeStamp = {0};
    GetFileTime(Handle.Data, NULL, &FileTimeStamp, NULL);

    ULONGLONG a = (((ULONGLONG)FileTimeStamp.dwHighDateTime) << 32) + FileTimeStamp.dwLowDateTime;
    return a;
}

u64 Filesystem_GetCreationTimeH(const FileHandle Handle)
{
    FILETIME FileTimeStamp = {0};
    GetFileTime(Handle.Data, &FileTimeStamp, NULL, NULL);

    ULONGLONG a = (((ULONGLONG)FileTimeStamp.dwHighDateTime) << 32) + FileTimeStamp.dwLowDateTime;
    return a;
}

FileTimeData Filesystem_GetFileTimeH(const FileHandle Handle)
{
    FileTimeData Time = {0};

    FILETIME CreationTime = {0};
    FILETIME LastAccessTime = {0};
    FILETIME LastWriteTime = {0};
    GetFileTime(Handle.Data, &CreationTime, &LastAccessTime, &LastWriteTime);

    Time.CreationTime = (((ULONGLONG)CreationTime.dwHighDateTime) << 32) + CreationTime.dwLowDateTime;
    Time.LastAccessTime = (((ULONGLONG)LastAccessTime.dwHighDateTime) << 32) + LastAccessTime.dwLowDateTime;
    Time.LastWriteTime = (((ULONGLONG)LastWriteTime.dwHighDateTime) << 32) + LastWriteTime.dwLowDateTime;

    return Time;
}

bool Filesystem_ReadPipe(PlatformPipe Handle, u64 DataSize, void* OutData, u64* OutBytesRead)
{
    if (NEVER(Handle[0] == NULL)) return false;
    if (NEVER(Handle[1] == NULL)) return false;

    DWORD BytesRead = 0;
    BOOL Result = ReadFile(Handle[0], OutData, (DWORD)DataSize, &BytesRead, NULL);

    if (OutBytesRead)
        *OutBytesRead = BytesRead;

    return Result;
}

// todo: no pointer as param
bool Filesystem_Read(const FileHandle Handle, u64 DataSize, void* OutData, u64* OutBytesRead)
{
    if (NEVER(!IsValidFileHandle(Handle)))
        return false;

    DWORD BytesRead = 0;
    BOOL Result = ReadFile(Handle.Data, OutData, (DWORD)DataSize, &BytesRead, NULL);

    if (OutBytesRead)
        *OutBytesRead = BytesRead;

    return Result;
}

bool Filesystem_ReadEntireFile(const FileHandle Handle, void* OutData, u64* OutBytesRead)
{
    if (NEVER(!IsValidFileHandle(Handle)))
        return false;

    u64 Size = 0;
    if (!Filesystem_GetFileSize(Handle, &Size))
        return false;

    Filesystem_SeekToBeginning(Handle);

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

    LARGE_INTEGER FileSize;
    GetFileSizeEx(Handle.Data, &FileSize);
    u64 Size = (u64)FileSize.QuadPart;

    if (CurrentPosition >= Size)
    {
        Filesystem_SeekToBeginning(Handle);
        return false;
    }

    char TempBuffer[8192] = {0};
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

    SetFilePointer(Handle.Data, (i32)(CurrentPosition + FilePointerOffset), NULL, FILE_BEGIN);

    return true;
}

// todo: make internal function, code duplication
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

bool Filesystem_Write(const FileHandle Handle, u64 DataSize, const void* Data, u64* OutBytesWritten)
{
    if (NEVER(!IsValidFileHandle(Handle))) return false;
    if (DataSize == 0) return false;

    Filesystem_SeekToBeginning(Handle);

    DWORD BytesWritten = 0;
    BOOL bResult = WriteFile(Handle.Data, Data, (DWORD)DataSize, &BytesWritten, NULL);

    if (OutBytesWritten)
        *OutBytesWritten = BytesWritten;

    if (!bResult)
    {
        StringLocal(Prefix, 2048);
        StringLocal(Path, MAX_PATH_LENGTH);
        Filesystem_GetFilePath(Handle, &Path);
        String_Format(&Prefix, S("Failed to write to file \"%S\""), 2048, Path);
        LogLastError(Prefix);
    }

    return bResult;
}

bool Filesystem_WriteLine(const FileHandle Handle, const String Text, u64* OutBytesWritten)
{
    if (NEVER(!IsValidFileHandle(Handle))) return false;

    Filesystem_SeekToEnd(Handle);

    DWORD BytesWritten = 0;
    BOOL bResult = WriteFile(Handle.Data, Text.Data, (DWORD)Text.Length, &BytesWritten, NULL);

    if (OutBytesWritten)
        *OutBytesWritten = BytesWritten;

    if (!bResult)
    {
        StringLocal(Prefix, 2048);
        StringLocal(Path, MAX_PATH_LENGTH);
        Filesystem_GetFilePath(Handle, &Path);
        String_Format(&Prefix, S("Failed to write line to file \"%S\""), 2048, Path);
        LogLastError(Prefix);
    }

    return bResult;
}

bool Filesystem_WriteLineFormatted(const FileHandle Handle, const String Text, u64* OutBytesWritten, ...)
{
    if (NEVER(!IsValidFileHandle(Handle))) return false;

    Filesystem_SeekToEnd(Handle);

    va_list Args;
    va_start(Args, OutBytesWritten);
    StringLocal(Buffer, 32768);
    String_FormatV(&Buffer, Text, 32768, Args);
    va_end(Args);

    DWORD BytesWritten = 0;
    BOOL bResult = WriteFile(Handle.Data, Buffer.Data, (DWORD)Buffer.Length, &BytesWritten, NULL);

    if (OutBytesWritten)
        *OutBytesWritten = BytesWritten;

    if (!bResult)
    {
        StringLocal(Prefix, 2048);
        StringLocal(Path, MAX_PATH_LENGTH);
        Filesystem_GetFilePath(Handle, &Path);
        String_Format(&Prefix, S("Failed to write line to file \"%S\""), 2048, Path);
        LogLastError(Prefix);
    }

    return bResult;
}

bool Filesystem_DoesFileExist(const String FilePath)
{
    if (FilePath.Length == 0) return false;

    return PathFileExists(FilePath.Data);
}

bool Filesystem_DoesDirectoryExist(const String FilePath)
{
    if (FilePath.Length == 0) return false;

    DWORD Attrib = GetFileAttributes(FilePath.Data);
    return (Attrib != INVALID_FILE_ATTRIBUTES && (Attrib & FILE_ATTRIBUTE_DIRECTORY));
}

bool Filesystem_GetFilePath(const FileHandle File, String* OutPath)
{
    if (!IsValidFileHandle(File))
    {
        return false;
    }

    u32 Length = GetFinalPathNameByHandle(File.Data, OutPath->Data, MAX_PATH, FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
    if (Length == 0)
    {
        LogLastError(S("Filesystem_GetFilePath failed"));
        return false;
    }

    OutPath->Length = Length;
    *OutPath = StrShiftF(*OutPath, 4); // ignore //?/
    return true;
}

bool Filesystem_GetFileSize(const FileHandle File, u64* OutSize)
{
    if (IsValidFileHandle(File))
    {
        LARGE_INTEGER FileSize;
        BOOL Result = GetFileSizeEx(File.Data, &FileSize);
        *OutSize = (u64)FileSize.QuadPart;
        return Result;
    }

    return false;
}

bool Filesystem_IsFile(const String Path)
{
    DWORD Attrib = GetFileAttributes(Path.Data);
    return (Attrib != INVALID_FILE_ATTRIBUTES && (Attrib & FILE_ATTRIBUTE_NORMAL));
}

bool Filesystem_IsDirectory(const String Path)
{
    DWORD Attrib = GetFileAttributes(Path.Data);
    return (Attrib != INVALID_FILE_ATTRIBUTES && (Attrib & FILE_ATTRIBUTE_DIRECTORY));
}

bool Filesystem_IsNewer(const String PathA, const String PathB)
{
    u64 a = Filesystem_GetLastWriteTime(PathA);
    u64 b = Filesystem_GetLastWriteTime(PathB);
    return a > b;
}

bool Filesystem_IsOlder(const String PathA, const String PathB)
{
    u64 a = Filesystem_GetLastWriteTime(PathA);
    u64 b = Filesystem_GetLastWriteTime(PathB);
    return a < b;
}

bool Filesystem_IsPathRelative(const String Path)
{
    bool bDriveSymbol = String_IndexOfChar(Path, ':', NULL);

    bool bRelative = !bDriveSymbol;

    return bRelative;
    
    // would this work? i'll test later
    //return PathIsRelative(Path.Data);
}

bool Filesystem_ConvertRelativeToAbsolutePath(String* OutFullPath)
{
    StringLocal(Copy, MAX_PATH);
    String_Copy(&Copy, *OutFullPath);

    bool bResult = PathCanonicalize(OutFullPath->Data, Copy.Data);
    OutFullPath->Length = String_GetLength_Ex(OutFullPath->Data, MAX_PATH);
    return bResult;
}

// transforms paths with " in them to paths without them
// for exmaple: "C:\Program Files"\MyApp -> C:\Program Files\MyApp
// TODO: move to core
bool Filesystem_SanitizeQuotes(String* Dest, const String Path)
{
    bool bHasQuote = false;
    for (u32 i = 0; i < Path.Length; i++)
    {
        char c = Path.Data[i];
        if (c == '"' && bHasQuote)
        {
            // ignore all subsequent quotes
            continue;
        }

        String_AppendChar(Dest, c);

        if (c == '"')
        {
            bHasQuote = true;
        }
    }

    if (bHasQuote)
    {
        String_AppendChar(Dest, '"');
    }

    return Dest->Length > 0;
}

bool Filesystem_SanitizePath(String* Dest, const String Path)
{
    bool bAnyChange = false;
    for (u32 i = 0; i < Path.Length; i++)
    {
        if (Path.Data[i] == '"')
            continue;
        
        bAnyChange = true;

        #if PLATFORM_WINDOWS
        char C = Path.Data[i] == '/' ? '\\' : Path.Data[i]; 
        #else
        char C = Path.Data[i] == '\\' ? '/' : Path.Data[i]; 
        #endif

        String_AppendChar(Dest, C);
    }

    return bAnyChange;
}

bool Filesystem_SanitizePathAndWrap(String* Dest, const String Path)
{
    if (Path.Length == 0)
        return false;

    bool bAnyChange = false;
    String_AppendChar(Dest, '"');
    for (u32 i = 0; i < Path.Length; i++)
    {
        if (Path.Data[i] == '"')
            continue;

        bAnyChange = true;

        #if PLATFORM_WINDOWS
        char C = Path.Data[i] == '/' ? '\\' : Path.Data[i]; 
        #else
        char C = Path.Data[i] == '\\' ? '/' : Path.Data[i]; 
        #endif

        String_AppendChar(Dest, C);
    }
    String_AppendChar(Dest, '"');

    return bAnyChange;
}

internal bool Internal_IterateDirectory(const String RootPath, const String DirectoryPath, DirectoryIterator Callback, bool bRecursive, void* UserData)
{
    WIN32_FIND_DATA ffd = {0};
    TCHAR Temp[MAX_PATH] = {0};

    HANDLE Find = INVALID_HANDLE_VALUE;

    const String RealDirectoryPath = DirectoryPath.Length == 0 ? S(".") : DirectoryPath;

    HRESULT Result = StringCchCopy(Temp, MAX_PATH, RealDirectoryPath.Data);
    if (Result != S_OK) goto Error;
    
    Result = StringCchCat(Temp, MAX_PATH, "\\*");
    if (Result != S_OK) goto Error;

    Find = FindFirstFile(Temp, &ffd);

    if (Find != INVALID_HANDLE_VALUE)
    {
        bool bSuccess = true;

        do
        {
            String FileName = CStr(ffd.cFileName);

            if (String_IsEqual(FileName, S("."), false) ||
                String_IsEqual(FileName, S(".."), false))
            {
                continue;
            }

            TCHAR FilePath[MAX_PATH] = {0};
            Result = StringCchCopy(FilePath, MAX_PATH, RealDirectoryPath.Data);
            if (Result != S_OK) goto Error;

            u32 Len = RealDirectoryPath.Length;
            if (Len > 0)
            {
                char LastChar = FilePath[Len-1];
                if (LastChar != '/' && LastChar != '\\')
                {
                    Result = StringCchCat(FilePath, MAX_PATH, "\\");
                    if (Result != S_OK) goto Error;
                    Len++;
                }
            }

            Result = StringCchCat(FilePath, MAX_PATH, ffd.cFileName);
            if (Result != S_OK) goto Error;

            Len += FileName.Length;

            String FullPath;
            FullPath.Data = FilePath;
            FullPath.Length = Len;
            FullPath.Capacity = Len;
            String_EatPathSeparatorsInline(&FullPath);

            String RelativePath = CStr(&FilePath[RootPath.Length]);
            String_EatPathSeparatorsInline(&RelativePath);

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

Error:
    StringLocal(Msg, 512);
    String_Format(&Msg, S("Failed to iterate directory: %S"), Msg.Capacity, RealDirectoryPath);
    LogLastError(Msg);
    if (Find != INVALID_HANDLE_VALUE)
    {
        FindClose(Find);
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

    HANDLE hFind = FindFirstFile(WildcardPath.Data, &fd);

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
                    Filesystem_DeleteFiles(SubPath, Wildcard, true);
                }
            }
            else
            {
                StringLocal(FullPath, MAX_PATH_LENGTH);
                String_BuildPath(&FullPath, FilePath, FileName);

                i32 Result = DeleteFile(FullPath.Data);
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

    HANDLE hFind = FindFirstFile(WildcardPath.Data, &fd);

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

                Filesystem_DeleteDirectory(SubPath);
            }
            else
            {
                StringLocal(FullPath, MAX_PATH_LENGTH);
                String_BuildPath(&FullPath, DirectoryPath, FileName);

                DeleteFile(FullPath.Data);
            }
        }
        while (FindNextFile(hFind, &fd));

        FindClose(hFind);
    }

    bool bResult = RemoveDirectory(DirectoryPath.Data);

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

    // remove the read only attribute if we're copying from a source which had a readonly attribute set on it,
    // otherwise the copy will fail if the file already exists at the destination
    if (Filesystem_DoesFileExist(DestinationCopy))
    {
        SetFileAttributes(DestinationCopy.Data, (u32)GetFileAttributes(DestinationCopy.Data) & (u32)~FILE_ATTRIBUTE_READONLY);
    }

    BOOL bResult = CopyFileEx(SourceCopy.Data, DestinationCopy.Data, NULL, NULL, NULL, COPY_FILE_NO_BUFFERING);
    if (bResult == 0)
    {
        StringLocal(Msg, 512);
        String_Format(&Msg, S("Failed to copy \"%S\" to \"%S\""), Msg.Capacity, Source, Destination);
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
    }

    BOOL bResult = MoveFileEx(SourceCopy.Data, DestinationCopy.Data, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
    if (bResult == 0)
    {
        StringLocal(Msg, 512);
        String_Format(&Msg, S("Failed to %S \"%S\" to \"%S\""), Msg.Capacity, bRename ? S("rename") : S("move"), Source, Destination);
        LogLastError(Msg);
        return false;
    }

    return true;
}

bool Filesystem_ArePathsCommon(String PathA, String PathB)
{
    StringLocal(CommonPath, MAX_PATH);
    i32 Len = PathCommonPrefix(PathA.Data, PathB.Data, CommonPath.Data);
    CommonPath.Length = (u32)Len;

    return String_IsEqual(CommonPath, PathA, false);
}

PlatformHandle Platform_CreateThread(const String Name, u32* OutThreadID, u32 (*ThreadEntryPoint)(void* ThreadParameter), void* UserData)
{
    HANDLE ThreadHandle = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ThreadEntryPoint, UserData, CREATE_SUSPENDED, (LPDWORD)OutThreadID);
    SetThreadPriority(ThreadHandle, THREAD_PRIORITY_ABOVE_NORMAL);

    wchar_t ThreadName[MAX_PATH] = {0};
    String16 ThreadNameBetter;
    ThreadNameBetter.Data = ThreadName;
    String_ToWide(Name, &ThreadNameBetter);

    SetThreadDescription(ThreadHandle, ThreadName);
    ResumeThread(ThreadHandle);

    return ThreadHandle;
}

PlatformHandle Platform_RunCommand(const String CmdLine, const String WorkingDirectory)
{
    STARTUPINFO StartupInfo = {0};
    PROCESS_INFORMATION ProcessInfo = {0};
    StartupInfo.cb = sizeof(StartupInfo);

    char* Dir = WorkingDirectory.Length > 0 ? WorkingDirectory.Data : NULL;
    if (!CreateProcess(NULL, CmdLine.Data, NULL, NULL, TRUE, 0, NULL, Dir, &StartupInfo, &ProcessInfo))
    {
        StringLocal(Prefix, Kibibytes(8));
        String_Format(&Prefix, S("Failed to run command: \"%S\""), Prefix.Capacity, CmdLine);
        LogLastError(Prefix);

        return INVALID_HANDLE_VALUE;
    }

    SetPriorityClass(ProcessInfo.hProcess, ABOVE_NORMAL_PRIORITY_CLASS);

    return ProcessInfo.hProcess;
}

PlatformHandle Platform_RunCommand_Ex(const String CmdLine, const String WorkingDirectory, PlatformPipe* StdOutPipe)
{
    STARTUPINFO StartupInfo = {0};
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

    StartupInfo.cb = sizeof(STARTUPINFO);
    StartupInfo.hStdError = w;
    StartupInfo.hStdOutput = w;
    StartupInfo.hStdInput = NULL;
    StartupInfo.dwFlags |= STARTF_USESTDHANDLES;

    char* Dir = WorkingDirectory.Length > 0 ? WorkingDirectory.Data : NULL;
    if (!CreateProcess(NULL, CmdLine.Data, NULL, NULL, TRUE, 0, NULL, Dir, &StartupInfo, &ProcessInfo))
    {
        StringLocal(Prefix, Kibibytes(8));
        String_Format(&Prefix, S("Failed to run command: \"%S\""), Prefix.Capacity, CmdLine);
        LogLastError(Prefix);

        return INVALID_HANDLE_VALUE;
    }

    SetPriorityClass(ProcessInfo.hProcess, ABOVE_NORMAL_PRIORITY_CLASS);

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

    char* Ext = NULL;
    if (ExtensionWithDot.Length > 1)
        Ext = ExtensionWithDot.Data;

    char FullPath[MAX_PATH] = {0};
    DWORD Len = SearchPath(NULL, FileName.Data, Ext, MAX_PATH, FullPath, NULL);
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
    return Handle != NULL && Handle != INVALID_HANDLE_VALUE && Handle != nullptr;
}

u64 Platform_GetCriticalSectionMemoryRequirement(void)
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

u32 Platform_GetExitCodeForProcess(PlatformHandle Handle)
{
    if (NEVER(!Platform_IsValidHandle(Handle))) return 0;

    DWORD ExitCode = 0;
    if (!GetExitCodeProcess(Handle, &ExitCode))
    {
        return UINT32_MAX;
    }

    return ExitCode;
}

u32 Platform_WaitForProcessAndGetExitCode(PlatformHandle Handle)
{
    if (NEVER(!Platform_IsValidHandle(Handle))) return 0;

    WaitForSingleObject(Handle, INFINITE);

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
    WaitForSingleObject(Handle, (u32)Time);
}

void Platform_CloseHandle(PlatformHandle Handle)
{
    if (NEVER(!Platform_IsValidHandle(Handle)))
    {
        return;
    }

    CloseHandle(Handle);
}

bool Platform_IsProgramRunning(const String ProgramName)
{
    DWORD Processes[4096] = {0};
    DWORD BytesRead = 0;
    EnumProcesses(Processes, sizeof Processes, &BytesRead);
    DWORD Count = BytesRead / sizeof(DWORD);

    DWORD ProcessId = GetCurrentProcessId();

    for (u16 i = 0; i < Count; i++)
    {
        TCHAR szProcessName[MAX_PATH] = "<unknown>";

        HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, Processes[i]);

        if (hProcess != NULL)
        {
            GetModuleFileNameEx(hProcess, NULL, szProcessName, MAX_PATH);
        }

        CloseHandle(hProcess);

        const String ProcessName = CStr(szProcessName);

        StringLocal(ProgramNameCopy, 512);
        String_Copy(&ProgramNameCopy, ProgramName);

        if (!String_EndsWith(ProgramName, S(".exe"), false))
            String_Append(&ProgramNameCopy, S(".exe"));

        if (String_EndsWith(ProcessName, ProgramNameCopy, false) && Processes[i] != ProcessId)
        {
            return true;
        }
    }

    return false;
}

bool Platform_GetTerminalDimensions(u32* OutRows, u32* OutColumns)
{
    CONSOLE_SCREEN_BUFFER_INFO csbi;

    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi) == 0)
    {
        LogLastError(S("Failed to get console screen buffer info"));
        return false;
    }

    *OutColumns = (u32)(csbi.srWindow.Right - csbi.srWindow.Left + 1);
    *OutRows    = (u32)(csbi.srWindow.Bottom - csbi.srWindow.Top + 1);

    return true;
}

#if COMPILER_CLANG
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-prototypes"
#endif

FORCENOINLINE BOOL __cdecl _DllMainCRTStartup(HANDLE hDllHandle, DWORD dwReason, LPVOID lpreserved)
{
    return true;
}

#if COMPILER_CLANG
#pragma clang diagnostic pop
#endif

#endif // PLATFORM_WINDOWS
