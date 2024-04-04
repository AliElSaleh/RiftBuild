#include "Backend.h"

#include "Structures/Array.h"
#include "String/StringUtils.h"
#include "Platform/Filesystem.h"
#include "Platform/Platform.h"
#include "Profiling/ProfilingSubsystem.h"
#include "Log.h"

/// TODO: if multithreaded and more than on soruce file. use /MP and call cl.exe only once

#if PLATFORM_WINDOWS
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
            String_BuildPath(&ObjectFilePath, Params->IntermediateDirectory, Params->SourceDirectory, FilePath);

            u64 ObjectFileWriteTime = Filesystem_GetLastWriteTime(ObjectFilePath);
            u64 SourceFileWriteTime = Filesystem_GetLastWriteTime(It->FullPath);

            if (SourceFileWriteTime >= ObjectFileWriteTime)
            {
                StringLocal(CmdLine, Kibibytes(4));
                String_Append(&CmdLine, StrLit("ml64 /nologo /c /Fo\""));

                StringLocal(ObjectPath, MAX_PATH_LENGTH);
                String_BuildPath(&ObjectPath, Params->IntermediateDirectory, Params->SourceDirectory);
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
        
        StringLocal(CmdLine, Kibibytes(4));
        String_Append(&CmdLine, StrLit("cl /nologo "));

        StringLocal(FilePath, MAX_PATH_LENGTH);
        String_Append(&FilePath, StrSlice(FileName.Data, LastDot));
        String_Append(&FilePath, StrLit(".obj"));

        // build object path
        StringLocal(ObjectFilePath, MAX_PATH_LENGTH);
        String_BuildPath(&ObjectFilePath, Params->IntermediateDirectory, Params->SourceDirectory, FilePath);

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
        String_BuildPath(&ObjectPath, Params->IntermediateDirectory, Params->SourceDirectory);
        String_EatPathSeparatorsInlineFromEnd(&ObjectPath);

        String_Append(&CmdLine, StrLit(" /Fo\""));
        String_Append(&CmdLine, ObjectPath);
        String_Append(&CmdLine, StrLit("\\\\\" /c"));

        if (Params->bShouldWaitPerCompileProcess)
            LOG_INLINE("[%i/%i] Compiling ", TotalWorkDone, Array_Num(Params->SourceFiles));

        PlatformHandle H = Platform_RunCommand(CmdLine, Params->RootDirectory);
        Array_Add(Processes, H);

        if (Params->bShouldWaitPerCompileProcess)
        {
            Platform_WaitForHandle(H, -1);

            const u32 ExitCode = Platform_GetExitCodeForProcess(H);
            if (ExitCode != 0)
            {
                LOG_ERROR("Compiler errors detected. See above errors to fix. Exit code for process: %u. Aborting build...", ExitCode);

                #ifdef DEVELOPER
                Platform_Sleep(5000);
                #endif

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

            #ifdef DEVELOPER
            Platform_Sleep(5000);
            #endif

            return false;
        }
    }

    // compile resource files
    u32 i = 0;
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
                LOG("Failed to build resource file \"%S\" for %S.%S. Aborting build...", It->RelativePath, Params->Assembly, Params->Extension);
                return false;
            }
        }
    }

    return true;
}

/*
bool MSVC_Compile(const BuildParams* Params, u32* NumCompiled)
{
    // build cmd line string
    StringLocal(CmdLine, Kibibytes(32));

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

    bool bAnyAsmFilesModified = false;
    u32 NumAsmFiles = 0;
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
            String_BuildPath(&ObjectFilePath, Params->IntermediateDirectory, Params->SourceDirectory, FilePath);

            u64 ObjectFileWriteTime = Filesystem_GetLastWriteTime(ObjectFilePath);
            u64 SourceFileWriteTime = Filesystem_GetLastWriteTime(It->FullPath);

            if (SourceFileWriteTime >= ObjectFileWriteTime)
            {
                if (NumAsmFiles > 0)
                    String_Append(&CmdLine, StrLit("&& "));

                String_Append(&CmdLine, StrLit("ml64 /nologo /c /Fo\""));

                StringLocal(ObjectPath, MAX_PATH_LENGTH);
                String_BuildPath(&ObjectPath, Params->IntermediateDirectory, Params->SourceDirectory);
                String_EatPathSeparatorsInlineFromEnd(&ObjectPath);

                String_Append(&CmdLine, ObjectPath);
                String_Append(&CmdLine, StrLit("\\\\\" "));

                String_Append(&CmdLine, Params->SourceDirectory);
                String_AppendPathSeparator(&CmdLine);
                String_Append(&CmdLine, It->RelativePath);
                String_AppendSpace(&CmdLine);

                bAnyAsmFilesModified = true;
                NumAsmFiles++;
            }
        }
    }

    if (NumAsmFiles > 0)
    {
        String_Append(&CmdLine, StrLit("&& "));
    }

    u32 Len = CmdLine.Length;
    StringLocal(ClCmd, Kibibytes(16));
    String_Append(&ClCmd, StrLit("cl /nologo "));

    StringLocal(ErrorLimit, 8);
    if (Params->MaxErrors > 0)
    {
        String_Format(&ErrorLimit, StrLit("/F%i"), 32, Params->MaxErrors);
    }

    u32 TotalWorkDone = 0;
    bool bAnySkipped = false;
    for each (It, Params->SourceFiles)
    {
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

        StringLocal(FilePath, MAX_PATH_LENGTH);
        String_Append(&FilePath, StrSlice(FileName.Data, LastDot));
        String_Append(&FilePath, StrLit(".obj"));

        // build object path
        StringLocal(ObjectPath, MAX_PATH_LENGTH);
        String_BuildPath(&ObjectPath, Params->IntermediateDirectory, Params->SourceDirectory, FilePath);

        u64 ObjectFileWriteTime = Filesystem_GetLastWriteTime(ObjectPath);
        u64 SourceFileWriteTime = Filesystem_GetLastWriteTime(File.FullPath);

        if (ObjectFileWriteTime >= SourceFileWriteTime)
        {
            #ifndef HOOD
            LOG("[Skipping] %S", File.FullPath);
            #else
            LOG("skip'n dis shit %S", File.FullPath);
            #endif

            bAnySkipped = true;

            continue;
        }

        if (String_EndsWith(File.RelativePath, StrLit(".asm"), false))
        {
            continue;
        }

        TotalWorkDone++;

        StringLocal(FullSourcePath, MAX_PATH_LENGTH+2);
        String_BuildPath(&FullSourcePath, Params->SourceDirectory, File.RelativePath);

        String_AppendChar(&ClCmd, '"');
        String_Append(&ClCmd, FullSourcePath);
        String_AppendChar(&ClCmd, '"');
        String_AppendSpace(&ClCmd);
    }

    *NumCompiled = TotalWorkDone;

    if (TotalWorkDone == 0 && !bAnyAsmFilesModified)
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

    if (TotalWorkDone > 0)
    {
        StringLocal(ObjectPath, MAX_PATH_LENGTH);
        String_BuildPath(&ObjectPath, Params->IntermediateBaseDirectory, Params->SourceDirectory);
        String_EatPathSeparatorsInlineFromEnd(&ObjectPath);

        String_BuildSeparator(&ClCmd, ' ', Params->CompilerFlags, Params->DefineFlags, Params->IncludeFlags);
        String_EatSpacesInlineFromEnd(&ClCmd);

        StringLocal(ObjectPathArg, MAX_PATH_LENGTH);
        StringInternal_Concat(&ObjectPathArg, 3, StrLit(" /Fo\""), ObjectPath, StrLit("\\\\\" /c")); // two backslashes are needed for cl, so it does not shit itself

        String_Append(&ClCmd, ObjectPathArg);
        String_EatSpacesInlineFromEnd(&ClCmd);

        String_Append(&CmdLine, ClCmd);
    }

    StringLocal(BuildPath, MAX_PATH_LENGTH);
    String_BuildPath(&BuildPath, Params->RootDirectory, Params->BuildDirectory);
    String_AppendPathSeparator(&BuildPath);

    StringLocal(AllObjFiles, Kibibytes(16));
    for each (It, Params->SourceFiles)
    {
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

        StringLocal(FilePath, MAX_PATH_LENGTH);
        String_Append(&FilePath, StrSlice(FileName.Data, LastDot));
        String_Append(&FilePath, StrLit(".obj"));

        // build object path
        StringLocal(ObjectPath, MAX_PATH_LENGTH);
        String_BuildPath(&ObjectPath, Params->IntermediateDirectory, Params->SourceDirectory, FilePath);

        String_AppendChar(&AllObjFiles, '"');
        String_Append(&AllObjFiles, ObjectPath);
        String_AppendChar(&AllObjFiles, '"');
        String_AppendSpace(&AllObjFiles);
    }

    String LinkTool = *String_Null();
    bool bIsDLL = String_IsEqual(Params->Extension, StrLit("dll"), false);
    if (String_IsEqual(Params->Extension, StrLit("exe"), false) ||
        bIsDLL)
    {
        if (bIsDLL)
        {
            LinkTool = StrLit("&& link /dll");
        }
        else
        {
            LinkTool = StrLit("&& link");
        }

        if (TotalWorkDone == 0)
        {
            LinkTool = StrLit("link");
            if (bIsDLL)
                LinkTool = StrLit("link /dll");
        }
        else
        {
            String_AppendSpace(&CmdLine);
        }

        String_Append(&CmdLine, LinkTool);
        String_Append(&CmdLine, StrLit(" /nologo "));

        String_BuildSeparator(&CmdLine, ' ', Params->LinkerDefineFlags, Params->LinkerFlags, Params->IconResFilePath, Params->Libraries, Params->LibraryDirectories, AllObjFiles);
        String_EatSpacesInlineFromEnd(&CmdLine);
        StringInternal_Concat(&CmdLine, 6, StrLit(" /OUT:\""), BuildPath, Params->Assembly, StrLit("."), Params->Extension, StrLit("\" "));
        String_EatSpacesInlineFromEnd(&CmdLine);
    }
    else if (String_IsEqual(Params->Extension, StrLit("lib"), false))
    {
        if (TotalWorkDone == 0)
        {
            String_Append(&CmdLine, StrLit("lib /nologo /OUT:\""));
        }
        else
        {
            String_Append(&CmdLine, StrLit(" && lib /nologo /OUT:\""));
        }

        String_Append(&CmdLine, BuildPath);
        String_Append(&CmdLine, Params->Assembly);
        String_Append(&CmdLine, StrLit(".lib\" "));

        String_Append(&CmdLine, AllObjFiles);
    }

    if (bAnyAsmFilesModified)
    {
        (*NumCompiled)++;
    }

    if (bAnySkipped)
    {
        LOG_LINE_BREAK();
    }

    LOG("CMD: %S\n", StrShiftF(CmdLine, Len));

    PlatformHandle Handle = Platform_RunCommand(CmdLine, Params->RootDirectory);
    Platform_WaitForHandle(Handle, -1);

    const u32 ExitCode = Platform_GetExitCodeForProcess(Handle);
    if (ExitCode != 0)
    {
        #ifndef HOOD
        LOG_ERROR("Compiler errors detected. See above errors to fix. Exit code for process: %u. Aborting build...", ExitCode);
        #else
        LOG_ERROR("seen some compiler errors homie. fix yo shit up, something aint linkin' right");
        #endif

        #ifdef DEVELOPER
        Platform_Sleep(5000);
        #endif
        
        return false;
    }
  
    return true;
}
*/

bool MSVC_Link(const BuildParams* Params)
{
    if (String_IsEqual(Params->Extension, StrLit("pch"), false))
    {
        return true;
    }

    StringLocal(BuildPath, MAX_PATH_LENGTH);
    String_BuildPath(&BuildPath, Params->RootDirectory, Params->BuildDirectory);
    String_AppendPathSeparator(&BuildPath);

    StringLocal(CmdLine, Kibibytes(32));

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

            String_BuildPath(&ObjectPath, Params->IntermediateDirectory, Params->SourceDirectory, FilePath);
        }

        String_AppendChar(&AllObjFiles, '"');
        String_Append(&AllObjFiles, ObjectPath);
        String_AppendChar(&AllObjFiles, '"');
        String_AppendSpace(&AllObjFiles);
    }

    bool bIsDLL = String_IsEqual(Params->Extension, StrLit("dll"), false);
    if (String_IsEqual(Params->Extension, StrLit("exe"), false) ||
        bIsDLL)
    {
        String_Append(&CmdLine, StrLit("link"));
        if (bIsDLL)
            String_Append(&CmdLine, StrLit(" /dll"));
        String_Append(&CmdLine, StrLit(" /nologo "));

        String_BuildSeparator(&CmdLine, ' ', Params->LinkerDefineFlags, Params->LinkerFlags, Params->IconResFilePath, Params->VersionResFilePath, Params->Libraries, Params->LibraryDirectories, AllObjFiles);
        String_EatSpacesInlineFromEnd(&CmdLine);
        StringInternal_Concat(&CmdLine, 6, StrLit(" /OUT:\""), BuildPath, Params->Assembly, StrLit("."), Params->Extension, StrLit("\" "));
        String_EatSpacesInlineFromEnd(&CmdLine);
    }
    else if (String_IsEqual(Params->Extension, StrLit("lib"), false))
    {
        String_Append(&CmdLine, StrLit("lib /nologo /OUT:\""));

        String_Append(&CmdLine, BuildPath);
        String_Append(&CmdLine, Params->Assembly);
        
        String_Append(&CmdLine, StrLit(".lib\" "));

        String_Append(&CmdLine, AllObjFiles);

        String_Append(&CmdLine, Params->Libraries);
        String_AppendSpace(&CmdLine);
        String_Append(&CmdLine, Params->LibraryDirectories);
        String_AppendSpace(&CmdLine);
        String_Append(&CmdLine, Params->VersionResFilePath);
    }

    LOG("\nLinking: %S\n", CmdLine);

    PlatformHandle Handle = Platform_RunCommand(CmdLine, Params->RootDirectory);
    Platform_WaitForHandle(Handle, -1);

    u32 ExitCode = Platform_GetExitCodeForProcess(Handle);
    if (ExitCode != 0)
    {
        #ifndef HOOD
        LOG_ERROR("Compiler errors detected. See above errors to fix. Exit code for process: %u. Aborting build...", ExitCode);
        #else
        LOG_ERROR("seen some compiler errors homie. fix yo shit up, something aint linkin' right");
        #endif

        #ifdef DEVELOPER
        Platform_Sleep(5000);
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
        Platform_WaitForHandle(H, -1);

        ExitCode = Platform_GetExitCodeForProcess(H);

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
