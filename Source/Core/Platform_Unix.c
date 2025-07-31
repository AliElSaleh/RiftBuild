// Copyright (c) Artisan Softworks
// Licensed under the BSD 3-Clause License. See the LICENSE file for details.

#include "Platform.h"

#if PLATFORM_UNIX

#include "Memory.h"
#include "Allocators.h"
#include "StringUtils.h"
#include "Uuid.h"
#include "Filesystem.h"
#include "Clock.h"
#include "Log.h"

#if PLATFORM_LINUX
#define _XOPEN_SOURCE 700
#endif

#include <signal.h>
#include <stdio.h>

#define _LARGEFILE64_SOURCE
#define _FILE_OFFSET_BITS 64
#define __USE_FILE_OFFSET64
#define __USE_GNU
#define __USE_MISC

#include <stdlib.h>
#include <memory.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <dirent.h>
#include <unistd.h>
#include <pwd.h>
#include <termios.h>
#include <limits.h>

#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/syscall.h>

#include <stdarg.h>

#ifndef NO_LOG 
static void LogLastError(const String Prefix)
{
    StringLocal(Message, 4096);
    String_Copy(&Message, CStr(strerror(errno)));

    //LOG_ERROR("%S\n        errno %i\n        Reason: %S\n", Prefix, errno, Message);

    StringLocal(FormattedMessage, 4096);
    String_Format(&FormattedMessage, S("%S\n        Error Code: %i\n        Reason: %S"), Prefix, errno, Message);
    Platform_ConsoleWrite_CustomLength((const char*)FormattedMessage.Data, FormattedMessage.Length, 3, true);
}
#else
#define LogLastError(...)
#endif

static void Internal_SignalHandler(int signal)
{
    exit(1);
}

void Platform_PreInitialize(void)
{
    //Platform_GetClockFrequency();

    struct sigaction act = {0};
    act.sa_handler = &Internal_SignalHandler;
    sigaction(SIGINT, &act, NULL);
    sigaction(SIGQUIT, &act, NULL);
    sigaction(SIGILL, &act, NULL);
    sigaction(SIGABRT, &act, NULL);
    sigaction(SIGSEGV, &act, NULL);
}

f64 Platform_GetClockFrequency(void)
{
    return 0;
}

NO_RETURN void Platform_Abort(u32 ExitCode)
{
    exit((i32)ExitCode);
}

// TODO: BUILD_LIB define
static String GArgV[128] = {0};
static i32 GArgC = 0;
static char** GEnv = NULL;

static String GProgramName = { 0 };
static uchar GEmptyBuffer[16] = {0};

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
        GArgV[i-1].Data = (uchar*)argv[i];
        GArgV[i-1].Length = String_GetLength_Ex(argv[i], UINT16_MAX);
        GArgV[i-1].Capacity = GArgV[i-1].Length;
    }

    GProgramName.Data = (uchar*)argv[0];
    GProgramName.Length = String_GetLength_Ex(argv[0], UINT16_MAX);
    GProgramName.Capacity = GProgramName.Length;
}

PRAGMA_ENABLE_WARNINGS

StringArray Platform_GetCommandLineArgs(void)
{
    StringArray Args = {0};
    Args.Num = (u32)(GArgC-1 <= 0 ? 0 : (GArgC-1 < 128 ? GArgC-1 : 128));
    Args.List = GArgV;
    return Args;
}

void* Platform_MemAlloc(usize Size)
{
    return malloc(Size);
}

void* Platform_MemAllocZero(usize Size)
{
    void* Memory = malloc(Size);
    memset(Memory, 0, Size);
    return Memory;
}

void* Platform_MemReAlloc(void* Block, usize Size)
{
    return realloc(Block, Size);
}

void  Platform_MemFree(void* Block)
{
    free(Block);
}

void Platform_MemZero(void* Block, usize Size)
{
    (void)memset(Block, 0, Size);
}

void Platform_MemCopy(void* Dest, const void* Source, usize Size)
{
    memcpy(Dest, Source, Size);
}

void Platform_MemMove(void* Dest, const void* Source, usize Size)
{
    memmove(Dest, Source, Size);
}

void Platform_MemSet(void* Dest, i32 Value, usize Size)
{
    memset(Dest, Value, Size);
}

bool Platform_MemEqual(const void* Block1, const void* Block2, usize Size)
{
    return memcmp(Block1, Block2, Size) == 0;
}

// GOAT'ed repo: https://github.com/zrafa/onscreenkeyboard/blob/master/key.c
//               https://web.archive.org/web/20180401093525/http://cc.byexamples.com/2007/04/08/non-blocking-user-input-in-loop-without-ncurses/

#define NB_DISABLE 1
#define NB_ENABLE  0

static void nonblock(int state)
{
    struct termios ttystate = {0};

    //get the terminal state
    tcgetattr(STDIN_FILENO, &ttystate);

    if (state == NB_ENABLE)
    {
        //turn off canonical mode
        ttystate.c_lflag &= ~(u32)ICANON;
        //minimum of number input read.
        ttystate.c_cc[VMIN] = 1;
    }
    else if (state == NB_DISABLE)
    {
        //turn on canonical mode
        ttystate.c_lflag |= ICANON;
    }

    //set the terminal attributes.
    tcsetattr(STDIN_FILENO, TCSANOW, &ttystate);
}

static i32 kbhit(void)
{
    struct timeval tv;
    fd_set fds;
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds); //STDIN_FILENO is 0
    select(STDIN_FILENO+1, &fds, NULL, NULL, &tv);
    return FD_ISSET(STDIN_FILENO, &fds);
}

void Platform_BeginNonBlockingMode(void)
{
    nonblock(NB_ENABLE);
}

void Platform_EndNonBlockingMode(void)
{
    nonblock(NB_DISABLE);
}

bool Platform_AnyKeyPressed(void)
{
    i32 Result = kbhit();
    if (Result)
    {
        i32 c = fgetc(stdin);
        return c != 0;
    }

    return false;
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

PRAGMA_DISABLE_WARNINGS

#if COMPILER_GCC
#pragma GCC diagnostic ignored "-Wunused-result"
#endif

void Platform_ConsoleWrite(const char* Message, u8 Color, bool bIsError)
{
    Platform_ConsoleWrite_CustomLength(Message, String_GetLength(Message), 0, false);
}

void Platform_ConsoleWrite_CustomLength(const char* Message, u32 Length, u8 Color, bool bIsError)
{
    static String colors[] = {S("0;37"), S("0;32"), S("1;33"), S("1;31"), S("0;41"), S("0;37"), S("0;37")};

    bool bIgnoreNewLine = Color == 4 && Message[Length-1] == '\n';
    if (UNLIKELY(bIgnoreNewLine)) Length--;

    StringLocal(Temp, 16384);
    String_Append(&Temp, S("\033["));
    String_Append(&Temp, LIKELY(Color < 6) ? colors[Color] : S("0:37"));
    String_Append(&Temp, S("m"));
    String_Append(&Temp, StrSlice((uchar*)Message, Length));
    String_Append(&Temp, S("\033[0m"));

    (void)write(STDOUT_FILENO, Temp.Data, Temp.Length);

    if (UNLIKELY(bIgnoreNewLine)) (void)write(STDOUT_FILENO, "\n", 1);

    fflush(stdout);
}

PRAGMA_ENABLE_WARNINGS

extern char** environ;

PlatformHandle Platform_RunProcess(const String ProcessExePath, const String Parameters, const String WorkingDirectory, const String EnvBlock)
{
    pid_t pid = fork();

    if (pid < 0)
    {
        LogLastError(S("fork() failed"));
    }
    else if (pid > 0) // parent path
    {
        return pid;
    }
    else // child path
    {
        bool bChangeSuccess = Platform_SetWorkingDirectory(WorkingDirectory);
        if (!bChangeSuccess)
        {
            exit(1);
            return 1;
        }

        StringLocal(EnvArgs, 4096);
        char* envp[256] = { NULL };

        if (EnvBlock.Length > 0)
        {
            u32 EnvCount = 0;
            while (environ[EnvCount] != NULL)
            {
                String_Append(&EnvArgs, CStrEx(environ[EnvCount], 4096));
                String_AppendChar(&EnvArgs, '\0');
                EnvCount++;
            }

            String_Append(&EnvArgs, EnvBlock);

            u32 i = 0;
            u32 Offset = 0;
            for (u32 j = 0; j < EnvArgs.Length; j++)
            {
                if (EnvArgs.Data[j] == '\0')
                {
                    envp[i] = (char*)&EnvArgs.Data[Offset];
                    Offset = j+1;
                    i++;
                }
            }
        }

        i8 ArenaMemory[UINT16_MAX] = {0};
        LinearAllocator TempArena = {0};
        LinearAllocator_Create(UINT16_MAX, ArenaMemory, &TempArena);

        StringList List = String_SplitIntoList(&TempArena, Parameters, ' ', true);

        // TODO: remove the program from the parameters from all the backend.c cmdline strings. match this behviour on windows
        // Args[0] = ProcessExePath.Data;
        char* Args[256] = {0};
        u8 i = 0;
        for each_string_in_list (List)
        {
            String Trimmed = String_TrimQuotes(It.String);
            // stomp on the data
            Trimmed.Data[Trimmed.Length] = 0;
            //LOG("%S", Trimmed);

            Args[i] = (char*)Trimmed.Data;
            i++;
            if (i == 255)
            {
                break;
            }
        }

        i32 Result = execve((char*)ProcessExePath.Data, Args, EnvArgs.Length == 0 ? environ : envp);
        exit(Result);
    }

    return pid;
}

PlatformHandle Platform_RunCommand(const String CmdLine, const String WorkingDirectory, const String EnvBlock)
{
    String Command;
    StringLocal(Copy, Kibibytes(32));
    if (WorkingDirectory.Length > 0)
    {
        String_Append(&Copy, S("cd \""));
        String_Append(&Copy, WorkingDirectory);
        (void)String_EatPathSeparatorsInlineFromEnd(&Copy);
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
    else if (pid > 0) // parent path
    {
        return pid;
    }
    else // child path
    {
        StringLocal(EnvArgs, 4096);
        char* envp[256] = { NULL };

        if (EnvBlock.Length > 0)
        {
            u32 EnvCount = 0;
            while (environ[EnvCount] != NULL)
            {
                String_Append(&EnvArgs, CStrEx(environ[EnvCount], 4096));
                String_AppendChar(&EnvArgs, '\0');
                EnvCount++;
            }

            String_Append(&EnvArgs, EnvBlock);

            u32 i = 0;
            u32 Offset = 0;
            for (u32 j = 0; j < EnvArgs.Length; j++)
            {
                if (EnvArgs.Data[j] == '\0')
                {
                    envp[i] = (char*)&EnvArgs.Data[Offset];
                    Offset = j+1;
                    i++;
                }
            }
        }

        execve("/bin/sh", (char*[]){"sh", "-c", (char*)Command.Data, NULL}, EnvArgs.Length == 0 ? environ : envp);
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
        (void)String_EatPathSeparatorsInlineFromEnd(&Copy);
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

        execvp("/bin/sh", (char*[]){"sh", "-c", (char*)Command.Data, NULL});
        exit(0);
    }

    return pid;
}

bool Platform_SetWorkingDirectory(const String Path)
{
    StringLocal(Copy, MAX_PATH_LENGTH);
    String_Copy(&Copy, Path);

    i32 Result = chdir((const char*)Path.Data);
    if (Result < 0)
    {
        LogLastError(String_Null());
        return false;
    }

    return Result >= 0;
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

            DIR* dir = opendir((const char*)P.Data);
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
                    stat((const char*)FullPath.Data, &filestat);
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

    i32 PidStatus = 0;
    // if you call this twice on the same pid, linux wont return the
    // same exit code like windows does... sadge :(
    pid_t pid = waitpid(Handle, &PidStatus, 0);
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

    i32 PidStatus = 0;
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

usize Platform_GetCriticalSectionMemoryRequirement(void)
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

bool Platform_IsRunningAsAdmin(void)
{
    uid_t euid = geteuid();
    return euid == 0;
}

SystemTime Platform_GetSystemLocalTime(void)
{
    time_t mytime = time(0);
    ctime(&mytime);

    struct tm lt = {0};

    localtime_r(&mytime, &lt);

    SystemTime t;
    t.Year = (u16)lt.tm_year + 1900;
    t.Month = (u16)lt.tm_mon + 1;
    t.DayOfWeek = (u16)lt.tm_mday/7;
    t.Day = (u16)lt.tm_mday;
    t.Hour = (u16)lt.tm_hour;
    t.Minute = (u16)lt.tm_min;
    t.Second = (u16)lt.tm_sec;
    t.Millisecond = 0;

    return t;
}

bool Platform_GetTimeZone(String* OutTimeZone)
{
    time_t mytime = time(0);
    ctime(&mytime);

    struct tm lt = {0};

    localtime_r(&mytime, &lt);

    String_Copy(OutTimeZone, CStr(lt.tm_zone));

    return true;
}

bool Platform_TerminateProcess(PlatformHandle Handle, u32 ExitCode)
{
    return kill(Handle, SIGKILL) == 0;
}

u64 Platform_GetMainThreadID(void)
{
    return (u64)getpid();
}

void Platform_GetWorkingDirectory(String* OutPath)
{
    char* Result = getcwd((char*)OutPath->Data, MAX_PATH_LENGTH);
    if (Result == NULL) return;
    OutPath->Length = String_GetLength_Ex((const char*)OutPath->Data, MAX_PATH_LENGTH);
}

bool Platform_GetEnvironmentVariableValue(String Name, String* OutVariable)
{
    StringLocal(NameCopy, MAX_PATH_LENGTH);
    String_Copy(&NameCopy, Name);

    char* Value = getenv((const char*)NameCopy.Data);
    if (Value == NULL)
    {
        return false;
    }

    String_Copy(OutVariable, CStrEx(Value, OutVariable->Capacity));
    return true;
}

bool Platform_DoesEnvironmentVariableExist(String Name)
{
    StringLocal(NameCopy, MAX_PATH_LENGTH);
    String_Copy(&NameCopy, Name);

    char* Value = getenv((const char*)NameCopy.Data);
    if (Value == NULL)
    {
        return false;
    }

    return true;
}

u32 Platform_GetNumLogicalProcessors(void)
{
    u32 NumProcessors = (u32)sysconf(_SC_NPROCESSORS_ONLN);
    return NumProcessors;
}

bool Platform_GetAccountName(String* OutName)
{
    struct passwd pwd = {0};
    struct passwd* result = NULL;
    char Buffer[4096] = {0};

    getpwuid_r(getuid(), &pwd, Buffer, 4096, &result);
    if (result == NULL)
    {
        StringLocal(Message, 512);
        String_Format(&Message, S("Failed to get user name"));
        LogLastError(Message);
        return false;
    }

    String_Copy(OutName, CStr(pwd.pw_name));
    return true;
}

bool Platform_GetUserName(String* OutName)
{
    struct passwd pwd = {0};
    struct passwd* result = NULL;
    char Buffer[4096] = {0};

    getpwuid_r(getuid(), &pwd, Buffer, 4096, &result);
    if (result == NULL)
    {
        StringLocal(Prefix, MAX_PATH_LENGTH);
        String_Format(&Prefix, S("Failed to get user directory"));
        LogLastError(Prefix);
        return false;
    }

    const String Directory = CStrEx(pwd.pw_dir, MAX_PATH_LENGTH);

    u32 LastSlash = 0;
    if (String_IndexOfLastPathSlash(Directory, &LastSlash))
    {
        String_Copy(OutName, StrShiftF(Directory, LastSlash+1));
    }
    else
    {
        String_Copy(OutName, Directory);
    }

    return true;
}

void Platform_GetComputerName(String* OutName)
{
    char HostName[256] = {0};
    i32 Result = gethostname(HostName, sizeof(HostName));
    if (Result == 0)
    {
        String_Copy(OutName, CStrEx(HostName, 255));
    }
    else
    {
        String_Copy(OutName, S("__unknown__"));
    }
}

// TODO: test and verify
bool Platform_GetUserDirectory(String* OutDirectory)
{
    // check the passwd database first as it is the real user's home directory
    struct passwd pwd = {0};
    struct passwd* result = NULL;
    char Buffer[4096] = {0};

    int Error = getpwuid_r(getuid(), &pwd, Buffer, sizeof(Buffer), &result);
    if (Error == 0 && result)
    {
        if (pwd.pw_dir)
        {
            String_Copy(OutDirectory, CStrEx(pwd.pw_dir, MAX_PATH_LENGTH));
            return true;
        }
    }

    return false;
}

void Platform_GetHomeDirectory(String* OutDirectory)
{
    // read the env var
    {
        StringLocal(Result, MAX_PATH_LENGTH);
        if (Platform_GetEnvironmentVariableValue(S("HOME"), &Result))
        {
            if (String_IsValid(Result))
            {
                if (access((char*)Result.Data, F_OK))
                {
                    String_Copy(OutDirectory, Result);
                    return;
                }
            }
        }
    }

    // Fallback: $TMPDIR or /tmp
    {
        StringLocal(Result, MAX_PATH_LENGTH);
        if (Platform_GetEnvironmentVariableValue(S("TMPDIR"), &Result))
        {
            if (String_IsValid(Result))
            {
                if (access((char*)Result.Data, F_OK))
                {
                    String_Copy(OutDirectory, Result);
                    return;
                }
            }
        }

        if (access("/tmp", F_OK))
        {
            String_Copy(OutDirectory, S("/tmp"));
            return;
        }
    }

    // Final fallback: use the current working directory
    {
        Platform_GetWorkingDirectory(OutDirectory);
    }
}

bool Platform_GetCurrentProcessName(String* OutName)
{
    // todo: from cmdline args
    UNIMPLEMENTED;
    return false;
}

u64 Platform_GetCurrentProcessID(void)
{
    return (u64)getpid();
}

u32 Platform_GetConsoleProcessCount(void)
{
    return 0;
}

bool Filesystem_Open(const String FilePath, EFileMode Mode, FileHandle* OutHandle)
{
    if (NEVER(OutHandle == NULL)) return false;

    String ModeStr;

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
        // TODO
        //LOG_WARNING("Invalid mode passed (%u) while trying to open file \"%S\"", Mode, FilePath);
        return false;
    }

    u32 LastSlash = 0;
    (void)String_IndexOfLastPathSlash(FilePath, &LastSlash);

    StringLocal(Copy, MAX_PATH_LENGTH);
    String_Copy(&Copy, StrSlice(FilePath.Data, LastSlash));

    (void)Filesystem_OpenDirectory(Copy);

    FILE* File = fopen((const char*)FilePath.Data, (const char*)ModeStr.Data); // refactor to just use open() instead of fopen()
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
        String_Format(&Message, S("Failed to open file %S -> \"%S\""), ModeString, FilePath);
        LogLastError(Message);
        return false;
    }

    OutHandle->Data  = File;
    OutHandle->Data2 = NULL;

#if PLATFORM_OPEN_BSD
    String Path = StrMake(OutHandle->Path);
    String_Copy(&Path, FilePath);
    OutHandle->Path.Length = Path.Length;
#endif

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
    StringLocal(Cmd, MAX_PATH_LENGTH);
    String_Append(&Cmd, S("rm -f \""));
    String_Append(&Cmd, FilePath);
    String_AppendChar(&Cmd, '"');
    String_Append(&Cmd, S(" 2> /dev/null"));
    i32 Result = system((const char*)Cmd.Data);
    return Result == 0;
}

bool Filesystem_Open_MemoryMapped(const String FilePath, EFileMode Mode, FileHandle* OutHandle, u8** OutData, usize* OutSize)
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

    usize Size = 0;
    (void)Filesystem_GetFileSize(*OutHandle, &Size);

    void* Address = mmap(NULL, Size, ProtectFlags, MAP_SHARED, fileno((FILE*)OutHandle->Data), 0);

    if (Address == MAP_FAILED)
    {
        StringLocal(Prefix, 512);
        String_Format(&Prefix, S("Failed to memory map file \"%S\""), FilePath);
        LogLastError(Prefix);
        return false;
    }

    OutHandle->Data2 = Address;

    if (OutData)
        *OutData = (u8*)Address;

    if (OutSize) 
        *OutSize = Size;

    return true;
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
                    i32 ErrorCode = mkdir((const char*)BaseDirectory.Data, 0700);
                    if (ErrorCode == -1)
                    {
                        StringLocal(Prefix, MAX_PATH_LENGTH);
                        String_Format(&Prefix, S("Failed to open directory \"%S\""), BaseDirectory);
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
    return Filesystem_OpenDirectory(FilePath);
}

void Filesystem_Close(FileHandle* Handle)
{
    if (NEVER(Handle == NULL))
        return;

    //bool bFailedUnmap = false;
    if (Handle->Data2 && Handle->Data2 != g_FileHandle.Data2)
    {
        Handle->Data2 = NULL;

        usize Size = 0;
        (void)Filesystem_GetFileSize(*Handle, &Size);

        if (munmap(Handle->Data2, Size) == -1)
        {
            StringLocal(Path, MAX_PATH_LENGTH);
            (void)Filesystem_GetFilePath(*Handle, &Path);

            StringLocal(Prefix, 512);
            String_Format(&Prefix, S("Failed to unmap memory for file \"%S\""), Path);
            LogLastError(Prefix);
            //bFailedUnmap = true;
        }
    }

    if (Handle->Data && IsValidFileHandle(*Handle))
    {
        fclose(Handle->Data);
        *Handle = FileHandle_Null();
        //return !bFailedUnmap;
    }
    
    return; //false;
}

bool Filesystem_Seek(const FileHandle Handle, isize Offset)
{
    i32 ErrorCode = fseek(Handle.Data, Offset, SEEK_CUR);
    if (ErrorCode == -1)
    {
        StringLocal(Path, MAX_PATH_LENGTH);
        (void)Filesystem_GetFilePath(Handle, &Path);

        StringLocal(Prefix, 512);
        String_Format(&Prefix, S("Seek failed for \"%S\""), Path);
        LogLastError(Prefix);
        return false;
    }

    return true;
}

bool Filesystem_SeekFromBeginning(const FileHandle Handle, usize Offset)
{
    i32 ErrorCode = fseek(Handle.Data, (i32)Offset, SEEK_SET);
    if (ErrorCode == -1)
    {
        StringLocal(Path, MAX_PATH_LENGTH);
        (void)Filesystem_GetFilePath(Handle, &Path);

        StringLocal(Prefix, 512);
        String_Format(&Prefix, S("SeekFromBeginning failed for \"%S\""), Path);
        LogLastError(Prefix);
        return false;
    }

    return true;
}

bool Filesystem_SeekFromEnd(const FileHandle Handle, usize Offset)
{
    i32 ErrorCode = fseek(Handle.Data, (i32)Offset, SEEK_END);
    if (ErrorCode == -1)
    {
        StringLocal(Path, MAX_PATH_LENGTH);
        (void)Filesystem_GetFilePath(Handle, &Path);

        StringLocal(Prefix, 512);
        String_Format(&Prefix, S("SeekFromEnd failed for \"%S\""), Path);
        LogLastError(Prefix);
        return false;
    }

    return true;
}

bool Filesystem_SeekToBeginning(const FileHandle Handle)
{
    i32 ErrorCode = fseek(Handle.Data, 0, SEEK_SET);
    if (ErrorCode == -1)
    {
        StringLocal(Path, MAX_PATH_LENGTH);
        (void)Filesystem_GetFilePath(Handle, &Path);

        StringLocal(Prefix, 512);
        String_Format(&Prefix, S("SeekToBeginning failed for \"%S\""), Path);
        LogLastError(Prefix);
        return false;
    }

    return true;
}

bool Filesystem_SeekToEnd(const FileHandle Handle)
{
    i32 ErrorCode = fseek(Handle.Data, 0, SEEK_END);
    if (ErrorCode == -1)
    {
        StringLocal(Path, MAX_PATH_LENGTH);
        (void)Filesystem_GetFilePath(Handle, &Path);

        StringLocal(Prefix, 512);
        String_Format(&Prefix, S("SeekToEnd failed for \"%S\""), Path);
        LogLastError(Prefix);
        return false;
    }

    return true;
}

usize Filesystem_GetCurrentFilePosition(const FileHandle Handle)
{
    return (usize)ftell(Handle.Data);
}

usize Filesystem_GetLastWriteTime(const String FilePath)
{
    if (!Filesystem_DoesFileExist(FilePath))
        return 0;

    struct stat FileStat = {0};
    i32 ErrorCode = stat((const char*)FilePath.Data, &FileStat);
    if (ErrorCode == -1)
    {
        StringLocal(Prefix, MAX_PATH_LENGTH);
        String_Format(&Prefix, S("Failed to retrieve last write time for file \"%S\""), FilePath);
        LogLastError(Prefix);
        return 0;
    }

    return (usize)FileStat.st_mtime;
}

usize Filesystem_GetLastAccessTime(const String FilePath)
{
    if (!Filesystem_DoesFileExist(FilePath))
        return 0;

    struct stat FileStat = {0};
    i32 ErrorCode = stat((const char*)FilePath.Data, &FileStat);
    if (ErrorCode == -1)
    {
        StringLocal(Prefix, MAX_PATH_LENGTH);
        String_Format(&Prefix, S("Failed to retrieve last access time for file \"%S\""), FilePath);
        LogLastError(Prefix);
        return 0;
    }

    return (usize)FileStat.st_atime;
}

FileTimeData Filesystem_GetFileTime(const String FilePath)
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

usize Filesystem_GetCreationTime(const String FilePath)
{
    if (!Filesystem_DoesFileExist(FilePath))
        return 0;

    struct stat FileStat = {0};
    i32 ErrorCode = stat((const char*)FilePath.Data, &FileStat);
    if (ErrorCode == -1)
    {
        StringLocal(Prefix, MAX_PATH_LENGTH);
        String_Format(&Prefix, S("Failed to retrieve creation time for file \"%S\""), FilePath);
        LogLastError(Prefix);
        return 0;
    }

    return (usize)birthtime(FileStat);
}

usize Filesystem_GetLastWriteTimeH(const FileHandle Handle)
{
    //fstat(fileno(Handle->Data), &FileStat);
    // TODO: something better
    StringLocal(Path, MAX_PATH_LENGTH);
    (void)Filesystem_GetFilePath(Handle, &Path);

    return Filesystem_GetLastWriteTime(Path);
}

usize Filesystem_GetLastAccessTimeH(const FileHandle Handle)
{
    StringLocal(Path, MAX_PATH_LENGTH);
    (void)Filesystem_GetFilePath(Handle, &Path);

    return Filesystem_GetLastAccessTime(Path);
}

usize Filesystem_GetCreationTimeH(const FileHandle Handle)
{
    StringLocal(Path, MAX_PATH_LENGTH);
    (void)Filesystem_GetFilePath(Handle, &Path);

    return Filesystem_GetCreationTime(Path);
}

FileTimeData Filesystem_GetFileTimeH(const FileHandle Handle)
{
    StringLocal(Path, MAX_PATH_LENGTH);
    (void)Filesystem_GetFilePath(Handle, &Path);

    FileTimeData a = {0};
    a.CreationTime = Filesystem_GetCreationTime(Path);
    a.LastAccessTime = Filesystem_GetLastAccessTime(Path);
    a.LastWriteTime = Filesystem_GetLastWriteTime(Path);
    return a;
}

bool Filesystem_ReadPipe(PlatformPipe Handle, usize DataSize, void* OutData, usize* OutBytesRead)
{
    if (NEVER(Handle[0] == -1)) return false;
    if (NEVER(Handle[1] == -1)) return false;

    isize BytesRead = read(Handle[0], OutData, DataSize);
    if (BytesRead < 0)
    {
        StringLocal(Prefix, MAX_PATH_LENGTH);
        String_Format(&Prefix, S("Failed to read pipe from handle -> Read: %i | Write: %i"), Handle[0], Handle[1]);
        LogLastError(Prefix);
        return false;
    }

    if (OutBytesRead)
        *OutBytesRead = (usize)BytesRead;

    return true;
}

bool Filesystem_Read(const FileHandle Handle, usize DataSize, void* OutData, usize* OutBytesRead)
{
    if (NEVER(!IsValidFileHandle(Handle))) return false;

    usize BytesRead = fread(OutData, 1, DataSize, Handle.Data);
    if (BytesRead == 0)
    {
        StringLocal(Path, MAX_PATH_LENGTH);
        (void)Filesystem_GetFilePath(Handle, &Path);

        StringLocal(Prefix, MAX_PATH_LENGTH);
        String_Format(&Prefix, S("Failed to read file \"%S\""), Path);
        LogLastError(Prefix);
        return false;
    }

    if (OutBytesRead)
        *OutBytesRead = BytesRead;

    return true;
}

bool Filesystem_ReadEntireFile(const FileHandle Handle, void* OutData, usize* OutBytesRead)
{
    if (NEVER(!IsValidFileHandle(Handle))) return false;

    // todo: remove this?
    (void)Filesystem_SeekToBeginning(Handle);
    
    usize Size = 0;
    (void)Filesystem_GetFileSize(Handle, &Size);

    usize BytesRead = fread(OutData, 1, Size, Handle.Data);
    if (BytesRead == 0)
    {
        StringLocal(Path, MAX_PATH_LENGTH);
        (void)Filesystem_GetFilePath(Handle, &Path);

        StringLocal(Prefix, MAX_PATH_LENGTH);
        String_Format(&Prefix, S("Failed to entire read file \"%S\""), Path);
        LogLastError(Prefix);
        return false;
    }

    if (OutBytesRead)
        *OutBytesRead = BytesRead;

    return true;
}

bool Filesystem_ReadLine(const FileHandle Handle, String* LineBuffer)
{
    if (NEVER(!IsValidFileHandle(Handle))) return false;
    if (NEVER(LineBuffer == NULL)) return false;
    if (NEVER(LineBuffer->Data == NULL || LineBuffer->Data == String_Null().Data)) return false;

    usize CurrentPosition = Filesystem_GetCurrentFilePosition(Handle);

    usize FileSize = 0;
    (void)Filesystem_GetFileSize(Handle, &FileSize);
    if (CurrentPosition >= FileSize)
    {
        (void)Filesystem_SeekToBeginning(Handle);
        return false;
    }

    uchar TempBuffer[8192] = {0};
    u64 BytesRead = fread(TempBuffer, 1, 8191, Handle.Data);
    if (BytesRead == 0)
    {
        StringLocal(Path, MAX_PATH_LENGTH);
        (void)Filesystem_GetFilePath(Handle, &Path);

        StringLocal(Prefix, MAX_PATH_LENGTH);
        String_Format(&Prefix, S("Failed to read line for file \"%S\""), Path);
        LogLastError(Prefix);
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

    (void)Filesystem_SeekFromBeginning(Handle, CurrentPosition + FilePointerOffset);

    return true;
}

bool Filesystem_Write(const FileHandle Handle, usize DataSize, const void* Data, usize* OutBytesWritten)
{
    if (NEVER(!IsValidFileHandle(Handle))) return false;
    if (DataSize == 0) return false;

    (void)Filesystem_SeekToBeginning(Handle);

    usize BytesWritten = fwrite(Data, 1, DataSize, Handle.Data);
    if (BytesWritten == 0)
    {
        StringLocal(Path, MAX_PATH_LENGTH);
        (void)Filesystem_GetFilePath(Handle, &Path);

        StringLocal(Prefix, MAX_PATH_LENGTH);
        String_Format(&Prefix, S("Failed to write to file \"%S\""), Path);
        LogLastError(Prefix);
        return false;
    }

    if (OutBytesWritten)
        *OutBytesWritten = BytesWritten;

    return true;
}

bool Filesystem_WriteLine(const FileHandle Handle, const String Text, usize* OutBytesWritten)
{
    if (NEVER(!IsValidFileHandle(Handle))) return false;

    (void)Filesystem_SeekToEnd(Handle);

    usize BytesWritten = fwrite(Text.Data, 1, Text.Length, Handle.Data);
    if (BytesWritten == 0)
    {
        StringLocal(Path, MAX_PATH_LENGTH);
        (void)Filesystem_GetFilePath(Handle, &Path);

        StringLocal(Prefix, MAX_PATH_LENGTH);
        String_Format(&Prefix, S("Failed to write line to file \"%S\""), Path);
        LogLastError(Prefix);
        return false;
    }

    if (OutBytesWritten)
        *OutBytesWritten = BytesWritten;

    return true;
}

bool Filesystem_WriteLineFormatted(const FileHandle Handle, const String Text, usize* OutBytesWritten, ...)
{
    if (NEVER(!IsValidFileHandle(Handle))) return false;

    (void)Filesystem_SeekToEnd(Handle);
    
    va_list Args;
    va_start(Args, OutBytesWritten);
    StringLocal(Buffer, 32768);
    String_FormatV(&Buffer, Text, 32768, Args);
    va_end(Args);

    usize BytesWritten = fwrite(Buffer.Data, 1, Buffer.Length, Handle.Data);
    if (BytesWritten == 0)
    {
        StringLocal(Path, MAX_PATH_LENGTH);
        (void)Filesystem_GetFilePath(Handle, &Path);

        StringLocal(Prefix, MAX_PATH_LENGTH);
        String_Format(&Prefix, S("Failed to write line to file \"%S\""), Path);
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

    if (access((const char*)Copy.Data, F_OK) == 0)
    {
        return true;
    }
    
    return false;
}

bool Filesystem_DoesDirectoryExist(const String FilePath)
{
    bool bSuccess = false;
    DIR* Found = opendir((const char*)FilePath.Data);
    if (Found)
    {
        closedir(Found);
        bSuccess = true;
    }

    return bSuccess;
}

bool Filesystem_GetFileSize(const FileHandle File, usize* OutSize)
{
    bool bSuccess = false;

    StringLocal(Path, MAX_PATH_LENGTH);
    if (Filesystem_GetFilePath(File, &Path))
    {
        struct stat filestat = {0};
        i32 Result = stat((const char*)Path.Data, &filestat);
        bSuccess = Result == 0;

        if (OutSize)
        {
            *OutSize = (usize)filestat.st_size;
        }
    }

    return bSuccess;
}

bool Filesystem_IsPathRelative(const String Path)
{
    return Path.Data[0] != '/';
}

bool Filesystem_IsFile(const String Path)
{
    struct stat filestat = {0};
    stat((const char*)Path.Data, &filestat);
    return S_ISREG(filestat.st_mode);
}

bool Filesystem_IsDirectory(const String Path)
{
    struct stat filestat = {0};
    stat((const char*)Path.Data, &filestat);
    return S_ISDIR(filestat.st_mode);
}

bool Filesystem_IsHidden(const String Path)
{
    // On Unix systems, a file is considered hidden if its name starts with a dot (.)
    // There is no separate hidden attribute in the file metadata.

    bool bHidden = false;

    String FileName = Filesystem_ExtractFileName(Path, true);
    if (String_IsFirst(FileName, '.'))
    {
        bHidden = true;
    }

    return bHidden;
}

bool Filesystem_ConvertRelativeToAbsolutePath(String* OutFullPath)
{
    StringLocal(Copy, MAX_PATH_LENGTH);
    String_Copy(&Copy, *OutFullPath);

    bool bLastIsSeparator = OutFullPath->Data[OutFullPath->Length-1] == '/';

    char* Result = realpath((const char*)Copy.Data, (char*)OutFullPath->Data);
    if (Result == NULL)
    {
        String_Copy(OutFullPath, Copy);
        return false;
    }

    OutFullPath->Length = String_GetLength_Ex(Result, MAX_PATH_LENGTH);

    // realpath() doesnt append a '/' (even if the original string had that)
    if (bLastIsSeparator) String_AppendPathSeparator_Checked(OutFullPath);

    return true;
}

static bool Internal_IterateDirectory(const String BasePath, const String DirectoryPath, DirectoryIterator Callback, bool bRecursive, void* UserData)
{
    const String RealBasePath = BasePath.Length == 0 ? S(".") : BasePath;
    
    struct dirent* entry = NULL;
    DIR* dp = opendir((const char*)RealBasePath.Data);
    if (!dp)
    {
        return true;
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
            stat((const char*)FullPath.Data, &filestat);
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
    
    closedir(dp); // this piece of shit malloc's
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

// todo: remove system() calls
// todo: error handling
bool Filesystem_DeleteFiles(const String FilePath, const String Wildcard, bool bRecursive)
{
    // TODO: do not use the shell for this
    if (FilePath.Length == 0 || (FilePath.Length == 1 && FilePath.Data[0] == '/' ))
        return false;

    StringLocal(Cmd, MAX_PATH_LENGTH);
    String_Append(&Cmd, S("rm -f "));
    if (bRecursive)
    {
        String_Append(&Cmd, S("-r \""));
    }
    String_Append(&Cmd, FilePath);
    String_AppendPathSeparator_Checked(&Cmd);
    String_AppendChar(&Cmd, '"');
    String_Append(&Cmd, Wildcard);
    String_Append(&Cmd, S(" 2> /dev/null"));
    i32 Result = system((const char*)Cmd.Data);
    return Result == 0;
}

bool Filesystem_DeleteDirectory(const String DirectoryPath)
{
    StringLocal(Cmd, MAX_PATH_LENGTH);
    String_Append(&Cmd, S("rm -r \""));
    String_Append(&Cmd, DirectoryPath);
    String_AppendPathSeparator_Checked(&Cmd);
    String_Append(&Cmd, S("\" 2> /dev/null"));
    i32 Result = system((const char*)Cmd.Data);
    return Result == 0;
}

bool Filesystem_Copy(const String Source, const String Destination)
{
    StringLocal(Cmd, MAX_PATH_LENGTH);
    String_Append(&Cmd, S("cp -r \""));
    String_Append(&Cmd, Source);
    String_AppendChar(&Cmd, '"');
    String_AppendSpace(&Cmd);
    String_AppendChar(&Cmd, '"');
    String_Append(&Cmd, Destination);
    String_AppendChar(&Cmd, '"');
    String_Append(&Cmd, S(" 2> /dev/null"));
    i32 Result = system((const char*)Cmd.Data);
    return Result == 0;
}

bool Filesystem_Move(const String Source, const String Destination, bool bRename)
{
    StringLocal(Cmd, MAX_PATH_LENGTH);
    String_Append(&Cmd, S("mv \""));
    String_Append(&Cmd, Source);
    String_AppendChar(&Cmd, '"');
    String_AppendSpace(&Cmd);
    String_AppendChar(&Cmd, '"');
    String_Append(&Cmd, Destination);
    String_AppendChar(&Cmd, '"');
    String_Append(&Cmd, S(" 2> /dev/null"));
    i32 Result = system((const char*)Cmd.Data);
    return Result == 0;
}

bool Filesystem_ArePathsCommon(String PathA, String PathB)
{
    bool bPrefixMatch = String_StartsWith(PathB, PathA, true);
    
    return bPrefixMatch;
}

bool Platform_GetTerminalDimensions(u32* OutRows, u32* OutColumns)
{
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == -1)
    {
        return false;
    }

    *OutRows = w.ws_row;
    *OutColumns = w.ws_col;

    return true;
}

u32 Platform_GetPosixVersion(void)
{
    #ifdef _POSIX_VERSION
    return _POSIX_VERSION;
    #else
    return 0;
    #endif
}

static FileHandle DevUrandomFile = {0};

i32 Rand(void)
{
    if (!IsValidFileHandle(DevUrandomFile))
    {
        xx Filesystem_Open(S("/dev/urandom"), FileMode_Read, &DevUrandomFile);
    }

    u32 Value = 0;
    xx Filesystem_Read(DevUrandomFile, sizeof(Value), &Value, NULL);

    return (i32)(Value & 0x7FFFFFFF); // 31-bit positive int
}

#endif // PLATFORM_UNIX
