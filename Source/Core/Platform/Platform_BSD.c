// Copyright (c) 2024 Ali El Saleh

#include "Platform.h"

#if PLATFORM_BSD
#include "Log.h"

#include "Uuid.h"
#include "Filesystem.h"
#include "Math/Math.h"
#include "String/BaseString.h"
#include "String/StringUtils.h"
#include "Structures/Array.h"

//#define _XOPEN_SOURCE 700
#define _BSD_SOURCE

#include <signal.h>
#include <stdio.h>

#define _LARGEFILE64_SOURCE
#define _FILE_OFFSET_BITS 64
#define __USE_FILE_OFFSET64
#define __USE_GNU
#define __USE_MISC

#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <ftw.h>
#include <fcntl.h>
#include <pwd.h>
#include <uuid.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/user.h>
#include <spawn.h>
#include <termios.h>
#include <semaphore.h>

#include <stdarg.h>

internal void LogLastError(const String Prefix)
{
    StringLocal(Message, 4096);
    String_Copy(&Message, CStr(strerror(errno)));

    LOG_ERROR("%S\n        errno %i\n        Reason: %S\n", Prefix, errno, Message);
}

bool Platform_Startup(void* State, const String ApplicationName, i32 X, i32 Y, u32 Width, u32 Height)
{
    return true;
}

void Platform_Shutdown(void)
{
}

u64 Platform_GetMemoryRequirement(void)
{
    return 4;
}

bool Platform_PushMessages(void)
{
    return true;
}

void Platform_ShowWindow(void)
{
}

void Platform_HideWindow(void)
{
}

internal void Internal_SignalHandler(int signal)
{
    Platform_Shutdown();

    exit(1);
}

void Platform_PreInitialize(void)
{
    Platform_GetClockFrequency();

    struct sigaction act = {0};
    act.sa_handler = &Internal_SignalHandler;
    sigaction(SIGINT, &act, NULL);
    sigaction(SIGKILL, &act, NULL);
    sigaction(SIGQUIT, &act, NULL);
    sigaction(SIGILL, &act, NULL);
    sigaction(SIGABRT, &act, NULL);
    sigaction(SIGSEGV, &act, NULL);
}

f64 Platform_GetClockFrequency(void)
{
    return 0;
}

void* Platform_GetWindowHandle(void)
{
    return NULL;
}

NO_RETURN void Platform_Abort(u32 ExitCode)
{
    exit((i32)ExitCode);
}

void* Platform_MemAlloc(u64 Size)
{
    return malloc(Size);
}

void* Platform_MemAllocZero(u64 Size)
{
    void* Mem = malloc(Size);
    memset(Mem, 0, Size);
    return Mem;
}

void* Platform_MemReAlloc(const void* Block, u64 Size)
{
    return realloc((void*)Block, Size);
}

void  Platform_MemFree(const void* Block)
{
    free((void*)Block);
}

void* Platform_MemZero(void* Block, u64 Size)
{
    return memset(Block, 0, Size);
}

void* Platform_MemCopy(void* restrict Dest, const void* restrict Source, u64 Size)
{
    return memcpy(Dest, Source, Size);
}

void* Platform_MemMove(void* restrict Dest, const void* restrict Source, u64 Size)
{
    return memmove(Dest, Source, Size);
}

void* Platform_MemSet(void* Dest, i32 Value, u64 Size)
{
    return memset(Dest, Value, Size);
}

bool Platform_MemEqual(const void* Block1, const void* Block2, u64 Size)
{
    return memcmp(Block1, Block2, Size) == 0;
}

/*
//General Formatting
#define GEN_FORMAT_RESET                "0"
#define GEN_FORMAT_BRIGHT               "1"
#define GEN_FORMAT_DIM                  "2"
#define GEN_FORMAT_UNDERSCORE           "3"
#define GEN_FORMAT_BLINK                "4"
#define GEN_FORMAT_REVERSE              "5"
#define GEN_FORMAT_HIDDEN               "6"

//Foreground Colors
#define FOREGROUND_COL_BLACK            "30"
#define FOREGROUND_COL_RED              "31"
#define FOREGROUND_COL_GREEN            "32"
#define FOREGROUND_COL_YELLOW           "33"
#define FOREGROUND_COL_BLUE             "34"
#define FOREGROUND_COL_MAGENTA          "35"
#define FOREGROUND_COL_CYAN             "36"
#define FOREGROUND_COL_WHITE            "37"

//Background Colors
#define BACKGROUND_COL_BLACK            "40"
#define BACKGROUND_COL_RED              "41"
#define BACKGROUND_COL_GREEN            "42"
#define BACKGROUND_COL_YELLOW           "43"
#define BACKGROUND_COL_BLUE             "44"
#define BACKGROUND_COL_MAGENTA          "45"
#define BACKGROUND_COL_CYAN             "46"
#define BACKGROUND_COL_WHITE            "47"
*/

void Platform_ConsoleWrite(const char* Message, u8 Color, bool bIsError)
{
    Platform_ConsoleWrite_CustomLength(Message, String_GetLength(Message), 0, false);
}

void Platform_ConsoleWrite_CustomLength(const char* Message, u64 Length, u8 Color, bool bIsError)
{
    static const char* colors[] = {"0;37", "0;32", "1;33", "1;31", "0;41", "0;37"};

    bool bIgnoreNewLine = Color == 4 && Message[Length-1] == '\n';
    if (UNLIKELY(bIgnoreNewLine))
        Length--;

    fprintf(UNLIKELY(bIsError) ? stderr : stdout, "\033[%.*sm%.*s\033[0m", 4, LIKELY(Color < 6) ? colors[Color] : "0:37", (i32)Length, Message);
    //printf("\033[%.*sm%.*s\033[0m", 4, LIKELY(Color < 6) ? colors[Color] : "0:37", (i32)Length, Message);

    if (UNLIKELY(bIgnoreNewLine))
        printf("\n");

    fflush(stdout);

    //fwrite(Message, 1, Length, bIsError ? stderr : stdout); // this shit prints nothing in some cases wtf??!?
}

PlatformHandle Platform_CreateThread(const String Name, u32* OutThreadID, u32 (*ThreadEntryPoint)(void* ThreadParameter), void* UserData)
{
    //UNIMPLEMENTED;
    //pthread_create();
    return -1;
}

PlatformHandle Platform_RunCommand(const String CmdLine, const String WorkingDirectory)
{
    String Command;
    StringLocal(Copy, MAX_PATH_LENGTH*2);
    if (WorkingDirectory.Length > 0)
    {
        String_Append(&Copy, S("cd \""));
        String_Append(&Copy, WorkingDirectory);
        String_EatPathSeparatorsInlineFromEnd(&Copy);
        String_Append(&Copy, S("\"; "));
        String_Append(&Copy, CmdLine);

        Command = Copy;
    }
    else
    {
        Command = CmdLine;
    }

    pid_t pid = fork();

    if (pid < 0)
    {
        LogLastError(S("fork() failed"));
    }
    else if (pid > 0)
    {
        return pid;
    }
    else
    {
        execvp("/bin/sh", (char*[]){"sh", "-c", Command.Data, NULL});

        exit(0);
    }

    return pid;
}

PlatformHandle Platform_RunCommand_Ex(const String CmdLine, const String WorkingDirectory, PlatformPipe* StdOutPipe)
{
    String Command;
    StringLocal(Copy, MAX_PATH_LENGTH*2);
    if (WorkingDirectory.Length > 0)
    {
        String_Append(&Copy, S("cd \""));
        String_Append(&Copy, WorkingDirectory);
        String_EatPathSeparatorsInlineFromEnd(&Copy);
        String_Append(&Copy, S("\"; "));
        String_Append(&Copy, CmdLine);

        Command = Copy;
    }
    else
    {
        Command = CmdLine;
    }

    i32 PipeData[2] = {0};
    if (pipe(PipeData) != 0)
    {
        LogLastError(S("pipe() failed"));
        return -1;
    }

    pid_t pid = fork();

    if (pid < 0)
    {
        close(PipeData[0]);
        close(PipeData[1]);
        LogLastError(S("fork() failed"));
    }
    else if (pid > 0) // parent path
    {
        close(PipeData[0]);
        close(PipeData[1]);
        return pid;
    }
    else // child path
    {
        close(PipeData[0]);
        dup2(PipeData[1], STDOUT_FILENO);

        (*StdOutPipe)[0] = PipeData[0]; // read pipe
        (*StdOutPipe)[1] = PipeData[1]; // write pipe

        execvp("/bin/sh", (char*[]){"sh", "-c", Command.Data, NULL});
        exit(0);
    }

    return pid;
}

bool Platform_FindProgram(String ProgramName)
{
    return Platform_FindFile_Ex(ProgramName, S(""), NULL);
}

bool Platform_FindProgram_Ex(String FileName, String* OutFilePath)
{
    return Platform_FindFile_Ex(FileName, S(""), OutFilePath);
}

bool Platform_FindFile(String FileName, String ExtensionWithDot)
{
    return Platform_FindFile_Ex(FileName, ExtensionWithDot, NULL);
}

bool Platform_FindFile_Ex(String FileName, String ExtensionWithDot, String* OutFilePath)
{
    char* Path = getenv("PATH");
    const String PathStr = CStr(Path);

    bool bFound = false;

    StringLocal(P, MAX_PATH_LENGTH);
    u32 Offset = 0;
    u32 Len = 0;
    for (u32 i = 0; i < PathStr.Length; i++)
    {
        if (PathStr.Data[i] == ':' || i == PathStr.Length-1) // end of an entry, process it
        {
            String_Copy(&P, StrSlice(StrShiftF(PathStr, Offset).Data, Len));
            Offset = i+1;
            Len = 0;

            if (bFound)
                break;

            DIR* dir = opendir(P.Data);
            if (dir == NULL)
            {
                continue;
            }

            struct dirent* Entry = NULL;
            while ((Entry = readdir(dir)))
            {
                if (Entry->d_name[0] == '.' && 
                    (!Entry->d_name[1] || (Entry->d_name[1] == '.' && !Entry->d_name[2])))
                {
                    continue;
                }

                const String EntryName = CStr(Entry->d_name);

                StringLocal(FullPath, MAX_PATH_LENGTH);
                String_BuildPath(&FullPath, P, EntryName);
                
                if (Entry->d_type != DT_DIR)
                {
                    u64 FileSize = 0;
                    struct stat filestat = {0};
                    stat(FullPath.Data, &filestat);
                    FileSize = (u64)filestat.st_size;

                    if (FileSize > 0)
                    {
                        // is this file an executeable?
                        if (((filestat.st_mode & S_IXUSR) || (filestat.st_mode & S_IXGRP) || (filestat.st_mode & S_IXOTH)))
                        {
                            //LOG("%S", EntryName);
                            if (ExtensionWithDot.Length > 0)
                            {
                                u32 LastDot = 0;
                                if (String_IndexOfLastChar(EntryName, '.', &LastDot))
                                {
                                    if (String_IsEqual(FileName, StrSlice(EntryName.Data, LastDot), false) &&
                                        String_EndsWith(FileName, ExtensionWithDot, true))
                                    {
                                        bFound = true;
                                    }
                                }
                            }
                            else
                            {
                                if (String_IsEqual(FileName, EntryName, false))
                                {
                                    bFound = true;
                                }
                            }

                            if (bFound)
                            {
                                if (OutFilePath)
                                    String_Copy(OutFilePath, FullPath);

                                break;
                            }
                        }
                    }
                }
            }

            closedir(dir);
        }
        else
        {
            Len++;
        }
    }

    return bFound;
}

u32 Platform_GetExitCodeForProcess(PlatformHandle Handle)
{
    if (Handle == 0)
        return 0;

    i32 PidStatus;
    pid_t pid = waitpid(Handle, &PidStatus, 0); // if you call this twice on the same pid, linux wont return the same exit code like windows does... sadge :(
    if (pid == -1)
    {
        return 0;
    }

    return (u32)WEXITSTATUS(PidStatus);
}

u32 Platform_WaitForProcessAndGetExitCode(PlatformHandle Handle)
{
    if (Handle == 0)
        return 0;

    i32 PidStatus;
    pid_t pid = waitpid(Handle, &PidStatus, 0);
    if (pid == -1)
    {
        return 0;
    }

    return (u32)WEXITSTATUS(PidStatus);
}

void Platform_WaitForHandle(PlatformHandle Handle, i32 Milliseconds)
{
    waitpid(Handle, NULL, 0);
}

u32 Platform_WaitForMultipleHandles(PlatformHandle* Handles, u32 NumHandles, i32 Milliseconds, bool bWaitAll)
{
    if (bWaitAll)
    {
        for (u32 i = 0; i < NumHandles; i++)
        {
            waitpid(Handles[i], NULL, 0);
        }

        return 0;
    }

    //bool bNeedsReset = false;
    for (u32 i = 0; i < NumHandles; i++)
    {
        Platform_WaitForHandle(Handles[i], Milliseconds);
        if (!bWaitAll)
        {
            return i;
        }
    }

    return 0;
}

void Platform_CloseHandle(PlatformHandle Handle)
{
    close(Handle);
}

bool Platform_IsValidHandle(const PlatformHandle Handle)
{
    return Handle >= 0;
}

u64 Platform_GetCriticalSectionMemoryRequirement(void)
{
    return 4;
}

void Platform_InitializeCriticalSection(PlatformCriticalSection OutCriticalSection)
{
}

void Platform_DeleteCriticalSection(PlatformCriticalSection CriticalSection)
{
}

void Platform_EnterCriticalSection(PlatformCriticalSection CriticalSection)
{
}

void Platform_ExitCriticalSection(PlatformCriticalSection CriticalSection)
{
}

bool Platform_CreateMutex(const String Name, PlatformMutex* OutMutex)
{
    if ((NEVER(Name.Length == 0)) || (NEVER(OutMutex == NULL)))
    {
        return false;
    }

    // BSD's implementation is quite strict with their naming rules...

    u32 NumSlashes = String_CountPathSeparators(Name);
    if (Name.Data[0] != '/' || NumSlashes > 1)
    {
        LOG_ERROR("Named mutex must begin with / and contain no other slash characters");
        return false;
    }

    //u32 Diff = Name.Length > 30 ? Name.Length - 30 : 0; // 31 is max name length for posix semaphores
    String ClampedName = Name;// StrShiftF(Name, Diff);

    sem_t* Semaphore = sem_open(ClampedName.Data, O_CREAT|O_EXCL, 0644, 1);
    if (Semaphore == SEM_FAILED)
    {
        if (errno == EACCES || errno == EEXIST)
        {
            return false;
        }

        StringLocal(Prefix, 512);
        String_Format(&Prefix, S("Failed to create mutex %S"), Prefix.Capacity, ClampedName);
        LogLastError(Prefix);
        return false;
    }

    OutMutex->Handle = Semaphore;
    OutMutex->Name = ClampedName;
    return true;
}

bool Platform_ReleaseMutex(PlatformMutex* Mutex)
{
    if ((NEVER(Mutex->Name.Length == 0)) || (NEVER(Mutex == NULL)))
    {
        return false;
    }

    if (sem_close(Mutex->Handle) == -1)
    {
        StringLocal(Prefix, 512);
        String_Format(&Prefix, S("Failed to release mutex %S"), Prefix.Capacity, Mutex->Name);
        LogLastError(Prefix);
        return false;
    }

    // for some fucking reason semaphores have kernel persistence, so we need to unlink them, otherwise the user will have to shutdown their machine
    // which is why again windows dominates the market, go look at the code in Platform_Windows.c!!!
    if (sem_unlink(Mutex->Name.Data) == -1)
    {
        StringLocal(Prefix, 512);
        String_Format(&Prefix, S("Failed to unlink mutex %S"), Prefix.Capacity, Mutex->Name);
        LogLastError(Prefix);
        return false;
    }

    return true;
}

f64 Platform_GetAbsoluteTime(void)
{
    struct timespec t = {0};
    clock_gettime(CLOCK_REALTIME, &t);
    const f64 a = (f64)t.tv_sec + ((f64)t.tv_nsec * 0.000000001); // 1e-9
    return a;
}

SystemTime Platform_GetSystemLocalTime(void)
{
    time_t mytime = time(0);
    ctime(&mytime);

    struct tm* lt = localtime(&mytime);

    SystemTime t;
    t.Year = (u16)lt->tm_year + 1900;
    t.Month = (u16)lt->tm_mon + 1;
    t.DayOfWeek = 0;
    t.Day = (u16)lt->tm_mday;
    t.Hour = (u16)lt->tm_hour;
    t.Minute = (u16)lt->tm_min;
    t.Second = (u16)lt->tm_sec;
    t.Millisecond = 0;

    return t;
}

void Platform_Sleep(f64 ms)
{
    if (ms > 0)
    {
        struct timespec t = {0};
        clock_gettime(CLOCK_REALTIME, &t);
        const f64 Start = (f64)t.tv_sec + ((f64)t.tv_nsec * 0.000000001); // 1e-9

        f64 Target = ms/1000.0;

        while (1)
        {
            clock_gettime(CLOCK_REALTIME, &t);
            const f64 Now = (f64)t.tv_sec + ((f64)t.tv_nsec * 0.000000001); // 1e-9
            if ((Now-Start) >= Target)
                break;
        }
    }
}

bool Platform_TerminateProcess(PlatformHandle Handle, u32 ExitCode)
{
    return kill(Handle, SIGKILL) == 0;
}

void Platform_ShowCursor(bool bShow)
{
    UNIMPLEMENTED;
}

void Platform_GetMousePosition(f32* X, f32* Y)
{
    UNIMPLEMENTED;
}

u64 Platform_GetCurrentThreadID(void)
{
    UNIMPLEMENTED;
    return 0;
    //u32 x = (u32)syscall(__NR_gettid);
    //return x;
}

u64 Platform_GetMainThreadID(void)
{
    return (u64)getpid();
}

u32 Platform_GetConsoleProcessCount(void)
{
    // TODO
    return 0;
}

void Platform_GetWorkingDirectory(String* OutPath)
{
    getcwd(OutPath->Data, MAX_PATH_LENGTH);
    OutPath->Length = String_GetLength_Ex(OutPath->Data, MAX_PATH_LENGTH);
}

bool Platform_GetEnvironmentVariableValue(String Name, String* OutVariable)
{
    char* Value = getenv(Name.Data);
    if (Value == NULL)
    {
        return false;
    }

    String_Copy(OutVariable, CStr(Value));
    return true;
}

bool Platform_DoesEnvironmentVariableExist(String Name)
{
    char* Value = getenv(Name.Data);
    if (Value == NULL)
    {
        return false;
    }

    return true;
}

bool Platform_CaptureStackTrace(LinearAllocator* Arena, TArray(StackTraceData)* OutInfo)
{
    UNIMPLEMENTED;
    return false;
}

u32 Platform_GetNumLogicalProcessors(void)
{
    u32 number_of_processors = (u32)sysconf(_SC_NPROCESSORS_ONLN);

    return number_of_processors;
}

bool Platform_GetAccountName(String* OutName)
{
    return Platform_GetUserName(OutName);
}

bool Platform_GetUserName(String* OutName)
{
    struct passwd pwd = {0};
    struct passwd* result = NULL;
    char Buffer[4096] = {0};

    getpwuid_r(getuid(), &pwd, Buffer, 4096, &result);
    if (result == NULL)
    {
        StringLocal(Message, 512);
        String_Format(&Message, S("Failed to get user name"), Message.Capacity);
        LogLastError(Message);
        return false;
    }

    String_Copy(OutName, CStr(pwd.pw_name));
    return true;
}

bool Platform_GetUserDirectory(String* OutDirectory)
{
    struct passwd pwd = {0};
    struct passwd* result = NULL;
    char Buffer[4096] = {0};

    getpwuid_r(getuid(), &pwd, Buffer, 4096, &result);
    if (result == NULL)
    {
        StringLocal(Prefix, MAX_PATH_LENGTH);
        String_Format(&Prefix, S("Failed to get user directory"), Prefix.Capacity);
        LogLastError(Prefix);
        return false;
    }

    String_Copy(OutDirectory, CStr(pwd.pw_dir));
    return true;
}

bool Filesystem_GetFilePath(const FileHandle* Handle, String* OutPath)
{
    if (!IsValidFileHandle(Handle))
    {
        return false;
    }

    const i32 fd = fileno((FILE*)Handle->Data);

    struct kinfo_file kinfo = {0};
    kinfo.kf_structsize = sizeof(struct kinfo_file);

    if (fcntl(fileno((FILE*)Handle->Data), F_KINFO, &kinfo) == -1)
    {
	StringLocal(Prefix, 512);
	String_Format(&Prefix, S("Failed to retrieve file path for given handle: %i"), 512, fd);
        LogLastError(Prefix);
        return false;
    }

    String_Copy(OutPath, CStr(kinfo.kf_path));

    return true;
}

bool Filesystem_IsPathRelative(const String Path)
{
    return Path.Data[0] != '/';
}

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

bool Platform_GetCurrentProcessName(String* OutName)
{
    // todo: use argv[0]. store it from main()
    //return S("");
    //return CStr(__progname);
    return false;
}

u64 Platform_GetCurrentProcessID(void)
{
    return (u64)getpid();
}

bool Platform_GetThreadName(void* ThreadHandle, String* OutName)
{
    //UNIMPLEMENTED;
    return false;
}

bool Platform_IsProgramRunning(const String ProgramName)
{
    return false;
}

static String GArgV[128] = {0};
static i32 GArgC = 0;
static char** GEnv = NULL;

static String GProgramName = { 0 };
static char GEmptyBuffer[16] = {0};

PRAGMA_DISABLE_MISSING_PROTOTYPES_WARNING

void pre_main(int argc, char* argv[], char* env[])
{
    GArgC = argc;
    GEnv  = env;

    for (u16 i = 0; i < 128; i++)
    {
        GArgV[i].Data = GEmptyBuffer;
        GArgV[i].Length = 0;
        GArgV[i].Capacity = 15;
    }

    for (int i = 1; i < argc; ++i)
    {
        GArgV[i-1].Data = argv[i];
        GArgV[i-1].Length = String_GetLength_Ex(argv[i], UINT16_MAX);
        GArgV[i-1].Capacity = GArgV[i-1].Length;
    }

    GProgramName.Data = argv[0];
    GProgramName.Length = String_GetLength_Ex(argv[0], UINT16_MAX);
    GProgramName.Capacity = GProgramName.Length;
}

PRAGMA_ENABLE_WARNINGS

StringArray Platform_GetCommandLineArgs(void)
{
    StringArray Args = {0};
    Args.List = GArgV;
    Args.Num = (u32)(GArgC-1 <= 0 ? 0 : (GArgC-1 < 128 ? GArgC-1 : 128));
    return Args;
}

void* Platform_GetDeviceContext(void)
{
    return NULL;
}

bool Filesystem_Open(const String FilePath, u32 Mode, FileHandle* OutHandle)
{
    if (NEVER(OutHandle == NULL)) return false;

    String ModeStr = String_Null();

    if (((Mode & FileMode_Read) != 0) && ((Mode & FileMode_Write) != 0)) // read and write
    {
        ModeStr = S("a+");
    }
    else if (((Mode & FileMode_Read) != 0) && ((Mode & FileMode_Write) == 0)) // read only
    {
        ModeStr = S("r");
    }
    else if (((Mode & FileMode_Read) == 0) && ((Mode & FileMode_Write) != 0)) // write only
    {
        ModeStr = S("w");
    }
    else
    {
        LOG_WARNING("Invalid mode passed (%u) while trying to open file \"%S\"", Mode, FilePath);
        return false;
    }

    u32 LastSlash = 0;
    String_IndexOfLastPathSlash(FilePath, &LastSlash);

    StringLocal(Copy, MAX_PATH_LENGTH);
    String_Copy(&Copy, StrSlice(FilePath.Data, LastSlash));

    Filesystem_OpenDirectory(Copy);

    FILE* File = fopen(FilePath.Data, ModeStr.Data); // refactor to just use open() instead of fopen()
    if (!File)
    {
        String ModeString;
        if (((Mode & FileMode_Read) != 0) && ((Mode & FileMode_Write) != 0)) // read and write
        {
            ModeString = S("for reading/writing");
        }
        else if (((Mode & FileMode_Read) != 0) && ((Mode & FileMode_Write) == 0)) // read only
        {
            ModeString = S("for reading");
        }
        else
        {
            ModeString = S("for writing");
        }

        StringLocal(Message, MAX_PATH_LENGTH);
        String_Format(&Message, S("Failed to open file %S -> \"%S\""), MAX_PATH_LENGTH, ModeString, FilePath);
        LogLastError(Message);
        return false;
    }

    OutHandle->Data = File;
    OutHandle->Data2 = NULL;

    return true;
}

// todo: move to core
bool Filesystem_NewFile(const String FilePath)
{
    FileHandle f = {0};
    bool bSuccess = Filesystem_Open(FilePath, FileMode_Write, &f);
    Filesystem_Close(&f);

    return bSuccess;
}

bool Filesystem_DeleteFile(String FilePath)
{
    UNIMPLEMENTED;
    return false;
}

bool Filesystem_Copy(const String Source, const String Destination)
{
    UNIMPLEMENTED;
    return false;
}

bool Filesystem_Open_MemoryMapped(const String FilePath, u32 Mode, FileHandle* OutHandle, u8** OutData, u64* OutSize)
{
    if (OutSize)
        *OutSize = 0;

    if (OutData)
        *OutData = NULL;

    if (!IsValidFileHandle(OutHandle))
    {
        if (!Filesystem_Open(FilePath, Mode, OutHandle))
            return false;
    }

    int ProtectFlags = 0;

    if (((Mode & FileMode_Read) != 0) && ((Mode & FileMode_Write) != 0)) // read and write
    {
        ProtectFlags = PROT_READ | PROT_WRITE;
    }
    else if (((Mode & FileMode_Read) != 0) && ((Mode & FileMode_Write) == 0)) // read only
    {
        ProtectFlags = PROT_READ;
    }
    else if (((Mode & FileMode_Read) == 0) && ((Mode & FileMode_Write) != 0)) // write only
    {
        ProtectFlags = PROT_WRITE;
    }

    u64 Size = 0;
    Filesystem_GetFileSize(OutHandle, &Size);

    void* Address = mmap(NULL, Size, ProtectFlags, MAP_SHARED, fileno((FILE*)OutHandle->Data), 0);

    if (Address == MAP_FAILED)
    {
        StringLocal(Prefix, 512);
        String_Format(&Prefix, S("Failed to memory map file \"%S\""), Prefix.Capacity, FilePath);
        LogLastError(Prefix);
        return false;
    }

    OutHandle->Data2 = Address;

    if (OutData)
        *OutData = (u8*)Address;

    if (OutSize) 
        *OutSize = Size;

    return 0;
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

                StringLocal(BaseDirectory, MAX_PATH_LENGTH);
                if (FilePath.Length-1 == i)
                    String_Copy(&BaseDirectory, StrSlice(FilePath.Data, i+1));
                else
                    String_Copy(&BaseDirectory, StrSlice(FilePath.Data, i));

                NextSlashIndex = i+1;

                bool bDirectoryCreated = Filesystem_DoesDirectoryExist(BaseDirectory) || BaseDirectory.Length == 0;
                if (!bDirectoryCreated)
                {
                    i32 ErrorCode = mkdir(BaseDirectory.Data, 0700);
                    if (ErrorCode == -1)
                    {
                        StringLocal(Prefix, MAX_PATH_LENGTH);
                        String_Format(&Prefix, S("Failed to open directory \"%S\""), Prefix.Capacity, BaseDirectory);
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

/*
    i32 ErrorCode = mkdir(FilePath.Data, 0700);
    if (ErrorCode == -1)
    {
        StringLocal(Prefix, MAX_PATH_LENGTH);
        String_Format(&Prefix, S("Failed to open directory \"%S\""), Prefix.Capacity, FilePath);
        LogLastError(Prefix);
        return false;
    }

    return true;*/
}

bool Filesystem_OpenDirectory_Ex(const String FilePath, FileHandle* OutHandle)
{
    return Filesystem_OpenDirectory(FilePath);
}

bool Filesystem_Close(FileHandle* Handle)
{
    bool bFailedUnmap = false;
    if (Handle->Data2)
    {
        Handle->Data2 = NULL;

        u64 Size = 0;
        Filesystem_GetFileSize(Handle, &Size);

        if (munmap(Handle->Data2, Size) == -1)
        {
            StringLocal(Path, MAX_PATH_LENGTH);
            Filesystem_GetFilePath(Handle, &Path);

            StringLocal(Prefix, 512);
            String_Format(&Prefix, S("Failed to unmap memory for file \"%S\""), Prefix.Capacity, Path);
            LogLastError(Prefix);
            bFailedUnmap = true;
        }
    }

    if (IsValidFileHandle(Handle))
    {
        fclose(Handle->Data);
        *Handle = FileHandle_Null();
        return !bFailedUnmap;
    }
    
    return false;
}

bool Filesystem_Seek(const FileHandle* Handle, i64 Offset)
{
    i32 ErrorCode = fseek(Handle->Data, Offset, SEEK_CUR);
    if (ErrorCode == -1)
    {
        StringLocal(Path, MAX_PATH_LENGTH);
        Filesystem_GetFilePath(Handle, &Path);

        StringLocal(Prefix, 512);
        String_Format(&Prefix, S("Seek failed for \"%S\""), Prefix.Capacity, Path);
        LogLastError(Prefix);
        return false;
    }

    return true;
}

bool Filesystem_SeekFromBeginning(const FileHandle* Handle, u64 Offset)
{
    i32 ErrorCode = fseek(Handle->Data, (i32)Offset, SEEK_SET);
    if (ErrorCode == -1)
    {
        StringLocal(Path, MAX_PATH_LENGTH);
        Filesystem_GetFilePath(Handle, &Path);

        StringLocal(Prefix, 512);
        String_Format(&Prefix, S("SeekFromBeginning failed for \"%S\""), Prefix.Capacity, Path);
        LogLastError(Prefix);
        return false;
    }

    return true;
}

bool Filesystem_SeekFromEnd(const FileHandle* Handle, u64 Offset)
{
    i32 ErrorCode = fseek(Handle->Data, (i32)Offset, SEEK_END);
    if (ErrorCode == -1)
    {
        StringLocal(Path, MAX_PATH_LENGTH);
        Filesystem_GetFilePath(Handle, &Path);

        StringLocal(Prefix, 512);
        String_Format(&Prefix, S("SeekFromEnd failed for \"%S\""), Prefix.Capacity, Path);
        LogLastError(Prefix);
        return false;
    }

    return true;
}

bool Filesystem_SeekToBeginning(const FileHandle* Handle)
{
    i32 ErrorCode = fseek(Handle->Data, 0, SEEK_SET);
    if (ErrorCode == -1)
    {
        StringLocal(Path, MAX_PATH_LENGTH);
        Filesystem_GetFilePath(Handle, &Path);

        StringLocal(Prefix, 512);
        String_Format(&Prefix, S("SeekToBeginning failed for \"%S\""), Prefix.Capacity, Path);
        LogLastError(Prefix);
        return false;
    }

    return true;
}

bool Filesystem_SeekToEnd(const FileHandle* Handle)
{
    i32 ErrorCode = fseek(Handle->Data, 0, SEEK_END);
    if (ErrorCode == -1)
    {
        StringLocal(Path, MAX_PATH_LENGTH);
        Filesystem_GetFilePath(Handle, &Path);

        StringLocal(Prefix, 512);
        String_Format(&Prefix, S("SeekToEnd failed for \"%S\""), Prefix.Capacity, Path);
        LogLastError(Prefix);
        return false;
    }

    return true;
}

u64  Filesystem_GetCurrentFilePosition(const FileHandle* Handle)
{
    return (u64)ftell(Handle->Data);
}

u64  Filesystem_GetLastWriteTime(const String FilePath)
{
    if (!Filesystem_DoesFileExist(FilePath))
        return 0;

    struct stat FileStat = {0};
    i32 ErrorCode = stat(FilePath.Data, &FileStat);
    if (ErrorCode == -1)
    {
        StringLocal(Prefix, MAX_PATH_LENGTH);
        String_Format(&Prefix, S("Failed to retrieve last write time for file \"%S\""), Prefix.Capacity, FilePath);
        LogLastError(Prefix);
        return 0;
    }

    return (u64)FileStat.st_mtime;
}

u64  Filesystem_GetLastAccessTime(const String FilePath)
{
    if (!Filesystem_DoesFileExist(FilePath))
        return 0;

    struct stat FileStat = {0};
    i32 ErrorCode = stat(FilePath.Data, &FileStat);
    if (ErrorCode == -1)
    {
        StringLocal(Prefix, MAX_PATH_LENGTH);
        String_Format(&Prefix, S("Failed to retrieve last access time for file \"%S\""), Prefix.Capacity, FilePath);
        LogLastError(Prefix);
        return 0;
    }

    return (u64)FileStat.st_atime;
}

FileTimeData  Filesystem_GetFileTime(const String FilePath)
{
    FileTimeData a = {0};

    if (!Filesystem_DoesFileExist(FilePath))
        return a;

    a.CreationTime = Filesystem_GetCreationTime(FilePath);
    a.LastAccessTime = Filesystem_GetLastAccessTime(FilePath);
    a.LastWriteTime = Filesystem_GetLastWriteTime(FilePath);
    return a;
}

#ifdef HAVE_ST_BIRTHTIME
#define birthtime(x) x.st_birthtime
#else
#define birthtime(x) x.st_ctime
#endif

u64  Filesystem_GetCreationTime(const String FilePath)
{
    if (!Filesystem_DoesFileExist(FilePath))
        return 0;

    struct stat FileStat = {0};
    i32 ErrorCode = stat(FilePath.Data, &FileStat);
    if (ErrorCode == -1)
    {
        StringLocal(Prefix, MAX_PATH_LENGTH);
        String_Format(&Prefix, S("Failed to retrieve creation time for file \"%S\""), Prefix.Capacity, FilePath);
        LogLastError(Prefix);
        return 0;
    }

    return (u64)birthtime(FileStat);
}

u64  Filesystem_GetLastWriteTimeH(const FileHandle* Handle)
{
    StringLocal(Path, MAX_PATH_LENGTH);
    Filesystem_GetFilePath(Handle, &Path);

    return Filesystem_GetLastWriteTime(Path);
}

u64  Filesystem_GetLastAccessTimeH(const FileHandle* Handle)
{
    StringLocal(Path, MAX_PATH_LENGTH);
    Filesystem_GetFilePath(Handle, &Path);

    return Filesystem_GetLastAccessTime(Path);
}

u64  Filesystem_GetCreationTimeH(const FileHandle* Handle)
{
    StringLocal(Path, MAX_PATH_LENGTH);
    Filesystem_GetFilePath(Handle, &Path);

    return Filesystem_GetCreationTime(Path);
}

FileTimeData  Filesystem_GetFileTimeH(const FileHandle* Handle)
{
    StringLocal(Path, MAX_PATH_LENGTH);
    Filesystem_GetFilePath(Handle, &Path);

    FileTimeData a = {0};
    a.CreationTime = Filesystem_GetCreationTime(Path);
    a.LastAccessTime = Filesystem_GetLastAccessTime(Path);
    a.LastWriteTime = Filesystem_GetLastWriteTime(Path);
    return a;
}

bool Filesystem_ReadPipe(PlatformPipe Handle, u64 DataSize, void* OutData, u64* OutBytesRead)
{
    if (NEVER(Handle[0] == -1)) return false;
    if (NEVER(Handle[1] == -1)) return false;

    i64 BytesRead = read(Handle[0], OutData, DataSize);
    if (BytesRead < 0)
    {
        StringLocal(Prefix, MAX_PATH_LENGTH);
        String_Format(&Prefix, S("Failed to read pipe from handle -> Read: %i | Write: %i"), Prefix.Capacity, Handle[0], Handle[1]);
        LogLastError(Prefix);
        return false;
    }

    if (OutBytesRead)
        *OutBytesRead = (u64)BytesRead;

    return true;
}

bool Filesystem_Read(const FileHandle* Handle, u64 DataSize, void* OutData, u64* OutBytesRead)
{
    if (NEVER(!IsValidFileHandle(Handle))) return false;

    u64 BytesRead = fread(OutData, 1, DataSize, Handle->Data);
    if (BytesRead == 0)
    {
        StringLocal(Path, MAX_PATH_LENGTH);
        Filesystem_GetFilePath(Handle, &Path);

        StringLocal(Prefix, MAX_PATH_LENGTH);
        String_Format(&Prefix, S("Failed to read file \"%S\""), Prefix.Capacity, Path);
        LogLastError(Prefix);
        return false;
    }

    if (OutBytesRead)
        *OutBytesRead = BytesRead;

    return true;
}

bool Filesystem_ReadEntireFile(const FileHandle* Handle, void* OutData, u64* OutBytesRead)
{
    if (NEVER(!IsValidFileHandle(Handle))) return false;

    // todo: remove this?
    Filesystem_SeekToBeginning(Handle);
    
    u64 Size = 0;
    Filesystem_GetFileSize(Handle, &Size);

    u64 BytesRead = fread(OutData, 1, Size, Handle->Data);
    if (BytesRead == 0)
    {
        StringLocal(Path, MAX_PATH_LENGTH);
        Filesystem_GetFilePath(Handle, &Path);

        StringLocal(Prefix, MAX_PATH_LENGTH);
        String_Format(&Prefix, S("Failed to entire read file \"%S\""), Prefix.Capacity, Path);
        LogLastError(Prefix);
        return false;
    }

    if (OutBytesRead)
        *OutBytesRead = BytesRead;

    return true;
}

// todo: move to core
bool Filesystem_ReadLine(const FileHandle* Handle, String* LineBuffer)
{
    if (NEVER(!IsValidFileHandle(Handle))) return false;
    if (NEVER(LineBuffer == NULL)) return false;
    if (NEVER(LineBuffer->Data == NULL || LineBuffer->Data == String_Null().Data)) return false;

    u64 CurrentPosition = Filesystem_GetCurrentFilePosition(Handle);

    u64 Size = 0;
    Filesystem_GetFileSize(Handle, &Size);
    u64 FileSize = Size;

    if (CurrentPosition >= FileSize)
    {
        Filesystem_SeekToBeginning(Handle);
        return false;
    }

    char TempBuffer[8192] = {0};
    u64 BytesRead = fread(TempBuffer, 1, 8191, Handle->Data);
    if (BytesRead == 0)
    {
        StringLocal(Path, MAX_PATH_LENGTH);
        Filesystem_GetFilePath(Handle, &Path);

        StringLocal(Prefix, MAX_PATH_LENGTH);
        String_Format(&Prefix, S("Failed to read line for file \"%S\""), Prefix.Capacity, Path);
        LogLastError(Prefix);
        return false;
    }

    // todo: move to core?
    /////////////////
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

    Filesystem_SeekFromBeginning(Handle, CurrentPosition + FilePointerOffset);

    return true;
}

bool Filesystem_ReadLine_Backwards(const FileHandle* Handle, String* LineBuffer)
{
    UNIMPLEMENTED;
    return 0;
}

bool Filesystem_Write(const FileHandle* Handle, u64 DataSize, const void* Data, u64* OutBytesWritten)
{
    if (NEVER(!IsValidFileHandle(Handle))) return false;
    if (DataSize == 0) return false;

    Filesystem_SeekToBeginning(Handle);

    u64 BytesWritten = fwrite(Data, 1, DataSize, Handle->Data);
    if (BytesWritten == 0)
    {
        StringLocal(Path, MAX_PATH_LENGTH);
        Filesystem_GetFilePath(Handle, &Path);

        StringLocal(Prefix, MAX_PATH_LENGTH);
        String_Format(&Prefix, S("Failed to write to file \"%S\""), Prefix.Capacity, Path);
        LogLastError(Prefix);
        return false;
    }

    if (OutBytesWritten)
        *OutBytesWritten = BytesWritten;

    return true;
}

bool Filesystem_WriteLineFormatted(const FileHandle* Handle, const String Text, u64* OutBytesWritten, ...)
{
    if (NEVER(!IsValidFileHandle(Handle))) return false;

    Filesystem_SeekToEnd(Handle);
    
    va_list Args;
    va_start(Args, OutBytesWritten);
    StringLocal(Buffer, 32768);
    String_FormatV(&Buffer, Text, 32768, Args);
    va_end(Args);

    u64 BytesWritten = fwrite(Buffer.Data, 1, Buffer.Length, Handle->Data);
    if (BytesWritten == 0)
    {
        StringLocal(Path, MAX_PATH_LENGTH);
        Filesystem_GetFilePath(Handle, &Path);

        StringLocal(Prefix, MAX_PATH_LENGTH);
        String_Format(&Prefix, S("Failed to write line to file \"%S\""), Prefix.Capacity, Path);
        LogLastError(Prefix);
        return false;
    }

    if (OutBytesWritten)
        *OutBytesWritten = BytesWritten;

    return true;
}

bool Filesystem_WriteLine(const FileHandle* Handle, const String Text, u64* OutBytesWritten)
{
    if (NEVER(!IsValidFileHandle(Handle))) return false;

    Filesystem_SeekToEnd(Handle);

    u64 BytesWritten = fwrite(Text.Data, 1, Text.Length, Handle->Data);
    if (BytesWritten == 0)
    {
        StringLocal(Path, MAX_PATH_LENGTH);
        Filesystem_GetFilePath(Handle, &Path);

        StringLocal(Prefix, MAX_PATH_LENGTH);
        String_Format(&Prefix, S("Failed to write line to file \"%S\""), Prefix.Capacity, Path);
        LogLastError(Prefix);
        return false;
    }

    if (OutBytesWritten)
        *OutBytesWritten = BytesWritten;

    return true;
}

bool Filesystem_DoesFileExist(const String FilePath)
{
    StringLocal(Copy, MAX_PATH_LENGTH);
    String_Copy(&Copy, FilePath);

    if (access(Copy.Data, F_OK) == 0)
    {
        return true;
    }
    
    return false;
}

bool Filesystem_DoesDirectoryExist(const String FilePath)
{
    DIR* Found = opendir(FilePath.Data);
    if (!Found)
    {
        //StringLocal(Prefix, MAX_PATH_LENGTH);
        //String_Format(&Prefix, S("Failed to open directory \"%S\""), Prefix.Capacity, FilePath);
        //LogLastError(Prefix);
        return false;
    }

    closedir(Found);

    return true;
}

bool Filesystem_GetFileSize(const FileHandle* File, u64* OutSize)
{
    StringLocal(Path, MAX_PATH_LENGTH);
    Filesystem_GetFilePath(File, &Path);

    struct stat filestat = {0};
    stat(Path.Data, &filestat);
    *OutSize = (u64)filestat.st_size;
    return true;
}

bool Filesystem_IsFile(const String Path)
{
    struct stat filestat = {0};
    stat(Path.Data, &filestat);
    return S_ISREG(filestat.st_mode);
}

bool Filesystem_IsDirectory(const String Path)
{
    struct stat filestat = {0};
    stat(Path.Data, &filestat);
    return S_ISDIR(filestat.st_mode);
}

// todo: move into platform core?
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

bool Filesystem_ConvertRelativeToAbsolutePath(String* OutFullPath)
{
    StringLocal(Copy, MAX_PATH_LENGTH);
    String_Copy(&Copy, *OutFullPath);

    char* Result = realpath(Copy.Data, OutFullPath->Data);
    if (Result == NULL)
    {
        StringLocal(Format, MAX_PATH_LENGTH);
        String_Format(&Format, S("Failed to convert \"%S\" to an absolute path"), MAX_PATH_LENGTH, Copy);
        LogLastError(Format);
        return false;
    }

    OutFullPath->Length = String_GetLength_Ex(Result, MAX_PATH_LENGTH);

    return true;
}

internal bool Internal_IterateDirectory(const String BasePath, const String DirectoryPath, DirectoryIterator Callback, bool bRecursive, void* UserData)
{
    const String RealBasePath = BasePath.Length == 0 ? S(".") : BasePath;
    
    struct dirent* entry = NULL;
    DIR* dp = opendir(RealBasePath.Data);
    if (!dp)
    {
        StringLocal(Message, MAX_PATH_LENGTH);
        String_Format(&Message, S("Failed to iterate directory for path \"%S\""), MAX_PATH_LENGTH, RealBasePath);
        LogLastError(Message);
        return false;
    }
    
    bool bSuccess = true;

    while ((entry = readdir(dp)))
    {
        if (entry->d_type != DT_REG && entry->d_type != DT_DIR)
        {
            continue;
        }

        if (entry->d_name[0] == '.' && 
            (!entry->d_name[1] || (entry->d_name[1] == '.' && !entry->d_name[2])))
        {
            continue;
        }

        //LOG("%S", CStr(entry->d_name));

        const String EntryName = CStr(entry->d_name);

        StringLocal(FullPath, MAX_PATH_LENGTH);
        String_BuildPath(&FullPath, RealBasePath, EntryName);
        
        StringLocal(RelativePath, MAX_PATH_LENGTH);

        if (entry->d_type == DT_DIR)
        {
            String_BuildPath(&RelativePath, DirectoryPath, EntryName);

            bool bResult = Callback(FullPath, RelativePath, EntryName, 0, true, UserData);
            if (!bResult)
            {
                bSuccess = false;
                break;
            }

            if (bRecursive)
            {
                if (!Internal_IterateDirectory(FullPath, RelativePath, Callback, true, UserData))
                {
                    bSuccess = false;
                    break;
                }
            }
        }
        else
        {
            u64 FileSize = 0;
            struct stat filestat = {0};
            stat(FullPath.Data, &filestat);
            FileSize = (u64)filestat.st_size;

            if (DirectoryPath.Length > 0)
            {
                String_BuildPath(&RelativePath, DirectoryPath, EntryName);
            }
            else
            {
                String_Copy(&RelativePath, EntryName);
            }

            bool bResult = Callback(FullPath, RelativePath, EntryName, FileSize, false, UserData);
            if (!bResult)
            {
                bSuccess = false;
                break;
            }
        }
    }
    
    closedir(dp);
    return bSuccess;
}

void Filesystem_IterateDirectory(const String BasePath, DirectoryIterator Callback, bool bRecursive)
{
    Internal_IterateDirectory(BasePath, S(""), Callback, bRecursive, NULL);
}

void Filesystem_IterateDirectory_Ex(const String BasePath, DirectoryIterator Callback, bool bRecursive, void* UserData)
{
    Internal_IterateDirectory(BasePath, S(""), Callback, bRecursive, UserData);
}

bool Filesystem_DeleteFiles(const String FilePath, const String Wildcard, bool bRecursive)
{
    StringLocal(Cmd, MAX_PATH_LENGTH);
    String_Append(&Cmd, S("rm -f "));
    if (bRecursive)
        String_Append(&Cmd, S("-r \""));
    String_Append(&Cmd, FilePath);
    String_AppendPathSeparator_Checked(&Cmd);
    String_AppendChar(&Cmd, '"');
    String_Append(&Cmd, Wildcard);
    String_Append(&Cmd, S(" 2> /dev/null"));
    i32 Result = system(Cmd.Data);
    return Result == 0;
}

bool Filesystem_DeleteDirectory(const String DirectoryPath)
{
    StringLocal(Cmd, MAX_PATH_LENGTH);
    String_Append(&Cmd, S("rm "));
    //if (bRecursive)
    //    String_Append(&Cmd, S("-r \""));A
    String_Append(&Cmd, DirectoryPath);
    String_AppendPathSeparator_Checked(&Cmd);
    //String_AppendChar(&Cmd, '"');
    //String_Append(&Cmd, Wildcard);
    String_Append(&Cmd, S(" 2> /dev/null"));
    i32 Result = system(Cmd.Data);
    return Result == 0;
}

bool Filesystem_ArePathsCommon(String PathA, String PathB)
{
    bool bPrefixMatch = String_StartsWith(PathB, PathA, true);
    
    return bPrefixMatch;
}

Uuid UUID_Generate(void)
{
    uuid_t id;
    u32 status = 0;
    uuid_create(&id, &status);

    return *(Uuid*)&id;
}

bool UUID_IsEqual(Uuid First, Uuid Second)
{
    //return Platform_MemEqual(&First, &Second, sizeof(Uuid));

    uuid_t* a = (uuid_t*)&First;
    uuid_t* b = (uuid_t*)&Second;

    u32 status = 0;
    const bool bSame = uuid_compare(a, b, &status) == 0;
    return bSame;
}

void UUID_ToString(Uuid ID, String* OutString)
{
    StringLocal(Temp, GUID_LENGTH);

    uuid_t* a = (uuid_t*)&ID;
    u32 status = 0;
    uuid_to_string(a, &Temp.Data, &status);

    String_Copy(OutString, Temp);
}

Uuid UUID_FromString(const String IDString)
{
    uuid_t id;
    u32 status = 0;
    uuid_from_string(IDString.Data, &id, &status);

    return *(Uuid*)&id;
}

bool Platform_GetTerminalDimensions(u32* OutRows, u32* OutColumns)
{
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == -1)
    {
        LogLastError(S("Failed to get terminal dimensions"));
        return false;
    }

    *OutRows = w.ws_row;
    *OutColumns = w.ws_col;

    return true;
}

#endif // PLATFORM_LINUX
