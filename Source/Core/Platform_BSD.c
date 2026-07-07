// Copyright (c) Artisan Softworks
// Licensed under the BSD 3-Clause License. See the LICENSE file for details.

#include "Platform.h"

#if PLATFORM_BSD
#include "Log.h"

#include "Uuid.h"
#include "Filesystem.h"
#include "StringUtils.h"
#include "Array.h"

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
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/sysctl.h>
#include <sys/utsname.h>

#if PLATFORM_FREE_BSD
#include <sys/user.h>
#include <sys/thr.h>
#else
#include <sys/vnode.h>
#endif

#if PLATFORM_NET_BSD
#include <lwp.h>
#endif

#if PLATFORM_OPEN_BSD && (defined(__x86_64__) || defined(__i386__))
#include <cpuid.h>
#endif
#include <spawn.h>
#include <termios.h>
#include <semaphore.h>

// include uuid.h because i dont fucking know why this shit does not work
// #include <uuid.h>

#include <sys/types.h>
#include <sys/uuid.h>

/* Status codes returned by the functions. */
#define	uuid_s_ok                  0
#define	uuid_s_bad_version         1
#define	uuid_s_invalid_string_uuid 2
#define	uuid_s_no_memory           3

__BEGIN_DECLS
int32_t	uuid_compare(const uuid_t *, const uuid_t *, uint32_t *);
void	uuid_create(uuid_t *, uint32_t *);
void	uuid_create_nil(uuid_t *, uint32_t *);
int32_t	uuid_equal(const uuid_t *, const uuid_t *, uint32_t *);
void	uuid_from_string(const char *, uuid_t *, uint32_t *);
uint16_t uuid_hash(const uuid_t *, uint32_t *);
int32_t	uuid_is_nil(const uuid_t *, uint32_t *);
void	uuid_to_string(const uuid_t *, char **, uint32_t *);

void	uuid_enc_le(void *, const uuid_t *);
void	uuid_dec_le(const void *, uuid_t *);
void	uuid_enc_be(void *, const uuid_t *);
void	uuid_dec_be(const void *, uuid_t *);
__END_DECLS

#include <stdarg.h>

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
#if PLATFORM_FREE_BSD
    pthread_mutex_t mutex = {0};
    pthread_mutex_init(&mutex, NULL);
    pthread_mutex_lock(&mutex);

    OutMutex->Handle = mutex;
    OutMutex->ID = -1;
    OutMutex->Name = String_Null();
#else
    sem_t Semaphore = {0};
    sem_init(&Semaphore, 0, 1); // 0 for thread-shared semaphore
    if (sem_trywait(&Semaphore) == -1)
    {
        return false;
    }
#endif

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
        #if PLATFORM_FREE_BSD
        pthread_mutex_unlock(Mutex->Handle);
        pthread_mutex_destroy(Mutex->Handle);
        #else
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
        #endif
    }

    return true;
}


u64 Platform_GetCurrentThreadID(void)
{
    u64 Result = 0;

#if PLATFORM_FREE_BSD
    long ThreadID = 0;
    (void)thr_self(&ThreadID);
    Result = (u64)ThreadID;
#elif PLATFORM_OPEN_BSD
    Result = (u64)getthrid();
#elif PLATFORM_NET_BSD
    Result = (u64)_lwp_self();
#endif

    return Result;
}

bool Filesystem_GetFilePath(const FileHandle Handle, String* OutPath)
{
    if (!IsValidFileHandle(Handle))
    {
        return false;
    }

#if PLATFORM_FREE_BSD
    const i32 fd = fileno((FILE*)Handle.Data);
    struct kinfo_file kinfo = {0};
    kinfo.kf_structsize = sizeof(struct kinfo_file);

    if (fcntl(fd, F_KINFO, &kinfo) == -1)
    {
        StringLocal(Prefix, 512);
        String_Format(&Prefix, S("Failed to retrieve file path for given handle: %i"), fd);
        LogLastError(Prefix);
        return false;
    }

    String_Copy(OutPath, CStr(kinfo.kf_path));
    return true;

#elif PLATFORM_NET_BSD
    const i32 fd = fileno((FILE*)Handle.Data);
    char Path[PATH_MAX] = {0};
    if (fcntl(fd, F_GETPATH, Path) == -1)
    {
        StringLocal(Prefix, MAX_PATH_LENGTH);
        String_Format(&Prefix, S("Failed to retrieve file path for file handle"));
        LogLastError(Prefix);
        return false;
    }

    String_Copy(OutPath, CStrEx(Path, MAX_PATH_LENGTH));
    return true;

// OpenBSD does not support retrieving the path data from a handle :(
// https://marc.info/?l=openbsd-tech&m=164250539119078&w=2
// https://www.mail-archive.com/misc@openbsd.org/msg188221.html
#elif PLATFORM_OPEN_BSD
    // String_Copy(OutPath, StrMake(Handle.Path));
    return false;
#endif

    return false;
}

bool Platform_GetCpuBrandName(String* OutName)
{
    bool bResult = false;

    // hw.model is the CPU brand string on all BSDs, and the MIB form works everywhere
    // (OpenBSD has no sysctlbyname).
    i32 Mib[2] = { CTL_HW, HW_MODEL };
    char Vendor[128] = {0};
    size_t Size = sizeof(Vendor);
    if (sysctl(Mib, 2, Vendor, &Size, NULL, 0) != -1)
    {
        String_Copy(OutName, CStrEx(Vendor, 127));
        bResult = true;
    }

    return bResult;
}

bool Platform_GetFullCpuName(String* OutName)
{
    return Platform_GetCpuBrandName(OutName);
}

u32 Platform_GetCpuCacheLineSize(void)
{
    u32 Result = CACHE_LINE_SIZE;

#if PLATFORM_OPEN_BSD
    // OpenBSD has no sysctl for this; CPUID leaf 1 EBX[15:8] is the CLFLUSH line size in 8-byte units.
    #if defined(__x86_64__) || defined(__i386__)
    u32 Eax = 0, Ebx = 0, Ecx = 0, Edx = 0;
    if (__get_cpuid(1, &Eax, &Ebx, &Ecx, &Edx) != 0)
    {
        const u32 LineSize = ((Ebx >> 8) & 0xFF) * 8;
        if (LineSize != 0)
        {
            Result = LineSize;
        }
    }
    #endif
#else
    i64 LineSize = 0;
    size_t Size = sizeof(LineSize);
    if (sysctlbyname("hw.cachelinesize", &LineSize, &Size, NULL, 0) != -1)
    {
        Result = (u32)LineSize;
    }
#endif

    return Result;
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

#if PLATFORM_OPEN_BSD
    // OpenBSD has no sysctlbyname; use the MIB form. HW_PHYSMEM is a 32-bit int, so use HW_PHYSMEM64.
    i32 Mib[2] = { CTL_HW, HW_PHYSMEM64 };
    i64 MemSize = 0;
    size_t Size = sizeof(MemSize);
    if (sysctl(Mib, 2, &MemSize, &Size, NULL, 0) == 0)
    {
        TotalBytes = (u64)MemSize;
    }
#else
    u64 MemSize = 0;
    size_t Size = sizeof(MemSize);
    if (sysctlbyname("hw.physmem", &MemSize, &Size, NULL, 0) == 0)
    {
        TotalBytes = MemSize;
    }
#endif

    return TotalBytes;
}

u32 Platform_GetUpdateBuildRevision(void)
{
    return 0; // no UBR-equivalent on BSD
}

void Platform_GetDisplayVersion(String* OutVersion)
{
    UNUSED_PARAM(OutVersion); // no DisplayVersion-equivalent on BSD
}

void Platform_GetMachineId(String* OutId)
{
    if (OutId)
    {
        char Buffer[64] = {0};
        size_t Size = sizeof(Buffer);

#if PLATFORM_OPEN_BSD
        // SMBIOS system UUID; may fail on machines whose firmware does not provide one.
        i32 Mib[2] = { CTL_HW, HW_UUID };
        i32 Result = sysctl(Mib, 2, Buffer, &Size, NULL, 0);
#elif PLATFORM_NET_BSD
        i32 Result = sysctlbyname("machdep.dmi.system-uuid", Buffer, &Size, NULL, 0);
#else
        i32 Result = sysctlbyname("kern.hostuuid", Buffer, &Size, NULL, 0);
#endif

        if (Result != -1)
        {
            String_Copy(OutId, CStrEx(Buffer, sizeof(Buffer)));
        }
    }
}

void Platform_GetDeviceId(String* OutId)
{
    UNUSED_PARAM(OutId); // no Settings "Device ID" equivalent on BSD
}

bool Platform_IsWindowFocused(void)
{
    // no BSD implementation
    return true;
}

#endif // PLATFORM_BSD
