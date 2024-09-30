#ifndef _FILESYSTEM_H_
#define _FILESYSTEM_H_

#ifndef UNITY_BUILD
#include "EngineTypes.h"
#endif

#if PLATFORM_WINDOWS
    #define MAX_PATH_LENGTH 260
    #define MAX_PATH_LENGTH_EX 32767
    #define PATH_SEPARATOR '\\'
#elif PLATFORM_LINUX
    #define MAX_PATH_LENGTH 4096
    #define MAX_PATH_LENGTH_EX 4096
    #define PATH_SEPARATOR '/'
#elif PLATFORM_APPLE // todo: subdivide into mac, ios
    #define MAX_PATH_LENGTH 1024
    #define MAX_PATH_LENGTH_EX 1024
    #define PATH_SEPARATOR '/'
#elif PLATFORM_BSD
    #define MAX_PATH_LENGTH 1024
    #define MAX_PATH_LENGTH_EX 1024
    #define PATH_SEPARATOR '/'
#else
    #define MAX_PATH_LENGTH 1024
    #define MAX_PATH_LENGTH_EX 1024
    #define PATH_SEPARATOR '/'
#endif

STRUCT(FileHandle)
{
    void* Data;
    void* Data2;
    bool bBypassSizeCheck; // TODO: move this out of here and make an "option" struct for file handles
#if PLATFORM_OPEN_BSD
    // OpenBSD does not a have a way to get the path of a file descriptor
    // as it is not a part of their design philosophy. This is a workaround.
    // https://marc.info/?l=openbsd-tech&m=164250539119078&w=2
    // https://www.mail-archive.com/misc@openbsd.org/msg188221.html
    StringN(MAX_PATH_LENGTH) Path;
#endif
};

ENUM(EFileMode)
{
    FileMode_Read = 0x1,
    FileMode_Write = 0x2,
};

STRUCT(FileTimeData)
{
    u64 CreationTime;
    u64 LastAccessTime;
    u64 LastWriteTime;
};

typedef bool (*DirectoryIterator)(const String FullPath, const String RelativePath, const String FileName, u64 FileSize, bool bIsDirectory, void* UserData);

RIFT_API bool Filesystem_Open(const String FilePath, u32 Mode, FileHandle* OutHandle);
RIFT_API bool Filesystem_NewFile(const String FilePath);
RIFT_API bool Filesystem_DeleteFile(String FilePath);
RIFT_API bool Filesystem_Open_MemoryMapped(const String FilePath, u32 Mode, FileHandle* OutHandle, u8** OutData, usize* OutSize);
RIFT_API bool Filesystem_OpenDirectory(const String FilePath);
RIFT_API bool Filesystem_OpenDirectory_Ex(const String FilePath, FileHandle* OutHandle);
RIFT_API bool Filesystem_Close(FileHandle* Handle);

RIFT_API bool Filesystem_Seek(const FileHandle Handle, isize Offset);
RIFT_API bool Filesystem_SeekFromBeginning(const FileHandle Handle, usize Offset);
RIFT_API bool Filesystem_SeekFromEnd(const FileHandle Handle, usize Offset);
RIFT_API bool Filesystem_SeekToBeginning(const FileHandle Handle);
RIFT_API bool Filesystem_SeekToEnd(const FileHandle Handle);
RIFT_API usize  Filesystem_GetCurrentFilePosition(const FileHandle Handle);

RIFT_API FileHandle Filesystem_GetStdInputHandle(void);

RIFT_API usize  Filesystem_GetLastWriteTime(const String FilePath);
RIFT_API usize  Filesystem_GetLastAccessTime(const String FilePath);
RIFT_API usize  Filesystem_GetCreationTime(const String FilePath);
RIFT_API usize  Filesystem_GetLastWriteTimeH(const FileHandle Handle);
RIFT_API usize  Filesystem_GetLastAccessTimeH(const FileHandle Handle);
RIFT_API usize  Filesystem_GetCreationTimeH(const FileHandle Handle);
RIFT_API FileTimeData  Filesystem_GetFileTime(const String FilePath);
RIFT_API FileTimeData  Filesystem_GetFileTimeH(const FileHandle Handle);

RIFT_API bool Filesystem_ReadPipe(PlatformPipe Handle, usize DataSize, void* OutData, usize* OutBytesRead);

RIFT_API bool Filesystem_Read(const FileHandle Handle, usize DataSize, void* OutData, usize* OutBytesRead);
RIFT_API bool Filesystem_ReadEntireFile(const FileHandle Handle, void* OutData, usize* OutBytesRead);
RIFT_API bool Filesystem_ReadLine(const FileHandle Handle, String* LineBuffer);

RIFT_API bool Filesystem_Write(const FileHandle Handle, usize DataSize, const void* Data, usize* OutBytesWritten);
RIFT_API bool Filesystem_WriteLine(const FileHandle Handle, const String Text, usize* OutBytesWritten);
RIFT_API bool Filesystem_WriteLineFormatted(const FileHandle Handle, const String Text, usize* OutBytesWritten, ...);

RIFT_API bool Filesystem_DoesFileExist(const String FilePath);
RIFT_API bool Filesystem_DoesDirectoryExist(const String FilePath);

RIFT_API bool Filesystem_GetFilePath(const FileHandle File, String* OutPath);
RIFT_API bool Filesystem_GetFileSize(const FileHandle File, usize* OutSize);

RIFT_API bool Filesystem_IsFile(const String Path);
RIFT_API bool Filesystem_IsDirectory(const String Path);

RIFT_API bool Filesystem_IsNewer(const String PathA, const String PathB);
RIFT_API bool Filesystem_IsOlder(const String PathA, const String PathB);

RIFT_API bool Filesystem_IsPathRelative(const String Path);

RIFT_API bool Filesystem_ConvertRelativeToAbsolutePath(String* OutFullPath);

RIFT_API void Filesystem_IterateDirectory(const String BasePath, DirectoryIterator Callback, bool bRecursive);
RIFT_API void Filesystem_IterateDirectory_Ex(const String BasePath, DirectoryIterator Callback, bool bRecursive, void* UserData);

RIFT_API bool Filesystem_DeleteFiles(const String FilePath, const String Wildcard, bool bRecursive);
RIFT_API bool Filesystem_DeleteDirectory(const String DirectoryPath);

RIFT_API bool Filesystem_Copy(const String Source, const String Destination);
RIFT_API bool Filesystem_Move(const String Source, const String Destination, bool bRename);

RIFT_API bool Filesystem_ArePathsCommon(String PathA, String PathB);

RIFT_API bool IsValidFileHandle(const FileHandle Handle);

RIFT_API FileHandle FileHandle_Null(void);

#endif // _FILESYSTEM_H_
