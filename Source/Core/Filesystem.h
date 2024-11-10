#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#ifndef UNITY_BUILD
#include "EngineTypes.h"
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

RIFT_API NO_DISCARD bool Filesystem_Open(const String FilePath, EFileMode Mode, FileHandle* OutHandle);
RIFT_API NO_DISCARD bool Filesystem_NewFile(const String FilePath);
RIFT_API NO_DISCARD bool Filesystem_DeleteFile(String FilePath);
RIFT_API NO_DISCARD bool Filesystem_Open_MemoryMapped(const String FilePath, EFileMode Mode, FileHandle* OutHandle, u8** OutData, usize* OutSize);
RIFT_API NO_DISCARD bool Filesystem_OpenDirectory(const String FilePath);
RIFT_API NO_DISCARD bool Filesystem_OpenDirectory_Ex(const String FilePath, FileHandle* OutHandle);
RIFT_API            void Filesystem_Close(FileHandle* Handle);

RIFT_API NO_DISCARD bool Filesystem_Seek(const FileHandle Handle, isize Offset);
RIFT_API NO_DISCARD bool Filesystem_SeekFromBeginning(const FileHandle Handle, usize Offset);
RIFT_API NO_DISCARD bool Filesystem_SeekFromEnd(const FileHandle Handle, usize Offset);
RIFT_API NO_DISCARD bool Filesystem_SeekToBeginning(const FileHandle Handle);
RIFT_API NO_DISCARD bool Filesystem_SeekToEnd(const FileHandle Handle);
RIFT_API NO_DISCARD usize Filesystem_GetCurrentFilePosition(const FileHandle Handle);

RIFT_API NO_DISCARD FileHandle Filesystem_GetStdInputHandle(void);

RIFT_API NO_DISCARD usize Filesystem_GetLastWriteTime(const String FilePath);
RIFT_API NO_DISCARD usize Filesystem_GetLastAccessTime(const String FilePath);
RIFT_API NO_DISCARD usize Filesystem_GetCreationTime(const String FilePath);
RIFT_API NO_DISCARD usize Filesystem_GetLastWriteTimeH(const FileHandle Handle);
RIFT_API NO_DISCARD usize Filesystem_GetLastAccessTimeH(const FileHandle Handle);
RIFT_API NO_DISCARD usize Filesystem_GetCreationTimeH(const FileHandle Handle);
RIFT_API NO_DISCARD FileTimeData Filesystem_GetFileTime(const String FilePath);
RIFT_API NO_DISCARD FileTimeData Filesystem_GetFileTimeH(const FileHandle Handle);

RIFT_API NO_DISCARD bool Filesystem_ReadPipe(PlatformPipe Handle, usize DataSize, void* OutData, usize* OutBytesRead);

RIFT_API NO_DISCARD bool Filesystem_Read(const FileHandle Handle, usize DataSize, void* OutData, usize* OutBytesRead);
RIFT_API NO_DISCARD bool Filesystem_ReadEntireFile(const FileHandle Handle, void* OutData, usize* OutBytesRead);
RIFT_API NO_DISCARD bool Filesystem_ReadLine(const FileHandle Handle, String* LineBuffer);

RIFT_API            bool Filesystem_Write(const FileHandle Handle, usize DataSize, const void* Data, usize* OutBytesWritten);
RIFT_API            bool Filesystem_WriteLine(const FileHandle Handle, const String Text, usize* OutBytesWritten);
RIFT_API            bool Filesystem_WriteLineFormatted(const FileHandle Handle, const String Text, usize* OutBytesWritten, ...);

RIFT_API NO_DISCARD bool Filesystem_DoesFileExist(const String FilePath);
RIFT_API NO_DISCARD bool Filesystem_DoesDirectoryExist(const String FilePath);

RIFT_API NO_DISCARD bool Filesystem_GetFilePath(const FileHandle File, String* OutPath);
RIFT_API NO_DISCARD bool Filesystem_GetFileSize(const FileHandle File, usize* OutSize);

RIFT_API NO_DISCARD bool Filesystem_IsFile(const String Path);
RIFT_API NO_DISCARD bool Filesystem_IsDirectory(const String Path);

RIFT_API NO_DISCARD bool Filesystem_IsNewer(const String PathA, const String PathB);
RIFT_API NO_DISCARD bool Filesystem_IsOlder(const String PathA, const String PathB);

RIFT_API NO_DISCARD bool Filesystem_IsPathRelative(const String Path);

RIFT_API NO_DISCARD bool Filesystem_ConvertRelativeToAbsolutePath(String* OutFullPath);

RIFT_API            void Filesystem_IterateDirectory(const String BasePath, DirectoryIterator Callback, bool bRecursive);
RIFT_API            void Filesystem_IterateDirectory_Ex(const String BasePath, DirectoryIterator Callback, bool bRecursive, void* UserData);

RIFT_API NO_DISCARD bool Filesystem_DeleteFiles(const String FilePath, const String Wildcard, bool bRecursive);
RIFT_API NO_DISCARD bool Filesystem_DeleteDirectory(const String DirectoryPath);

RIFT_API NO_DISCARD bool Filesystem_Copy(const String Source, const String Destination);
RIFT_API NO_DISCARD bool Filesystem_Move(const String Source, const String Destination, bool bRename);

RIFT_API NO_DISCARD bool Filesystem_ArePathsCommon(String PathA, String PathB);

RIFT_API NO_DISCARD bool Filesystem_DoesPathHaveFileExtension(const String Path);
RIFT_API NO_DISCARD String Filesystem_ExtractFileNameFromPath(const String Path, bool bIncludeExtension);

RIFT_API NO_DISCARD bool IsValidFileHandle(const FileHandle Handle);

RIFT_API NO_DISCARD FileHandle FileHandle_Null(void);

#endif // FILESYSTEM_H
