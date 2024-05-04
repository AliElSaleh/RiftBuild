#include "Backend.h"

#if PLATFORM_WINDOWS
#include "Structures/Array.h"
#include "String/StringUtils.h"
#include "Platform/Filesystem.h"
#include "Platform/Platform.h"
#include "Log.h"

/// TODO: if multithreaded and more than on soruce file. use /MP and call cl.exe only once

STRUCT(CompileData)
{
    const BuildParams* Params;
    u32* NumCompiled;
    u32 NumSources;
    u32 NumHeaders;
    u32 NumRcSources;
    u32 Index;
    bool bSuccess;
};

STRUCT(LinkData)
{
    const BuildParams* Params;
    String* CmdLine;
};

bool MSVC_DoCompile(CompileData* Data, const String FullPath, const String RelativePath);

internal bool AsmSourceFileDirectoryIterator(const String FullPath, const String RelativePath, const String FileName, u64 FileSize, bool bIsDirectory, void* UserData)
{
    if (FileSize > 0)
    {
        if (String_StartsWith(FileName, StrLit("__"), false))
        {
            return true;
        }

        CompileData* Data = (CompileData*)UserData;
        const BuildParams* Params = Data->Params;

        if (String_EndsWith(RelativePath, StrLit(".asm"), false) &&
            FilterSourceFile(Data->Params->RootDirectory, Data->Params->SourceDirectory, FullPath, RelativePath, Data->Params->WhitelistFiles, Data->Params->BlacklistFiles, Data->Params->WhitelistDirectories, Data->Params->BlacklistDirectories))
        {
            // ignore the intermediate and build directories
            if (String_IndexOfFirstPathSlash(RelativePath, NULL))
            {
                if (String_StartsWith(RelativePath, Data->Params->IntermediateDirectory, false) ||
                    String_StartsWith(RelativePath, Data->Params->BuildDirectory, false))
                {
                    return true;
                }
            }

            u32 LastDot = 0;
            String_IndexOfLastChar(FileName, '.', &LastDot);

            StringLocal(FilePath, MAX_PATH_LENGTH);
            String_Append(&FilePath, StrSlice(FileName.Data, LastDot));
            String_Append(&FilePath, StrLit(".obj"));

            StringLocal(ObjectFilePath, MAX_PATH_LENGTH);
            String_BuildPath(&ObjectFilePath, Params->IntermediateDirectory, FilePath);

            u64 ObjectFileWriteTime = Filesystem_GetLastWriteTime(ObjectFilePath);
            u64 SourceFileWriteTime = Filesystem_GetLastWriteTime(FullPath);

            if (SourceFileWriteTime >= ObjectFileWriteTime)
            {
                StringLocal(CmdLine, Kibibytes(4));
                String_Append(&CmdLine, StrLit("ml64 /nologo /c /Fo\""));

                StringLocal(ObjectPath, MAX_PATH_LENGTH);
                String_BuildPath(&ObjectPath, Params->IntermediateDirectory);
                String_EatPathSeparatorsInlineFromEnd(&ObjectPath);

                String_Append(&CmdLine, ObjectPath);
                String_Append(&CmdLine, StrLit("\\\\\" "));

                StringLocal(SourcePath, MAX_PATH_LENGTH);
                String_BuildPath(&SourcePath, Params->SourceDirectory, RelativePath);
                String_Append(&CmdLine, SourcePath);
                String_AppendSpace(&CmdLine);

                String_Append(&CmdLine, Params->IncludeFlags);

                //LOG("CMD: %S", CmdLine);

                // todo: parallelize this
                PlatformHandle H = Platform_RunCommand(CmdLine, Params->RootDirectory);
                const u32 ExitCode = Platform_WaitForProcessAndGetExitCode(H);
                if (ExitCode != 0)
                {
                    Data->bSuccess = false;
                    return false;
                }
            }
        }
    }

    return true;
}

internal bool SourceFileDirectoryIterator(const String FullPath, const String RelativePath, const String FileName, u64 FileSize, bool bIsDirectory, void* UserData)
{
    if (FileSize > 0)
    {
        if (String_StartsWith(FileName, StrLit("__"), false))
        {
            return true;
        }

        CompileData* Data = (CompileData*)UserData;

        // ignore the intermediate and build directories
        if (String_IndexOfFirstPathSlash(RelativePath, NULL))
        {
            if (String_StartsWith(RelativePath, Data->Params->IntermediateDirectory, false) ||
                String_StartsWith(RelativePath, Data->Params->BuildDirectory, false))
            {
                return true;
            }
        }

        u32 DotIndex = 0;
        String_IndexOfLastChar(FileName, '.', &DotIndex);

        const String Extension = StrShiftF(FileName, DotIndex);

        if (IsSource(Extension))
        {
            if (String_IsEqual(Extension, StrLit(".asm"), false) ||
                String_IsEqual(Extension, StrLit(".rc"), false))
            {
                return true;
            }

            if (FilterSourceFile(Data->Params->RootDirectory, Data->Params->SourceDirectory, FullPath, RelativePath, Data->Params->WhitelistFiles, Data->Params->BlacklistFiles, Data->Params->WhitelistDirectories, Data->Params->BlacklistDirectories))
            {
                if (!MSVC_DoCompile(Data, FullPath, RelativePath))
                {
                    Data->bSuccess = false;
                    return false;
                }
            }
        }
    }

    return true;
}

internal bool ResourceFileDirectoryIterator(const String FullPath, const String RelativePath, const String FileName, u64 FileSize, bool bIsDirectory, void* UserData)
{
    if (FileSize > 0)
    {
        if (String_StartsWith(FileName, StrLit("__"), false))
        {
            return true;
        }

        CompileData* Data = (CompileData*)UserData;

        // ignore the intermediate and build directories
        if (String_IndexOfFirstPathSlash(RelativePath, NULL))
        {
            if (String_StartsWith(RelativePath, Data->Params->IntermediateDirectory, false) ||
                String_StartsWith(RelativePath, Data->Params->BuildDirectory, false))
            {
                return true;
            }
        }

        if (String_EndsWith(FileName, StrLit(".rc"), false))
        {
            if (String_EndsWith(RelativePath, StrLit("icon.rc"), false))
                return true;

            if (FilterSourceFile(Data->Params->RootDirectory, Data->Params->SourceDirectory, FullPath, RelativePath, Data->Params->WhitelistFiles, Data->Params->BlacklistFiles, Data->Params->WhitelistDirectories, Data->Params->BlacklistDirectories))
            {
                StringLocal(CmdLine, 1024);
                String_Append(&CmdLine, StrLit("llvm-rc"));
                String_Append(&CmdLine, StrLit(" \""));
                String_Append(&CmdLine, FullPath);
                String_AppendChar(&CmdLine, '"');

                LOG("Compiling resource file \"%S\"", RelativePath);
                
                if (Data->Params->bVerbose)
                    LOG("    %S", CmdLine);

                /*
                if (Data->NumRcSources > 1 && Data->Index < Data->NumRcSources-1)
                {
                    LOG_LINE_BREAK();
                }
                */

                PlatformHandle h = Platform_RunCommand(CmdLine, Data->Params->RootDirectory);
                u32 ExitCode = Platform_WaitForProcessAndGetExitCode(h);
                if (ExitCode != 0)
                {
                    Data->bSuccess = false;
                    LOG("Failed to build resource file \"%S\" for %S. Aborting build...", RelativePath, Data->Params->AssemblyWithExt);
                    return false;
                }
            }
        }

        Data->Index++;
    }

    return true;
}

internal bool Link_SourceFileDirectoryIterator(const String FullPath, const String RelativePath, const String FileName, u64 FileSize, bool bIsDirectory, void* UserData)
{
    if (FileSize > 0)
    {
        if (String_StartsWith(FileName, StrLit("__"), false))
        {
            return true;
        }

        LinkData* Data = (LinkData*)UserData;

        u32 DotIndex = 0;
        String_IndexOfLastChar(FileName, '.', &DotIndex);

        const String Extension = StrShiftF(FileName, DotIndex);

        if (IsSource(Extension))
        {
            if (String_IndexOfFirstPathSlash(RelativePath, NULL))
            {
                if (String_StartsWith(RelativePath, Data->Params->IntermediateDirectory, false) ||
                    String_StartsWith(RelativePath, Data->Params->BuildDirectory, false))
                {
                    return true;
                }
            }

            if (FilterSourceFile(Data->Params->RootDirectory, Data->Params->SourceDirectory, FullPath, RelativePath, Data->Params->WhitelistFiles, Data->Params->BlacklistFiles, Data->Params->WhitelistDirectories, Data->Params->BlacklistDirectories))
            {
                StringLocal(ObjectPath, MAX_PATH_LENGTH);

                u32 LastDot = 0;
                String_IndexOfLastChar(FileName, '.', &LastDot);

                if (String_EndsWith(RelativePath, StrLit(".rc"), false))
                {
                    if (String_EndsWith(RelativePath, StrLit("icon.rc"), false))
                        return true;

                    u32 LastSlash = 0;
                    String_IndexOfLastPathSlash(FullPath, &LastSlash);

                    StringLocal(FilePath, MAX_PATH_LENGTH);
                    String_Append(&FilePath, StrSlice(FileName.Data, LastDot));
                    String_Append(&FilePath, StrLit(".res"));

                    const String Dir = StrSlice(FullPath.Data, LastSlash);
                    String_BuildPath(&ObjectPath, Dir, FilePath);
                }
                else
                {
                    u32 LastPathDot = 0;
                    String_IndexOfLastChar(RelativePath, '.', &LastPathDot);

                    StringLocal(FilePath, MAX_PATH_LENGTH);
                    String_Append(&FilePath, StrSlice(RelativePath.Data, LastPathDot));
                    String_Append(&FilePath, StrLit(".obj"));

                    String_BuildPath(&ObjectPath, Data->Params->IntermediateDirectory, FilePath);
                }

                String_AppendChar (Data->CmdLine, '"');
                String_Append     (Data->CmdLine, ObjectPath);
                String_AppendChar (Data->CmdLine, '"');
                String_AppendSpace(Data->CmdLine);
            }
        }
    }

    return true;
}

bool MSVC_CompileV2(const BuildParams* Params, u32* NumCompiled)
{
    StringLocal(SourceDir, MAX_PATH_LENGTH);
    String_BuildPath(&SourceDir, Params->RootDirectory, Params->SourceDirectory);

    // compile all .asm files first
    {
        CompileData UserData = { Params, NumCompiled, Params->NumSources, Params->NumHeaders, Params->NumRcSources, 0, true };
        Filesystem_IterateDirectory_Ex(SourceDir, AsmSourceFileDirectoryIterator, true, &UserData);
        if (!UserData.bSuccess)
        {
            return false;
        }
    }

    {
        CompileData UserData = { Params, NumCompiled, Params->NumSources, Params->NumHeaders, Params->NumRcSources, 0, true };
        Filesystem_IterateDirectory_Ex(SourceDir, SourceFileDirectoryIterator, true, &UserData);
        if (!UserData.bSuccess)
        {
            return false;
        }
    }

    if (*NumCompiled == 0)
    {
        #ifndef HOOD
        LOG("\nNothing to compile - source files unchanged since last build");
        #else
        LOG("\nno work to do homie");
        #endif

        return true;
    }

    for each (Process, *Params->Processes)
    {
        const u32 ExitCode = Platform_WaitForProcessAndGetExitCode(Process);
        if (ExitCode != 0)
        {
            #ifndef HOOD
            LOG_ERROR("Compiler errors detected. See above errors to fix. Exit code for process: %u. Aborting build...", ExitCode);
            #else
            LOG_ERROR("seen some compiler errors homie, gon' stop right here");
            #endif

            return false;
        }
    }

    // compile resource files
    if (Platform_FindProgram(StrLit("llvm-rc"))) // TODO: use rc.exe instead
    {
        CompileData RcUserData = { Params, NumCompiled, Params->NumSources, Params->NumHeaders, Params->NumRcSources, 0, true };
        Filesystem_IterateDirectory_Ex(SourceDir, ResourceFileDirectoryIterator, true, &RcUserData);
        if (!RcUserData.bSuccess)
        {
            return false;
        }
    }

    return true;
}

bool MSVC_DoCompile(CompileData* Data, const String FullPath, const String RelativePath)
{
    const BuildParams* Params = Data->Params;

    TArray(PlatformHandle) Processes = *Params->Processes;

    if (Params->MaxCompilersAtOnce > 0 && !Params->bShouldWaitPerCompileProcess)
    {
        u32 Num = (u32)Array_Num(Processes);
        if (Num == Params->MaxCompilersAtOnce)
        {
            u32 Index = Platform_WaitForMultipleHandles(*Params->Processes, (u32)Array_Num(*Params->Processes), -1, false);

            const u32 ExitCode = Platform_GetExitCodeForProcess(Processes[Index]);

            if (ExitCode != 0)
            {
                #ifndef HOOD
                LOG_ERROR("Compiler errors detected. See above errors to fix. Exit code for process: %u. Aborting build...", ExitCode);
                #else
                LOG_ERROR("seen some compiler errors homie, gon' stop right here");
                #endif

                return false;
            }

            Array_RemoveAt(Processes, NULL, Index);
        }
    }

    Data->Index++;
    
    StringLocal(CmdLine, UINT16_MAX);
    String_Append(&CmdLine, StrLit("cl /nologo "));

    u32 LastSlash = 0;
    String_IndexOfLastPathSlash(RelativePath, &LastSlash);

    u32 LastDot = 0;
    String_IndexOfLastChar(RelativePath, '.', &LastDot);

    StringLocal(FilePath, MAX_PATH_LENGTH);
    String_Append(&FilePath, StrSlice(RelativePath.Data, LastDot));
    String_Append(&FilePath, StrLit(".obj"));

    // build object path
    StringLocal(ObjectFilePath, MAX_PATH_LENGTH);
    String_BuildPath(&ObjectFilePath, Params->RootDirectory, Params->IntermediateDirectory, FilePath);
    Filesystem_ConvertRelativeToAbsolutePath(&ObjectFilePath);
    
    u64 ObjectFileWriteTime = Filesystem_GetLastWriteTime(ObjectFilePath);
    u64 SourceFileWriteTime = Filesystem_GetLastWriteTime(FullPath);

    if (ObjectFileWriteTime >= SourceFileWriteTime)
    {
        #ifndef HOOD
        LOG("[Skipping] %S", FullPath);
        #else
        LOG("skip'n dis shit %S", FullPath);
        #endif

        return true;
    }

    StringLocal(FullSourcePath, MAX_PATH_LENGTH+2);
    String_BuildPath(&FullSourcePath, Params->SourceDirectory, RelativePath);

    String_AppendChar(&CmdLine, '"');
    String_Append(&CmdLine, FullSourcePath);
    String_AppendChar(&CmdLine, '"');
    String_AppendSpace(&CmdLine);

    String_BuildSeparator(&CmdLine, ' ', Params->CompilerFlags, Params->DefineFlags, Params->IncludeFlags);
    String_EatSpacesInlineFromEnd(&CmdLine);

    StringLocal(ObjectPath, MAX_PATH_LENGTH);
    String_BuildPath(&ObjectPath, Params->IntermediateDirectory, StrSlice(RelativePath.Data, LastSlash));
    String_EatPathSeparatorsInlineFromEnd(&ObjectPath);

    StringLocal(FullObjectPath, MAX_PATH_LENGTH);
    String_BuildPath(&FullObjectPath, Params->RootDirectory, ObjectPath);
    Filesystem_ConvertRelativeToAbsolutePath(&FullObjectPath);

    Filesystem_OpenDirectory(FullObjectPath);

    String_Append(&CmdLine, StrLit(" /Fo\""));
    String_Append(&CmdLine, ObjectPath);
    String_Append(&CmdLine, StrLit("\\\\\" /c"));

    if (Params->bShouldWaitPerCompileProcess)
        LOG_INLINE(" [%i/%i] Compiling ", Data->Index, Data->NumSources);

    if (Params->bVerbose)
    {
        LOG("    %S\n", CmdLine);
    }

    PlatformHandle H = Platform_RunCommand(CmdLine, Params->RootDirectory);
    Array_Add(Processes, H);
    (*Data->NumCompiled)++;

    if (Params->bShouldWaitPerCompileProcess)
    {
        const u32 ExitCode = Platform_WaitForProcessAndGetExitCode(H);
        if (ExitCode != 0)
        {
            LOG_ERROR("Compiler errors detected. See above errors to fix. Exit code for process: %u. Aborting build...", ExitCode);
            return false;
        }
    }

    return true;
}

/*
bool MSVC_Compile(const BuildParams* Params, u32* NumCompiled)
{
    // compile all .asm files first
    for each (It, Params->SourceFiles)
    {
        if (String_EndsWith(It->RelativePath, StrLit(".asm"), false))
        {
            u32 LastSlash = 0;
            String_IndexOfLastPathSlash(It->RelativePath, &LastSlash);
            String FileName = LastSlash > 0 ? StrShiftF(It->RelativePath, LastSlash+1) : It->RelativePath;

            u32 LastDot = 0;
            String_IndexOfLastChar(FileName, '.', &LastDot);

            StringLocal(FilePath, MAX_PATH_LENGTH);
            String_Append(&FilePath, StrSlice(FileName.Data, LastDot));
            String_Append(&FilePath, StrLit(".obj"));

            StringLocal(ObjectFilePath, MAX_PATH_LENGTH);
            String_BuildPath(&ObjectFilePath, Params->IntermediateDirectory, FilePath);

            u64 ObjectFileWriteTime = Filesystem_GetLastWriteTime(ObjectFilePath);
            u64 SourceFileWriteTime = Filesystem_GetLastWriteTime(It->FullPath);

            if (SourceFileWriteTime >= ObjectFileWriteTime)
            {
                StringLocal(CmdLine, Kibibytes(4));
                String_Append(&CmdLine, StrLit("ml64 /nologo /c /Fo\""));

                StringLocal(ObjectPath, MAX_PATH_LENGTH);
                String_BuildPath(&ObjectPath, Params->IntermediateDirectory);
                String_EatPathSeparatorsInlineFromEnd(&ObjectPath);

                String_Append(&CmdLine, ObjectPath);
                String_Append(&CmdLine, StrLit("\\\\\" "));

                StringLocal(SourcePath, MAX_PATH_LENGTH);
                String_BuildPath(&SourcePath, Params->SourceDirectory, It->RelativePath);
                String_Append(&CmdLine, SourcePath);
                String_AppendSpace(&CmdLine);

                String_Append(&CmdLine, Params->IncludeFlags);

                //LOG("CMD: %S", CmdLine);

                PlatformHandle H = Platform_RunCommand(CmdLine, Params->RootDirectory);
                Platform_WaitForHandle(H, -1);

                const u32 ExitCode = Platform_GetExitCodeForProcess(H);
                if (ExitCode != 0)
                {
                    return false;
                }
            }
        }
    }

    TArray(PlatformHandle) Processes = *Params->Processes;

    u32 TotalWorkDone = 0;
    for each (It, Params->SourceFiles)
    {
        if (String_EndsWith(It->RelativePath, StrLit(".rc"), false))
        {
            // we will build this later
            continue;
        }

        SourceFileData File = *It;

        u32 LastSlash = 0;
        String_IndexOfLastPathSlash(File.RelativePath, &LastSlash);
        String FileName = LastSlash > 0 ? StrShiftF(File.RelativePath, LastSlash+1) : File.RelativePath;

        u32 LastDot = 0;
        if (!String_IndexOfLastChar(FileName, '.', &LastDot))
        {
            // no extension? huh??
            // this should theoretically never happen because of C_IsSource() func which checks for extensions before we get here
            // but it's good to cover these edge cases anyway
            continue;
        }

        bool bIsAsm = String_EndsWith(File.RelativePath, StrLit(".asm"), false);
        if (bIsAsm)
            continue;

        if (Params->MaxCompilersAtOnce > 0 && !Params->bShouldWaitPerCompileProcess)
        {
            u32 Num = (u32)Array_Num(Processes);
            if (Num == Params->MaxCompilersAtOnce)
            {
                for each (Process, Processes)
                {
                    Platform_WaitForHandle(Process, -1);
                    const u32 ExitCode = Platform_GetExitCodeForProcess(Process);
                    if (ExitCode != 0)
                    {
                        #ifndef HOOD
                        LOG_ERROR("Compiler errors detected. See above errors to fix. Exit code for process: %u. Aborting build...", ExitCode);
                        #else
                        LOG_ERROR("seen some compiler errors homie, gon' stop right here");
                        #endif

                        return false;
                    }

                    Array_Remove(Processes, Process);
                    break;
                }
            }
        }
        
        StringLocal(CmdLine, UINT16_MAX);
        String_Append(&CmdLine, StrLit("cl /nologo "));

        StringLocal(FilePath, MAX_PATH_LENGTH);
        String_Append(&FilePath, StrSlice(FileName.Data, LastDot));
        String_Append(&FilePath, StrLit(".obj"));

        // build object path
        StringLocal(ObjectFilePath, MAX_PATH_LENGTH);
        String_BuildPath(&ObjectFilePath, Params->IntermediateDirectory, FilePath);

        u64 ObjectFileWriteTime = Filesystem_GetLastWriteTime(ObjectFilePath);
        u64 SourceFileWriteTime = Filesystem_GetLastWriteTime(File.FullPath);

        if (ObjectFileWriteTime >= SourceFileWriteTime)
        {
            #ifndef HOOD
            LOG("[Skipping] %S", File.FullPath);
            #else
            LOG("skip'n dis shit %S", File.FullPath);
            #endif

            continue;
        }

        TotalWorkDone++;

        StringLocal(FullSourcePath, MAX_PATH_LENGTH+2);
        String_BuildPath(&FullSourcePath, Params->SourceDirectory, File.RelativePath);

        String_AppendChar(&CmdLine, '"');
        String_Append(&CmdLine, FullSourcePath);
        String_AppendChar(&CmdLine, '"');
        String_AppendSpace(&CmdLine);

        String_BuildSeparator(&CmdLine, ' ', Params->CompilerFlags, Params->DefineFlags, Params->IncludeFlags);
        String_EatSpacesInlineFromEnd(&CmdLine);

        StringLocal(ObjectPath, MAX_PATH_LENGTH);
        String_BuildPath(&ObjectPath, Params->IntermediateDirectory);
        String_EatPathSeparatorsInlineFromEnd(&ObjectPath);

        String_Append(&CmdLine, StrLit(" /Fo\""));
        String_Append(&CmdLine, ObjectPath);
        String_Append(&CmdLine, StrLit("\\\\\" /c"));

        if (Params->bShouldWaitPerCompileProcess)
            LOG_INLINE("[%i/%i] Compiling ", TotalWorkDone, Array_Num(Params->SourceFiles));

        if (Params->bVerbose)
        {
            LOG("    %S", CmdLine);
        }

        PlatformHandle H = Platform_RunCommand(CmdLine, Params->RootDirectory);
        Array_Add(Processes, H);

        if (Params->bShouldWaitPerCompileProcess)
        {
            Platform_WaitForHandle(H, -1);

            const u32 ExitCode = Platform_GetExitCodeForProcess(H);
            if (ExitCode != 0)
            {
                LOG_ERROR("Compiler errors detected. See above errors to fix. Exit code for process: %u. Aborting build...", ExitCode);

                return false;
            }
        }
    }

    *NumCompiled = TotalWorkDone;

    if (TotalWorkDone == 0)
    {
        #ifndef HOOD
        LOG("\nNothing to compile - source files unchanged since last build");
        #else
        LOG("\nno work to do homie");
        #endif
    
        return true;
    }

    Platform_WaitForMultipleHandles(Processes, (u32)Array_Num(Processes), -1, true);

    for each (Process, Processes)
    {
        const u32 ExitCode = Platform_GetExitCodeForProcess(Process);
        if (ExitCode != 0)
        {
            #ifndef HOOD
            LOG_ERROR("Compiler errors detected. See above errors to fix. Exit code for process: %u. Aborting build...", ExitCode);
            #else
            LOG_ERROR("seen some compiler errors homie, gon' stop right here");
            #endif

            return false;
        }
    }

    // compile resource files
    u32 i = 0;
    // TODO: use rc.exe instead of llvm-rc
    String RCProgram = StrLit("llvm-rc");
    bool bHasRcProgram = Platform_FindProgram(RCProgram);

    if (bHasRcProgram)
    {
        for each_i (i, It, Params->SourceFiles)
        {
            if (String_EndsWith(It->RelativePath, StrLit(".rc"), false))
            {
                if (String_EndsWith(It->RelativePath, StrLit("icon.rc"), false))
                    continue;

                StringLocal(CmdLine, 1024);
                String_Append(&CmdLine, RCProgram);
                String_Append(&CmdLine, StrLit(" \""));
                String_Append(&CmdLine, It->FullPath);
                String_AppendChar(&CmdLine, '"');

                LOG("\nCompiling resource file \"%S\"", It->FullPath);
                LOG("    %S", CmdLine);

                if (i < Array_Num(Params->SourceFiles)-1)
                {
                    LOG_LINE_BREAK();
                }

                PlatformHandle h = Platform_RunCommand(CmdLine, Params->RootDirectory);
                u32 ExitCode = Platform_WaitForProcessAndGetExitCode(h);
                if (ExitCode != 0)
                {
                    LOG("Failed to build resource file \"%S\" for %S. Aborting build...", It->RelativePath, Params->AssemblyWithExt);
                    return false;
                }
            }
        }
    }

    return true;
}
*/

bool MSVC_LinkV2(const BuildParams* Params)
{
    if (String_IsEqual(Params->Extension, StrLit(".pch"), false))
    {
        return true;
    }

    StringLocal(SourceDir, MAX_PATH_LENGTH);
    String_BuildPath(&SourceDir, Params->RootDirectory, Params->SourceDirectory);

    StringLocal(BuildPath, MAX_PATH_LENGTH);
    String_BuildPath(&BuildPath, Params->RootDirectory, Params->BuildDirectory);
    String_AppendPathSeparator(&BuildPath);

    StringLocal(CmdLine, UINT16_MAX);

    bool bIsDLL = String_IsEqual(Params->Extension, StrLit(".dll"), false);
    if (String_IsEqual(Params->Extension, StrLit(".exe"), false) ||
        bIsDLL)
    {
        String_Append(&CmdLine, StrLit("link"));
        if (bIsDLL)
            String_Append(&CmdLine, StrLit(" /dll"));
        String_Append(&CmdLine, StrLit(" /nologo "));

        String_BuildSeparator(&CmdLine, ' ', Params->LinkerDefineFlags, Params->LinkerFlags, Params->IconResFilePath, Params->VersionResFilePath, Params->Libraries, Params->LibraryDirectories)//, AllObjFiles);
        String_AppendSpace(&CmdLine);

        LinkData Data = { Params, &CmdLine };
        Filesystem_IterateDirectory_Ex(SourceDir, Link_SourceFileDirectoryIterator, true, &Data);

        String_EatSpacesInlineFromEnd(&CmdLine);
        String_Concat(&CmdLine, StrLit(" /OUT:\""), BuildPath, Params->AssemblyWithExt, StrLit("\"")); // make this first then the flags?
    }
    else if (String_IsEqual(Params->Extension, StrLit(".lib"), false))
    {
        String_Append(&CmdLine, StrLit("lib /nologo /OUT:\""));

        String_Append(&CmdLine, BuildPath);
        String_Append(&CmdLine, Params->Assembly);
        
        String_Append(&CmdLine, StrLit(".lib\" "));

        String_BuildSeparator(&CmdLine, ' ', Params->Libraries, Params->LibraryDirectories, Params->VersionResFilePath);
        String_AppendSpace(&CmdLine);

        LinkData Data = { Params, &CmdLine };
        Filesystem_IterateDirectory_Ex(SourceDir, Link_SourceFileDirectoryIterator, true, &Data);
    }

    LOG_LINE_BREAK();
    LogString_WordWrapped(S("Linking: "), CmdLine, false);
    //LOG("\nLinking: %S\n", CmdLine);
    LOG_LINE_BREAK();

    PlatformHandle Handle = Platform_RunCommand(CmdLine, Params->RootDirectory);
    u32 ExitCode = Platform_WaitForProcessAndGetExitCode(Handle);
    if (ExitCode != 0)
    {
        #ifndef HOOD
        LOG_ERROR("Compiler errors detected. See above errors to fix. Exit code for process: %u. Aborting build...", ExitCode);
        #else
        LOG_ERROR("seen some compiler errors homie. fix yo shit up, something aint linkin' right");
        #endif
        
        return false;
    }

    // generate a .def file if we are building a dll file
    if (bIsDLL)
    {
        String_Empty(&CmdLine);
        String_Append(&CmdLine, StrLit("dumpbin /EXPORTS /NOLOGO /OUT:\""));

        String_Append(&CmdLine, BuildPath);
        String_Append(&CmdLine, Params->Assembly);
        String_Append(&CmdLine, StrLit(".def\" \""));

        String_Append(&CmdLine, BuildPath);
        String_Append(&CmdLine, Params->Assembly);
        String_Append(&CmdLine, StrLit(".dll\""));

        PlatformHandle H = Platform_RunCommand(CmdLine, Params->RootDirectory);
        ExitCode = Platform_WaitForProcessAndGetExitCode(H);

        if (ExitCode != 0)
        {
            #ifndef HOOD
            LOG_ERROR("Dumpbin errors detected. See above errors to fix. Aborting build...");
            #else
            LOG_ERROR("seen some dump bin errors homie. fix yo shit up, something aint right");
            #endif
            return false;
        }
    }

    return true;
}

/*
bool MSVC_Link(const BuildParams* Params)
{
    if (String_IsEqual(Params->Extension, StrLit(".pch"), false))
    {
        return true;
    }

    StringLocal(BuildPath, MAX_PATH_LENGTH);
    String_BuildPath(&BuildPath, Params->RootDirectory, Params->BuildDirectory);
    String_AppendPathSeparator(&BuildPath);

    StringLocal(CmdLine, UINT16_MAX);

    StringLocal(AllObjFiles, Kibibytes(24));
    for each (It, Params->SourceFiles)
    {
        SourceFileData File = *It;

        StringLocal(ObjectPath, MAX_PATH_LENGTH);

        if (String_EndsWith(File.RelativePath, StrLit(".rc"), false))
        {
            if (String_EndsWith(It->RelativePath, StrLit("icon.rc"), false))
                continue;

            u32 LastSlash = 0;
            String_IndexOfLastPathSlash(File.FullPath, &LastSlash);
            String FileName = LastSlash > 0 ? StrShiftF(File.FullPath, LastSlash+1) : File.FullPath;

            u32 LastDot = 0;
            String_IndexOfLastChar(FileName, '.', &LastDot);

            StringLocal(FilePath, MAX_PATH_LENGTH);
            String_Append(&FilePath, StrSlice(FileName.Data, LastDot));
            String_Append(&FilePath, StrLit(".res"));

            const String Dir = StrSlice(File.FullPath.Data, LastSlash);
            String_BuildPath(&ObjectPath, Dir, FilePath);
        }
        else
        {
            u32 LastSlash = 0;
            String_IndexOfLastPathSlash(File.RelativePath, &LastSlash);
            String FileName = LastSlash > 0 ? StrShiftF(File.RelativePath, LastSlash+1) : File.RelativePath;

            u32 LastDot = 0;
            String_IndexOfLastChar(FileName, '.', &LastDot);

            StringLocal(FilePath, MAX_PATH_LENGTH);
            String_Append(&FilePath, StrSlice(FileName.Data, LastDot));
            String_Append(&FilePath, StrLit(".obj"));

            String_BuildPath(&ObjectPath, Params->IntermediateDirectory, FilePath);
        }

        String_AppendChar(&AllObjFiles, '"');
        String_Append(&AllObjFiles, ObjectPath);
        String_AppendChar(&AllObjFiles, '"');
        String_AppendSpace(&AllObjFiles);
    }

    bool bIsDLL = String_IsEqual(Params->Extension, StrLit(".dll"), false);
    if (String_IsEqual(Params->Extension, StrLit(".exe"), false) ||
        bIsDLL)
    {
        String_Append(&CmdLine, StrLit("link"));
        if (bIsDLL)
            String_Append(&CmdLine, StrLit(" /dll"));
        String_Append(&CmdLine, StrLit(" /nologo "));

        String_BuildSeparator(&CmdLine, ' ', Params->LinkerDefineFlags, Params->LinkerFlags, Params->IconResFilePath, Params->VersionResFilePath, Params->Libraries, Params->LibraryDirectories, AllObjFiles);

        String_EatSpacesInlineFromEnd(&CmdLine);
        String_Concat(&CmdLine, StrLit(" /OUT:\""), BuildPath, Params->AssemblyWithExt, StrLit("\" "));
        String_EatSpacesInlineFromEnd(&CmdLine);
    }
    else if (String_IsEqual(Params->Extension, StrLit(".lib"), false))
    {
        String_Append(&CmdLine, StrLit("lib /nologo /OUT:\""));

        String_Append(&CmdLine, BuildPath);
        String_Append(&CmdLine, Params->Assembly);
        
        String_Append(&CmdLine, StrLit(".lib\" "));

        String_BuildSeparator(&CmdLine, ' ', Params->Libraries, Params->LibraryDirectories, Params->VersionResFilePath, AllObjFiles);
    }

    LOG("\nLinking: %S\n", CmdLine);

    PlatformHandle Handle = Platform_RunCommand(CmdLine, Params->RootDirectory);
    u32 ExitCode = Platform_WaitForProcessAndGetExitCode(Handle);
    if (ExitCode != 0)
    {
        #ifndef HOOD
        LOG_ERROR("Compiler errors detected. See above errors to fix. Exit code for process: %u. Aborting build...", ExitCode);
        #else
        LOG_ERROR("seen some compiler errors homie. fix yo shit up, something aint linkin' right");
        #endif
        
        return false;
    }

    // generate a .def file if we are building a dll file (for convenience)
    if (bIsDLL)
    {
        String_Empty(&CmdLine);
        String_Append(&CmdLine, StrLit("dumpbin /EXPORTS /NOLOGO /OUT:\""));

        String_Append(&CmdLine, BuildPath);
        String_Append(&CmdLine, Params->Assembly);
        String_Append(&CmdLine, StrLit(".def\" \""));

        String_Append(&CmdLine, BuildPath);
        String_Append(&CmdLine, Params->Assembly);
        String_Append(&CmdLine, StrLit(".dll\""));

        PlatformHandle H = Platform_RunCommand(CmdLine, Params->RootDirectory);
        ExitCode = Platform_WaitForProcessAndGetExitCode(H);

        if (ExitCode != 0)
        {
            #ifndef HOOD
            LOG_ERROR("Dumpbin errors detected. See above errors to fix. Aborting build...");
            #else
            LOG_ERROR("seen some dump bin errors homie. fix yo shit up, something aint right");
            #endif
            return false;
        }
    }

    return true;
}
*/
#else
bool MSVC_Compile(const BuildParams* Params, u32* OutNumCompiled) { return false; }
bool MSVC_Link(const BuildParams* Params) { return false; }
bool MSVC_CompileV2(const BuildParams* Params, u32* OutNumCompiled) { return false; }
bool MSVC_LinkV2(const BuildParams* Params) { return false; }
#endif // PLATFORM_WINDOWS
