// Copyright (c) 2024 Ali El Saleh

#include "Platform.h"

#if PLATFORM_BSD
#include "Globals.h"
#include "Log.h"

#include "Uuid.h"
#include "Filesystem.h"
#include "String/StringUtils.h"
#include "Structures/Array.h"

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

#if PLATFORM_FREE_BSD
#include <sys/user.h>
#else
#include <sys/vnode.h>
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
        #if PLATFORM_FREE_BSD
        pthread_mutex_unlock(Mutex->Handle);
        pthread_mutex_destroy(Mutex->Handle);
        #else
        sem_post(Mutex->Handle);

        if (sem_close(Mutex->Handle) == -1)
        {
            StringLocal(Prefix, 512);
            String_Format(&Prefix, S("Failed to release mutex %S"), Prefix.Capacity, Mutex->Name);
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
    UNIMPLEMENTED;
    return 0;
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
        String_Format(&Prefix, S("Failed to retrieve file path for given handle: %i"), 512, fd);
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
        String_Format(&Prefix, S("Failed to retrieve file path for file handle"), Prefix.Capacity);
        LogLastError(Prefix);
        return false;
    }

    String_Copy(OutPath, CStrEx(Path, MAX_PATH_LENGTH));
    return true;

// OpenBSD does not support retrieving the path data from a handle :(
// https://marc.info/?l=openbsd-tech&m=164250539119078&w=2
// https://www.mail-archive.com/misc@openbsd.org/msg188221.html
#elif PLATFORM_OPEN_BSD
    String_Copy(OutPath, StrView(Handle.Path));
    return true;
#endif

    return false;
}

bool Platform_GetCpuBrandName(String* OutName)
{
#if PLATFORM_OPEN_BSD
    // TODO: can't be bothered right now
    // hw.model
    return false;
#else
    char Vendor[128] = {0};
    size_t Size = sizeof(Vendor);
    i32 Result = sysctlbyname("machdep.cpu_brand", Vendor, &Size, NULL, 0);
    if (Result == -1)
    {
        return false;
    }

    String_Copy(OutName, CStrEx(Vendor, 127));
    return true;
#endif
}

bool Platform_GetFullCpuName(String* OutName)
{
    return Platform_GetCpuBrandName(OutName);
}

u32 Platform_GetCpuCacheLineSize(void)
{
#if PLATFORM_OPEN_BSD
    // TODO: can't be bothered right now
    return __CACHE_LINE_SIZE;
#else
    i64 LineSize = 0;
    size_t Size = sizeof(LineSize);
    i32 Result = sysctlbyname("hw.cachelinesize", &LineSize, &Size, NULL, 0);
    if (Result == -1)
    {
        return __CACHE_LINE_SIZE;
    }

    return (u32)LineSize;
#endif
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

#endif // PLATFORM_LINUX
