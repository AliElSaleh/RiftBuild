#include "Backend.h"

#include "Structures/Array.h"
#include "String/StringUtils.h"
#include "Platform/Filesystem.h"
#include "Platform/Platform.h"
#include "Profiling/ProfilingSubsystem.h"
#include "Log.h"

bool C_Compile(const BuildParams* Params, u32* NumCompiled)
{
    // Compile .c files to .o and put them in the Intermediate directory
    // Use compiler flags, defines and include flags only
    // clang/gcc [File.c] [CompilerFlags] -c -o [Intermediate/SubDir/.../File.c.o] [Defines] [IncludeFlags]

    TArray(PlatformHandle) Processes = *Params->Processes;
    const bool bShouldWaitPerCompileProcess = Params->bShouldWaitPerCompileProcess;

    u32 NumSources = 0;
    //todo: separate .rc files from real source files
    for each (It, Params->SourceFiles)
    {
        if (String_EndsWith(It->RelativePath, StrLit(".rc"), false))
        {
            // we will build this later
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

                        #ifdef DEVELOPER
                        Platform_Sleep(5000);
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
        String_BuildPath(&ObjectPath, Params->IntermediateBaseDirectory, Params->SourceDirectory, FilePath);

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

        #if PLATFORM_LINUX
        if (String_IsEqual(Params->Extension, StrLit("so"), false) ||
            String_IsEqual(Params->Extension, StrLit("a"), false))
        {
            AdditionalPlatformFlags = StrLit("-fPIC -fvisibility=default");
        }
        else if (Params->bIsAssemblyExe)
        {
            AdditionalPlatformFlags = StrLit("-fPIE");
        }
        #endif

        // build cmd line string
        StringLocal(CmdLine, 16384);
        String_BuildSeparator(&CmdLine, ' ', Params->CompilerProgram, FullSourcePath, Params->CompilerFlags, ErrorLimit, AdditionalPlatformFlags);
        StringInternal_Concat(&CmdLine, 7, StrLit(" -c -o "), StrLit("\""), ObjectPath, StrLit("\" "), Params->DefineFlags, StrLit(" "), Params->IncludeFlags);

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
        LOG("[%i/%i] Compiling %S", i, NumSources, File.FullPath, CmdLine);
        #else
        LOG("compil'n %i o' %i %S", i, NumSources, File.FullPath, CmdLine);
        #endif

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
        if (Array_Num(Params->SourceFiles_Unfiltered) == 0)
        {
            LOG("Nothing to compile");
        }
        else
        {
            LOG("\nNothing to compile - source files unchanged since last build");
        }
        #else
        LOG("no work to do homie");
        #endif
    
        return true;
    }

    PROFILE_SCOPE("Wait")
    {
        #if PLATFORM_WINDOWS
        Platform_WaitForMultipleHandles(Processes, (u32)Array_Num(Processes), -1, true);
        #endif
    }

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

            #ifdef DEVELOPER
            Platform_Sleep(5000);
            #endif

            return false;
        }
    }

    // compile resource files
    #if PLATFORM_WINDOWS
    i = 0;
    for each_i (i, It, Params->SourceFiles)
    {
        if (String_EndsWith(It->RelativePath, StrLit(".rc"), false))
        {
            if (String_EndsWith(It->RelativePath, StrLit("icon.rc"), false))
                continue;

            StringLocal(CmdLine, 1024);
            const String RCProgram = StrLit("llvm-rc ");
            String_Append(&CmdLine, RCProgram);
            String_AppendChar(&CmdLine, '"');
            String_Append(&CmdLine, It->FullPath);
            String_AppendChar(&CmdLine, '"');

            LOG("\nCompiling resource file \"%S\"", It->RelativePath);
            LOG("    %S", CmdLine);

            if (i < Array_Num(Params->SourceFiles)-1)
            {
                LOG_LINE_BREAK();
            }

            PlatformHandle h = Platform_RunCommand(CmdLine, Params->RootDirectory);
            u32 ExitCode = Platform_WaitForProcessAndGetExitCode(h);
            if (ExitCode != 0)
            {
                LOG("Failed to build resource file \"%S\" for %S.%S. Aborting build...", It->RelativePath, Params->Assembly, Params->Extension);
                return false;
            }
        }
    }
    #endif

    return true;
}

bool C_Link(const BuildParams* Params)
{
    // todo: make better
    #if PLATFORM_LINUX
    if (!String_IsEqual(Params->Extension, StrLit("a"), false))
    #else
    if (!String_IsEqual(Params->Extension, StrLit("lib"), false))
    #endif
    {
        PROFILE_SCOPE("Link")
        {
            StringLocal(CmdLine, 16384);
            String_Append(&CmdLine, Params->CompilerProgram);
            String_AppendChar(&CmdLine, ' ');

            PROFILE_SCOPE("Cmd Link String Building")
            {
                for each (It, Params->SourceFiles)
                {
                    SourceFileData File = *It;

                    if (String_EndsWith(File.RelativePath, StrLit(".rc"), false))
                    {
                        #if PLATFORM_WINDOWS
                        if (String_EndsWith(It->RelativePath, StrLit("icon.rc"), false))
                            continue;

                        u32 LastSlash = 0;
                        String_IndexOfLastPathSlash(File.RelativePath, &LastSlash);
                        String FileName = LastSlash > 0 ? StrShiftF(File.RelativePath, LastSlash+1) : File.RelativePath;

                        u32 LastDot = 0;
                        String_IndexOfLastChar(FileName, '.', &LastDot);

                        StringLocal(FilePath, MAX_PATH_LENGTH);
                        String_Append(&FilePath, StrSlice(FileName.Data, LastDot));
                        String_Append(&FilePath, StrLit(".res"));

                        StringLocal(ObjectPath, MAX_PATH_LENGTH);
                        const String Dir = StrSlice(File.RelativePath.Data, LastSlash);
                        String_BuildPath(&ObjectPath, Dir, FilePath);

                        StringInternal_Concat(&CmdLine, 3, StrLit("\""), ObjectPath, StrLit("\" "));
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
                        String_BuildPath(&ObjectPath, Params->IntermediateDirectory, Params->SourceDirectory, FilePath);

                        StringInternal_Concat(&CmdLine, 3, StrLit("\""), ObjectPath, StrLit("\" "));
                    }
                }

                StringInternal_BuildSeparator(&CmdLine, ' ',  3, Params->IconResFilePath, Params->VersionResFilePath, StrLit(" -o \" "));

                StringLocal(BuildPath, MAX_PATH_LENGTH*2);
                String_BuildPath(&BuildPath, Params->RootDirectory, Params->BuildDirectory);
                String_AppendPathSeparator(&BuildPath);

                String SharedFlag = String_Null();
                if (Params->Extension.Length > 0)
                {
                    // todo: make better
                    #if PLATFORM_LINUX
                    if (String_IsEqual(Params->Extension, StrLit("so"), false))
                    #else
                    if (String_IsEqual(Params->Extension, StrLit("dll"), false))
                    #endif
                    {
                        SharedFlag = StrLit("-shared");
                    }
                }

                String RunPathLinkFlag = String_Null();

                #if PLATFORM_LINUX
                RunPathLinkFlag = StrLit("-Wl,-rpath '-Wl,$ORIGIN'");
                #endif

                if (Params->Extension.Length == 0)
                {
                    StringInternal_Concat(&CmdLine, 3, BuildPath, Params->Assembly, StrLit("\" "));
                }
                else
                {
                    StringInternal_Concat(&CmdLine, 5, BuildPath, Params->Assembly, StrLit("."), Params->Extension, StrLit("\" "));
                }

                String_BuildSeparator(&CmdLine, ' ',  Params->LinkerDefineFlags, Params->LinkerFlags, SharedFlag, RunPathLinkFlag, Params->Libraries, Params->LibraryDirectories);
                String_EatSpacesInlineFromEnd(&CmdLine);
            }

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

                #ifdef DEVELOPER
                Platform_Sleep(5000);
                #endif
                return false;
            }
        }
    }

    // todo: make better
    #if PLATFORM_LINUX
    bool bExtensionHasLib = ExtensionHas(Params->Extension_Og, StrLit("a"));
    #else
    bool bExtensionHasLib = ExtensionHas(Params->Extension_Og, StrLit("lib"));
    #endif

    // compile a static library if we're trying to make a shared one as well (for convenience sake)
    if (bExtensionHasLib ||
        // todo: make better
        #if PLATFORM_LINUX
        String_IsEqual(Params->Extension, StrLit("a"), false)
        #else
        String_IsEqual(Params->Extension, StrLit("lib"), false)
        #endif
        )
    {
        PROFILE_SCOPE("LLVM-AR")
        {
            StringLocal(CmdLine, 8192);
            String_Append(&CmdLine, StrLit("llvm-ar r \""));

            PROFILE_SCOPE("Cmd LLMV-AR String Building")
            {
                StringLocal(BuildPath, 512);
                String_BuildPath(&BuildPath, Params->RootDirectory, Params->BuildDirectory);
                String_AppendPathSeparator(&BuildPath);

                String_Append(&CmdLine, BuildPath);
                String_Append(&CmdLine, Params->Assembly);

                // todo: make better
                #if PLATFORM_LINUX
                if (String_IsEqual(Params->Extension, StrLit("so"), false))
                #else
                if (String_IsEqual(Params->Extension, StrLit("dll"), false))
                #endif
                    #if PLATFORM_LINUX
                    String_Append(&CmdLine, StrLit("S.a"));
                    #else
                    String_Append(&CmdLine, StrLit("S.lib"));
                    #endif
                else
                    #if PLATFORM_LINUX
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

                        StringInternal_Concat(&CmdLine, 3, StrLit("\""), ObjectPath, StrLit("\" "));
                    }
                    else
                    #endif
                    {
                        StringLocal(ObjectPath, MAX_PATH_LENGTH);

                        StringLocal(FilePath, MAX_PATH_LENGTH);
                        String_Append(&FilePath, File.RelativePath);
                        String_Append(&FilePath, StrLit(".o"));

                        String_BuildPath(&ObjectPath, Params->IntermediateDirectory, Params->SourceDirectory, FilePath);

                        StringInternal_Concat(&CmdLine, 3, StrLit("\""), ObjectPath, StrLit("\" "));
                    }
                }

                String_BuildSeparator(&CmdLine, ' ', Params->VersionResFilePath);
                String_EatSpacesInlineFromEnd(&CmdLine);
            }

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
    }

    // generate a .def file if we are building a dll file (windows only)
    #if PLATFORM_WINDOWS
    if (String_IsEqual(Params->Extension, StrLit("dll"), false))
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
    #endif // PLATFORM_WINDOWS

    return true;
}

bool C_IsSource(const String Extension)
{
    return  String_IsEqual(Extension, StrLit(".c"), false) ||
            String_IsEqual(Extension, StrLit(".cc"), false) ||
            String_IsEqual(Extension, StrLit(".cxx"), false) ||
            String_IsEqual(Extension, StrLit(".cp"), false) ||
            String_IsEqual(Extension, StrLit(".c++"), false) ||
            String_IsEqual(Extension, StrLit(".cpp"), false)
            #if PLATFORM_WINDOWS
            || String_IsEqual(Extension, StrLit(".rc"), false);
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
