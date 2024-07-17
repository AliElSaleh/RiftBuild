// Copyright (c) 2024 Ali El Saleh

#include "Backend.h"

#include "Structures/Array.h"
#include "String/StringUtils.h"
#include "Platform/Filesystem.h"
#include "Platform/Platform.h"
#include "Log.h"

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
            String_Append(&FilePath, S(".o"));

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

                if (Params->bVerbose) LOG("    CMD: %S", CmdLine);

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

            /*
            if (String_StartsWith(RelativePath, Data->Params->IntermediateDirectory, false) ||
                String_StartsWith(RelativePath, Data->Params->BuildDirectory, false))
            {
                return true;
            }
            */
        }

        u32 DotIndex = 0;
        String_IndexOfLastChar(FileName, '.', &DotIndex);

        const String Extension = StrShiftF(FileName, DotIndex);

        if (IsSource(Extension))
        {
            if (String_IsEqual(Extension, S(".asm"), false) ||
                String_IsEqual(Extension, S(".rc"), false))
            {
                // we will build this later
                return true;
            }

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

    PlatformHandle h = Platform_RunCommand(CmdLine, Params->RootDirectory);
    u32 ExitCode = Platform_WaitForProcessAndGetExitCode(h);
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

bool C_Compile(const BuildParams* Params, u32* NumCompiled)
{
    StringLocal(SourceDir, MAX_PATH_LENGTH);
    String_BuildPath(&SourceDir, Params->RootDirectory, Params->SourceDirectory);

    // compile all .asm files first
    {
        CompileData UserData = { NULL, Params, NumCompiled, 0, true, NULL };
        Filesystem_IterateDirectory_Ex(SourceDir, AsmSourceFileDirectoryIterator, true, &UserData);
        if (!UserData.bSuccess)
        {
            return false;
        }
    }

    // compile all .c/c++ files
    {
        CompileData UserData = { C_DoCompile, Params, NumCompiled, 0, true, NULL };
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

        #if !PLATFORM_WINDOWS
        //LOG_LINE_BREAK();
        #endif

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
        CompileData RcUserData = { NULL, Params, NumCompiled, 0, true, NULL };
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

                for (u8 i = 0; i < Num; i++)
                {
                    Platform_TerminateProcess(Processes[i], 1);
                }

                return false;
            }

            Array_RemoveAt(Processes, NULL, Index);
        }
    }

    Data->Index++;

    StringLocal(FilePath, MAX_PATH_LENGTH);
    String_Append(&FilePath, RelativePath);
    String_Append(&FilePath, S(".o"));

    StringLocal(ObjectPath, MAX_PATH_LENGTH);
    String_BuildPath(&ObjectPath, Params->IntermediateBaseDirectory, FilePath);

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
    if (String_IsEqual(Params->Extension, S(".so"), false) ||
        String_IsEqual(Params->Extension, S(".dylib"), false) ||
        String_IsEqual(Params->Extension, S(".a"), false))
    {
        AdditionalPlatformFlags = S("-fPIC -fvisibility=default");
    }
    else if (Params->bIsAssemblyExe)
    {
        AdditionalPlatformFlags = S("-fPIE");
    }
    #endif

    // build cmd line string
    StringLocal(CmdLine, UINT16_MAX);
    String_BuildSeparator(&CmdLine, ' ', Params->CompilerProgram, FullSourcePath, Params->CompilerFlags, ErrorLimit, AdditionalPlatformFlags);
    String_Concat(&CmdLine, S(" -c -o "), S("\""), ObjectPath, S("\" "), Params->DefineFlags, S(" "), Params->IncludeFlags);

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

    PlatformHandle Handle = Platform_RunCommand(CmdLine, Params->RootDirectory);
    Array_Add(Processes, Handle);
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
    StringLocal(SourceDir, MAX_PATH_LENGTH);
    String_BuildPath(&SourceDir, Params->RootDirectory, Params->SourceDirectory);

    // todo: make better
    #if PLATFORM_WINDOWS
    if (!String_IsEqual(Params->Extension, S(".lib"), false))
    #else
    if (!String_IsEqual(Params->Extension, S(".a"), false))
    #endif
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
        if (Params->Extension.Length > 0)
        {
            // todo: make better
            #if PLATFORM_WINDOWS
            if (String_IsEqual(Params->Extension, S(".dll"), false))
            #else
            if (String_IsEqual(Params->Extension, S(".so"), false) ||
                String_IsEqual(Params->Extension, S(".dylib"), false))
            #endif
            {
                SharedFlag = S("-shared");
            }
        }

        String RunPathLinkFlag = String_Null();

        #if !PLATFORM_WINDOWS
        if (Params->bIsAssemblyExe)
        {
            //RunPathLinkFlag = S("-Wl,-rpath '-Wl,$ORIGIN'");
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
    #if PLATFORM_WINDOWS
    bool bExtensionHasLib = ExtensionHas(*Params->Arena, Params->Extension_Og, S("lib"));
    #else
    bool bExtensionHasLib = ExtensionHas(*Params->Arena, Params->Extension_Og, S("a"));
    #endif

    // compile a static library if we're trying to make a shared one as well (for convenience sake)
    if (bExtensionHasLib ||
        // todo: make better
        #if PLATFORM_WINDOWS
        String_IsEqual(Params->Extension, S(".lib"), false)
        #else
        String_IsEqual(Params->Extension, S(".a"), false)
        #endif
        )
    {
        StringLocal(CmdLine, UINT16_MAX);
        #if PLATFORM_WINDOWS
        String_Append(&CmdLine, S("llvm-ar r \"")); // TODO: GCC version
        #else
        String_Append(&CmdLine, S("ar rcs \""));
        #endif

        StringLocal(BuildPath, MAX_PATH_LENGTH);
        String_BuildPath(&BuildPath, Params->RootDirectory, Params->BuildDirectory);
        String_AppendPathSeparator(&BuildPath);

        String_Append(&CmdLine, BuildPath);

        StringLocal(LibFile, MAX_PATH_LENGTH);
        String_Append(&LibFile, Params->Assembly);

        // todo: make better
        #if PLATFORM_WINDOWS
        if (String_IsEqual(Params->Extension, S(".dll"), false))
        #elif PLATFORM_APPLE
        if (String_IsEqual(Params->Extension, S(".dylib"), false))
        #else
        if (String_IsEqual(Params->Extension, S(".so"), false))
        #endif
            #if PLATFORM_WINDOWS
            String_Append(&LibFile, S("S.lib"));
            #else
            String_Append(&LibFile, S("S.a"));
            #endif
        else
            #if PLATFORM_WINDOWS
            String_Append(&LibFile, S(".lib"));
            #else
            String_Append(&LibFile, S(".a"));
            #endif

        String_Append(&CmdLine, LibFile);
        String_Append(&CmdLine, S("\" "));

        LinkData Data = { Params, &CmdLine };
        Filesystem_IterateDirectory_Ex(SourceDir, Link_SourceFileDirectoryIterator, true, &Data);

        String_BuildSeparator(&CmdLine, ' ', Params->VersionResFilePath);//, Params->bVerbose ? S("-v") : String_Null());
        String_EatSpacesInlineFromEnd(&CmdLine);

        if (bQuietBuild) Logging_Enable();

        #ifndef HOOD
        LOG("\nLinking %S", LibFile);
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
    if (Platform_FindProgram(S("dumpbin")))
    {
        if (String_IsEqual(Params->Extension, S(".dll"), false))
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

bool IsSource(const String Extension)
{
    return  String_IsEqual(Extension, S(".c"), false) ||
            String_IsEqual(Extension, S(".cc"), false) ||
            String_IsEqual(Extension, S(".cxx"), false) ||
            String_IsEqual(Extension, S(".c++"), false) ||
            String_IsEqual(Extension, S(".cpp"), false) ||
            String_IsEqual(Extension, S(".asm"), false)
            #if PLATFORM_WINDOWS
            || String_IsEqual(Extension, S(".rc"), false);
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
            String_IsEqual(Extension, S(".inl"), false);
}




////////////////////////////////////

// MSVC BACKEND

////////////////////////////////////




#include "Backend.h"

#if PLATFORM_WINDOWS

/// TODO: if multithreaded and more than on soruce file. use /MP and call cl.exe only once

// C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build 

/*
    // find the vcvars bat file so we can run cl from a regular cmd line. luckily the bat file is always in the same place relative to where cl.exe lives
    u32 LastSlash = 0;
    String_IndexOfLastPathSlash(Params->CompilerPath, &LastSlash);
    const String CompilerPathNoExt = StrSlice(Params->CompilerPath.Data, LastSlash+1);
    String_AppendChar(&CmdLine, '"');
    String_Append(&CmdLine, CompilerPathNoExt);
    String_Append(&CmdLine, StrLit("../../../../../../Auxiliary/Build/"));
    if (Filesystem_DoesDirectoryExist(StrShiftF(CmdLine, 1)))
    {
        String_Append(&CmdLine, StrLit("vcvars64.bat")); //todo: switch between 32 or 64 bit?
        String_AppendChar(&CmdLine, '"');
        String_Append(&CmdLine, StrLit(" >NUL 2>&1 && ")); // suppress output logs from the bat script
    }
    else
    {
        String_Empty(&CmdLine);
    }


*/

bool MSVC_DoCompile(CompileData* Data, const String FullPath, const String RelativePath);

internal bool AsmSourceFileDirectoryIterator_MSVC(const String FullPath, const String RelativePath, const String FileName, u64 FileSize, bool bIsDirectory, void* UserData)
{
    if (FileSize > 0)
    {
        if (String_StartsWith(FileName, S("__"), false))
        {
            return true;
        }

        CompileData* Data = (CompileData*)UserData;
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
            String_BuildPath(&ObjectFilePath, Params->IntermediateDirectory, FilePath);

            u64 ObjectFileWriteTime = Filesystem_GetLastWriteTime(ObjectFilePath);
            u64 SourceFileWriteTime = Filesystem_GetLastWriteTime(FullPath);

            if (SourceFileWriteTime >= ObjectFileWriteTime)
            {
                StringLocal(CmdLine, Kibibytes(4));

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

bool MSVC_Compile(const BuildParams* Params, u32* NumCompiled)
{
    if (NEVER(Params == NULL)) return false;

    StringLocal(SourceDir, MAX_PATH_LENGTH);
    String_BuildPath(&SourceDir, Params->RootDirectory, Params->SourceDirectory);

    // compile all .asm files first
    {
        CompileData UserData = { NULL, Params, NumCompiled, 0, true, NULL };
        Filesystem_IterateDirectory_Ex(SourceDir, AsmSourceFileDirectoryIterator_MSVC, true, &UserData);
        if (!UserData.bSuccess)
        {
            return false;
        }
    }

    // compile all .c files
    {
        CompileData UserData = { MSVC_DoCompile, Params, NumCompiled, 0, true, NULL };
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
        CompileData RcUserData = { NULL, Params, NumCompiled, 0, true, NULL };
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
    String_Append(&CmdLine, S("cl /nologo "));

    u32 LastSlash = 0;
    String_IndexOfLastPathSlash(RelativePath, &LastSlash);

    u32 LastDot = 0;
    String_IndexOfLastChar(RelativePath, '.', &LastDot);

    StringLocal(FilePath, MAX_PATH_LENGTH);
    String_Append(&FilePath, StrSlice(RelativePath.Data, LastDot));
    String_Append(&FilePath, S(".obj"));

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

    String_Append(&CmdLine, S(" /Fo\""));
    String_Append(&CmdLine, ObjectPath);
    String_Append(&CmdLine, S("\\\\\" /c"));

    if (bQuietBuild) Logging_Enable();

    if (Params->bShouldWaitPerCompileProcess)
        LOG_INLINE("[%i/%i] Compiling ", Data->Index, Params->NumSources);

    if (bQuietBuild) Logging_Disable();

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

bool MSVC_Link(const BuildParams* Params)
{
    if (NEVER(Params == NULL)) return false;

    if (String_IsEqual(Params->Extension, S(".pch"), false))
    {
        return true;
    }

    StringLocal(SourceDir, MAX_PATH_LENGTH);
    String_BuildPath(&SourceDir, Params->RootDirectory, Params->SourceDirectory);

    StringLocal(BuildPath, MAX_PATH_LENGTH);
    String_BuildPath(&BuildPath, Params->RootDirectory, Params->BuildDirectory);
    String_AppendPathSeparator(&BuildPath);

    StringLocal(CmdLine, UINT16_MAX);

    bool bIsDLL = String_IsEqual(Params->Extension, S(".dll"), false);
    if (String_IsEqual(Params->Extension, S(".exe"), false) ||
        bIsDLL)
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
        String_Concat(&CmdLine, S(" /OUT:\""), BuildPath, Params->AssemblyWithExt, S("\"")); // make this first then the flags?
    }
    else if (String_IsEqual(Params->Extension, S(".lib"), false))
    {
        String_Append(&CmdLine, S("lib /nologo /OUT:\""));

        String_Append(&CmdLine, BuildPath);
        String_Append(&CmdLine, Params->Assembly);
        
        String_Append(&CmdLine, S(".lib\" "));

        String_BuildSeparator(&CmdLine, ' ', Params->Libraries, Params->LibraryDirectories, Params->VersionResFilePath);
        String_AppendSpace(&CmdLine);

        LinkData Data = { Params, &CmdLine };
        Filesystem_IterateDirectory_Ex(SourceDir, Link_SourceFileDirectoryIterator_MSVC, true, &Data);
    }

    if (bQuietBuild) Logging_Enable();

    LOG("\nLinking %S%S", Params->Assembly, Params->Extension);

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
        String_Append(&CmdLine, S("dumpbin /EXPORTS /NOLOGO /OUT:\""));

        String_Append(&CmdLine, BuildPath);
        String_Append(&CmdLine, Params->Assembly);
        String_Append(&CmdLine, S(".def\" \""));

        String_Append(&CmdLine, BuildPath);
        String_Append(&CmdLine, Params->Assembly);
        String_Append(&CmdLine, S(".dll\""));

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
#else
bool MSVC_Compile(const BuildParams* Params, u32* OutNumCompiled) { return false; }
bool MSVC_Link(const BuildParams* Params) { return false; }
#endif // PLATFORM_WINDOWS
