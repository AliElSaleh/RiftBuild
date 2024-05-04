#include "Backend.h"

#include "Structures/Array.h"
#include "String/StringUtils.h"
#include "Platform/Filesystem.h"
#include "Platform/Platform.h"
#include "Log.h"

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

bool C_DoCompile(CompileData* Data, const String FullPath, const String RelativePath);

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
                // we will build this later
                return true;
            }

            if (FilterSourceFile(Data->Params->RootDirectory, Data->Params->SourceDirectory, FullPath, RelativePath, Data->Params->WhitelistFiles, Data->Params->BlacklistFiles, Data->Params->WhitelistDirectories, Data->Params->BlacklistDirectories))
            {
                // compile this file
                if (!C_DoCompile(Data, FullPath, RelativePath))
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

        Data->Index++;
    }

    return true;
}
#endif

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
                if (String_EndsWith(RelativePath, StrLit(".rc"), false))
                {
                    if (String_EndsWith(RelativePath, StrLit("icon.rc"), false))
                        return true;
                    
                    // TODO: really should use relative path here
                    u32 LastSlash = 0;
                    String_IndexOfLastPathSlash(FullPath, &LastSlash);

                    StringLocal(FilePath, MAX_PATH_LENGTH);
                    String_Append(&FilePath, StrSlice(FileName.Data, DotIndex));
                    String_Append(&FilePath, StrLit(".res"));

                    StringLocal(ObjectPath, MAX_PATH_LENGTH);
                    const String Dir = StrSlice(FullPath.Data, LastSlash);
                    String_BuildPath(&ObjectPath, Dir, FilePath);

                    String_Concat(Data->CmdLine, StrLit("\""), ObjectPath, StrLit("\" "));
                }
                else
                #endif
                {
                    StringLocal(FilePath, MAX_PATH_LENGTH);
                    String_Append(&FilePath, RelativePath);
                    String_Append(&FilePath, StrLit(".o"));

                    StringLocal(ObjectPath, MAX_PATH_LENGTH);
                    String_BuildPath(&ObjectPath, Data->Params->IntermediateDirectory, FilePath);

                    String_Concat(Data->CmdLine, StrLit("\""), ObjectPath, StrLit("\" "));
                }
            }
        }
    }

    return true;
}

bool C_CompileV2(const BuildParams* Params, u32* NumCompiled)
{
    StringLocal(SourceDir, MAX_PATH_LENGTH);
    String_BuildPath(&SourceDir, Params->RootDirectory, Params->SourceDirectory);

    CompileData UserData = { Params, NumCompiled, Params->NumSources, Params->NumHeaders, Params->NumRcSources, 0, true };
    Filesystem_IterateDirectory_Ex(SourceDir, SourceFileDirectoryIterator, true, &UserData);
    if (!UserData.bSuccess)
    {
        return false;
    }

    if (*NumCompiled == 0)
    {
        #ifndef HOOD
        LOG("\nNothing to compile - source files unchanged since last build");
        #else
        LOG("\nno work to do homie");
        #endif

        #if !PLATFORM_WINDOWS
        LOG_LINE_BREAK();
        #endif

        return true;
    }

    for each (Process, *Params->Processes)
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
    if (Platform_FindProgram(StrLit("llvm-rc")))
    {
        CompileData RcUserData = { Params, NumCompiled, Params->NumSources, Params->NumHeaders, Params->NumRcSources, 0, true };
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

    StringLocal(FilePath, MAX_PATH_LENGTH);
    String_Append(&FilePath, RelativePath);
    String_Append(&FilePath, StrLit(".o"));

    StringLocal(ObjectPath, MAX_PATH_LENGTH);
    String_BuildPath(&ObjectPath, Params->IntermediateBaseDirectory, FilePath);

    StringLocal(FullSourcePath, MAX_PATH_LENGTH+2);
    String_AppendChar(&FullSourcePath, '"');
    String_Append(&FullSourcePath, FullPath);
    String_AppendChar(&FullSourcePath, '"');

    StringLocal(ErrorLimit, 32);
    if (Params->MaxErrors > 0)
    {
        if (String_IsEqual(Params->CompilerProgram, StrLit("gcc"), false) ||
            String_IsEqual(Params->CompilerProgram, StrLit("g++"), false) ||
            String_Contains(Params->CompilerProgram, StrLit("gcc"), false))
        {
            String_Format(&ErrorLimit, StrLit("-fmax-errors=%i"), 32, Params->MaxErrors);
        }
        else
        {
            String_Format(&ErrorLimit, StrLit("-ferror-limit=%i"), 32, Params->MaxErrors);
        }
    }

    String AdditionalPlatformFlags = String_Null();

    #if PLATFORM_UNIX
    if (String_IsEqual(Params->Extension, StrLit(".so"), false) ||
        String_IsEqual(Params->Extension, StrLit(".dylib"), false) ||
        String_IsEqual(Params->Extension, StrLit(".a"), false))
    {
        AdditionalPlatformFlags = StrLit("-fPIC -fvisibility=default");
    }
    else if (Params->bIsAssemblyExe)
    {
        AdditionalPlatformFlags = StrLit("-fPIE");
    }
    #endif

    // build cmd line string
    StringLocal(CmdLine, UINT16_MAX);
    String_BuildSeparator(&CmdLine, ' ', Params->CompilerProgram, FullSourcePath, Params->CompilerFlags, ErrorLimit, AdditionalPlatformFlags);
    String_Concat(&CmdLine, StrLit(" -c -o "), StrLit("\""), ObjectPath, StrLit("\" "), Params->DefineFlags, StrLit(" "), Params->IncludeFlags);

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

    #ifndef HOOD
    LOG("[%i/%i] Compiling %S", Data->Index, Data->NumSources, FullPath);
    #else
    LOG("compil'n %i o' %i %S", Data->Index, Data->NumSources, FullPath);
    #endif

    if (Params->bVerbose)
    {
        LOG("\n    %S\n", CmdLine);
    }

    PlatformHandle Handle = Platform_RunCommand(CmdLine, Params->RootDirectory);
    Array_Add(Processes, Handle);
    (*Data->NumCompiled)++;

    if (Params->bShouldWaitPerCompileProcess)
    {
        const u32 ExitCode = Platform_WaitForProcessAndGetExitCode(Handle);
        if (ExitCode != 0)
        {
            LOG_ERROR("[DoCompile] Compiler errors detected. See above errors to fix. Exit code for process: %u. Aborting build...", ExitCode);
            return false;
        }
    }

    return true;
}

/*
bool C_Compile(const BuildParams* Params, u32* NumCompiled)
{
    // Compile .c files to .o and put them in the Intermediate directory
    // Use compiler flags, defines and include flags only
    // clang/gcc [File.c] [CompilerFlags] -c -o [Intermediate/SubDir/.../File.c.o] [Defines] [IncludeFlags]

    TArray(PlatformHandle) Processes = *Params->Processes;
    const bool bShouldWaitPerCompileProcess = Params->bShouldWaitPerCompileProcess;

    u32 NumSources = 0;
    #if PLATFORM_WINDOWS
    u32 NumRcSources = 0;
    #endif
    //todo: separate .rc files from real source files
    for each (It, Params->SourceFiles)
    {
        if (String_EndsWith(It->RelativePath, StrLit(".rc"), false))
        {
            // we will build this later
            #if PLATFORM_WINDOWS
            NumRcSources++;
            #endif

            continue;
        }
        
        NumSources++;
    }

    u32 i = 0;
    u32 TotalWorkDone = 0;
    for each (It, Params->SourceFiles)
    {
        if (String_EndsWith(It->RelativePath, StrLit(".rc"), false))
        {
            // we will build this later
            continue;
        }

        SourceFileData File = *It;

        if (Params->MaxCompilersAtOnce > 0 && !bShouldWaitPerCompileProcess)
        {
            u32 Num = (u32)Array_Num(Processes);
            if (Num == Params->MaxCompilersAtOnce)
            {
                for each (Process, Processes)
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

                    Array_Remove(Processes, Process);
                    break;
                }
            }
        }

        i++;

        StringLocal(FilePath, MAX_PATH_LENGTH);
        String_Append(&FilePath, File.RelativePath);
        String_Append(&FilePath, StrLit(".o"));

        // build object path
        StringLocal(ObjectPath, MAX_PATH_LENGTH);
        String_BuildPath(&ObjectPath, Params->IntermediateBaseDirectory, FilePath);

        StringLocal(FullSourcePath, MAX_PATH_LENGTH+2);
        String_AppendChar(&FullSourcePath, '"');
        String_Append(&FullSourcePath, File.FullPath);
        String_AppendChar(&FullSourcePath, '"');

        StringLocal(ErrorLimit, 32);
        if (Params->MaxErrors > 0)
        {
            if (String_IsEqual(Params->CompilerProgram, StrLit("gcc"), false) ||
                String_IsEqual(Params->CompilerProgram, StrLit("g++"), false) ||
                String_Contains(Params->CompilerProgram, StrLit("gcc"), false))
            {
                String_Format(&ErrorLimit, StrLit("-fmax-errors=%i"), 32, Params->MaxErrors);
            }
            else
            {
                String_Format(&ErrorLimit, StrLit("-ferror-limit=%i"), 32, Params->MaxErrors);
            }
        }

        String AdditionalPlatformFlags = String_Null();

        #if PLATFORM_UNIX
        if (String_IsEqual(Params->Extension, StrLit(".so"), false) ||
            String_IsEqual(Params->Extension, StrLit(".dylib"), false) ||
            String_IsEqual(Params->Extension, StrLit(".a"), false))
        {
            AdditionalPlatformFlags = StrLit("-fPIC -fvisibility=default");
        }
        else if (Params->bIsAssemblyExe)
        {
            AdditionalPlatformFlags = StrLit("-fPIE");
        }
        #endif

        // build cmd line string
        StringLocal(CmdLine, UINT16_MAX);
        String_BuildSeparator(&CmdLine, ' ', Params->CompilerProgram, FullSourcePath, Params->CompilerFlags, ErrorLimit, AdditionalPlatformFlags);
        String_Concat(&CmdLine, StrLit(" -c -o "), StrLit("\""), ObjectPath, StrLit("\" "), Params->DefineFlags, StrLit(" "), Params->IncludeFlags);

        u64 ObjectFileWriteTime = Filesystem_GetLastWriteTime(ObjectPath);
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

        Filesystem_NewFile(ObjectPath);

        #ifndef HOOD
        LOG("[%i/%i] Compiling %S", i, NumSources, File.FullPath);
        #else
        LOG("compil'n %i o' %i %S", i, NumSources, File.FullPath);
        #endif

        if (Params->bVerbose)
        {
            LOG("\n    %S\n", CmdLine);
        }

        PlatformHandle Handle = Platform_RunCommand(CmdLine, Params->RootDirectory);
        Array_Add(Processes, Handle);
        TotalWorkDone++;

        if (bShouldWaitPerCompileProcess)
        {
            const u32 ExitCode = Platform_WaitForProcessAndGetExitCode(Handle);
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

        #if !PLATFORM_WINDOWS
        LOG_LINE_BREAK();
        #endif
    
        return true;
    }

    // on linux we can't call wait() twice unlike windows, so just wait in the for loop below
    #if PLATFORM_WINDOWS
    Platform_WaitForMultipleHandles(Processes, (u32)Array_Num(Processes), -1, true);
    #endif

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
    #if PLATFORM_WINDOWS
    i = 0;
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

                LOG("\nCompiling resource file \"%S\"", It->RelativePath);
                LOG("    %S", CmdLine);

                if (NumRcSources > 1 && i < NumRcSources-1)
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
    #endif

    return true;
}
*/

bool C_LinkV2(const BuildParams* Params)
{
    StringLocal(SourceDir, MAX_PATH_LENGTH);
    String_BuildPath(&SourceDir, Params->RootDirectory, Params->SourceDirectory);

    // todo: make better
    #if PLATFORM_UNIX
    if (!String_IsEqual(Params->Extension, StrLit(".a"), false))
    #else
    if (!String_IsEqual(Params->Extension, StrLit(".lib"), false))
    #endif
    {
        StringLocal(CmdLine, UINT16_MAX);
        String_Append(&CmdLine, Params->CompilerProgram);
        String_AppendChar(&CmdLine, ' ');

        LinkData Data = { Params, &CmdLine };
        Filesystem_IterateDirectory_Ex(SourceDir, Link_SourceFileDirectoryIterator, true, &Data);

        String_BuildSeparator(&CmdLine, ' ',  Params->IconResFilePath, Params->VersionResFilePath, StrLit(" -o \" "));

        StringLocal(BuildPath, MAX_PATH_LENGTH*2);
        String_BuildPath(&BuildPath, Params->RootDirectory, Params->BuildDirectory);
        String_AppendPathSeparator(&BuildPath);

        String SharedFlag = String_Null();
        if (Params->Extension.Length > 0)
        {
            // todo: make better
            #if PLATFORM_UNIX
            if (String_IsEqual(Params->Extension, StrLit(".so"), false) ||
                String_IsEqual(Params->Extension, StrLit(".dylib"), false))
            #else
            if (String_IsEqual(Params->Extension, StrLit(".dll"), false))
            #endif
            {
                SharedFlag = StrLit("-shared");
            }
        }

        String RunPathLinkFlag = String_Null();

        #if PLATFORM_UNIX
        if (Params->bIsAssemblyExe)
        {
            //RunPathLinkFlag = StrLit("-Wl,-rpath '-Wl,$ORIGIN'");
            RunPathLinkFlag = StrLit("-Wl,-rpath,'$ORIGIN'");
        }
        #endif

        String_Append(&CmdLine, BuildPath);
        String_Append(&CmdLine, Params->AssemblyWithExt);
        String_Append(&CmdLine, StrLit("\" "));

        String_BuildSeparator(&CmdLine, ' ',  Params->LinkerDefineFlags, Params->LinkerFlags, SharedFlag, RunPathLinkFlag, Params->Libraries, Params->LibraryDirectories, Params->bVerbose ? StrLit("-v") : String_Null());
        String_EatSpacesInlineFromEnd(&CmdLine);

        #ifndef HOOD
        LOG_LINE_BREAK();
        LogString_WordWrapped(S("Linking: "), CmdLine, false);
        //LOG("\nLinking: %S", CmdLine);
        #else
        LOG("\nlink'n it up: %S", CmdLine);
        #endif

        PlatformHandle H = Platform_RunCommand(CmdLine, Params->RootDirectory);

        const u32 ExitCode = Platform_WaitForProcessAndGetExitCode(H);
        if (ExitCode != 0)
        {
            #ifndef HOOD
            LOG_ERROR("Linker errors detected. See above errors to fix. Exit code for process: %u. Aborting build...", ExitCode);
            #else
            LOG_ERROR("seen some linker errors homie. fix yo shit up, something aint linkin' right");
            #endif

            return false;
        }
    }

    // todo: make better
    #if PLATFORM_UNIX
    bool bExtensionHasLib = ExtensionHas(Params->Extension_Og, StrLit("a"));
    #else
    bool bExtensionHasLib = ExtensionHas(Params->Extension_Og, StrLit("lib"));
    #endif

    // compile a static library if we're trying to make a shared one as well (for convenience sake)
    if (bExtensionHasLib ||
        // todo: make better
        #if PLATFORM_UNIX
        String_IsEqual(Params->Extension, StrLit(".a"), false)
        #else
        String_IsEqual(Params->Extension, StrLit(".lib"), false)
        #endif
        )
    {
        StringLocal(CmdLine, UINT16_MAX);
        #if PLATFORM_UNIX
        String_Append(&CmdLine, StrLit("ar rcs \""));
        #else
        String_Append(&CmdLine, StrLit("llvm-ar r \""));
        #endif

        StringLocal(BuildPath, MAX_PATH_LENGTH);
        String_BuildPath(&BuildPath, Params->RootDirectory, Params->BuildDirectory);
        String_AppendPathSeparator(&BuildPath);

        String_Append(&CmdLine, BuildPath);
        String_Append(&CmdLine, Params->Assembly);

        // todo: make better
        #if PLATFORM_APPLE
        if (String_IsEqual(Params->Extension, StrLit(".dylib"), false))
        #elif PLATFORM_UNIX
        if (String_IsEqual(Params->Extension, StrLit(".so"), false))
        #else
        if (String_IsEqual(Params->Extension, StrLit(".dll"), false))
        #endif
            #if PLATFORM_UNIX
            String_Append(&CmdLine, StrLit("S.a"));
            #else
            String_Append(&CmdLine, StrLit("S.lib"));
            #endif
        else
            #if PLATFORM_UNIX
            String_Append(&CmdLine, StrLit(".a"));
            #else
            String_Append(&CmdLine, StrLit(".lib"));
            #endif

        String_Append(&CmdLine, StrLit("\" "));

        LinkData Data = { Params, &CmdLine };
        Filesystem_IterateDirectory_Ex(SourceDir, Link_SourceFileDirectoryIterator, true, &Data);

        String_BuildSeparator(&CmdLine, ' ', Params->VersionResFilePath, Params->bVerbose ? StrLit("-v") : String_Null());
        String_EatSpacesInlineFromEnd(&CmdLine);

        #ifndef HOOD
        LOG_LINE_BREAK();
        LogString_WordWrapped(S("Static Linking: "), CmdLine, false);
        //LOG("\nStatic Linking: %S", CmdLine);
        #else
        LOG("\nstatic link'n it up: %S", CmdLine);
        #endif

        PlatformHandle H = Platform_RunCommand(CmdLine, Params->RootDirectory);
        u32 ExitCode = Platform_WaitForProcessAndGetExitCode(H);
        if (ExitCode != 0)
        {
            #ifndef HOOD
            LOG_ERROR("Linker errors detected. See above errors to fix. Exit code for process: %u. Aborting build...", ExitCode);
            #else
            LOG_ERROR("seen some linker errors homie. fix yo shit up, something aint linkin' right");
            #endif
            return false;
        }
    }

    // generate a .def file if we are building a dll file (windows only)
    #if PLATFORM_WINDOWS
    if (Platform_FindProgram(StrLit("dumpbin")))
    {
        if (String_IsEqual(Params->Extension, StrLit(".dll"), false))
        {
            StringLocal(CmdLine, 8192);
            String_Append(&CmdLine, StrLit("dumpbin /EXPORTS /NOLOGO /OUT:\""));

            StringLocal(BuildPath, 512);
            String_BuildPath(&BuildPath, Params->RootDirectory, Params->BuildDirectory);
            String_AppendPathSeparator(&BuildPath);

            String_Append(&CmdLine, BuildPath);
            String_Append(&CmdLine, Params->Assembly);
            String_Append(&CmdLine, StrLit(".def\" "));

            String_Append(&CmdLine, StrLit("\""));
            String_Append(&CmdLine, BuildPath);
            String_Append(&CmdLine, Params->Assembly);
            String_Append(&CmdLine, StrLit(".dll\""));

            PlatformHandle H = Platform_RunCommand(CmdLine, Params->RootDirectory);
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

/*
bool C_Link(const BuildParams* Params)
{
    // todo: make better
    #if PLATFORM_UNIX
    if (!String_IsEqual(Params->Extension, StrLit(".a"), false))
    #else
    if (!String_IsEqual(Params->Extension, StrLit(".lib"), false))
    #endif
    {
        StringLocal(CmdLine, UINT16_MAX);
        String_Append(&CmdLine, Params->CompilerProgram);
        String_AppendChar(&CmdLine, ' ');

        for each (It, Params->SourceFiles)
        {
            SourceFileData File = *It;

            if (String_EndsWith(File.RelativePath, StrLit(".rc"), false))
            {
                #if PLATFORM_WINDOWS
                if (String_EndsWith(It->RelativePath, StrLit("icon.rc"), false))
                    continue;
                
                // TODO: really should use relative path here
                u32 LastSlash = 0;
                String_IndexOfLastPathSlash(File.FullPath, &LastSlash);
                String FileName = LastSlash > 0 ? StrShiftF(File.FullPath, LastSlash+1) : File.FullPath;

                u32 LastDot = 0;
                String_IndexOfLastChar(FileName, '.', &LastDot);

                StringLocal(FilePath, MAX_PATH_LENGTH);
                String_Append(&FilePath, StrSlice(FileName.Data, LastDot));
                String_Append(&FilePath, StrLit(".res"));

                StringLocal(ObjectPath, MAX_PATH_LENGTH);
                const String Dir = StrSlice(File.FullPath.Data, LastSlash);
                String_BuildPath(&ObjectPath, Dir, FilePath);

                String_Concat(&CmdLine, StrLit("\""), ObjectPath, StrLit("\" "));
                #else
                continue;
                #endif
            }
            else
            {
                StringLocal(FilePath, MAX_PATH_LENGTH);
                String_Append(&FilePath, File.RelativePath);
                String_Append(&FilePath, StrLit(".o"));

                StringLocal(ObjectPath, MAX_PATH_LENGTH);
                String_BuildPath(&ObjectPath, Params->IntermediateDirectory, FilePath);

                String_Concat(&CmdLine, StrLit("\""), ObjectPath, StrLit("\" "));
            }
        }

        String_BuildSeparator(&CmdLine, ' ',  Params->IconResFilePath, Params->VersionResFilePath, StrLit(" -o \" "));

        StringLocal(BuildPath, MAX_PATH_LENGTH*2);
        String_BuildPath(&BuildPath, Params->RootDirectory, Params->BuildDirectory);
        String_AppendPathSeparator(&BuildPath);

        String SharedFlag = String_Null();
        if (Params->Extension.Length > 0)
        {
            // todo: make better
            #if PLATFORM_UNIX
            if (String_IsEqual(Params->Extension, StrLit(".so"), false) ||
                String_IsEqual(Params->Extension, StrLit(".dylib"), false))
            #else
            if (String_IsEqual(Params->Extension, StrLit(".dll"), false))
            #endif
            {
                SharedFlag = StrLit("-shared");
            }
        }

        String RunPathLinkFlag = String_Null();

        #if PLATFORM_UNIX
        if (Params->bIsAssemblyExe)
        {
            //RunPathLinkFlag = StrLit("-Wl,-rpath '-Wl,$ORIGIN'");
            RunPathLinkFlag = StrLit("-Wl,-rpath,'$ORIGIN'");
        }
        #endif

        String_Append(&CmdLine, BuildPath);
        String_Append(&CmdLine, Params->AssemblyWithExt);
        String_Append(&CmdLine, StrLit("\" "));

        String_BuildSeparator(&CmdLine, ' ',  Params->LinkerDefineFlags, Params->LinkerFlags, SharedFlag, RunPathLinkFlag, Params->Libraries, Params->LibraryDirectories, Params->bVerbose ? StrLit("-v") : String_Null());
        String_EatSpacesInlineFromEnd(&CmdLine);

        #ifndef HOOD
        LOG("\nLinking: %S", CmdLine);
        #else
        LOG("\nlink'n it up: %S", CmdLine);
        #endif

        PlatformHandle H = Platform_RunCommand(CmdLine, Params->RootDirectory);

        const u32 ExitCode = Platform_WaitForProcessAndGetExitCode(H);
        if (ExitCode != 0)
        {
            #ifndef HOOD
            LOG_ERROR("Linker errors detected. See above errors to fix. Exit code for process: %u. Aborting build...", ExitCode);
            #else
            LOG_ERROR("seen some linker errors homie. fix yo shit up, something aint linkin' right");
            #endif

            return false;
        }
    }

    // todo: make better
    #if PLATFORM_UNIX
    bool bExtensionHasLib = ExtensionHas(Params->Extension_Og, StrLit("a"));
    #else
    bool bExtensionHasLib = ExtensionHas(Params->Extension_Og, StrLit("lib"));
    #endif

    // compile a static library if we're trying to make a shared one as well (for convenience sake)
    if (bExtensionHasLib ||
        // todo: make better
        #if PLATFORM_UNIX
        String_IsEqual(Params->Extension, StrLit(".a"), false)
        #else
        String_IsEqual(Params->Extension, StrLit(".lib"), false)
        #endif
        )
    {
        StringLocal(CmdLine, UINT16_MAX);
        #if PLATFORM_UNIX
        String_Append(&CmdLine, StrLit("ar rcs \""));
        #else
        String_Append(&CmdLine, StrLit("llvm-ar r \""));
        #endif

        StringLocal(BuildPath, 512);
        String_BuildPath(&BuildPath, Params->RootDirectory, Params->BuildDirectory);
        String_AppendPathSeparator(&BuildPath);

        String_Append(&CmdLine, BuildPath);
        String_Append(&CmdLine, Params->Assembly);

        // todo: make better
        #if PLATFORM_APPLE
        if (String_IsEqual(Params->Extension, StrLit(".dylib"), false))
        #elif PLATFORM_UNIX
        if (String_IsEqual(Params->Extension, StrLit(".so"), false))
        #else
        if (String_IsEqual(Params->Extension, StrLit(".dll"), false))
        #endif
            #if PLATFORM_UNIX
            String_Append(&CmdLine, StrLit("S.a"));
            #else
            String_Append(&CmdLine, StrLit("S.lib"));
            #endif
        else
            #if PLATFORM_UNIX
            String_Append(&CmdLine, StrLit(".a"));
            #else
            String_Append(&CmdLine, StrLit(".lib"));
            #endif

        String_Append(&CmdLine, StrLit("\" "));

        for each (It, Params->SourceFiles)
        {
            SourceFileData File = *It;

            #if PLATFORM_WINDOWS
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

                StringLocal(ObjectPath, MAX_PATH_LENGTH);
                const String Dir = StrSlice(File.FullPath.Data, LastSlash);
                String_BuildPath(&ObjectPath, Dir, FilePath);

                String_Concat(&CmdLine, StrLit("\""), ObjectPath, StrLit("\" "));
            }
            else
            #endif
            {
                StringLocal(ObjectPath, MAX_PATH_LENGTH);

                StringLocal(FilePath, MAX_PATH_LENGTH);
                String_Append(&FilePath, File.RelativePath);
                String_Append(&FilePath, StrLit(".o"));

                String_BuildPath(&ObjectPath, Params->IntermediateDirectory, FilePath);

                String_Concat(&CmdLine, StrLit("\""), ObjectPath, StrLit("\" "));
            }
        }

        String_BuildSeparator(&CmdLine, ' ', Params->VersionResFilePath, Params->bVerbose ? StrLit("-v") : String_Null());
        String_EatSpacesInlineFromEnd(&CmdLine);

        #ifndef HOOD
        LOG("\nStatic Linking: %S", CmdLine);
        #else
        LOG("\nstatic link'n it up: %S", CmdLine);
        #endif

        PlatformHandle H = Platform_RunCommand(CmdLine, Params->RootDirectory);
        u32 ExitCode = Platform_WaitForProcessAndGetExitCode(H);
        if (ExitCode != 0)
        {
            #ifndef HOOD
            LOG_ERROR("Linker errors detected. See above errors to fix. Exit code for process: %u. Aborting build...", ExitCode);
            #else
            LOG_ERROR("seen some linker errors homie. fix yo shit up, something aint linkin' right");
            #endif
            return false;
        }
    }

    // generate a .def file if we are building a dll file (windows only)
    #if PLATFORM_WINDOWS
    if (Platform_FindProgram(StrLit("dumpbin")))
    {
        if (String_IsEqual(Params->Extension, StrLit(".dll"), false))
        {
            StringLocal(CmdLine, 8192);
            String_Append(&CmdLine, StrLit("dumpbin /EXPORTS /NOLOGO /OUT:\""));

            StringLocal(BuildPath, 512);
            String_BuildPath(&BuildPath, Params->RootDirectory, Params->BuildDirectory);
            String_AppendPathSeparator(&BuildPath);

            String_Append(&CmdLine, BuildPath);
            String_Append(&CmdLine, Params->Assembly);
            String_Append(&CmdLine, StrLit(".def\" "));

            String_Append(&CmdLine, StrLit("\""));
            String_Append(&CmdLine, BuildPath);
            String_Append(&CmdLine, Params->Assembly);
            String_Append(&CmdLine, StrLit(".dll\""));

            PlatformHandle H = Platform_RunCommand(CmdLine, Params->RootDirectory);
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
*/

bool C_IsSource(const String Extension)
{
    return  String_IsEqual(Extension, StrLit(".c"), false) ||
            String_IsEqual(Extension, StrLit(".cc"), false) ||
            String_IsEqual(Extension, StrLit(".cxx"), false) ||
            String_IsEqual(Extension, StrLit(".c++"), false) ||
            String_IsEqual(Extension, StrLit(".cpp"), false) ||
            String_IsEqual(Extension, StrLit(".asm"), false)
            #if PLATFORM_WINDOWS
            || String_IsEqual(Extension, StrLit(".rc"), false);
            #elif PLATFORM_APPLE
            || String_IsEqual(Extension, StrLit(".m"), false);
            #else
            ;
            #endif
}

bool C_IsHeader(const String Extension)
{
    return  String_IsEqual(Extension, StrLit(".h"), false) ||
            String_IsEqual(Extension, StrLit(".hh"), false) ||
            String_IsEqual(Extension, StrLit(".hpp"), false) ||
            String_IsEqual(Extension, StrLit(".hxx"), false) ||
            String_IsEqual(Extension, StrLit(".h++"), false) ||
            String_IsEqual(Extension, StrLit(".inl"), false);
}
