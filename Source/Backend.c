// Copyright (c) Artisan Softworks
// Licensed under the BSD 3-Clause License. See the LICENSE file for details.

#ifndef UNITY_BUILD
#include "Backend.h"

#include "Core/Globals.h"
#include "Core/Allocators.h"
#include "Core/Array.h"
#include "Core/StringUtils.h"
#include "Core/Filesystem.h"
#include "Core/Platform.h"
#include "Core/Log.h"
#endif

bool C_DoCompile(CompileData* Data, const String FullPath, const String RelativePath);

static void LogCompilingFile(u32 Index, u32 NumSources, String FullPath)
{
    if (bQuietBuild) { Logging_Enable(); }
    #ifndef HOOD
    u8 NumDigits1 = Integer_CountDigits(NumSources);
    u8 NumDigits2 = Integer_CountDigits(Index);
    u8 Diff = (NumDigits1 - NumDigits2) + 1;

    StringLocal(Spaces, 128);
    Spaces.Length = Diff;
    String_Fill(&Spaces, ' ');

    LOG("[%i/%i]%SCompiling %S", Index, NumSources, Spaces, FullPath);
    #else
    LOG("compil'n %i o' %i %S", Index, NumSources, FullPath);
    #endif
    if (bQuietBuild) { Logging_Disable(); }
}

static bool AsmSourceFileDirectoryIterator(const String FullPath, const String RelativePath, const String FileName, u64 FileSize, bool bIsDirectory, void* UserData)
{
    UNUSED_PARAM(bIsDirectory);

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

            StringLocal(FilePath, MAX_PATH_LENGTH);

            u32 LastDot = 0;
            if (String_IndexOfLastChar(FileName, '.', &LastDot))
            {
                String_Append(&FilePath, StrSlice(FileName.Data, LastDot));
            }
            else
            {
                String_Copy(&FilePath, FileName);
            }
            
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

                if (Params->bVerbose) { LOG("    CMD: %S", CmdLine); }

                LOG("Assembling %S", FullPath);

                // todo: parallelize this
                PlatformHandle H = Platform_RunCommand(CmdLine, Params->RootDirectory, String_Null());
                if (!Platform_IsValidHandle(H)) { return false; }
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
    UNUSED_PARAM(bIsDirectory);

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
            if (String_StartsWith(RelativePath, Data->Params->IntermediateDirectory, false) ||
                String_StartsWith(RelativePath, Data->Params->BuildDirectory, false))
            {
                return true;
            }
        }

        // ignore the intermediate and build directories
        /* TODO: verify if commenting this doesnt break anything?
        if (String_IndexOfFirstPathSlash(RelativePath, NULL))
        {
            if (RelativePath.Length == Data->Params->IntermediateBaseDirectory.Length)
            {
                if (String_StartsWith(RelativePath, Data->Params->IntermediateDirectory, false))
                {

                    return true;
                }
            }

            if (RelativePath.Length == Data->Params->BuildDirectory.Length)
            {
                if (String_StartsWith(RelativePath, Data->Params->BuildDirectory, false))
                {
                    return true;
                }
            }
        }
        */

        u32 DotIndex = 0;
        bool bHasExt = String_IndexOfLastChar(FileName, '.', &DotIndex);

        const String Extension = bHasExt ? StrShiftF(FileName, DotIndex) : String_Null();

        if (String_IsEqual(Extension, S(".asm"), false) ||
            String_IsEqual(Extension, S(".rc"), false) ||
            String_IsEqual(Extension, S(".manifest"), false))
        {
            // we will build these later
            return true;
        }

        const bool bIsPCH = Data->Params->Type == AssemblyType_PCH;
        const bool bIsSource = bIsPCH ? IsHeader(Extension) : IsSource(Extension);
        const bool bIsCustomSource = IsSourceCustom(Extension, Data->Params->SourceFileExtensions);

        if (bIsSource || bIsCustomSource)
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
static bool ResourceFileDirectoryIterator(const String FullPath, const String RelativePath, const String FileName, u64 FileSize, bool bIsDirectory, void* UserData)
{
    UNUSED_PARAM(bIsDirectory);

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
            if (String_StartsWith(RelativePath, Data->Params->IntermediateDirectory, false) ||
                String_StartsWith(RelativePath, Data->Params->BuildDirectory, false))
            {
                return true;
            }
        }

        // ignore the intermediate and build directories
        /*
        if (String_IndexOfFirstPathSlash(RelativePath, NULL))
        {
            if (RelativePath.Length == Data->Params->IntermediateBaseDirectory.Length)
            {
                if (String_StartsWith(RelativePath, Data->Params->IntermediateDirectory, false))
                {
                    return true;
                }
            }

            if (RelativePath.Length == Data->Params->BuildDirectory.Length)
            {
                if (String_StartsWith(RelativePath, Data->Params->BuildDirectory, false))
                {
                    return true;
                }
            }
        }
        */

        if (String_EndsWith(FileName, S(".rc"), false))
        {
            if (String_EndsWith(RelativePath, S("icon.rc"), false))
            {
                return true;
            }

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
#endif

bool RC_Compile(const BuildParams* Params, const String FullRCPath, String* OutResPath)
{
    bool bSuccess = true;

    #if PLATFORM_WINDOWS
    StringLocal(CmdLine, 1024);
    String_AppendChar(&CmdLine, '"');
    String_Append(&CmdLine, Params->RCProgramPath);
    String_AppendChar(&CmdLine, '"');

    if (Params->RCProgramFlags.Length > 0)
    {
        String_AppendSpace(&CmdLine);
        String_Append(&CmdLine, Params->RCProgramFlags);
    }

    const bool bWindres = String_IsEqual(Params->RCProgram, S("windres"), false);

    u32 LastDot = 0;
    bool bHasDot = String_IndexOfLastChar(FullRCPath, '.', &LastDot);

    StringLocal(ResPath, MAX_PATH_LENGTH);
    String_Append(&ResPath, S("\""));
    String_Append(&ResPath, bHasDot ? StrSlice(FullRCPath.Data, LastDot) : FullRCPath);
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
    
    if (Params->bVerbose) { LOG("    %S", CmdLine); }

    PlatformHandle H = Platform_RunProcess(Params->RCProgramPath, CmdLine, Params->RootDirectory, String_Null());
    if (!Platform_IsValidHandle(H)) { return false; }
    u32 ExitCode = Platform_WaitForProcessAndGetExitCode(H);
    if (ExitCode != 0)
    {
        bSuccess = false;
    }
    #endif

    return bSuccess;
}

static bool Link_SourceFileDirectoryIterator(const String FullPath, const String RelativePath, const String FileName, u64 FileSize, bool bIsDirectory, void* UserData)
{
    UNUSED_PARAM(bIsDirectory);

    if (FileSize > 0)
    {
        if (String_StartsWith(FileName, S("__"), false))
        {
            return true;
        }

        LinkData* Data = UserData;

        u32 DotIndex = 0;
        bool bHasExt = String_IndexOfLastChar(FileName, '.', &DotIndex);

        const String Extension = bHasExt ? StrShiftF(FileName, DotIndex) : String_Null();

        if (String_EndsWith(Extension, S(".manifest"), false))
        {
            return true;
        }

        const bool bIsSource       = IsSource(Extension);
        const bool bIsCustomSource = IsSourceCustom(Extension, Data->Params->SourceFileExtensions);

        if (bIsSource || bIsCustomSource)
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
                    {
                        return true;
                    }
                    
                    // TODO: really should use relative path here
                    u32 LastSlash = 0;
                    xx String_IndexOfLastPathSlash(FullPath, &LastSlash);

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

                    const String Ext = String_IsValid(Data->Params->CompilerObjectExt) ? Data->Params->CompilerObjectExt : S(".o");
                    if (!String_IsFirst(Ext, '.'))
                    {
                        String_AppendChar(&FilePath, '.');
                    }

                    String_Append(&FilePath, Ext);

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
        Filesystem_IterateDirectory_Ex(SourceDir, &AsmSourceFileDirectoryIterator, true, &UserData);
        if (!UserData.bSuccess)
        {
            return false;
        }
    }

    // compile all source files
    {
        CompileData UserData = { &C_DoCompile, Params, OutNumCompiled, 0, true, NULL };
        Filesystem_IterateDirectory_Ex(SourceDir, &SourceFileDirectoryIterator, true, &UserData);
        if (!UserData.bSuccess)
        {
            return false;
        }
    }

    if (*OutNumCompiled == 0)
    {
        if (bQuietBuild) { Logging_Enable(); }

        //TODO: say how long ago the last build was like -> (5.3 secs ago)
        #ifndef HOOD
        LOG("\nNothing to compile - source files unchanged since last build");
        #else
        LOG("\nno work to do homie");
        #endif

        if (bQuietBuild) { Logging_Disable(); }

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
        Filesystem_IterateDirectory_Ex(SourceDir, &ResourceFileDirectoryIterator, true, &RcUserData);
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
        if (!Params->bDumpObjFilesInOneDirectory)
        {
            String_Append(&FilePath, RelativePath);
        }
        
        String_Append(&FilePath, S(".gch"));
    }
    else
    {
        if (!Params->bDumpObjFilesInOneDirectory)
        {
            String_Append(&FilePath, RelativePath);
        }

        // TODO: msvc
        const String Ext = String_IsValid(Params->CompilerObjectExt) ? Params->CompilerObjectExt : S(".o");
        if (!String_IsFirst(Ext, '.'))
        {
            String_AppendChar(&FilePath, '.');
        }

        String_Append(&FilePath, Ext);
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
    else
    {
        // no action required
    }
    #endif

    // build cmd line string
    StringLocal(CmdLine, UINT16_MAX);
    if (Params->Type == AssemblyType_CompilerObject) // TODO: if (custom compiler) ?? 
    {
        String_Empty(&ObjectPath);

        // int/relativepath/assmeblyprefix|filename.no_ext|assemblypostfix|ext

        String FileName = RelativePath;
        String RelativePathNoFile = String_Null();
        u32 LastSlash = 0;
        if (String_IndexOfLastPathSlash(RelativePath, &LastSlash))
        {
            RelativePathNoFile = StrSlice(RelativePath.Data, LastSlash);
            FileName = StrShiftF(RelativePath, LastSlash+1);
        }

        u32 LastDot = 0;
        if (String_IndexOfLastChar(FileName, '.', &LastDot))
        {
            FileName = StrSlice(FileName.Data, LastDot);
        }

        if (Params->bDumpObjFilesInOneDirectory)
        {
            RelativePathNoFile = String_Null();
        }

        String_BuildPath(&ObjectPath, Params->IntermediateBaseDirectory, RelativePathNoFile);
        String_AppendPathSeparator(&ObjectPath);
        String_Append(&ObjectPath, Params->AssemblyPrefix);
        String_Append(&ObjectPath, FileName);
        String_Append(&ObjectPath, Params->AssemblyPostfix);
        String_Append(&ObjectPath, Params->Extension);

        String_AppendChar(&CmdLine, '"');
        String_Append(&CmdLine, Params->CompilerPath);
        String_AppendChar(&CmdLine, '"');

        String_BuildSeparator(&CmdLine, ' ', FullSourcePath, Params->CompilerFlags, Params->CompilerOutputFlag);
        xx String_EatSpacesInlineFromEnd(&CmdLine);
        String_Append(&CmdLine, S(" \""));
        String_Append(&CmdLine, ObjectPath);
        String_Append(&CmdLine, S("\""));
    }
    else
    {
        String_AppendChar(&CmdLine, '"');
        String_Append(&CmdLine, Params->CompilerPath);
        String_AppendChar(&CmdLine, '"');

        String_BuildSeparator(&CmdLine, ' ', S("-c"), FullSourcePath, Params->CompilerFlags, AdditionalPlatformFlags, Params->DefineFlags, Params->IncludeFlags);
        xx String_EatSpacesInlineFromEnd(&CmdLine);
        String_Append(&CmdLine, S(" -o \""));
        String_Append(&CmdLine, ObjectPath);
        String_Append(&CmdLine, S("\""));
    }

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

    xx Filesystem_NewFile(ObjectPath);

    LogCompilingFile(Data->Index, Params->NumSources, FullPath);

    if (Params->bVerbose)
    {
        LOG("\n    %S\n", CmdLine);
    }

    PlatformHandle Handle = Platform_RunProcess(Params->CompilerPath, CmdLine, Params->RootDirectory, String_Null());
    if (!Platform_IsValidHandle(Handle)) { return false; }
    Array_Add(Processes, Handle);
    (*Data->NumCompiled) += 1;

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
    if (NEVER(Params == NULL)) { return false; }

    if (Params->Type == AssemblyType_PCH)
    {
        return true;
    }

    StringLocal(SourceDir, MAX_PATH_LENGTH);
    String_BuildPath(&SourceDir, Params->RootDirectory, Params->SourceDirectory);

    if (Params->Type != AssemblyType_StaticLibrary)
    {
        StringLocal(CmdLine, UINT16_MAX);
        String_AppendChar(&CmdLine, '"');
        String_Append(&CmdLine, Params->LinkerPath);
        String_AppendChar(&CmdLine, '"');
        String_AppendChar(&CmdLine, ' ');

        LinkData Data = { Params, &CmdLine };
        Filesystem_IterateDirectory_Ex(SourceDir, &Link_SourceFileDirectoryIterator, true, &Data);

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

        StringLocal(RunPathLinkFlag, MAX_PATH_LENGTH);

        #if !PLATFORM_WINDOWS
        if (Params->bIsAssemblyExe)
        {
            String ChosenRPath = S("$ORIGIN");
            if (String_IsValid(Params->RPath))
            {
                ChosenRPath = Params->RPath;
            }
            String_AppendF(&RunPathLinkFlag, S("-Wl,-rpath,\"%S\""), ChosenRPath);
        }
        #endif

        // additional linker settings that are annoying to specify in the build file for all 3 major compilers
        // as clang, gcc and msvc have different ways of doing this
        // (and for all the different platforms as well)
        StringLocal(AdditionalFlags, 512);
        if (Params->Type == AssemblyType_Executable)
        {
            const String NoStd         = Params->bLinkerNoStd ? S("-nostdlib -nostdlib++") : String_Null();
            const String NoDefaultLibs = Params->bLinkerNoDefaultLibs ? S("-nodefaultlibs") : String_Null();

            // TODO: linux, macos and bsd
            // --entry=entry
            // -Wl,-stack_size,0x800000
            #if PLATFORM_WINDOWS
            bool bCustomEntry     = String_IsValid(Params->LinkerEntryPoint);
            bool bCustomSubsystem = String_IsValid(Params->LinkerSubsystem);
            bool bCustomStack     = String_IsValid(Params->LinkerStack);
            bool bAnyValid        = bCustomEntry || bCustomSubsystem || bCustomStack;

            StringLocal(WlFlags, 256);
            StringLocal(XlinkerFlags, 256);
            if (bAnyValid)
            {
                bool bIsClang = String_IsEqual(Params->CompilerProgram, S("clang"), false) ||
                                String_IsEqual(Params->CompilerProgram, S("clang++"), false);

                String_Append(&WlFlags, S("-Wl,"));

                if (bCustomEntry)
                {
                    if (bIsClang)
                    {
                        String_AppendF(&WlFlags, S("-entry:%S,"), Params->LinkerEntryPoint);
                    }
                    else // GCC
                    {
                        String_AppendF(&WlFlags, S("--entry,%S,"), Params->LinkerEntryPoint);
                    }
                }

                if (bCustomSubsystem)
                {
                    if (bIsClang)
                    {
                        String_AppendF(&WlFlags, S("-subsystem:%S,"), Params->LinkerSubsystem);
                    }
                    else // GCC
                    {
                        String_AppendF(&WlFlags, S("--subsystem,%S,"), Params->LinkerSubsystem);
                    }
                }

                if (bCustomStack)
                {
                    u32 Space = 0;
                    xx String_IndexOfFirstWhitespace(Params->LinkerStack, &Space);

                    String Reserve = Params->LinkerStack;
                    String Commit  = Params->LinkerStack;
                    if (Space)
                    {
                        Reserve = StrSlice (Params->LinkerStack.Data, Space);
                        Commit  = StrShiftF(Params->LinkerStack, Space+1);
                    }

                    if (bIsClang)
                    {
                        String_AppendF(&XlinkerFlags, S("-Xlinker /stack:%S,%S"), Reserve, Commit);
                    }
                    else // GCC
                    {
                        // i dont know how to add Commit here.
                        String_AppendF(&WlFlags, S("--stack,%S,"), Reserve);
                    }
                }
            }

            xx String_EatCharInlineFromEnd(&WlFlags, ',');

            String_BuildSeparator(&AdditionalFlags, ' ', NoStd, NoDefaultLibs, WlFlags, XlinkerFlags);
            #else
            String_BuildSeparator(&AdditionalFlags, ' ', NoStd, NoDefaultLibs);
            #endif
        }

        String_Append(&CmdLine, BuildPath);
        String_Append(&CmdLine, Params->AssemblyWithExt);
        String_Append(&CmdLine, S("\" "));

        String_BuildSeparator(&CmdLine, ' ',  Params->LinkerDefineFlags, Params->LinkerFlags, AdditionalFlags, SharedFlag, RunPathLinkFlag, Params->Libraries, Params->LibraryDirectories, Params->bVerbose ? S("-v") : String_Null());
        xx String_EatSpacesInlineFromEnd(&CmdLine);

        if (bQuietBuild) { Logging_Enable(); }

        #ifndef HOOD
        LOG("Linking %S", Params->AssemblyWithExt);
        #else
        LOG("link'n it up: %S", Params->AssemblyWithExt);
        #endif

        if (bQuietBuild) { Logging_Disable(); }

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

        PlatformHandle H = Platform_RunProcess(Params->CompilerPath, CmdLine, Params->RootDirectory, String_Null());
        if (!Platform_IsValidHandle(H)) { return false; }
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
        
        String_AppendChar(&CmdLine, '"');
        String_Append(&CmdLine, Params->ArchiverPath);
        String_AppendChar(&CmdLine, '"');
        String_AppendSpace(&CmdLine);

        #if PLATFORM_WINDOWS
        if (String_IsEqual(Params->CompilerProgram, S("clang"), false) ||
            String_IsEqual(Params->CompilerProgram, S("clang++"), false))
        {
            String_Append(&CmdLine, S("r \""));
        }
        #else
        String_Append(&CmdLine, S("rcs \""));
        #endif

        /*
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
        */

        StringLocal(BuildPath, MAX_PATH_LENGTH);
        String_BuildPath(&BuildPath, Params->RootDirectory, Params->BuildDirectory);
        String_AppendPathSeparator(&BuildPath);

        String_Append(&CmdLine, BuildPath);

        StringLocal(LibFile, MAX_PATH_LENGTH);
        String_Append(&LibFile, Params->Assembly);

        if (Params->Type == AssemblyType_Library)
        {
            String_Append(&LibFile, S("S"));
        }

        #if PLATFORM_WINDOWS
        String_Append(&LibFile, S(".lib"));
        #else
        String_Append(&LibFile, S(".a"));
        #endif

        String_Append(&CmdLine, LibFile);
        String_Append(&CmdLine, S("\" "));

        LinkData Data = { Params, &CmdLine };
        Filesystem_IterateDirectory_Ex(SourceDir, &Link_SourceFileDirectoryIterator, true, &Data);

        String_BuildSeparator(&CmdLine, ' ', Params->VersionResFilePath);
        xx String_EatSpacesInlineFromEnd(&CmdLine);

        if (bQuietBuild) { Logging_Enable(); }

        #ifndef HOOD
        LOG("Linking %S [static]", LibFile);
        #else
        LOG("static link'n it up: %S", LibFile);
        #endif

        if (bQuietBuild) { Logging_Disable(); }
        
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

        PlatformHandle H = Platform_RunProcess(Params->ArchiverPath, CmdLine, Params->RootDirectory, String_Null());
        if (!Platform_IsValidHandle(H)) { return false; }
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
            //String_AppendChar(&CmdLine, '"');
            //String_Append(&CmdLine, Params->DumpBinPath);
            //String_AppendChar(&CmdLine, '"');
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
            if (!Platform_IsValidHandle(H)) { return false; }
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

bool IsSourceCustom(const String Extension, const StringList CustomExtensions)
{
    for each_str_list (CustomExtensions)
    {
        if (String_IsEqual(Extension, It.String, false))
        {
            return true;
        }
    }

    return false;
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

bool IsCppSource(const String Extension)
{
    return  String_IsEqual(Extension, S(".cc"), false) ||
            String_IsEqual(Extension, S(".cxx"), false) ||
            String_IsEqual(Extension, S(".c++"), false) ||
            String_IsEqual(Extension, S(".cpp"), false);
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
            String_IsEqual(Extension, S(".ipp"), false) ||
            String_IsEqual(Extension, S(".ixx"), false) ||
            String_IsEqual(Extension, S(".tpp"), false) ||
            String_IsEqual(Extension, S(".txx"), false);
}

bool IsCppHeader(const String Extension)
{
    return  String_IsEqual(Extension, S(".hh"), false) ||
            String_IsEqual(Extension, S(".hpp"), false) ||
            String_IsEqual(Extension, S(".hxx"), false) ||
            String_IsEqual(Extension, S(".h++"), false) ||
            String_IsEqual(Extension, S(".ipp"), false) ||
            String_IsEqual(Extension, S(".ixx"), false) ||
            String_IsEqual(Extension, S(".tpp"), false) ||
            String_IsEqual(Extension, S(".txx"), false);
}


////////////////////////////////////

// MSVC BACKEND

////////////////////////////////////


// TODO: dont call vcvarsall.bat every time, pass the lib and include directories to the compiler instead

LinearAllocator GMSVCFindAllocator = {0};

#if PLATFORM_WINDOWS

/// TODO: if multithreaded and more than on soruce file. use /MP and call cl.exe only once

bool MSVC_DoCompile(CompileData* Data, const String FullPath, const String RelativePath);

static bool AsmSourceFileDirectoryIterator_MSVC(const String FullPath, const String RelativePath, const String FileName, u64 FileSize, bool bIsDirectory, void* UserData)
{
    UNUSED_PARAM(bIsDirectory);

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
            bool bHasDot = String_IndexOfLastChar(FileName, '.', &LastDot);

            StringLocal(FilePath, MAX_PATH_LENGTH);
            String_Append(&FilePath, bHasDot ? StrSlice(FileName.Data, LastDot) : FileName);
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
                xx String_EatPathSeparatorsInlineFromEnd(&ObjectPath);

                String_Append(&CmdLine, ObjectPath);
                String_Append(&CmdLine, S("\\\\\" "));

                StringLocal(SourcePath, MAX_PATH_LENGTH);
                String_BuildPath(&SourcePath, Params->SourceDirectory, RelativePath);
                String_BuildSeparator(&CmdLine, ' ', Params->AssemblerFlags, SourcePath);
                String_AppendSpace(&CmdLine);

                String_Append(&CmdLine, Params->IncludeFlags);

                if (Params->bVerbose) { LOG("    CMD: %S", CmdLine); }

                // todo: parallelize this
                PlatformHandle H = Platform_RunCommand(CmdLine, Params->RootDirectory, String_Null());
                if (!Platform_IsValidHandle(H)) { return false; }
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

static bool Link_SourceFileDirectoryIterator_MSVC(const String FullPath, const String RelativePath, const String FileName, u64 FileSize, bool bIsDirectory, void* UserData)
{
    UNUSED_PARAM(bIsDirectory);

    if (FileSize > 0)
    {
        if (String_StartsWith(FileName, S("__"), false))
        {
            return true;
        }

        LinkData* Data = UserData;

        u32 DotIndex = 0;
        bool bHasExt = String_IndexOfLastChar(FileName, '.', &DotIndex);

        const String Extension = bHasExt ? StrShiftF(FileName, DotIndex) : String_Null();

        if (String_EndsWith(Extension, S(".manifest"), false))
        {
            return true;
        }

        const bool bIsSource       = IsSource(Extension);
        const bool bIsCustomSource = IsSourceCustom(Extension, Data->Params->SourceFileExtensions);

        if (bIsSource || bIsCustomSource)
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
                bool bHasDot = String_IndexOfLastChar(FileName, '.', &LastDot);

                if (String_EndsWith(RelativePath, S(".rc"), false))
                {
                    if (String_EndsWith(RelativePath, S("icon.rc"), false))
                    {
                        return true;
                    }

                    u32 LastSlash = 0;
                    bool bHasSlash = String_IndexOfLastPathSlash(FullPath, &LastSlash);

                    StringLocal(FilePath, MAX_PATH_LENGTH);
                    String_Append(&FilePath, bHasDot ? StrSlice(FileName.Data, LastDot) : FileName);
                    String_Append(&FilePath, S(".res"));

                    const String Dir = bHasSlash ? StrSlice(FullPath.Data, LastSlash) : FullPath;
                    String_BuildPath(&ObjectPath, Dir, FilePath);
                }
                else
                {
                    StringLocal(FilePath, MAX_PATH_LENGTH);
                    // todo: make asm behave the same
                    if (String_EndsWith(FileName, S(".asm"), false))
                    {
                        bHasDot = String_IndexOfLastChar(FileName, '.', &LastDot);
                        String_Append(&FilePath, bHasDot ? StrSlice(FileName.Data, LastDot) : FileName);
                    }
                    else
                    {
                        u32 LastPathDot = 0;
                        bHasDot = String_IndexOfLastChar(RelativePath, '.', &LastPathDot);
                        String_Append(&FilePath, bHasDot ? StrSlice(RelativePath.Data, LastPathDot) : RelativePath);
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
    if (NEVER(Params == NULL)) { return false; }

    StringLocal(SourceDir, MAX_PATH_LENGTH);
    String_BuildPath(&SourceDir, Params->RootDirectory, Params->SourceDirectory);

    // compile all .asm files first
    {
        CompileData UserData = { NULL, Params, OutNumCompiled, 0, true, NULL };
        // TODO: rework, ugly
        bool bMASM = String_IsEqual(Params->AsmProgram, S("ml"), false) ||
                     String_IsEqual(Params->AsmProgram, S("ml64"), false);
        Filesystem_IterateDirectory_Ex(SourceDir, bMASM ? &AsmSourceFileDirectoryIterator_MSVC : &AsmSourceFileDirectoryIterator, true, &UserData);
        if (!UserData.bSuccess)
        {
            return false;
        }
    }

    // compile all .c files
    {
        CompileData UserData = { &MSVC_DoCompile, Params, OutNumCompiled, 0, true, NULL };
        Filesystem_IterateDirectory_Ex(SourceDir, &SourceFileDirectoryIterator, true, &UserData);
        if (!UserData.bSuccess)
        {
            return false;
        }
    }

    if (*OutNumCompiled == 0)
    {
        if (bQuietBuild) { Logging_Enable(); }

        #ifndef HOOD
        LOG("\nNothing to compile - source files unchanged since last build");
        #else
        LOG("\nno work to do homie");
        #endif

        if (bQuietBuild) { Logging_Disable(); }

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
        Filesystem_IterateDirectory_Ex(SourceDir, &ResourceFileDirectoryIterator, true, &RcUserData);
        if (!RcUserData.bSuccess)
        {
            return false;
        }
    }

    return true;
}

// todo: compiling with cl makes this run serially? (happens on release mode only)

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
            //LOG("waiting multiple...");
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
    String_AppendChar(&CmdLine, '"');
    String_Append(&CmdLine, Params->CompilerPath);
    String_AppendChar(&CmdLine, '"');
    String_Append(&CmdLine, S(" /nologo /c "));

    u32 LastSlash = 0;
    bool bHasSlash = String_IndexOfLastPathSlash(RelativePath, &LastSlash);

    u32 LastDot = 0;
    bool bHasDot = String_IndexOfLastChar(RelativePath, '.', &LastDot);

    StringLocal(FilePath, MAX_PATH_LENGTH);

    if (Params->Type == AssemblyType_PCH)
    {
        String_BuildPath(&FilePath, bHasSlash ? StrSlice(RelativePath.Data, LastSlash) : String_Null(), Params->Assembly);
        String_Append(&FilePath, S(".pch"));
    }
    else
    {
        String_Append(&FilePath, bHasDot ? StrSlice(RelativePath.Data, LastDot) : RelativePath);
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

    xx Filesystem_ConvertRelativeToAbsolutePath(&ObjectFilePath);

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
                    xx Filesystem_WriteLineFormatted(f, S("#include \"%S\"\n"), NULL, RelativePath);
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

    StringLocal(WinSDKInclude, MAX_PATH_LENGTH*7); // 7 paths
    if (!Params->bWasVCVarsBatchRan)
    {
        if (Params->WindowsSDKIncludePath.Length)
        {
            String_Append(&WinSDKInclude, S("/I\""));
            String_Append(&WinSDKInclude, Params->WindowsSDKIncludePath);
            String_Append(&WinSDKInclude, S("\" "));

            String_Append(&WinSDKInclude, S("/I\""));
            String_Append(&WinSDKInclude, Params->WindowsSDKIncludePath);
            String_Append(&WinSDKInclude, S("\\shared\" "));

            String_Append(&WinSDKInclude, S("/I\""));
            String_Append(&WinSDKInclude, Params->WindowsSDKIncludePath);
            String_Append(&WinSDKInclude, S("\\ucrt\" "));

            String_Append(&WinSDKInclude, S("/I\""));
            String_Append(&WinSDKInclude, Params->WindowsSDKIncludePath);
            String_Append(&WinSDKInclude, S("\\um\" "));

            String_Append(&WinSDKInclude, S("/I\""));
            String_Append(&WinSDKInclude, Params->WindowsSDKIncludePath);
            String_Append(&WinSDKInclude, S("\\winrt\" "));

            String_Append(&WinSDKInclude, S("/I\""));
            String_Append(&WinSDKInclude, Params->WindowsSDKIncludePath);
            String_Append(&WinSDKInclude, S("\\cppwinrt\" "));
        }

        if (Params->VisualStudioIncludePath.Length)
        {
            String_Append(&WinSDKInclude, S("/I\""));
            String_Append(&WinSDKInclude, Params->VisualStudioIncludePath);
            String_Append(&WinSDKInclude, S("\" "));
        }
    }

    String_BuildSeparator(&CmdLine, ' ', Params->CompilerFlags, Params->DefineFlags, Params->IncludeFlags, WinSDKInclude);
    xx String_EatSpacesInlineFromEnd(&CmdLine);

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
    xx String_EatPathSeparatorsInlineFromEnd(&ObjectPath);

    StringLocal(FullObjectPath, MAX_PATH_LENGTH);
    String_BuildPath(&FullObjectPath, Params->RootDirectory, ObjectPath);
    xx Filesystem_ConvertRelativeToAbsolutePath(&FullObjectPath);

    if (!Filesystem_OpenDirectory(FullObjectPath))
    {
        return false;
    }

    if (ObjectPath.Length > 0)
    {
        String_Append(&CmdLine, S(" /Fo\""));
        String_Append(&CmdLine, ObjectPath);
        String_Append(&CmdLine, S("\\\\\""));
    }

    if (bQuietBuild) { Logging_Enable(); }

    if (Params->bShouldWaitPerCompileProcess)
    {
        LogCompilingFile(Data->Index, Params->NumSources, FullPath);
    }

    if (bQuietBuild) { Logging_Disable(); }

    if (Params->bVerbose)
    {
        LOG("    %S\n", CmdLine);
    }

    PlatformHandle H = Platform_RunProcess(Params->CompilerPath, CmdLine, Params->RootDirectory, String_Null());
    if (!Platform_IsValidHandle(H)) { return false; }
    Array_Add(Processes, H);
    (*Data->NumCompiled) += 1;

        //LOG_UINT(Array_Num(Processes));

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

static void Internal_ParseAndLogLinkerOutput_MSVC(String StdOutData)
{
    String LastObjFile = String_Null();

    u32 Offset = 0;
    while (1)
    {
        if (Offset >= StdOutData.Length)
        {
            break;
        }
    
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
                    String Trimmed = StrShiftF(Line, 7);

                    if (String_StartsWith(Trimmed, S("warning LNK"), true))
                    {
                        //LastObjFile = S("Linker Warnings");

                        u32 ColonIndex = 0;

                        String Meta = StrShiftF(Trimmed, 8);
                        bool bHasColon = String_IndexOfChar(Meta, ':', &ColonIndex);
                        LOG_INLINE_WARNING("[WARNING] %S", bHasColon ? StrSlice(Meta.Data, ColonIndex) : Meta);

                        ColonIndex = 0;
                        xx String_IndexOfChar(Trimmed, ':', &ColonIndex);
                        String Message = StrShiftF(Trimmed, ColonIndex+1);

                        const String SymbolDefineWarningPhrases[3] =
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
                                {
                                    LOG_INLINE(" |%S\n                        ", FirstPart);
                                }
                                else
                                {
                                    LOG_INLINE("%S\n                        ", FirstPart);
                                }

                                String SecondPart = StrShiftF(TempLine, Index);
                                TempLine = SecondPart;

                                if (i == SArray_Capacity(SymbolDefineWarningPhrases)-1)
                                {
                                    // mute the name mangled part
                                    u32 QuestionIndex = 0;
                                    if (String_IndexOfSubstring(SecondPart, S("(?"), true, &QuestionIndex))
                                    {
                                        LOG_INLINE("%S\n\n", StrSlice(SecondPart.Data, QuestionIndex));
                                        //LOG_MUTE("%S\n", StrShiftF(SecondPart, QuestionIndex));
                                        //LOG_LINE_BREAK();
                                        //LOG_LINE_BREAK();
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
                                {
                                    LOG(" |%S", TempLine);
                                }
                                else
                                {
                                    LOG("%S", TempLine);
                                }

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
                        bool bHasColon = String_IndexOfChar(Meta, ':', &ColonIndex);
                        LOG_INLINE_ERROR("[ERROR] %S", bHasColon ? StrSlice(Meta.Data, ColonIndex) : Meta);

                        ColonIndex = 0;
                        xx String_IndexOfChar(Trimmed, ':', &ColonIndex);
                        String Message = StrShiftF(Trimmed, ColonIndex+1);
                        LOG(" |%S", Message);
                    }
                    else
                    {
                        LOG("%S", Trimmed);
                    }
                }
                else if (String_StartsWith(Line, S("Creating library "), true) ||
                         String_EndsWith(Line, S(" unresolved externals"), true))
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
                            if (String_StartsWith(Trimmed, S("fatal error LNK"), true))
                            {
                                Trimmed = StrShiftF(Trimmed, 6);
                            }

                            if (String_StartsWith(Trimmed, S("error LNK"), true))
                            {
                                ColonIndex = 0;
                                String Meta = StrShiftF(Trimmed, 6);
                                bool bHasColon = String_IndexOfChar(Meta, ':', &ColonIndex);
                                LOG_INLINE_ERROR("[ERROR] %S", bHasColon ? StrSlice(Meta.Data, ColonIndex) : Meta);

                                ColonIndex = 0;
                                xx String_IndexOfChar(Trimmed, ':', &ColonIndex);
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
                            else
                            {
                                LOG("%S", Line);
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

static void Internal_ProcessLinkerOutput_MSVC(PlatformPipe StdOutHandle)
{
    Platform_CloseHandle(StdOutHandle[1]);

    StringLocal(StdOutData, UINT16_MAX);

    do
    {
        StringLocal(PipeData, UINT16_MAX);

        usize BytesRead = 0;
        if (!Filesystem_ReadPipe(StdOutHandle, PipeData.Capacity, PipeData.Data, &BytesRead))
        {
            break;
        }
        
        if (BytesRead == 0)
        {
            break;
        }

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
    if (NEVER(Params == NULL)) { return false; }

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

    StringLocal(WinSDKLibPaths, MAX_PATH_LENGTH*3);
    if (!Params->bWasVCVarsBatchRan)
    {
        if (Params->WindowsSDKLibUmPath.Length)
        {
            String_Append(&WinSDKLibPaths, S("/LIBPATH:\""));
            String_Append(&WinSDKLibPaths, Params->WindowsSDKLibUmPath);
            String_Append(&WinSDKLibPaths, S("\" "));
        }

        if (Params->WindowsSDKLibUcrtPath.Length)
        {
            String_Append(&WinSDKLibPaths, S("/LIBPATH:\""));
            String_Append(&WinSDKLibPaths, Params->WindowsSDKLibUcrtPath);
            String_Append(&WinSDKLibPaths, S("\" "));
        }

        if (Params->VisualStudioLibraryPath.Length)
        {
            String_Append(&WinSDKLibPaths, S("/LIBPATH:\""));
            String_Append(&WinSDKLibPaths, Params->VisualStudioLibraryPath);
            String_Append(&WinSDKLibPaths, S("\" "));
        }
    }

    StringLocal(CmdLine, UINT16_MAX);

    if (bIsExe || bIsDLL)
    {
        String_AppendChar(&CmdLine, '"');
        String_Append(&CmdLine, Params->LinkerPath);
        String_AppendChar(&CmdLine, '"');

        if (bIsDLL)
        {
            String_Append(&CmdLine, S(" /DLL"));
        }

        String_Append(&CmdLine, S(" /NOLOGO "));

        // additional linker settings that are annoying to specify in the build file for all 3 major compilers
        // as clang, gcc and msvc have different ways of doing this
        // (and for all the different platforms as well)
        StringLocal(AdditionalFlags, 512);
        if (bIsExe)
        {
            // todo: support
            // /NODEFAULTLIB:somelibrary /NODEFAULTLIB:anotherlibrary etc..
            // from this syntax: Linker.NoDefaultLibs somelibrary anotherlibrary
            const String NoDefaultLibs = Params->bLinkerNoDefaultLibs ? S("/NODEFAULTLIB ") : String_Null();

            String_Append(&AdditionalFlags, NoDefaultLibs);

            bool bCustomEntry     = String_IsValid(Params->LinkerEntryPoint);
            bool bCustomSubsystem = String_IsValid(Params->LinkerSubsystem);
            bool bCustomStack     = String_IsValid(Params->LinkerStack);
            bool bAnyValid        = bCustomEntry || bCustomSubsystem || bCustomStack;

            if (bAnyValid)
            {
                if (bCustomEntry)
                {
                    String_AppendF(&AdditionalFlags, S("/ENTRY:%S "), Params->LinkerEntryPoint);
                }

                if (bCustomSubsystem)
                {
                    String_AppendF(&AdditionalFlags, S("/SUBSYSTEM:%S "), Params->LinkerSubsystem);
                }

                if (bCustomStack)
                {
                    u32 Space = 0;
                    xx String_IndexOfFirstWhitespace(Params->LinkerStack, &Space);

                    String Reserve = Params->LinkerStack;
                    String Commit  = Params->LinkerStack;
                    if (Space)
                    {
                        Reserve = StrSlice (Params->LinkerStack.Data, Space);
                        Commit  = StrShiftF(Params->LinkerStack, Space+1);
                    }

                    String_AppendF(&AdditionalFlags, S("/STACK:%S,%S "), Reserve, Commit);
                }
            }

            xx String_EatSpacesInlineFromEnd(&AdditionalFlags);
        }

        String_BuildSeparator(&CmdLine, ' ', Params->LinkerDefineFlags, Params->LinkerFlags, AdditionalFlags, Params->IconResFilePath, Params->VersionResFilePath, Params->Libraries, Params->LibraryDirectories, WinSDKLibPaths);
        String_AppendSpace(&CmdLine);

        LinkData Data = { Params, &CmdLine };
        Filesystem_IterateDirectory_Ex(SourceDir, &Link_SourceFileDirectoryIterator_MSVC, true, &Data);

        xx String_EatSpacesInlineFromEnd(&CmdLine);

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

        if (bQuietBuild) { Logging_Enable(); }

        LOG("Linking %S", Params->AssemblyWithExt);

        if (bQuietBuild) { Logging_Disable(); }

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

        /*
        PlatformHandle Handle = Platform_RunProcess(Params->LinkerPath, CmdLine, Params->RootDirectory, String_Null());
        //PlatformHandle Handle = Platform_RunCommand(CmdLine, Params->RootDirectory, String_Null());
        if (!Platform_IsValidHandle(Handle)) return false;
        */

        PlatformPipe StdOutHandle = {0};
        PlatformHandle Handle = Platform_RunProcess_Ex(Params->LinkerPath, CmdLine, Params->RootDirectory, &StdOutHandle);
        if (!Platform_IsValidHandle(Handle)) { return false; }

        Internal_ProcessLinkerOutput_MSVC(StdOutHandle);

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
    
    if (bIsLib)
    {
        String_Empty(&CmdLine);
        String_AppendChar(&CmdLine, '"');
        String_Append(&CmdLine, Params->ArchiverPath);
        String_AppendChar(&CmdLine, '"');
        String_Append(&CmdLine, S(" /nologo "));

        String_BuildSeparator(&CmdLine, ' ', Params->LinkerFlags, Params->Libraries, Params->LibraryDirectories, Params->VersionResFilePath, WinSDKLibPaths);
        String_AppendSpace(&CmdLine);

        LinkData Data = { Params, &CmdLine };
        Filesystem_IterateDirectory_Ex(SourceDir, &Link_SourceFileDirectoryIterator_MSVC, true, &Data);

        String_Append(&CmdLine, S("/OUT:\""));
        String_Append(&CmdLine, BuildPath);

        StringLocal(LibFile, MAX_PATH_LENGTH);
        String_Append(&LibFile, Params->Assembly);

        if (Params->Type == AssemblyType_Library)
        {
            String_Append(&LibFile, S("S"));
        }
        
        String_Append(&LibFile, S(".lib"));

        String_Append(&CmdLine, LibFile);
        String_AppendChar(&CmdLine, '"');

        if (bQuietBuild) { Logging_Enable(); }

        LOG("Linking %S [static]", LibFile);

        if (bQuietBuild) { Logging_Disable(); }

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

        PlatformHandle Handle = Platform_RunProcess(Params->ArchiverPath, CmdLine, Params->RootDirectory, String_Null());
        if (!Platform_IsValidHandle(Handle)) { return false; }
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
    if (bIsDLL && Platform_FindProgram(S("dumpbin")))
    {
        String_Empty(&CmdLine);
        //String_AppendChar(&CmdLine, '"');
        //String_Append(&CmdLine, Params->DumpBinPath);
        //String_AppendChar(&CmdLine, '"');
        String_Append(&CmdLine, S("dumpbin /EXPORTS /NOLOGO /OUT:\""));

        String_Append(&CmdLine, BuildPath);
        String_Append(&CmdLine, Params->Assembly);
        String_Append(&CmdLine, S(".def\" \""));

        String_Append(&CmdLine, BuildPath);
        String_Append(&CmdLine, Params->Assembly);
        String_Append(&CmdLine, S(".dll\""));

        PlatformHandle H = Platform_RunCommand(CmdLine, Params->RootDirectory, String_Null());
        if (!Platform_IsValidHandle(H)) { return false; }
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

void* MSVC_Find_Allocate(usize Size)
{
    return LinearAllocator_Allocate(&GMSVCFindAllocator, Size);
}

void MSVC_Find_Release(void* Memory)
{
    UNUSED_PARAM(Memory);

    // don't free anything
}

#else
bool MSVC_Compile(UNUSED const BuildParams* Params, UNUSED u32* OutNumCompiled) { return true; }
bool MSVC_Link(UNUSED const BuildParams* Params) { return true; }
void* MSVC_Find_Allocate(usize Size) { return NULL; }
void MSVC_Find_Release(void* Memory) {}
#endif // PLATFORM_WINDOWS
