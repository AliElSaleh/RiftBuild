// Copyright (c) Artisan Softworks
// Licensed under the BSD 3-Clause License. See the LICENSE file for details.

#include "Platform.h"

#if PLATFORM_LINUX

#ifndef UNITY_BUILD
#include "Log.h"

#include "Filesystem.h"
#include "StringUtils.h"
#include "Array.h"
#endif

#undef constant // time.h uses this phrase

#define _XOPEN_SOURCE 700

PRAGMA_DISABLE_WARNINGS

#if COMPILER_GCC
#pragma GCC diagnostic ignored "-Wcpp"
#endif

#include <signal.h>

PRAGMA_ENABLE_WARNINGS

#include <stdio.h>

#define _LARGEFILE64_SOURCE
#define _FILE_OFFSET_BITS 64
#define __USE_FILE_OFFSET64
#define __USE_GNU
#define __USE_MISC
#define _BSD_SOURCE

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
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <spawn.h>
#include <termios.h>
#include <semaphore.h>
#include <stdarg.h>

#include <sys/utsname.h>

extern int fileno (FILE *__stream) NO_THROW NO_DISCARD;

#ifndef NO_LOG 
static void LogLastError(const String Prefix)
{
    StringLocal(Message, 4096);
    String_Copy(&Message, CStr(strerror(errno)));

    LOG_ERROR("%S\n        errno %i\n        Reason: %S\n", Prefix, errno, Message);
}
#else
#define LogLastError(...)
#endif

bool Platform_CreateMutex(PlatformMutex* OutMutex)
{
    sem_t* Semaphore = malloc(sizeof(sem_t));
    sem_init(Semaphore, 0, 1); // 0 for thread-shared semaphore
    if (sem_trywait(Semaphore) == -1)
    {
        return false;
    }

    OutMutex->Handle = Semaphore;
    OutMutex->ID = -1;
    OutMutex->Name = String_Null();

    return true;
}

static bool Internal_TryLockFile(int fd, pid_t* OutPID)
{
    struct flock lock = {0};
    lock.l_type       = F_WRLCK;  // Write lock (exclusive)
    lock.l_whence     = SEEK_SET;
    lock.l_start      = 0;
    lock.l_len        = 0;        // Lock the entire file

    if (fcntl(fd, F_SETLK, &lock) == -1)
    {
        // The lock is held by another process
        if (OutPID)
        {
            fcntl(fd, F_GETLK, &lock);
            *OutPID = lock.l_pid;
        }

        return false;
    }

    return true;
}

static bool Internal_TryUnlockFile(i32 fd)
{
    struct flock lock = {0};
    lock.l_type       = F_UNLCK;
    lock.l_whence     = SEEK_SET;
    lock.l_start      = 0;
    lock.l_len        = 0;        // Unlock the entire file

    if (fcntl(fd, F_SETLK, &lock) == -1)
    {
        return false;
    }

    return true;
}

bool Platform_CreateNamedMutex(const String Name, PlatformMutex* OutMutex)
{
    if (NEVER(Name.Length == 0) || NEVER(OutMutex == NULL))
    {
        return false;
    }

    StringLocal(Temp, MAX_PATH_LENGTH);
    String_Append(&Temp, S("/tmp/lock_"));
    String_Append(&Temp, Name);

    i32 fd = open((const char*)Temp.Data, O_CREAT | O_RDWR, 0666);
    if (fd == -1)
    {
        StringLocal(Prefix, 512);
        String_Format(&Prefix, S("Failed to temporary lock file %S"), Temp);
        LogLastError(Prefix);
        return false;
    }

    pid_t PID = -1;
    if (Internal_TryLockFile(fd, &PID))
    {
        OutMutex->Handle = NULL;
        OutMutex->ID = fd;
        OutMutex->Name = Name;
        return true;
    }

    OutMutex->Handle = NULL;
    OutMutex->ID = PID;
    OutMutex->Name = Name;

    close(fd);
    return false;
}

bool Platform_ReleaseMutex(PlatformMutex* Mutex)
{
    if (NEVER(Mutex == NULL)) return false;

    // release based on type of mutex (named or unnamed)
    if (String_IsValid(Mutex->Name))
    {
        if (!Internal_TryUnlockFile(Mutex->ID))
        {
            close(Mutex->ID);
            return false;
        }

        close(Mutex->ID);
    }
    else
    {
        sem_post(Mutex->Handle);

        if (sem_close(Mutex->Handle) == -1)
        {
            StringLocal(Prefix, 512);
            String_Format(&Prefix, S("Failed to release mutex %S"), Mutex->Name);
            LogLastError(Prefix);
            return false;
        }

        if (sem_destroy(Mutex->Handle) == -1)
        {
            return false;
        }

        free(Mutex->Handle);
        Mutex->Handle = NULL;
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

void Platform_Wait(f64 ms)
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

void Platform_Sleep(u32 ms)
{
    sleep(ms);
}

u64 Platform_GetCurrentThreadID(void)
{
    u32 x = (u32)syscall(__NR_gettid);
    return x;
}

bool Filesystem_GetFilePath(const FileHandle Handle, String* OutPath)
{
    if (!IsValidFileHandle(Handle))
    {
        return false;
    }

    char Path[PATH_MAX] = {0};

    const i32 fd = fileno(Handle.Data);
    snprintf(Path, PATH_MAX, "/proc/self/fd/%d", fd);

    ssize_t Result = readlink(Path, (char*)OutPath->Data, OutPath->Capacity);
    if (Result == -1)
    {
        LogLastError(S("Failed to retrieve file path for file handle"));
        return false;
    }

    OutPath->Length = (u32)Result;
    return true;
}

bool Platform_GetFullCpuName(String* OutName)
{
    bool bFound = false;

    FileHandle f = FileHandle_Null();

    if (Filesystem_Open(S("/proc/cpuinfo"), FileMode_Read, &f))
    {
        StringLocal(Line, 256);
        while (Filesystem_ReadLine(f, &Line))
        {
            if (String_StartsWith(Line, S("model name"), false))
            {
                u32 Colon = 0;
                if (String_IndexOfChar(Line, ':', &Colon))
                {
                    bFound = true;

                    String_Copy(OutName, StrShiftF(Line, Colon+2)); // +2 because there is a space after :
                }

                break;
            }
        }

        Filesystem_Close(&f);
    }

    return bFound;
}

PlatformVersion Platform_GetVersion(void)
{
    PlatformVersion Result = {0};

    struct utsname VersionInfo = {0};
    if (uname(&VersionInfo) == 0)
    {
        (void)sscanf(VersionInfo.release, "%d.%d.%d", &Result.Major, &Result.Minor, &Result.Patch);
    }

    return Result;
}

u64 Platform_GetTotalRam(void)
{
    u64 TotalBytes = 0;

    long Pages    = sysconf(_SC_PHYS_PAGES);
    long PageSize = sysconf(_SC_PAGE_SIZE);
    if (Pages > 0 && PageSize > 0)
    {
        TotalBytes = (u64)Pages * (u64)PageSize;
    }

    return TotalBytes;
}

u32 Platform_GetUpdateBuildRevision(void)
{
    return 0; // no UBR-equivalent on Linux
}

void Platform_GetDisplayVersion(String* OutVersion)
{
    UNUSED_PARAM(OutVersion); // no DisplayVersion-equivalent on Linux
}

void Platform_GetMachineId(String* OutId)
{
    UNUSED_PARAM(OutId); // TODO: /etc/machine-id
}

void Platform_GetDeviceId(String* OutId)
{
    UNUSED_PARAM(OutId); // no Settings "Device ID" equivalent on Linux
}

bool Platform_IsWindowFocused(void)
{
    // no linux implementation
    return true;
}

#endif // PLATFORM_LINUX
