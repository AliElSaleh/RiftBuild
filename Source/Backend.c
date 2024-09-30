// Copyright (c) 2024 Ali El Saleh

#include "Backend.h"

#ifndef UNITY_BUILD
#include "Globals.h"
#include "Allocators.h"
#include "Array.h"
#include "StringUtils.h"
#include "Filesystem.h"
#include "Platform.h"
#include "Log.h"
#endif

bool C_DoCompile(CompileData* Data, const String FullPath, const String RelativePath);

internal bool AsmSourceFileDirectoryIterator(const String FullPath, const String RelativePath, const String FileName, u64 FileSize, bool bIsDirectory, void* UserData)
{
    if (FileSize > 0)
    {
        if (String_StartsWith(FileName, S("__"), false))
        {
            return true;
        }

        CompileData* Data = UserData;
        const BuildParams* Params = Data->Params;

        if (String_EndsWith(RelativePath, S(".asm"), false) &&
            FilterSourceFile(Data->Params->RootDirectory, Data->Params->SourceDirectory, FullPath, RelativePath, Data->Params->WhitelistFiles, Data->Params->BlacklistFiles, Data->Params->WhitelistDirectories, Data->Params->BlacklistDirectories))
        {
            // ignore the intermediate and build directories
            // TODO: check this if logic
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
            
            // TODO: rework, hack for now
            if (String_IsEqual(Params->CompilerProgram, S("cl"), false))
            {
                String_Append(&FilePath, S(".obj"));
            }
            else
            {
                String_Append(&FilePath, S(".o"));
            }

            StringLocal(ObjectFilePath, MAX_PATH_LENGTH);
            String_BuildPath(&ObjectFilePath, Params->IntermediateDirectory, FilePath);

            u64 ObjectFileWriteTime = Filesystem_GetLastWriteTime(ObjectFilePath);
            u64 SourceFileWriteTime = Filesystem_GetLastWriteTime(FullPath);

            if (SourceFileWriteTime >= ObjectFileWriteTime)
            {
                StringLocal(CmdLine, Kibibytes(4));
                String_Append(&CmdLine, Params->AsmProgram);
                String_AppendSpace(&CmdLine);

                StringLocal(SourcePath, MAX_PATH_LENGTH);
                String_BuildPath(&SourcePath, Params->SourceDirectory, RelativePath);

                // todo: assmembler defines and includes
                String_BuildSeparator(&CmdLine, ' ', Params->AssemblerFlags, SourcePath);

                StringLocal(ObjectPath, MAX_PATH_LENGTH);
                String_BuildPath(&ObjectPath, Params->IntermediateBaseDirectory, FilePath);

                String_Append(&CmdLine, S(" -o \""));
                String_Append(&CmdLine, ObjectPath);
                String_Append(&CmdLine, S("\""));

                if (Params->bVerbose) LOG("    CMD: %S", CmdLine);

                LOG("Assembling %S", FullPath);

                // todo: parallelize this
                PlatformHandle H = Platform_RunCommand(CmdLine, Params->RootDirectory, String_Null());
                if (!Platform_IsValidHandle(H)) return false;
                const u32 ExitCode = Platform_WaitForProcessAndGetExitCode(H);
                if (ExitCode != 0)
                {
                    Data->bSuccess = false;
                    return false;
                }
            }
            else
            {
                #ifndef HOOD
                LOG("[Skipping] %S", FullPath);
                #else
                LOG("skip'n dis shit %S", FullPath);
                #endif
            }
        }
    }

    return true;
}

bool SourceFileDirectoryIterator(const String FullPath, const String RelativePath, const String FileName, u64 FileSize, bool bIsDirectory, void* UserData)
{
    if (FileSize > 0)
    {
        if (String_StartsWith(FileName, S("__"), false))
        {
            return true;
        }

        CompileData* Data = UserData;

        // ignore the intermediate and build directories
        if (String_IndexOfFirstPathSlash(RelativePath, NULL))
        {
            if (RelativePath.Length == Data->Params->IntermediateBaseDirectory.Length)
                if (String_StartsWith(RelativePath, Data->Params->IntermediateDirectory, false))
                    return true;

            if (RelativePath.Length == Data->Params->BuildDirectory.Length)
                if (String_StartsWith(RelativePath, Data->Params->BuildDirectory, false))
                    return true;
        }

        u32 DotIndex = 0;
        String_IndexOfLastChar(FileName, '.', &DotIndex);

        const String Extension = StrShiftF(FileName, DotIndex);

        if (String_IsEqual(Extension, S(".asm"), false) ||
            String_IsEqual(Extension, S(".rc"), false) ||
            String_IsEqual(Extension, S(".manifest"), false))
        {
            // we will build this later
            return true;
        }

        const bool bIsPCH    = Data->Params->Type == AssemblyType_PCH;
        const bool bIsSource = bIsPCH ? IsHeader(Extension) : IsSource(Extension);

        if (bIsSource)
        {
            if (FilterSourceFile(Data->Params->RootDirectory, Data->Params->SourceDirectory, FullPath, RelativePath, Data->Params->WhitelistFiles, Data->Params->BlacklistFiles, Data->Params->WhitelistDirectories, Data->Params->BlacklistDirectories))
            {
                // compile this file
                if (!Data->Callback(Data, FullPath, RelativePath))
                {
                    Data->bSuccess = false;
                    return false;
                }
            }
        }
    }

    return true;
}

#if PLATFORM_WINDOWS
internal bool ResourceFileDirectoryIterator(const String FullPath, const String RelativePath, const String FileName, u64 FileSize, bool bIsDirectory, void* UserData)
{
    if (FileSize > 0)
    {
        if (String_StartsWith(FileName, S("__"), false))
        {
            return true;
        }

        CompileData* Data = UserData;

        // ignore the intermediate and build directories
        if (String_IndexOfFirstPathSlash(RelativePath, NULL))
        {
            if (RelativePath.Length == Data->Params->IntermediateBaseDirectory.Length)
                if (String_StartsWith(RelativePath, Data->Params->IntermediateDirectory, false))
                    return true;

            if (RelativePath.Length == Data->Params->BuildDirectory.Length)
                if (String_StartsWith(RelativePath, Data->Params->BuildDirectory, false))
                    return true;
        }

        if (String_EndsWith(FileName, S(".rc"), false))
        {
            if (String_EndsWith(RelativePath, S("icon.rc"), false))
                return true;

            if (FilterSourceFile(Data->Params->RootDirectory, Data->Params->SourceDirectory,
                                FullPath, RelativePath,
                                Data->Params->WhitelistFiles, Data->Params->BlacklistFiles,
                                Data->Params->WhitelistDirectories, Data->Params->BlacklistDirectories))
            {
                bool bSuccess = RC_Compile(Data->Params, FullPath, NULL);
                if (!bSuccess)
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

bool RC_Compile(const BuildParams* Params, const String FullRCPath, String* OutResPath)
{
    StringLocal(CmdLine, 1024);
    String_Append(&CmdLine, Params->RCProgram);
    String_Append(&CmdLine, Params->RCProgramFlags);

    const bool bWindres = String_IsEqual(Params->RCProgram, S("windres"), false);

    u32 LastDot = 0;
    String_IndexOfLastChar(FullRCPath, '.', &LastDot);

    StringLocal(ResPath, MAX_PATH_LENGTH);
    String_Append(&ResPath, S("\""));
    String_Append(&ResPath, StrSlice(FullRCPath.Data, LastDot));
    String_Append(&ResPath, S(".res"));
    String_Append(&ResPath, S("\""));

    if (OutResPath)
    {
        String_Copy(OutResPath, ResPath);
    }

    if (bWindres)
    {
        String_Append(&CmdLine, S(" -O coff"));
        String_Append(&CmdLine, S(" -o "));
        String_Append(&CmdLine, ResPath);

        String_Append(&CmdLine, S(" -i"));
    }

    // todo: resource defines

    String_Append(&CmdLine, S(" \""));
    String_Append(&CmdLine, FullRCPath);
    String_AppendChar(&CmdLine, '"');

    LOG("Compiling resource %S", FullRCPath);
    
    if (Params->bVerbose) LOG("    %S", CmdLine);

    PlatformHandle H = Platform_RunCommand(CmdLine, Params->RootDirectory, String_Null());
    if (!Platform_IsValidHandle(H)) return false;
    u32 ExitCode = Platform_WaitForProcessAndGetExitCode(H);
    if (ExitCode != 0)
    {
        return false;
    }

    return true;
}
#endif

internal bool Link_SourceFileDirectoryIterator(const String FullPath, const String RelativePath, const String FileName, u64 FileSize, bool bIsDirectory, void* UserData)
{
    if (FileSize > 0)
    {
        if (String_StartsWith(FileName, S("__"), false))
        {
            return true;
        }

        LinkData* Data = (LinkData*)UserData;

        u32 DotIndex = 0;
        String_IndexOfLastChar(FileName, '.', &DotIndex);

        const String Extension = StrShiftF(FileName, DotIndex);

        if (String_EndsWith(Extension, S(".manifest"), false))
        {
            return true;
        }

        if (IsSource(Extension))
        {
            // TODO: ignore .asm files for now when compiling with clang/gcc, i still need to support them
            if (String_IsEqual(Extension, S(".asm"), false))
            {
                return true;
            }

            // ignore the intermediate and build directories
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
                #if PLATFORM_WINDOWS
                if (String_EndsWith(RelativePath, S(".rc"), false))
                {
                    if (String_EndsWith(RelativePath, S("icon.rc"), false))
                        return true;
                    
                    // TODO: really should use relative path here
                    u32 LastSlash = 0;
                    String_IndexOfLastPathSlash(FullPath, &LastSlash);

                    StringLocal(FilePath, MAX_PATH_LENGTH);
                    String_Append(&FilePath, StrSlice(FileName.Data, DotIndex));
                    String_Append(&FilePath, S(".res"));

                    StringLocal(ObjectPath, MAX_PATH_LENGTH);
                    const String Dir = StrSlice(FullPath.Data, LastSlash);
                    String_BuildPath(&ObjectPath, Dir, FilePath);
                }
                else
                #endif
                {
                    StringLocal(FilePath, MAX_PATH_LENGTH);
                    String_Append(&FilePath, RelativePath);
                    String_Append(&FilePath, S(".o"));

                    StringLocal(ObjectPath, MAX_PATH_LENGTH);
                    String_BuildPath(&ObjectPath, Data->Params->IntermediateDirectory, FilePath);

                    String_Concat(Data->CmdLine, S("\""), ObjectPath, S("\" "));
                }
            }
        }
    }

    return true;
}

bool C_Compile(const BuildParams* Params, u32* OutNumCompiled)
{
    StringLocal(SourceDir, MAX_PATH_LENGTH);
    String_BuildPath(&SourceDir, Params->RootDirectory, Params->SourceDirectory);

    // compile all .asm files first
    {
        CompileData UserData = { NULL, Params, OutNumCompiled, 0, true, NULL };
        Filesystem_IterateDirectory_Ex(SourceDir, AsmSourceFileDirectoryIterator, true, &UserData);
        if (!UserData.bSuccess)
        {
            return false;
        }
    }

    // compile all .c/c++ files
    {
        CompileData UserData = { C_DoCompile, Params, OutNumCompiled, 0, true, NULL };
        Filesystem_IterateDirectory_Ex(SourceDir, SourceFileDirectoryIterator, true, &UserData);
        if (!UserData.bSuccess)
        {
            return false;
        }
    }

    if (*OutNumCompiled == 0)
    {
        if (bQuietBuild) Logging_Enable();

        #ifndef HOOD
        LOG("\nNothing to compile - source files unchanged since last build");
        #else
        LOG("\nno work to do homie");
        #endif

        if (bQuietBuild) Logging_Disable();

        return true;
    }

    for each (PlatformHandle, Process, *Params->Processes)
    {
        const u32 ExitCode = Platform_WaitForProcessAndGetExitCode(Process);
        if (ExitCode != 0)
        {
            // todo: better wording?
            #ifndef HOOD
            LOG_ERROR("Compiler errors detected. See above errors to fix. Exit code for process: %u. Aborting build...", ExitCode);
            #else
            LOG_ERROR("seen some compiler errors homie, gon' stop right here");
            #endif

            return false;
        }
    }

    // compile resource files
    #if PLATFORM_WINDOWS
    if (Params->bHasRCProgram)
    {
        CompileData RcUserData = { NULL, Params, OutNumCompiled, 0, true, NULL };
        Filesystem_IterateDirectory_Ex(SourceDir, ResourceFileDirectoryIterator, true, &RcUserData);
        if (!RcUserData.bSuccess)
        {
            return false;
        }
    }
    #endif

    return true;
}

bool C_DoCompile(CompileData* Data, const String FullPath, const String RelativePath)
{
    const BuildParams* Params = Data->Params;
    TArray(PlatformHandle) Processes = *Params->Processes;
    //TArray(PlatformPipe) Pipes = *Params->Pipes;

    /*
    // exit if any process failed
    u32 i = 0;
    for each_i (i, PlatformHandle, Process, Processes)
    {
        const u32 ExitCode = Platform_GetExitCodeForProcess(Process);
        if (ExitCode == UINT32_MAX) continue;
        if (ExitCode != 0)
        {
            StringLocal(StdOutData, UINT16_MAX);
            u64 BytesRead = 0;
            Filesystem_ReadPipe(Pipes[i], StdOutData.Capacity, StdOutData.Data, &BytesRead);

            StdOutData.Length = Min((u32)BytesRead, StdOutData.Capacity);

            LOG_INLINE_ERROR("%S\n", StdOutData);

            Platform_CloseHandle(Pipes[i][0]);

            return false;
        }
    }
    */

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
            //Array_RemoveAt(Pipes, NULL, Index);
        }
    }

    Data->Index++;

    StringLocal(FilePath, MAX_PATH_LENGTH);
    if (Params->Type == AssemblyType_PCH)
    {
        String_Append(&FilePath, RelativePath);
        String_Append(&FilePath, S(".gch"));
    }
    else
    {
        String_Append(&FilePath, RelativePath);
        String_Append(&FilePath, S(".o"));
    }

    StringLocal(ObjectPath, MAX_PATH_LENGTH);
    if (Params->Type == AssemblyType_PCH)
    {
        String_BuildPath(&ObjectPath, Params->RootDirectory, Params->BuildDirectory, FilePath);
    }
    else
    {
        String_BuildPath(&ObjectPath, Params->IntermediateBaseDirectory, FilePath);
    }

    StringLocal(FullSourcePath, MAX_PATH_LENGTH+2);
    String_AppendChar(&FullSourcePath, '"');
    String_Append(&FullSourcePath, FullPath);
    String_AppendChar(&FullSourcePath, '"');

    StringLocal(ErrorLimit, 32);
    // todo: i could dynamically test whether this is supported by the compiler??
    /*
    if (Params->MaxErrors > 0)
    {
        if (String_IsEqual(Params->CompilerProgram, S("gcc"), false) ||
            String_IsEqual(Params->CompilerProgram, S("cc"), false) ||
            String_IsEqual(Params->CompilerProgram, S("g++"), false) ||
            String_Contains(Params->CompilerProgram, S("mingw32"), false) ||
            String_Contains(Params->CompilerProgram, S("-gcc"), false) ||
            String_Contains(Params->CompilerProgram, S("-g++"), false))
        {
            String_Format(&ErrorLimit, S("-fmax-errors=%i"), 32, Params->MaxErrors);
        }
        else
        {
            String_Format(&ErrorLimit, S("-ferror-limit=%i"), 32, Params->MaxErrors);
        }
    }
    */

    String AdditionalPlatformFlags = String_Null();

    #if !PLATFORM_WINDOWS
    if (Params->Type == AssemblyType_Library ||
        Params->Type == AssemblyType_DynamicLibrary)
    {
        AdditionalPlatformFlags = S("-fPIC -fvisibility=default");
    }
    else if (Params->Type == AssemblyType_Library ||
             Params->Type == AssemblyType_StaticLibrary)
    {
        AdditionalPlatformFlags = S("-fPIC");
    }
    else if (Params->Type == AssemblyType_Executable)
    {
        AdditionalPlatformFlags = S("-fPIE");
    }
    #endif

    // build cmd line string
    StringLocal(CmdLine, UINT16_MAX);
    String_BuildSeparator(&CmdLine, ' ', Params->CompilerProgram, S("-c"), FullSourcePath, Params->CompilerFlags, ErrorLimit, AdditionalPlatformFlags, Params->DefineFlags, Params->IncludeFlags);
    String_EatSpacesInlineFromEnd(&CmdLine);
    String_Append(&CmdLine, S(" -o \""));
    String_Append(&CmdLine, ObjectPath);
    String_Append(&CmdLine, S("\""));
    //String_Concat(&CmdLine, S(" -c -o "), S("\""), ObjectPath, S("\" "), Params->DefineFlags, S(" "), Params->IncludeFlags);

    if (Params->PCHPath.Length > 0)
    {
        // TODO: no hardcoded string compiler
        if (String_IsEqual(Params->CompilerProgram, S("clang"), false) ||
            String_IsEqual(Params->CompilerProgram, S("clang++"), false))
        {
            u32 LastDot = 0;
            bool bHasExt = String_IndexOfLastChar(Params->PCHPath, '.', &LastDot);

            const String Trimmed = bHasExt ? StrSlice(Params->PCHPath.Data, LastDot) : Params->PCHPath;

            String_Concat(&CmdLine, S(" -include-pch "), S("\""), Trimmed, S(".h.gch\""));
        }
    }

    u64 ObjectFileWriteTime = Filesystem_GetLastWriteTime(ObjectPath);
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

    Filesystem_NewFile(ObjectPath);

    if (bQuietBuild) Logging_Enable();

    #ifndef HOOD
    LOG("[%i/%i] Compiling %S", Data->Index, Params->NumSources, FullPath);
    #else
    LOG("compil'n %i o' %i %S", Data->Index, Params->NumSources, FullPath);
    #endif

    if (bQuietBuild) Logging_Disable();

    if (Params->bVerbose)
    {
        LOG("\n    %S\n", CmdLine);
    }

    //Clock CompileTime;
    //Clock_Start(&CompileTime);

    //PlatformPipe StdOutPipe = {0};
    //PlatformHandle Handle = Platform_RunCommand_Ex(CmdLine, Params->RootDirectory, &StdOutPipe);
    PlatformHandle Handle = Platform_RunCommand(CmdLine, Params->RootDirectory, String_Null());
    if (!Platform_IsValidHandle(Handle)) return false;
    //Platform_CloseHandle(StdOutPipe[1]);
    Array_Add(Processes, Handle);
    //Array_Add(Pipes, StdOutPipe);
    (*Data->NumCompiled)++;

    if (Params->bShouldWaitPerCompileProcess)
    {
        const u32 ExitCode = Platform_WaitForProcessAndGetExitCode(Handle);
        if (ExitCode != 0)
        {
            LOG_ERROR("Compiler errors detected. See above errors to fix. Exit code for process: %u. Aborting build...", ExitCode);
            return false;
        }

        /*
        Clock_Tick(&CompileTime);
        StringLocal(TimeString, 32);
        Clock_GetElapsedTime_ToString(&CompileTime, true, &TimeString);
        LOG("       - %S", TimeString);
        */
    }

    return true;
}

bool C_Link(const BuildParams* Params)
{
    if (NEVER(Params == NULL)) return false;

    if (Params->Type == AssemblyType_PCH)
    {
        return true;
    }

    StringLocal(SourceDir, MAX_PATH_LENGTH);
    String_BuildPath(&SourceDir, Params->RootDirectory, Params->SourceDirectory);

    if (Params->Type != AssemblyType_StaticLibrary)
    {
        StringLocal(CmdLine, UINT16_MAX);
        String_Append(&CmdLine, Params->CompilerProgram);
        String_AppendChar(&CmdLine, ' ');

        LinkData Data = { Params, &CmdLine };
        Filesystem_IterateDirectory_Ex(SourceDir, Link_SourceFileDirectoryIterator, true, &Data);

        String_BuildSeparator(&CmdLine, ' ',  Params->IconResFilePath, Params->VersionResFilePath, S(" -o \" "));

        StringLocal(BuildPath, MAX_PATH_LENGTH*2);
        String_BuildPath(&BuildPath, Params->RootDirectory, Params->BuildDirectory);
        String_AppendPathSeparator(&BuildPath);

        String SharedFlag = String_Null();

        if (Params->Type == AssemblyType_Library ||
            Params->Type == AssemblyType_DynamicLibrary)
        {
            SharedFlag = S("-shared");
        }

        String RunPathLinkFlag = String_Null();

        #if !PLATFORM_WINDOWS
        if (Params->bIsAssemblyExe)
        {
            RunPathLinkFlag = S("-Wl,-rpath,'$ORIGIN'");
        }
        #endif

        String_Append(&CmdLine, BuildPath);
        String_Append(&CmdLine, Params->AssemblyWithExt);
        String_Append(&CmdLine, S("\" "));

        String_BuildSeparator(&CmdLine, ' ',  Params->LinkerDefineFlags, Params->LinkerFlags, SharedFlag, RunPathLinkFlag, Params->Libraries, Params->LibraryDirectories, Params->bVerbose ? S("-v") : String_Null());
        String_EatSpacesInlineFromEnd(&CmdLine);

        if (bQuietBuild) Logging_Enable();

        #ifndef HOOD
        LOG("\nLinking %S", Params->AssemblyWithExt);
        #else
        LOG("\nlink'n it up: %S", Params->AssemblyWithExt);
        #endif

        if (bQuietBuild) Logging_Disable();

        if (Params->bVerbose)
        {
            if (bNoWordWrapLogging)
            {
                LOG("    %S", CmdLine);
            }
            else
            {
                LogString_WordWrapped(*Params->Arena, S("    "), CmdLine, false);
            }
        }

        PlatformHandle H = Platform_RunCommand(CmdLine, Params->RootDirectory, String_Null());
        if (!Platform_IsValidHandle(H)) return false;
        const u32 ExitCode = Platform_WaitForProcessAndGetExitCode(H);
        if (ExitCode != 0)
        {
            #ifndef HOOD
            LOG_ERROR("\nLinker errors detected. See above errors to fix. Exit code for process: %u. Aborting build...", ExitCode);
            #else
            LOG_ERROR("seen some linker errors homie. fix yo shit up, something aint linkin' right");
            #endif

            return false;
        }
    }

    // compile a static library if we're trying to make a shared one as well (for convenience sake)
    if (Params->Type == AssemblyType_Library ||
        Params->Type == AssemblyType_StaticLibrary)
    {
        StringLocal(CmdLine, UINT16_MAX);

        #if PLATFORM_WINDOWS
        if (String_IsEqual(Params->CompilerProgram, S("clang"), false) ||
            String_IsEqual(Params->CompilerProgram, S("clang++"), false))
        {
            String_Append(&CmdLine, S("llvm-ar r \""));
        }
        else
        {
            String_Append(&CmdLine, S("gcc-ar r \""));
        }
        #else
        String_Append(&CmdLine, S("ar rcs \""));
        #endif

        StringLocal(BuildPath, MAX_PATH_LENGTH);
        String_BuildPath(&BuildPath, Params->RootDirectory, Params->BuildDirectory);
        String_AppendPathSeparator(&BuildPath);

        String_Append(&CmdLine, BuildPath);

        StringLocal(LibFile, MAX_PATH_LENGTH);
        String_Append(&LibFile, Params->Assembly);

        if (Params->Type == AssemblyType_Library)
            String_Append(&LibFile, S("S"));

        #if PLATFORM_WINDOWS
        String_Append(&LibFile, S(".lib"));
        #else
        String_Append(&LibFile, S(".a"));
        #endif

        String_Append(&CmdLine, LibFile);
        String_Append(&CmdLine, S("\" "));

        LinkData Data = { Params, &CmdLine };
        Filesystem_IterateDirectory_Ex(SourceDir, Link_SourceFileDirectoryIterator, true, &Data);

        String_BuildSeparator(&CmdLine, ' ', Params->VersionResFilePath);
        String_EatSpacesInlineFromEnd(&CmdLine);

        if (bQuietBuild) Logging_Enable();

        #ifndef HOOD
        LOG("\nLinking %S [static]", LibFile);
        #else
        LOG("\nstatic link'n it up: %S", LibFile);
        #endif

        if (bQuietBuild) Logging_Disable();
        
        if (Params->bVerbose)
        {
            if (bNoWordWrapLogging)
            {
                LOG("    %S", CmdLine);
            }
            else
            {
                LogString_WordWrapped(*Params->Arena, S("    "), CmdLine, false);
            }
        }

        PlatformHandle H = Platform_RunCommand(CmdLine, Params->RootDirectory, String_Null());
        if (!Platform_IsValidHandle(H)) return false;
        u32 ExitCode = Platform_WaitForProcessAndGetExitCode(H);
        if (ExitCode != 0)
        {
            #ifndef HOOD
            LOG_ERROR("\nLinker errors detected. See above errors to fix. Exit code for process: %u. Aborting build...", ExitCode);
            #else
            LOG_ERROR("seen some linker errors homie. fix yo shit up, something aint linkin' right");
            #endif
            return false;
        }
    }

    // generate a .def file if we are building a dll file (windows only)
    #if PLATFORM_WINDOWS
    if (Platform_FindProgram(S("dumpbin")))
    {
        if (Params->Type == AssemblyType_Library ||
            Params->Type == AssemblyType_DynamicLibrary)
        {
            StringLocal(CmdLine, 8192);
            String_Append(&CmdLine, S("dumpbin /EXPORTS /NOLOGO /OUT:\""));

            StringLocal(BuildPath, 512);
            String_BuildPath(&BuildPath, Params->RootDirectory, Params->BuildDirectory);
            String_AppendPathSeparator(&BuildPath);

            String_Append(&CmdLine, BuildPath);
            String_Append(&CmdLine, Params->Assembly);
            String_Append(&CmdLine, S(".def\" "));

            String_Append(&CmdLine, S("\""));
            String_Append(&CmdLine, BuildPath);
            String_Append(&CmdLine, Params->Assembly);
            String_Append(&CmdLine, S(".dll\""));

            PlatformHandle H = Platform_RunCommand(CmdLine, Params->RootDirectory, String_Null());
            if (!Platform_IsValidHandle(H)) return false;
            u32 ExitCode = Platform_WaitForProcessAndGetExitCode(H);
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
    }
    #endif // PLATFORM_WINDOWS

    return true;
}

bool IsSource(const String Extension)
{
    return  String_IsEqual(Extension, S(".c"), false) ||
            String_IsEqual(Extension, S(".cc"), false) ||
            String_IsEqual(Extension, S(".cxx"), false) ||
            String_IsEqual(Extension, S(".c++"), false) ||
            String_IsEqual(Extension, S(".cpp"), false) ||
            String_IsEqual(Extension, S(".asm"), false) ||
            String_IsEqual(Extension, S(".s"), false) ||
            String_IsEqual(Extension, S(".spp"), false)
            #if PLATFORM_WINDOWS
            || String_IsEqual(Extension, S(".rc"), false)
            || String_IsEqual(Extension, S(".manifest"), false);
            #elif PLATFORM_APPLE
            || String_IsEqual(Extension, S(".m"), false)
            || String_IsEqual(Extension, S(".mm"), false);
            #else
            ;
            #endif
}

bool IsHeader(const String Extension)
{
    return  String_IsEqual(Extension, S(".h"), false) ||
            String_IsEqual(Extension, S(".hh"), false) ||
            String_IsEqual(Extension, S(".hpp"), false) ||
            String_IsEqual(Extension, S(".hxx"), false) ||
            String_IsEqual(Extension, S(".h++"), false) ||
            String_IsEqual(Extension, S(".inc"), false) ||
            String_IsEqual(Extension, S(".inl"), false) ||
            String_IsEqual(Extension, S(".ipp"), false);
}


////////////////////////////////////

// MSVC BACKEND

////////////////////////////////////


// TODO: dont call vcvarsall.bat every time, pass the lib and include directories to the compiler instead

#if PLATFORM_WINDOWS

/// TODO: if multithreaded and more than on soruce file. use /MP and call cl.exe only once

bool MSVC_DoCompile(CompileData* Data, const String FullPath, const String RelativePath);

internal bool AsmSourceFileDirectoryIterator_MSVC(const String FullPath, const String RelativePath, const String FileName, u64 FileSize, bool bIsDirectory, void* UserData)
{
    if (FileSize > 0)
    {
        if (String_StartsWith(FileName, S("__"), false))
        {
            return true;
        }

        CompileData* Data = UserData;
        const BuildParams* Params = Data->Params;

        if (String_EndsWith(RelativePath, S(".asm"), false) &&
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
            String_Append(&FilePath, S(".obj"));

            StringLocal(ObjectFilePath, MAX_PATH_LENGTH);
            String_BuildPath(&ObjectFilePath, Params->IntermediateBaseDirectory, FilePath);

            u64 ObjectFileWriteTime = Filesystem_GetLastWriteTime(ObjectFilePath);
            u64 SourceFileWriteTime = Filesystem_GetLastWriteTime(FullPath);

            if (SourceFileWriteTime >= ObjectFileWriteTime)
            {
                StringLocal(CmdLine, Kibibytes(4));

                // todo: not this, something different
                #if PLATFORM_64_BIT
                String_Append(&CmdLine, S("ml64 /nologo /c /Fo\""));
                #else
                String_Append(&CmdLine, S("ml /nologo /c /Fo\""));
                #endif

                StringLocal(ObjectPath, MAX_PATH_LENGTH);
                String_BuildPath(&ObjectPath, Params->IntermediateDirectory);
                String_EatPathSeparatorsInlineFromEnd(&ObjectPath);

                String_Append(&CmdLine, ObjectPath);
                String_Append(&CmdLine, S("\\\\\" "));

                StringLocal(SourcePath, MAX_PATH_LENGTH);
                String_BuildPath(&SourcePath, Params->SourceDirectory, RelativePath);
                String_BuildSeparator(&CmdLine, ' ', Params->AssemblerFlags, SourcePath);
                String_AppendSpace(&CmdLine);

                String_Append(&CmdLine, Params->IncludeFlags);

                if (Params->bVerbose) LOG("    CMD: %S", CmdLine);

                // todo: parallelize this
                PlatformHandle H = Platform_RunCommand(CmdLine, Params->RootDirectory, String_Null());
                if (!Platform_IsValidHandle(H)) return false;
                const u32 ExitCode = Platform_WaitForProcessAndGetExitCode(H);
                if (ExitCode != 0)
                {
                    Data->bSuccess = false;
                    return false;
                }
            }
            else
            {
                #ifndef HOOD
                LOG("[Skipping] %S", FullPath);
                #else
                LOG("skip'n dis shit %S", FullPath);
                #endif
            }
        }
    }

    return true;
}

internal bool Link_SourceFileDirectoryIterator_MSVC(const String FullPath, const String RelativePath, const String FileName, u64 FileSize, bool bIsDirectory, void* UserData)
{
    if (FileSize > 0)
    {
        if (String_StartsWith(FileName, S("__"), false))
        {
            return true;
        }

        LinkData* Data = (LinkData*)UserData;

        u32 DotIndex = 0;
        String_IndexOfLastChar(FileName, '.', &DotIndex);

        const String Extension = StrShiftF(FileName, DotIndex);

        if (String_EndsWith(Extension, S(".manifest"), false))
        {
            return true;
        }

        if (IsSource(Extension))
        {
            if (String_IndexOfFirstPathSlash(RelativePath, NULL))
            {
                // todo: update this?
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

                if (String_EndsWith(RelativePath, S(".rc"), false))
                {
                    if (String_EndsWith(RelativePath, S("icon.rc"), false))
                        return true;

                    u32 LastSlash = 0;
                    String_IndexOfLastPathSlash(FullPath, &LastSlash);

                    StringLocal(FilePath, MAX_PATH_LENGTH);
                    String_Append(&FilePath, StrSlice(FileName.Data, LastDot));
                    String_Append(&FilePath, S(".res"));

                    const String Dir = StrSlice(FullPath.Data, LastSlash);
                    String_BuildPath(&ObjectPath, Dir, FilePath);
                }
                else
                {

                    StringLocal(FilePath, MAX_PATH_LENGTH);
                    // todo: make asm behave the same
                    if (String_EndsWith(FileName, S(".asm"), false))
                    {
                        String_IndexOfLastChar(FileName, '.', &LastDot);
                        String_Append(&FilePath, StrSlice(FileName.Data, LastDot));
                    }
                    else
                    {
                        u32 LastPathDot = 0;
                        String_IndexOfLastChar(RelativePath, '.', &LastPathDot);
                        String_Append(&FilePath, StrSlice(RelativePath.Data, LastPathDot));
                    }
                    String_Append(&FilePath, S(".obj"));

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

bool MSVC_Compile(const BuildParams* Params, u32* OutNumCompiled)
{
    if (NEVER(Params == NULL)) return false;

    StringLocal(SourceDir, MAX_PATH_LENGTH);
    String_BuildPath(&SourceDir, Params->RootDirectory, Params->SourceDirectory);

    // compile all .asm files first
    {
        CompileData UserData = { NULL, Params, OutNumCompiled, 0, true, NULL };
        // TODO: rework, ugly
        bool bMASM = String_IsEqual(Params->AsmProgram, S("ml"), false) ||
                     String_IsEqual(Params->AsmProgram, S("ml64"), false);
        Filesystem_IterateDirectory_Ex(SourceDir, bMASM ? AsmSourceFileDirectoryIterator_MSVC : AsmSourceFileDirectoryIterator, true, &UserData);
        if (!UserData.bSuccess)
        {
            return false;
        }
    }

    // compile all .c files
    {
        CompileData UserData = { MSVC_DoCompile, Params, OutNumCompiled, 0, true, NULL };
        Filesystem_IterateDirectory_Ex(SourceDir, SourceFileDirectoryIterator, true, &UserData);
        if (!UserData.bSuccess)
        {
            return false;
        }
    }

    if (*OutNumCompiled == 0)
    {
        if (bQuietBuild) Logging_Enable();

        #ifndef HOOD
        LOG("\nNothing to compile - source files unchanged since last build");
        #else
        LOG("\nno work to do homie");
        #endif

        if (bQuietBuild) Logging_Disable();

        return true;
    }

    for each (PlatformHandle, Process, *Params->Processes)
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
    if (Params->bHasRCProgram)
    {
        CompileData RcUserData = { NULL, Params, OutNumCompiled, 0, true, NULL };
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
    ASSERT(Data != NULL);
    ASSERT(Data->Params != NULL);

    const BuildParams* Params = Data->Params;
    TArray(PlatformHandle) Processes = *Params->Processes;

    // exit if any process failed
    /*
    u32 i = 0;
    for each_i (i, PlatformHandle, Process, Processes)
    {
        const u32 ExitCode = Platform_GetExitCodeForProcess(Process);
        if (ExitCode == UINT32_MAX) continue;
        if (ExitCode != 0)
        {
            StringLocal(StdOutData, UINT16_MAX);
            u64 BytesRead = 0;
            Filesystem_ReadPipe(Pipes[i], StdOutData.Capacity, StdOutData.Data, &BytesRead);

            StdOutData.Length = Min((u32)BytesRead, StdOutData.Capacity);

            LOG_INLINE_ERROR("%S", StdOutData);

            Platform_CloseHandle(Pipes[i][0]);

            return false;
        }
    }
    */

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
    String_Append(&CmdLine, Params->CompilerProgram);
    String_Append(&CmdLine, S(" /nologo /c "));

    u32 LastSlash = 0;
    String_IndexOfLastPathSlash(RelativePath, &LastSlash);

    u32 LastDot = 0;
    String_IndexOfLastChar(RelativePath, '.', &LastDot);

    StringLocal(FilePath, MAX_PATH_LENGTH);

    if (Params->Type == AssemblyType_PCH)
    {
        String_BuildPath(&FilePath, StrSlice(RelativePath.Data, LastSlash), Params->Assembly);
        String_Append(&FilePath, S(".pch"));
    }
    else
    {
        String_Append(&FilePath, StrSlice(RelativePath.Data, LastDot));
        String_Append(&FilePath, S(".obj"));
    }

    // build object path
    StringLocal(ObjectFilePath, MAX_PATH_LENGTH);
    if (Params->Type == AssemblyType_PCH)
    {
        String_BuildPath(&ObjectFilePath, Params->RootDirectory, Params->BuildDirectory, FilePath);
    }
    else
    {
        String_BuildPath(&ObjectFilePath, Params->IntermediateBaseDirectory, FilePath);
        //String_BuildPath(&ObjectFilePath, Params->RootDirectory, Params->IntermediateDirectory, FilePath);
    }

    Filesystem_ConvertRelativeToAbsolutePath(&ObjectFilePath);

    //String FinalFullPath     = FullPath;
    String FinalRelativePath = RelativePath;

    bool bIsCppHeader = String_EndsWith(RelativePath, S(".hh"), false) ||
                        String_EndsWith(RelativePath, S(".hpp"), false) ||
                        String_EndsWith(RelativePath, S(".hxx"), false) ||
                        String_EndsWith(RelativePath, S(".h++"), false);

    // generate a source file for the precompiled header (if it doesnt exist yet)
    StringLocal(PchSourceFile, MAX_PATH_LENGTH);
    StringLocal(RelativePathCopy, MAX_PATH_LENGTH);
    String_Copy(&RelativePathCopy, StrSlice(RelativePath.Data, LastDot));
    if (Params->Type == AssemblyType_PCH)
    {
        const String Exts[] = { S(".c"), S(".cc"), S(".cxx"), S(".c++"), S(".cpp") };
        bool bAnyPchSourceExists = false;
        for (u8 i = 0; i < SArray_Capacity(Exts); i++)
        {
            String_Empty(&PchSourceFile);
            String_BuildPath(&PchSourceFile, Params->RootDirectory, Params->SourceDirectory, RelativePathCopy);
            String_Append(&PchSourceFile, Exts[i]);
            if (Filesystem_DoesFileExist(PchSourceFile))
            {
                String_Append(&RelativePathCopy, Exts[i]);
                bAnyPchSourceExists = true;
                break;
            }
        }

        if (!bAnyPchSourceExists)
        {
            String_Append(&RelativePathCopy, bIsCppHeader ? S(".cpp") : S(".c"));
            String_Empty(&PchSourceFile);
            String_BuildPath(&PchSourceFile, Params->RootDirectory, Params->SourceDirectory, RelativePathCopy);

            if (!Filesystem_DoesFileExist(PchSourceFile))
            {
                FileHandle f = FileHandle_Null();
                if (Filesystem_Open(PchSourceFile, FileMode_Write, &f))
                {
                    Filesystem_WriteLineFormatted(f, S("#include \"%S\"\n"), NULL, RelativePath);
                    Filesystem_Close(&f);
                }
            }
        }

        //FinalFullPath     = PchSourceFile;
        FinalRelativePath = RelativePathCopy;
    }

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
    String_BuildPath(&FullSourcePath, Params->SourceDirectory, FinalRelativePath);

    String_AppendChar(&CmdLine, '"');
    String_Append(&CmdLine, FullSourcePath);
    String_AppendChar(&CmdLine, '"');
    String_AppendSpace(&CmdLine);

    String_BuildSeparator(&CmdLine, ' ', Params->CompilerFlags, Params->DefineFlags, Params->IncludeFlags);
    String_EatSpacesInlineFromEnd(&CmdLine);

    if (Params->Type == AssemblyType_PCH)
    {
        String_AppendSpace(&CmdLine);

        String_Append(&CmdLine, S("/Yc\""));
        String_Append(&CmdLine, RelativePath);
        String_Append(&CmdLine, S("\" "));

        String_Append(&CmdLine, S("/Fp\""));
        String_Append(&CmdLine, ObjectFilePath);
        String_Append(&CmdLine, S("\""));
    }
    else
    {
        if (Params->PCHPath.Length > 0)
        {
            String_AppendSpace(&CmdLine);

            bool bHasExt = String_IndexOfLastChar(Params->PCHPath, '.', &LastDot);

            const String Trimmed = bHasExt ? StrSlice(Params->PCHPath.Data, LastDot) : Params->PCHPath;

            String_Append(&CmdLine, S("/Yu\""));
            if (Params->PCHHeaderPath.Length > 0)
            {
                String_Append(&CmdLine, Params->PCHHeaderPath);
            }
            else
            {
                // test all header extensions
                bool bAnyFound = false;
                const String Exts[] = { S(".h"), S(".hh"), S(".hpp"), S(".hxx"), S(".h++") };
                for (u8 i = 0; i < SArray_Capacity(Exts); i++)
                {
                    StringLocal(Test, MAX_PATH_LENGTH);
                    String_Append(&Test, Trimmed);
                    String_Append(&Test, Exts[i]);
                    if (Filesystem_DoesFileExist(Test))
                    {
                        String_Append(&CmdLine, Trimmed);
                        String_Append(&CmdLine, Exts[i]);
                        bAnyFound = true;
                        break;
                    }
                }

                // hardcode the extension as failsafe
                if (!bAnyFound)
                {
                    String_Append(&CmdLine, Trimmed);
                    String_Append(&CmdLine, S(".h"));
                }
            }

            String_Append(&CmdLine, S("\" "));

            String_Append(&CmdLine, S("/Fp\""));
            String_Append(&CmdLine, Trimmed);
            String_Append(&CmdLine, S(".pch\""));
        }
    }

    StringLocal(ObjectPath, MAX_PATH_LENGTH);
    String_BuildPath(&ObjectPath, Params->IntermediateDirectory, StrSlice(RelativePath.Data, LastSlash));
    String_EatPathSeparatorsInlineFromEnd(&ObjectPath);

    StringLocal(FullObjectPath, MAX_PATH_LENGTH);
    String_BuildPath(&FullObjectPath, Params->RootDirectory, ObjectPath);
    Filesystem_ConvertRelativeToAbsolutePath(&FullObjectPath);

    Filesystem_OpenDirectory(FullObjectPath);

    if (ObjectPath.Length > 0)
    {
        String_Append(&CmdLine, S(" /Fo\""));
        String_Append(&CmdLine, ObjectPath);
        String_Append(&CmdLine, S("\\\\\""));
    }

    if (bQuietBuild) Logging_Enable();

    if (Params->bShouldWaitPerCompileProcess)
        LOG("[%i/%i] Compiling %S", Data->Index, Params->NumSources, FullPath);

    if (bQuietBuild) Logging_Disable();

    if (Params->bVerbose)
    {
        LOG("    %S\n", CmdLine);
    }

    PlatformHandle H = Platform_RunCommand(CmdLine, Params->RootDirectory, String_Null());
    if (!Platform_IsValidHandle(H)) return false;
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

internal void Internal_ParseAndLogLinkerOutput_MSVC(String StdOutData)
{
    if (StdOutData.Length == 0) return;

    String LastObjFile = String_Null();

    u32 Offset = 0;
    while (Offset < StdOutData.Length)
    {
        String PipeDataSlice = StrShiftF(StdOutData, Offset);

        u32 NewLineIndex = 0;
        if (String_IndexOfFirstNewline(PipeDataSlice, &NewLineIndex))
        {
            Offset += NewLineIndex+1;
            if (PipeDataSlice.Data[NewLineIndex] == '\r')
            {
                Offset++;
            }

            String Line = String_EatSpaces(StrSlice(PipeDataSlice.Data, NewLineIndex));
            {
                if (String_StartsWith(Line, S("LINK : "), true))
                {
                    /*
                    if (String_IsValid(LastObjFile) && String_IsEqual(LastObjFile, S("Linker Warnings"), true))
                    {
                        LOG_INLINE("    ");
                    }
                    else
                    {
                        LOG_INLINE_WARNING("\nLinker Warnings\n    ");
                    }
                    */

                    String Trimmed = StrShiftF(Line, 7);

                    if (String_StartsWith(Trimmed, S("warning LNK"), true))
                    {
                        //LastObjFile = S("Linker Warnings");

                        u32 ColonIndex = 0;

                        String Meta = StrShiftF(Trimmed, 8);
                        String_IndexOfChar(Meta, ':', &ColonIndex);
                        LOG_INLINE_WARNING("[WARNING] %S", StrSlice(Meta.Data, ColonIndex));

                        String_IndexOfChar(Trimmed, ':', &ColonIndex);
                        String Message = StrShiftF(Trimmed, ColonIndex+1);

                        const String SymbolDefineWarningPhrases[] =
                        {
                            S("defined in"),
                            S("is imported by"),
                            S("in function")
                        };

                        String TempLine = Message;
                        for (u8 i = 0; i < SArray_Capacity(SymbolDefineWarningPhrases); i++)
                        {
                            u32 Index = 0;
                            if (String_IndexOfSubstring(TempLine, SymbolDefineWarningPhrases[i], true, &Index))
                            {
                                String FirstPart = StrSlice(TempLine.Data, Index);
                                if (i == 0)
                                    LOG_INLINE(" |%S\n                        ", FirstPart);
                                else
                                    LOG_INLINE("%S\n                        ", FirstPart);

                                String SecondPart = StrShiftF(TempLine, Index);
                                TempLine = SecondPart;

                                if (i == SArray_Capacity(SymbolDefineWarningPhrases)-1)
                                {
                                    // mute the name mangled part
                                    u32 QuestionIndex = 0;
                                    if (String_IndexOfSubstring(SecondPart, S("(?"), true, &QuestionIndex))
                                    {
                                        LOG_INLINE("%S", StrSlice(SecondPart.Data, QuestionIndex));
                                        //LOG_MUTE("%S\n", StrShiftF(SecondPart, QuestionIndex));
                                        LOG_LINE_BREAK();
                                        LOG_LINE_BREAK();
                                    }
                                    else
                                    {
                                        LOG("%S\n", SecondPart);
                                    }
                                }
                            }
                            else
                            {
                                if (i == 0)
                                    LOG(" |%S", TempLine);
                                else
                                    LOG("%S", TempLine);

                                break;
                            }
                        }
                    }
                    else if (String_StartsWith(Trimmed, S("fatal error LNK"), true) ||
                             String_StartsWith(Trimmed, S("error LNK"), true))
                    {
                        u32 ColonIndex = 0;
                        u32 Blah = String_StartsWith(Trimmed, S("fatal error LNK"), true) ? 12 : 6;
                        String Meta = StrShiftF(Trimmed, Blah);
                        String_IndexOfChar(Meta, ':', &ColonIndex);
                        LOG_INLINE_ERROR("[ERROR] %S", StrSlice(Meta.Data, ColonIndex));

                        String_IndexOfChar(Trimmed, ':', &ColonIndex);
                        String Message = StrShiftF(Trimmed, ColonIndex+1);
                        LOG(" |%S", Message);
                    }
                    else
                    {
                        LOG("%S", Trimmed);
                    }
                }
                else if (String_StartsWith(Line, S("Creating library "), true))
                {
                    LOG("\n%S", Line);
                }
                else if (String_EndsWith(Line, S(" unresolved externals"), true))
                {
                    LOG("\n%S", Line);
                }
                else
                {
                    u32 ColonIndex = 0;
                    if (String_IndexOfSubstring(Line, S(" : "), true, &ColonIndex))
                    {
                        String ObjFile = StrSlice(Line.Data, ColonIndex+1);

                        if (String_IsValid(LastObjFile) && String_IsEqual(LastObjFile, ObjFile, true))
                        {
                            LOG_INLINE("    ");
                        }
                        else
                        {
                            LOG_INLINE_WARNING("\n%S\n    ", ObjFile);
                        }

                        LastObjFile = ObjFile;

                        String Trimmed = String_EatSpaces(StrShiftF(Line, ColonIndex+2));
                        {
                            if (String_StartsWith(Trimmed, S("error LNK"), true))
                            {
                                ColonIndex = 0;
                                String Meta = StrShiftF(Trimmed, 6);
                                String_IndexOfChar(Meta, ':', &ColonIndex);
                                LOG_INLINE_ERROR("[ERROR] %S", StrSlice(Meta.Data, ColonIndex));

                                String_IndexOfChar(Trimmed, ':', &ColonIndex);
                                String Message = StrShiftF(Trimmed, ColonIndex+1);

                                u32 ReferencedIndex = 0;
                                if (String_IndexOfSubstring(Message, S("referenced in function"), true, &ReferencedIndex))
                                {
                                    String FirstPart = StrSlice(Message.Data, ReferencedIndex);
                                    String SecondPart = StrShiftF(Message, ReferencedIndex);

                                    // mute the name mangled part
                                    u32 QuestionIndex = 0;
                                    if (String_IndexOfSubstring(FirstPart, S("(?"), true, &QuestionIndex))
                                    {
                                        LOG_INLINE(" |%S", StrSlice(FirstPart.Data, QuestionIndex));
                                        //LOG_MUTE("%S", StrShiftF(FirstPart, QuestionIndex));

                                        LOG_LINE_BREAK();
                                    }
                                    else
                                    {
                                        LOG(" |%S", FirstPart);
                                    }

                                    QuestionIndex = 0;
                                    if (String_IndexOfSubstring(SecondPart, S("(?"), true, &QuestionIndex))
                                    {
                                        LOG_INLINE("                          %S", StrSlice(SecondPart.Data, QuestionIndex));
                                        //LOG_MUTE("%S", StrShiftF(SecondPart, QuestionIndex));
                                        LOG_LINE_BREAK();
                                    }
                                    else
                                    {
                                        LOG("                          %S", SecondPart);
                                    }
                                }
                                else
                                {
                                    String FirstPart = Message;

                                    // mute the name mangled part
                                    u32 QuestionIndex = 0;
                                    if (String_IndexOfSubstring(FirstPart, S("(?"), true, &QuestionIndex))
                                    {
                                        LOG_INLINE(" |%S", StrSlice(FirstPart.Data, QuestionIndex));
                                        //LOG_MUTE("%S", StrShiftF(FirstPart, QuestionIndex));
                                        LOG_LINE_BREAK();
                                    }
                                    else
                                    {
                                        LOG(" |%S", FirstPart);
                                    }
                                }
                            }
                        }
                    }
                    else
                    {
                        LOG("%S", Line);
                    }
                }
            }
        }
        else
        {
            break;
        }
    }
}

internal void Internal_ProcessLinkerOutput_MSVC(PlatformPipe StdOutHandle)
{
    Platform_CloseHandle(StdOutHandle[1]);

    StringLocal(StdOutData, UINT16_MAX);

    // TODO: think about logging speed
    do
    {
        StringLocal(PipeData, UINT16_MAX);

        usize BytesRead = 0;
        if (!Filesystem_ReadPipe(StdOutHandle, PipeData.Capacity, PipeData.Data, &BytesRead))
            break;
        
        if (BytesRead == 0)
            break;

        PipeData.Length = Min((u32)BytesRead, StdOutData.Capacity);

        if (PipeData.Length + StdOutData.Length > StdOutData.Capacity)
        {
            Internal_ParseAndLogLinkerOutput_MSVC(StdOutData);
            String_Empty(&StdOutData);
        }

        String_Append(&StdOutData, PipeData);
    }
    while (1);

    Internal_ParseAndLogLinkerOutput_MSVC(StdOutData);

    Platform_CloseHandle(StdOutHandle[0]);
}

bool MSVC_Link(const BuildParams* Params)
{
    if (NEVER(Params == NULL)) return false;

    if (Params->Type == AssemblyType_PCH)
    {
        return true;
    }

    StringLocal(SourceDir, MAX_PATH_LENGTH);
    String_BuildPath(&SourceDir, Params->RootDirectory, Params->SourceDirectory);

    StringLocal(BuildPath, MAX_PATH_LENGTH);
    String_BuildPath(&BuildPath, Params->RootDirectory, Params->BuildDirectory);
    String_AppendPathSeparator(&BuildPath);

    bool bIsExe = Params->Type == AssemblyType_Executable;
    bool bIsDLL = Params->Type == AssemblyType_Library || Params->Type == AssemblyType_DynamicLibrary;
    bool bIsLib = Params->Type == AssemblyType_Library || Params->Type == AssemblyType_StaticLibrary;

    StringLocal(CmdLine, UINT16_MAX);

    if (bIsExe || bIsDLL)
    {
        String_Append(&CmdLine, S("link"));
        if (bIsDLL)
            String_Append(&CmdLine, S(" /dll"));
        String_Append(&CmdLine, S(" /nologo "));

        String_BuildSeparator(&CmdLine, ' ', Params->LinkerDefineFlags, Params->LinkerFlags, Params->IconResFilePath, Params->VersionResFilePath, Params->Libraries, Params->LibraryDirectories); //, AllObjFiles);
        String_AppendSpace(&CmdLine);

        LinkData Data = { Params, &CmdLine };
        Filesystem_IterateDirectory_Ex(SourceDir, Link_SourceFileDirectoryIterator_MSVC, true, &Data);

        String_EatSpacesInlineFromEnd(&CmdLine);

        if (Params->PCHPath.Length > 0)
        {
            String_AppendSpace(&CmdLine);

            String_Append(&CmdLine, S("\""));

            if (Params->PCHHeaderPath.Length > 0)
            {
                u32 LastDot = 0;
                bool bHasExt = String_IndexOfLastChar(Params->PCHHeaderPath, '.', &LastDot);
                const String Trimmed = bHasExt ? StrSlice(Params->PCHHeaderPath.Data, LastDot) : Params->PCHHeaderPath;

                String_Append(&CmdLine, Trimmed);
                String_Append(&CmdLine, S(".obj"));
            }
            else
            {
                u32 LastDot = 0;
                bool bHasExit = String_IndexOfLastChar(Params->PCHPath, '.', &LastDot);
                const String Trimmed = bHasExit ? StrSlice(Params->PCHPath.Data, LastDot) : Params->PCHPath;

                String_Append(&CmdLine, Trimmed);
                String_Append(&CmdLine, S(".obj"));
            }

            String_Append(&CmdLine, S("\""));
        }

        String_Concat(&CmdLine, S(" /OUT:\""), BuildPath, Params->AssemblyWithExt, S("\"")); // make this first then the flags?

        if (bQuietBuild) Logging_Enable();

        LOG("\nLinking %S", Params->AssemblyWithExt);

        if (bQuietBuild) Logging_Disable();

        if (Params->bVerbose)
        {
            if (bNoWordWrapLogging)
            {
                LOG("    %S", CmdLine);
            }
            else
            {
                LogString_WordWrapped(*Params->Arena, S("    "), CmdLine, false);
            }
        }

        // TODO: switch between fancy and non fancy logging

        //PlatformHandle Handle = Platform_RunCommand(CmdLine, Params->RootDirectory, String_Null());
        //if (!Platform_IsValidHandle(Handle)) return false;
        //u32 ExitCode = Platform_WaitForProcessAndGetExitCode(Handle);

        PlatformPipe StdOutHandle = {0};
        PlatformHandle H = Platform_RunCommand_Ex(CmdLine, Params->RootDirectory, &StdOutHandle);
        if (!Platform_IsValidHandle(H)) return false;

        Internal_ProcessLinkerOutput_MSVC(StdOutHandle);

        u32 ExitCode = Platform_WaitForProcessAndGetExitCode(H);

        if (ExitCode != 0)
        {
            #ifndef HOOD
            LOG_ERROR("\nLinker errors detected. See above errors to fix. Exit code for process: %u. Aborting build...", ExitCode);
            #else
            LOG_ERROR("seen some linker errors homie. fix yo shit up, something aint linkin' right");
            #endif
            
            return false;
        }
    }
    
    if (bIsLib)
    {
        String_Empty(&CmdLine);
        String_Append(&CmdLine, S("lib /nologo "));

        String_BuildSeparator(&CmdLine, ' ', Params->LinkerFlags, Params->Libraries, Params->LibraryDirectories, Params->VersionResFilePath);
        String_AppendSpace(&CmdLine);

        LinkData Data = { Params, &CmdLine };
        Filesystem_IterateDirectory_Ex(SourceDir, Link_SourceFileDirectoryIterator_MSVC, true, &Data);

        String_Append(&CmdLine, S("/OUT:\""));
        String_Append(&CmdLine, BuildPath);

        StringLocal(LibFile, MAX_PATH_LENGTH);
        String_Append(&LibFile, Params->Assembly);

        if (Params->Type == AssemblyType_Library)
            String_Append(&LibFile, S("S"));
        
        String_Append(&LibFile, S(".lib"));

        String_Append(&CmdLine, LibFile);
        String_AppendChar(&CmdLine, '"');

        if (bQuietBuild) Logging_Enable();

        LOG("\nLinking %S [static]", LibFile);

        if (bQuietBuild) Logging_Disable();

        if (Params->bVerbose)
        {
            if (bNoWordWrapLogging)
            {
                LOG("    %S", CmdLine);
            }
            else
            {
                LogString_WordWrapped(*Params->Arena, S("    "), CmdLine, false);
            }
        }

        PlatformHandle Handle = Platform_RunCommand(CmdLine, Params->RootDirectory, String_Null());
        if (!Platform_IsValidHandle(Handle)) return false;
        u32 ExitCode = Platform_WaitForProcessAndGetExitCode(Handle);
        if (ExitCode != 0)
        {
            #ifndef HOOD
            LOG_ERROR("\nLinker errors detected. See above errors to fix. Exit code for process: %u. Aborting build...", ExitCode);
            #else
            LOG_ERROR("seen some linker errors homie. fix yo shit up, something aint linkin' right");
            #endif
            
            return false;
        }
    }

    // generate a .def file if we are building a dll file
    if (bIsDLL)
    {
        String_Empty(&CmdLine);
        String_Append(&CmdLine, S("dumpbin /EXPORTS /NOLOGO /OUT:\""));

        String_Append(&CmdLine, BuildPath);
        String_Append(&CmdLine, Params->Assembly);
        String_Append(&CmdLine, S(".def\" \""));

        String_Append(&CmdLine, BuildPath);
        String_Append(&CmdLine, Params->Assembly);
        String_Append(&CmdLine, S(".dll\""));

        PlatformHandle H = Platform_RunCommand(CmdLine, Params->RootDirectory, String_Null());
        if (!Platform_IsValidHandle(H)) return false;
        u32 ExitCode = Platform_WaitForProcessAndGetExitCode(H);

        if (ExitCode != 0)
        {
            #ifndef HOOD
            LOG_ERROR("Dumpbin errors detected. See above errors to fix. Aborting build...");
            #else
            LOG_ERROR("seen some dumpbin errors homie. fix yo shit up, something aint right");
            #endif
            return false;
        }
    }

    return true;
}

LinearAllocator GMSVCFindAllocator = {0};

void* MSVC_Find_Allocate(usize Size)
{
    return LinearAllocator_Allocate(&GMSVCFindAllocator, Size);
}

void MSVC_Find_Release(void* Memory)
{
    // don't free anything
}

#else
bool MSVC_Compile(UNUSED const BuildParams* Params, UNUSED u32* OutNumCompiled) { return true; }
bool MSVC_Link(UNUSED const BuildParams* Params) { return true; }
#endif // PLATFORM_WINDOWS
