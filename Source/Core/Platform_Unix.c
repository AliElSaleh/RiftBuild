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

#undef constant // time.h uses this phrase

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
#define _BSD_SOURCE

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
#include <poll.h>
#include <fcntl.h>

#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/syscall.h>

#include <stdarg.h>

#if PLATFORM_LINUX || PLATFORM_BSD
static EUnixDesktopEnvironment gDesktopEnvironment = Desktop_Unknown;
#endif

#ifndef NO_LOG 
static void LogLastError(const String Prefix)
{
    StringLocal(Message, 4096);
    String_Copy(&Message, CStr(strerror(errno)));

    //LOG_ERROR("%S\n        errno %i\n        Reason: %S\n", Prefix, errno, Message);

    StringLocal(FormattedMessage, 4096);
    String_Format(&FormattedMessage, S("%S\n        Error Code: %i\n        Reason: %S\n"), Prefix, errno, Message);
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
    
    #if PLATFORM_LINUX || PLATFORM_BSD
    StringLocal(DesktopEnvName, 128);
    Platform_DetectDesktopEnvironment(&DesktopEnvName, NULL, NULL);
    if (String_Contains(DesktopEnvName, S("KDE"), false))
    {
        gDesktopEnvironment = Desktop_KDE;
    }
    else if (String_Contains(DesktopEnvName, S("GNOME"), false))
    {
        gDesktopEnvironment = Desktop_Gnome;
    }
    else if (String_Contains(DesktopEnvName, S("Cinnamon"), false))
    {
        gDesktopEnvironment = Desktop_Cinnamon;
    }
    else
    {
    }
    #endif
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
        GArgV[i-1].Length = String_GetLength_N(argv[i], UINT16_MAX);
        GArgV[i-1].Capacity = GArgV[i-1].Length;
    }

    GProgramName.Data = (uchar*)argv[0];
    GProgramName.Length = String_GetLength_N(argv[0], UINT16_MAX);
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
    Platform_ConsoleWrite_CustomLength(Message, String_GetLength(Message), Color, bIsError);
}

static void Internal_WriteToFd(int Fd, const void* Data, usize Length)
{
    const u8* Bytes = (const u8*)Data;
    usize Written = 0;
    bool bFailed = false;

    while (Written < Length && !bFailed)
    {
        isize Result = write(Fd, Bytes + Written, Length - Written);
        if (Result > 0)
        {
            Written += (usize)Result;
        }
        else if (Result < 0 && errno == EINTR)
        {
            // interrupted before anything was written - just retry
        }
        else
        {
            bFailed = true; // closed or broken output; nowhere to report it, stop trying
        }
    }
}

void Platform_ConsoleWrite_CustomLength(const char* Message, u32 Length, u8 Color, bool bIsError)
{
    static String colors[] = {S("0;37"), S("0;32"), S("1;33"), S("1;31"), S("0;41"), S("0;37"), S("0;37")};

    // Errors to stderr, everything else to stdout (mirrors the Windows path).
    int Fd = bIsError ? STDERR_FILENO : STDOUT_FILENO;

    bool bDone = (Length == 0);
    if (!bDone)
    {
        bool bIgnoreNewLine = (Color == 4) && (Length > 0) && (Message[Length-1] == '\n');
        if (UNLIKELY(bIgnoreNewLine)) { Length--; }

        // write() delivers bytes the same to a tty, pipe or file, so output is never lost when
        // redirected (unlike Windows' WriteConsole). ANSI color escapes, however, only belong on a
        // real terminal -- writing them into a pipe or file would pollute the captured text -- so we
        // only wrap the message in color when the target fd is actually a tty. The escape prefix,
        // payload and reset go out as separate writes on purpose: no staging buffer means no size
        // limit, and we are the console's only writer, so nothing interleaves between the calls.
        bool bIsTTY = isatty(Fd) != 0;

        if (bIsTTY)
        {
            StringLocal(Prefix, 16);
            String_Append(&Prefix, S("\033["));
            String_Append(&Prefix, LIKELY(Color < 6) ? colors[Color] : S("0;37"));
            String_Append(&Prefix, S("m"));

            Internal_WriteToFd(Fd, Prefix.Data, Prefix.Length);
            Internal_WriteToFd(Fd, Message, Length);
            Internal_WriteToFd(Fd, "\033[0m", 4);
        }
        else
        {
            Internal_WriteToFd(Fd, Message, Length);
        }

        if (UNLIKELY(bIgnoreNewLine)) { Internal_WriteToFd(Fd, "\n", 1); }
    }
}

bool Platform_IsConsoleOutput(void)
{
    return isatty(STDOUT_FILENO) != 0;
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

PlatformHandle Platform_RunProcess_Ex(const String ProcessExePath, const String Parameters, const String WorkingDirectory, PlatformPipe* StdOutPipe)
{
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
        (*StdOutPipe)[0] = PipeData[0]; // read pipe
        (*StdOutPipe)[1] = PipeData[1]; // write pipe
    }
    else // child path
    {
        bool bChangeSuccess = Platform_SetWorkingDirectory(WorkingDirectory);
        if (!bChangeSuccess)
        {
            exit(1);
            return 1;
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

        dup2(PipeData[1], STDOUT_FILENO); // capture stdout
        dup2(PipeData[1], STDERR_FILENO); // capture stderr as well

        // the dup2'd copies live on; the originals would only keep the pipe from ever hitting EOF
        close(PipeData[0]);
        close(PipeData[1]);

        i32 Result = execve((char*)ProcessExePath.Data, Args, environ);
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
        (*StdOutPipe)[0] = PipeData[0]; // read pipe
        (*StdOutPipe)[1] = PipeData[1]; // write pipe
    }
    else // child path
    {
        dup2(PipeData[1], STDOUT_FILENO); // capture stdout
        dup2(PipeData[1], STDERR_FILENO); // capture stderr as well

        // the dup2'd copies live on; the originals would only keep the pipe from ever hitting EOF
        close(PipeData[0]);
        close(PipeData[1]);

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

const String* Platform_GetExecutableExtensions(u32* OutCount)
{
    // Unix has no executable file extensions — runnability is decided by the file mode bits.
    if (OutCount)
    {
        *OutCount = 0;
    }

    return NULL;
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

bool Platform_GetExitCodeForProcess(PlatformHandle Handle, u32* OutExitCode)
{
    // Non-blocking: reports whether the process has exited, and only then fills OutExitCode.
    // Note this reaps the child - a later wait on the same pid reports exit code 0.
    bool bExited = true;
    u32 Code = 0;

    i32 PidStatus = 0;
    pid_t pid = waitpid(Handle, &PidStatus, WNOHANG);
    if (pid == 0)
    {
        bExited = false;
    }
    else if (pid < 0)
    {
        Code = 0; // already reaped or not our child - the blocking waits report 0 here too
    }
    else if (WIFSIGNALED(PidStatus))
    {
        Code = 128 + (u32)WTERMSIG(PidStatus); // a crashed compiler must not read as success
    }
    else
    {
        Code = (u32)WEXITSTATUS(PidStatus);
    }

    if (OutExitCode && bExited)
    {
        *OutExitCode = Code;
    }

    return bExited;
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
    if (Platform_IsValidHandle(Handle))
    {
        close(Handle);
    }
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
    t.Year        = (u16)lt.tm_year + 1900;
    t.Month       = (u16)lt.tm_mon + 1;
    t.Week        = (u16)lt.tm_yday / 7;
    t.DayOfWeek   = (u16)lt.tm_wday;
    t.DayOfYear   = (u16)lt.tm_yday;
    t.Day         = (u16)lt.tm_mday;
    t.Hour        = (u16)lt.tm_hour;
    t.Minute      = (u16)lt.tm_min;
    t.Second      = (u16)lt.tm_sec;
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
    OutPath->Length = String_GetLength_N((const char*)OutPath->Data, MAX_PATH_LENGTH);
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

bool Platform_RemoveEnvironmentVariable(String Name)
{
    StringLocal(NameCopy, MAX_PATH_LENGTH);
    String_Copy(&NameCopy, Name);

    return unsetenv((const char*)NameCopy.Data) == 0;
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

#if !PLATFORM_MAC
void Platform_GetFriendlyComputerName(String* OutName)
{
    Platform_GetComputerName(OutName);
}
#endif

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
    // query the user database
    {
        struct passwd* pw = getpwuid(getuid());
        if (pw && pw->pw_dir)
        {
            String_Copy(OutDirectory, CStrEx(pw->pw_dir, MAX_PATH_LENGTH));
            return;
        }
    }

    // Fallback: read the env var
    {
        StringLocal(Result, MAX_PATH_LENGTH);
        if (Platform_GetEnvironmentVariableValue(S("HOME"), &Result))
        {
            if (String_IsValid(Result))
            {
                // if (access((char*)Result.Data, F_OK))
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
                // if (access((char*)Result.Data, F_OK))
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
    bool bSuccess = false;

    #if PLATFORM_LINUX
    // /proc/self/exe is a symlink to the running executable's full path
    char Path[MAX_PATH_LENGTH] = {0};
    ssize_t Len = readlink("/proc/self/exe", Path, sizeof(Path) - 1);
    if (Len > 0)
    {
        String FullPath = StrSlice((uchar*)Path, (u32)Len);

        u32 LastSlash = 0;
        if (String_IndexOfLastPathSlash(FullPath, &LastSlash))
        {
            String_Copy(OutName, StrShiftF(FullPath, LastSlash + 1));
        }
        else
        {
            String_Copy(OutName, FullPath);
        }

        bSuccess = true;
    }
    #else
    // macOS and the BSDs (including OpenBSD, which has no executable-path API)
    // provide the program name directly
    const char* ProgName = getprogname();
    if (ProgName != NULL)
    {
        String_Copy(OutName, CStrEx(ProgName, 4096));
        bSuccess = true;
    }
    #endif

    return bSuccess;
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
        /*
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
        */

        return false;
    }

    OutHandle->Data = File;

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
    StringLocal(Copy, MAX_PATH_LENGTH);
    String_Copy(&Copy, FilePath);

    return unlink((const char*)Copy.Data) == 0;
}

bool Filesystem_Open_MemoryMapped(const String FilePath, EFileMode Mode, MemoryMappedFile* OutFile)
{
    if (NEVER(OutFile == NULL))
    {
        return false;
    }

    *OutFile = (MemoryMappedFile){0};

    FileHandle TempHandle = {0};
    if (!Filesystem_Open(FilePath, Mode, &TempHandle))
    {
        return false;
    }

    int ProtectFlags = 0;

    if (((Mode & FileMode_Read) != 0) && ((Mode & FileMode_Write) != 0))
    {
        ProtectFlags = PROT_READ | PROT_WRITE;
    }
    else if (((Mode & FileMode_Read) != 0) && ((Mode & FileMode_Write) == 0))
    {
        ProtectFlags = PROT_READ;
    }
    else if (((Mode & FileMode_Read) == 0) && ((Mode & FileMode_Write) != 0))
    {
        ProtectFlags = PROT_WRITE;
    }

    usize Size = 0;
    (void)Filesystem_GetFileSize(TempHandle, &Size);

    void* Address = mmap(NULL, Size, ProtectFlags, MAP_SHARED, fileno((FILE*)TempHandle.Data), 0);

    if (Address == MAP_FAILED)
    {
        StringLocal(Prefix, 512);
        String_Format(&Prefix, S("Failed to memory map file \"%S\""), FilePath);
        LogLastError(Prefix);
        Filesystem_Close(&TempHandle);
        return false;
    }

    OutFile->Handle  = TempHandle.Data;
    OutFile->Mapping = Address;
    OutFile->Data    = (u8*)Address;
    OutFile->Size    = Size;

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
    {
        return;
    }

    if (Handle->Data && IsValidFileHandle(*Handle))
    {
        fclose(Handle->Data);
        *Handle = FileHandle_Null();
    }
}

void Filesystem_Close_MemoryMapped(MemoryMappedFile* File)
{
    if (File)
    {
        if (File->Mapping)
        {
            munmap(File->Mapping, File->Size);
        }
        if (File->Handle)
        {
            fclose(File->Handle);
        }
        *File = (MemoryMappedFile){0};
    }
}

bool Filesystem_Seek(const FileHandle Handle, isize Offset)
{
    i32 ErrorCode = fseek(Handle.Data, Offset, SEEK_CUR);
    if (ErrorCode == -1)
    {
        //LogLastError(S("Seek failed"));
        return false;
    }

    return true;
}

bool Filesystem_SeekFromBeginning(const FileHandle Handle, usize Offset)
{
    i32 ErrorCode = fseek(Handle.Data, (i32)Offset, SEEK_SET);
    if (ErrorCode == -1)
    {
        //LogLastError(S("SeekFromBeginning failed"));
        return false;
    }

    return true;
}

bool Filesystem_SeekFromEnd(const FileHandle Handle, usize Offset)
{
    i32 ErrorCode = fseek(Handle.Data, (i32)Offset, SEEK_END);
    if (ErrorCode == -1)
    {
        //LogLastError(S("SeekFromEnd failed"));
        return false;
    }

    return true;
}

bool Filesystem_SeekToBeginning(const FileHandle Handle)
{
    i32 ErrorCode = fseek(Handle.Data, 0, SEEK_SET);
    if (ErrorCode == -1)
    {
        //LogLastError(S("SeekToBeginning failed"));
        return false;
    }

    return true;
}

bool Filesystem_SeekToEnd(const FileHandle Handle)
{
    i32 ErrorCode = fseek(Handle.Data, 0, SEEK_END);
    if (ErrorCode == -1)
    {
        //LogLastError(S("SeekToEnd failed"));
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
    usize Result = 0;
    struct stat FileStat = {0};
    if (fstat(fileno((FILE*)Handle.Data), &FileStat) != -1)
    {
        Result = (usize)FileStat.st_mtime;
    }

    return Result;
}

usize Filesystem_GetLastAccessTimeH(const FileHandle Handle)
{
    usize Result = 0;
    struct stat FileStat = {0};
    if (fstat(fileno((FILE*)Handle.Data), &FileStat) != -1)
    {
        Result = (usize)FileStat.st_atime;
    }

    return Result;
}

usize Filesystem_GetCreationTimeH(const FileHandle Handle)
{
    usize Result = 0;
    struct stat FileStat = {0};
    if (fstat(fileno((FILE*)Handle.Data), &FileStat) != -1)
    {
        Result = (usize)birthtime(FileStat);
    }

    return Result;
}

FileTimeData Filesystem_GetFileTimeH(const FileHandle Handle)
{
    FileTimeData a = {0};
    struct stat FileStat = {0};
    if (fstat(fileno((FILE*)Handle.Data), &FileStat) != -1)
    {
        a.CreationTime = (usize)birthtime(FileStat);
        a.LastAccessTime = (usize)FileStat.st_atime;
        a.LastWriteTime = (usize)FileStat.st_mtime;
    }
    return a;
}

bool Filesystem_ReadPipe(PlatformPipe Handle, usize DataSize, void* OutData, usize* OutBytesRead)
{
    // Non-blocking: reads only what is already buffered in the pipe. A true return with zero bytes
    // read means "no data right now"; a false return means the pipe is finished (every writer
    // closed it and the buffer is drained) and will never produce data again.
    usize TotalRead = 0;

    struct pollfd PipePoll = {0};
    PipePoll.fd = Handle[0];
    PipePoll.events = POLLIN;

    bool bAlive = Platform_IsValidHandle(Handle[0]) && poll(&PipePoll, 1, 0) >= 0;
    if (bAlive)
    {
        if (PipePoll.revents & POLLIN)
        {
            isize BytesRead = read(Handle[0], OutData, DataSize);
            if (BytesRead > 0)
            {
                TotalRead = (usize)BytesRead;
            }
            else
            {
                bAlive = false; // 0 is EOF, negative is a dead pipe
            }
        }
        else if (PipePoll.revents & (POLLHUP | POLLERR | POLLNVAL))
        {
            bAlive = false; // no data left and no writers to produce more
        }
    }

    if (OutBytesRead)
    {
        *OutBytesRead = TotalRead;
    }

    return bAlive;
}

bool Filesystem_Read(const FileHandle Handle, usize DataSize, void* OutData, usize* OutBytesRead)
{
    if (NEVER(!IsValidFileHandle(Handle))) return false;

    usize BytesRead = fread(OutData, 1, DataSize, Handle.Data);
    if (BytesRead == 0)
    {
        //LogLastError(S("Failed to read file"));
        return false;
    }

    if (OutBytesRead)
        *OutBytesRead = BytesRead;

    return true;
}

bool Filesystem_ReadEntireFile(const FileHandle Handle, void* OutData, usize* OutBytesRead)
{
    if (NEVER(!IsValidFileHandle(Handle))) return false;

    (void)Filesystem_SeekToBeginning(Handle);
    
    usize Size = 0;
    (void)Filesystem_GetFileSize(Handle, &Size);

    usize BytesRead = fread(OutData, 1, Size, Handle.Data);
    if (BytesRead == 0)
    {
        //LogLastError(S("Failed to read entire file"));
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
        //LogLastError(S("Failed to read line from file"));
        return false;
    }

    u32 Counter = 0;
    u32 FilePointerOffset = 0;
    for (u32 i = 0; i < BytesRead; i++)
    {
        if (TempBuffer[i] == '\0' || TempBuffer[i] == '\n' || TempBuffer[i] == '\r')
        {
            FilePointerOffset = Counter;

            if (TempBuffer[i] == '\r' && i + 1 < BytesRead && TempBuffer[i+1] == '\n')
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
        //LogLastError(S("Failed to write to file"));
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
        //LogLastError(S("Failed to write line to file"));
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
    String_FormatV(&Buffer, Text, Args);
    va_end(Args);

    usize BytesWritten = fwrite(Buffer.Data, 1, Buffer.Length, Handle.Data);
    if (BytesWritten == 0)
    {
        //LogLastError(S("Failed to write formatted line to file"));
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

    bool bSuccess = false;

    struct stat st = {0};
    if (stat((const char*)Copy.Data, &st) == 0)
    {
        bSuccess = S_ISREG(st.st_mode);
    }

    return bSuccess;

    /*
    if (access((const char*)Copy.Data, F_OK) == 0)
    {
        return true;
    }
    
    return false;
    */
}

bool Filesystem_DoesDirectoryExist(const String FilePath)
{
    StringLocal(Copy, MAX_PATH_LENGTH);
    String_Copy(&Copy, FilePath);

    bool bSuccess = false;

    struct stat st = {0};
    if (stat((const char*)Copy.Data, &st) == 0)
    {
        bSuccess = S_ISDIR(st.st_mode);
    }

    return bSuccess;

    /*
    DIR* Found = opendir((const char*)FilePath.Data);
    if (Found)
    {
        closedir(Found);
        bSuccess = true;
    }

    return bSuccess;
    */
}

bool Filesystem_GetFileSize(const FileHandle File, usize* OutSize)
{
    struct stat FileStat = {0};
    i32 Result = fstat(fileno((FILE*)File.Data), &FileStat);
    bool bSuccess = Result == 0;

    if (OutSize)
    {
        *OutSize = (usize)FileStat.st_size;
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

    OutFullPath->Length = String_GetLength_N(Result, MAX_PATH_LENGTH);

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

bool Filesystem_DeleteFiles(const String FilePath, const String Wildcard, bool bRecursive)
{
    // Deletes only files; matched directories are descended into (when recursive), never removed.
    // Returns whether at least one file was actually deleted.
    bool bAnyFilesDeleted = false;

    // refuse to sweep an empty path or the filesystem root
    bool bValidPath = FilePath.Length > 0 && !(FilePath.Length == 1 && FilePath.Data[0] == '/');
    if (bValidPath)
    {
        StringLocal(PathCopy, MAX_PATH_LENGTH);
        String_Copy(&PathCopy, FilePath);

        DIR* dp = opendir((const char*)PathCopy.Data);
        if (dp != NULL)
        {
            struct dirent* Entry = NULL;
            while ((Entry = readdir(dp)))
            {
                if (Entry->d_name[0] == '.' &&
                    (!Entry->d_name[1] || (Entry->d_name[1] == '.' && !Entry->d_name[2])))
                {
                    continue;
                }

                const String EntryName = CStr(Entry->d_name);
                if (!String_MatchesWildcard(EntryName, Wildcard, true))
                {
                    continue;
                }

                StringLocal(FullPath, MAX_PATH_LENGTH);
                String_BuildPath(&FullPath, FilePath, EntryName);

                // lstat so symlinks are unlinked as files, never followed
                struct stat EntryStat = {0};
                bool bIsDirectory = lstat((const char*)FullPath.Data, &EntryStat) == 0 && S_ISDIR(EntryStat.st_mode);

                if (bIsDirectory)
                {
                    if (bRecursive)
                    {
                        xx Filesystem_DeleteFiles(FullPath, Wildcard, true);
                    }
                }
                else
                {
                    if (unlink((const char*)FullPath.Data) == 0)
                    {
                        bAnyFilesDeleted = true;
                    }
                }
            }

            closedir(dp);
        }
    }

    return bAnyFilesDeleted;
}

bool Filesystem_DeleteDirectory(const String DirectoryPath)
{
    StringLocal(PathCopy, MAX_PATH_LENGTH);
    String_Copy(&PathCopy, DirectoryPath);

    DIR* dp = opendir((const char*)PathCopy.Data);
    if (dp != NULL)
    {
        struct dirent* Entry = NULL;
        while ((Entry = readdir(dp)))
        {
            if (Entry->d_name[0] == '.' &&
                (!Entry->d_name[1] || (Entry->d_name[1] == '.' && !Entry->d_name[2])))
            {
                continue;
            }

            const String EntryName = CStr(Entry->d_name);

            StringLocal(FullPath, MAX_PATH_LENGTH);
            String_BuildPath(&FullPath, DirectoryPath, EntryName);

            // lstat so symlinks are unlinked as files, never followed
            struct stat EntryStat = {0};
            bool bIsDirectory = lstat((const char*)FullPath.Data, &EntryStat) == 0 && S_ISDIR(EntryStat.st_mode);

            if (bIsDirectory)
            {
                xx Filesystem_DeleteDirectory(FullPath);
            }
            else
            {
                xx unlink((const char*)FullPath.Data);
            }
        }

        closedir(dp);
    }

    return rmdir((const char*)PathCopy.Data) == 0;
}

static bool Internal_CopyFileContents(const String Source, const String Destination)
{
    bool bSuccess = false;

    int SourceFd = open((const char*)Source.Data, O_RDONLY);
    if (SourceFd >= 0)
    {
        struct stat SourceStat = {0};
        if (fstat(SourceFd, &SourceStat) == 0)
        {
            int DestFd = open((const char*)Destination.Data, O_WRONLY | O_CREAT | O_TRUNC, SourceStat.st_mode & 0777);
            if (DestFd >= 0)
            {
                bSuccess = true;

                u8 Buffer[Kibibytes(64)];
                while (bSuccess)
                {
                    isize BytesRead = read(SourceFd, Buffer, sizeof(Buffer));
                    if (BytesRead == 0)
                    {
                        break; // end of source
                    }

                    if (BytesRead < 0)
                    {
                        if (errno != EINTR)
                        {
                            bSuccess = false;
                        }
                        continue;
                    }

                    isize Written = 0;
                    while (Written < BytesRead && bSuccess)
                    {
                        isize Result = write(DestFd, Buffer + Written, (usize)(BytesRead - Written));
                        if (Result > 0)
                        {
                            Written += Result;
                        }
                        else if (Result < 0 && errno == EINTR)
                        {
                            // interrupted before anything was written - just retry
                        }
                        else
                        {
                            bSuccess = false;
                        }
                    }
                }

                // the destination may have pre-existed with different mode bits, and the umask filters
                // the mode passed to open() - either way a copied executable must stay executable
                xx fchmod(DestFd, SourceStat.st_mode & 0777);

                close(DestFd);
            }
        }

        close(SourceFd);
    }

    return bSuccess;
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
        StringLocal(Destination, MAX_PATH_LENGTH);
        String_BuildPath(&Destination, Meta->DestinationDirectory, RelativePath);

        xx Filesystem_OpenDirectory(Destination);
    }
    else
    {
        StringLocal(Source, MAX_PATH_LENGTH);
        String_BuildPath(&Source, FullPath);

        StringLocal(Destination, MAX_PATH_LENGTH);
        String_BuildPath(&Destination, Meta->DestinationDirectory, RelativePath);

        bResult = Internal_CopyFileContents(Source, Destination);
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

bool Filesystem_Copy(const String Source, const String Destination)
{
    StringLocal(SourceCopy, MAX_PATH_LENGTH);
    StringLocal(DestinationCopy, MAX_PATH_LENGTH);
    String_Copy(&SourceCopy, Source);
    String_Copy(&DestinationCopy, Destination);

    bool bResult = false;

    // the source must exist
    if (!(Filesystem_IsFile(SourceCopy) || Filesystem_IsDirectory(SourceCopy)))
    {
        StringLocal(Msg, 512);
        String_Format(&Msg, S("\n    Source path \"%S\" does not exist.\n"), SourceCopy);
        Platform_ConsoleWrite_CustomLength((const char*)Msg.Data, Msg.Length, 3, true);
    }
    else
    {
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

        // recursively copy all files within the source directory to the destination directory
        if (Filesystem_IsDirectory(SourceCopy))
        {
            // it is an error to try to copy a directory into a file
            if (Filesystem_IsFile(DestinationCopy))
            {
                StringLocal(Msg, 1024);
                String_Format(&Msg, S("Source \"%S\" can not be copied into destination \"%S\" because the destination is a file. You likely have the two mixed up."), SourceCopy, DestinationCopy);
                Platform_ConsoleWrite_CustomLength((const char*)Msg.Data, Msg.Length, 3, true);
            }
            else
            {
                xx Filesystem_OpenDirectory(DestinationCopy);

                CopyDirectoryMetadata Meta = {DestinationCopy, true};
                Filesystem_IterateDirectory_Ex(SourceCopy, Internal_CopyFilesToDirectory_Recursive, true, &Meta);

                bResult = (bool)Meta.bSuccess;
            }
        }
        // copy single file to the destination directory
        else
        {
            bResult = Internal_CopyFileContents(SourceCopy, DestinationCopy);
            if (bResult == false)
            {
                StringLocal(Msg, 512);
                String_Format(&Msg, S("Failed to copy \"%S\" to \"%S\""), Source, Destination);
                LogLastError(Msg);
            }
        }
    }

    return bResult;
}

bool Filesystem_Move(const String Source, const String Destination, bool bRename)
{
    StringLocal(SourceCopy, MAX_PATH_LENGTH);
    StringLocal(DestinationCopy, MAX_PATH_LENGTH);
    String_Copy(&SourceCopy, Source);
    String_Copy(&DestinationCopy, Destination);

    if (!bRename)
    {
        u32 LastSlash = 0;
        xx String_IndexOfLastPathSlash(SourceCopy, &LastSlash);

        const String FileName = StrShiftF(SourceCopy, LastSlash);
        if (!String_EndsWith(DestinationCopy, FileName, false))
        {
            String_BuildPath(&DestinationCopy, FileName);
        }

        // try to create the directory if it doesn't exist
        LastSlash = 0;
        bool bHasSlash = String_IndexOfLastPathSlash(DestinationCopy, &LastSlash);
        xx Filesystem_OpenDirectory(bHasSlash ? StrSlice(DestinationCopy.Data, LastSlash) : DestinationCopy);
    }

    bool bResult = rename((const char*)SourceCopy.Data, (const char*)DestinationCopy.Data) == 0;

    // rename() cannot cross filesystems; fall back to copy + delete like mv does
    if (!bResult && errno == EXDEV)
    {
        if (Filesystem_IsDirectory(SourceCopy))
        {
            xx Filesystem_OpenDirectory(DestinationCopy);

            CopyDirectoryMetadata Meta = {DestinationCopy, true};
            Filesystem_IterateDirectory_Ex(SourceCopy, Internal_CopyFilesToDirectory_Recursive, true, &Meta);

            bResult = (bool)Meta.bSuccess && Filesystem_DeleteDirectory(SourceCopy);
        }
        else
        {
            bResult = Internal_CopyFileContents(SourceCopy, DestinationCopy);
            if (bResult)
            {
                bResult = unlink((const char*)SourceCopy.Data) == 0;
            }
        }
    }

    if (!bResult)
    {
        StringLocal(Msg, 512);
        String_Format(&Msg, S("Failed to %S \"%S\" to \"%S\""), bRename ? S("rename") : S("move"), Source, Destination);
        LogLastError(Msg);
    }

    return bResult;
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

#if PLATFORM_BSD || PLATFORM_LINUX
void Platform_DetectDesktopEnvironment(String* DesktopEnv, String* DesktopSession, String* SessionType)
{
    // detect desktop environment
    if (DesktopEnv)
    {
        xx Platform_GetEnvironmentVariableValue(S("XDG_CURRENT_DESKTOP"), DesktopEnv);
        
        // in case we return "distro:env". just get the "env" part
        u32 Colon = 0;
        if (String_IndexOfChar(*DesktopEnv, ':', &Colon))
        {
            *DesktopEnv = StrShiftF(*DesktopEnv, Colon+1);
        }
    }

    if (DesktopSession)
    {
        xx Platform_GetEnvironmentVariableValue(S("DESKTOP_SESSION"), DesktopSession);
    }

    // detect wayland or x11
    if (SessionType)
    {
        bool bIsWaylandDisplay = Platform_DoesEnvironmentVariableExist(S("WAYLAND_DISPLAY"));
        
        StringLocal(XdgSession, 128);
        xx Platform_GetEnvironmentVariableValue(S("XDG_SESSION_TYPE"), &XdgSession);
        {
            if (bIsWaylandDisplay || String_IsEqual(XdgSession, S("wayland"), false))
            {
                String_Copy(SessionType, S("Wayland"));
            }
            else if (String_IsEqual(XdgSession, S("x11"), false))
            {
                String_Copy(SessionType, S("X11"));
            }
            else if (XdgSession.Length > 0)
            {
                String_Copy(SessionType, XdgSession);
            }
            else if (Platform_DoesEnvironmentVariableExist(S("DISPLAY")))
            {
                // XDG_SESSION_TYPE is set by systemd-logind, so it does not exist on the BSDs or
                // non-systemd Linux. A set DISPLAY still means an X server session.
                String_Copy(SessionType, S("X11"));
            }
            else
            {
                String_Copy(SessionType, S("Unknown"));
            }
        }
    }
}

void Platform_DetectDistro(String* DistroName, String* PrettyName, String* ID)
{
    bool bGotFromOsRelease = false;

    FileHandle f = {0};
    if (Filesystem_Open(S("/etc/os-release"), FileMode_Read, &f))
    {
        bool bGotName = !DistroName, bGotPretty = !PrettyName, bGotID = !ID;

        StringLocal(Line, 128);
        while (Filesystem_ReadLine(f, &Line))
        {
            if (DistroName && String_StartsWith(Line, S("NAME=\""), false))
            {
                String Name = StrShiftF(Line, 6);
                Name = String_EatCharFromEnd(Name, '"');

                String_Copy(DistroName, Name);
                bGotName = true;
                bGotFromOsRelease = true;
            }
            else if (PrettyName && String_StartsWith(Line, S("PRETTY_NAME=\""), false))
            {
                String Name = StrShiftF(Line, 13);
                Name = String_EatCharFromEnd(Name, '"');
                
                String_Copy(PrettyName, Name);
                bGotPretty = true;
                bGotFromOsRelease = true;
            }
            else if (ID && String_StartsWith(Line, S("ID="), false))
            {
                String Name = StrShiftF(Line, 3);

                String_Copy(ID, Name);
                bGotID = true;
                bGotFromOsRelease = true;
            }
            
            if (bGotName && bGotPretty && bGotID)
            {
                break;
            }
        }

        Filesystem_Close(&f);
    }


    // fallback just in case, look in the Linux Standard Base release file
    if (!bGotFromOsRelease)
    {
        f = (FileHandle){0};
        if (Filesystem_Open(S("/etc/lsb-release"), FileMode_Read, &f))
        {
            StringLocal(Line, 128);
            while (Filesystem_ReadLine(f, &Line))
            {
                if (String_StartsWith(Line, S("DISTRIB_ID="), false))
                {
                    String Name = StrShiftF(Line, 11);

                    if (DistroName)
                    {
                        String_Copy(DistroName, Name);
                    }

                    if (PrettyName)
                    {
                        String_Copy(PrettyName, Name);
                    }

                    if (ID)
                    {
                        String_Copy(ID, Name);
                    }

                    break;
                }
            }

            Filesystem_Close(&f);
        }
    }
}

NO_DISCARD EUnixDesktopEnvironment Platform_GetDesktopEnvironment(void)
{
    return gDesktopEnvironment;
}

NO_DISCARD bool Platform_DesktopIsGnome(void)
{
    return gDesktopEnvironment == Desktop_Gnome;
}

NO_DISCARD bool Platform_DesktopIsKDE(void)
{
    return gDesktopEnvironment == Desktop_KDE;
}

NO_DISCARD bool Platform_DesktopIsCinnamon(void)
{
    return gDesktopEnvironment == Desktop_Cinnamon;
}
#endif

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
