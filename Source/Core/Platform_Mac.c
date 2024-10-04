// Copyright (c) 2024 Ali El Saleh

#include "Platform.h"

#if PLATFORM_MAC

#include "Memory.h"
#include "StringUtils.h"
#include "Globals.h"
#include "Uuid.h"
#include "Filesystem.h"
#include "Log.h"

#undef internal
#undef global

#include <mach/mach_time.h>
#include <dispatch/dispatch.h>
#include <CoreFoundation/CoreFoundation.h>

#define internal static
#define global extern

#include <stdio.h>
#include <pwd.h>
#include <errno.h>
#include <sys/syslimits.h>
#include <sys/sysctl.h>

#ifndef NO_LOG 
internal void LogLastError(const String Prefix)
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
    dispatch_semaphore_t semaphore = dispatch_semaphore_create(1);
    dispatch_semaphore_wait(semaphore, DISPATCH_TIME_NOW);

    OutMutex->Handle = semaphore;
    OutMutex->ID = -1;
    OutMutex->Name = String_Null();

    return true;
}

internal bool Internal_TryLockFile(int fd, pid_t* OutPID)
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

internal bool Internal_TryUnlockFile(i32 fd)
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

    i32 fd = open(Temp.Data, O_CREAT | O_RDWR, 0666);
    if (fd == -1)
    {
        StringLocal(Prefix, 512);
        String_Format(&Prefix, S("Failed to temporary lock file %S"), Prefix.Capacity, Temp);
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
        dispatch_semaphore_signal(Mutex->Handle);
    }

    return true;
}

f64 Platform_GetAbsoluteTime(void)
{
    mach_timebase_info_data_t info;
    mach_timebase_info(&info);

    u64 Time = mach_absolute_time();
    f64 Nano = ((f64)Time * (f64)info.numer) / (f64)info.denom;

    return Nano/1.0e9;
}

void Platform_Sleep(f64 ms)
{
    if (ms > 0)
    {
        mach_timebase_info_data_t info;
        mach_timebase_info(&info);

        u64 Time = mach_absolute_time();
        f64 Start = (((f64)Time * (f64)info.numer) / (f64)info.denom) / 1.0e-9; // 1e-9

        f64 Target = ms/1000.0;

        while (1)
        {
            mach_timebase_info(&info);

            Time = mach_absolute_time();
            f64 Now = (((f64)Time * (f64)info.numer) / (f64)info.denom) / 1.0e-9; // 1e-9

            if ((Now-Start) >= Target)
                break;
        }
    }
}

u64 Platform_GetCurrentThreadID(void)
{
    UNIMPLEMENTED;
    return 0;
}

bool Filesystem_GetFilePath(const FileHandle Handle, String* OutPath)
{
    if (!IsValidFileHandle(Handle))
        return false;

    char Path[PATH_MAX] = {0};
    if (fcntl(fileno(Handle.Data), F_GETPATH, Path) == -1)
    {
        StringLocal(Prefix, MAX_PATH_LENGTH);
        String_Format(&Prefix, S("Failed to retrieve file path for file handle"), Prefix.Capacity);
        LogLastError(Prefix);
        return false;
    }

    String_Copy(OutPath, CStrEx(Path, MAX_PATH_LENGTH));
    return true;
}

bool Platform_GetCpuBrandName(String* OutName)
{
    char Vendor[128] = {0};
    size_t Size = sizeof(Vendor);
    i32 Result = sysctlbyname("machdep.cpu.brand_string", Vendor, &Size, NULL, 0);
    if (Result == -1)
    {
        return false;
    }

    String_Copy(OutName, CStrEx(Vendor, 127));
    return true;
}

bool Platform_GetFullCpuName(String* OutName)
{
    return Platform_GetCpuBrandName(OutName);
}

u32 Platform_GetCpuCacheLineSize(void)
{
    i64 LineSize = 0;
    size_t Size = sizeof(LineSize);
    i32 Result = sysctlbyname("hw.cachelinesize", &LineSize, &Size, NULL, 0);
    if (Result == -1)
    {
        return 0;
    }

    return (u32)LineSize;
}

PlatformVersion Platform_GetVersion(void)
{
    PlatformVersion Result = {0};

    // Path to the system version plist
    CFURLRef url = CFURLCreateWithFileSystemPath(kCFAllocatorDefault,
            CFSTR("/System/Library/CoreServices/SystemVersion.plist"),
            kCFURLPOSIXPathStyle, false);

    CFReadStreamRef stream = CFReadStreamCreateWithFile(kCFAllocatorDefault, url);
    CFRelease(url);

    if (!stream || !CFReadStreamOpen(stream))
    {
        return Result;
    }

    CFPropertyListRef plist = CFPropertyListCreateWithStream(kCFAllocatorDefault, stream, 0, kCFPropertyListImmutable, NULL, NULL);
    CFReadStreamClose(stream);
    CFRelease(stream);

    if (!plist || CFGetTypeID(plist) != CFDictionaryGetTypeID())
    {
        return Result;
    }

    CFDictionaryRef dict = (CFDictionaryRef)plist;
    CFStringRef versionString = CFDictionaryGetValue(dict, CFSTR("ProductVersion"));

    if (versionString)
    {
        StringLocal(Version, 32);
        if (CFStringGetCString(versionString, Version.Data, Version.Capacity, kCFStringEncodingUTF8))
        {
            sscanf(Version.Data, "%d.%d.%d", &Result.Major, &Result.Minor, &Result.Patch);
        }
    }

    CFRelease(plist);

    return Result;
}

Uuid UUID_Generate(void)
{
    uuid_t id;
    uuid_generate(id);

    return *(Uuid*)id;
}

bool UUID_IsEqual(Uuid First, Uuid Second)
{
    unsigned char* a = (unsigned char*)&First;
    unsigned char* b = (unsigned char*)&Second;

    const bool bSame = uuid_compare(a, b) == 0;
    return bSame;
}

void UUID_ToString(Uuid ID, String* OutString)
{
    StringLocal(Temp, GUID_LENGTH);

    unsigned char* a = (unsigned char*)&ID;
    uuid_unparse(a, Temp.Data);

    String_Copy(OutString, Temp);
}

Uuid UUID_FromString(const String IDString)
{
    uuid_t id;
    uuid_parse(IDString.Data, id);

    return *(Uuid*)id;
}

#endif // PLATFORM_MAC
