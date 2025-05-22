// Copyright (c) Artisan Softworks
// Licensed under the BSD 3-Clause License. See the LICENSE file for details.

#ifndef UNITY_BUILD
#include "Backend.h"

#include "Core/Allocators.h"
#include "Core/Array.h"
#include "Core/StringUtils.h"
#include "Core/Filesystem.h"
#include "Core/Platform.h"
#include "Core/Log.h"
#endif

STRUCT(CompileData)
{
    const BuildParams* Params;
    u32* NumCompiled;
    u32 Index;
    u32 Padding;
};

bool C_DoCompile(CompileData* Data, const String RelativePath);
bool ASM_DoCompile(CompileData* Data, const String RelativePath);

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
        xx String_EatSpacesInlineFromEnd(&CmdLine);
        String_AppendSpace(&CmdLine);

        // ergghh i hate this... TODO: something better
        #if PLATFORM_WINDOWS
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

        xx String_EatSpacesInlineFromEnd(&WinSDKInclude);

        String_Append(&CmdLine, WinSDKInclude);
        #endif
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

bool C_Compile(const BuildParams* Params, u32* OutNumCompiled)
{
    if (NEVER(Params == NULL)) { return false; }

    // TODO: no early returns

    // compile all source files
    {
        CompileData UserData = { 0 };
        UserData.Params = Params;
        UserData.NumCompiled = OutNumCompiled;
        UserData.Index = 0;
        for each_string_in_list (Params->SourceFiles)
        {
            bool bSuccess;
            if (String_EndsWith(It.String, S(".asm"), false))
            {
                bSuccess = ASM_DoCompile(&UserData, It.String);
            }
            #if PLATFORM_WINDOWS
            else if (String_EndsWith(It.String, S(".manifest"), false))
            {
                UserData.Index++;

                // TODO
                bSuccess = true;
            }
            else if (String_EndsWith(It.String, S(".rc"), false))
            {
                UserData.Index++;

                StringLocal(FullPath, MAX_PATH_LENGTH);
                String_BuildPath(&FullPath, Params->RootDirectory, Params->SourceDirectory, It.String);

                // TODO: make async version
                bSuccess = RC_Compile(Params, FullPath, NULL);
                if (!bSuccess)
                {
                    LOG("Failed to build resource file \"%S\" for %S. Aborting build...", It.String, Params->AssemblyWithExt);
                    return false;
                }
            }
            #endif
            else
            {
                bSuccess = C_DoCompile(&UserData, It.String);
            }

            if (!bSuccess)
            {
                return false;
            }
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

    return true;
}

// TODO: collapse this function and C_DoCompile into one
bool ASM_DoCompile(CompileData* Data, const String RelativePath)
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


    StringLocal(ObjectPath, MAX_PATH_LENGTH);
    {
        StringLocal(ObjFile, MAX_PATH_LENGTH);
        {
            u32 LastDot = 0;
            bool bHasDot = String_IndexOfLastChar(RelativePath, '.', &LastDot);

            if (!Params->bDumpObjFilesInOneDirectory)
            {
                String_Append(&ObjFile, bHasDot ? StrSlice(RelativePath.Data, LastDot) : RelativePath);
            }

            const String Ext = String_IsValid(Params->CompilerObjectExt) ? Params->CompilerObjectExt : S(".o");
            if (!String_IsFirst(Ext, '.'))
            {
                String_AppendChar(&ObjFile, '.');
            }

            String_Append(&ObjFile, Ext);
        }

        String_BuildPath(&ObjectPath, Params->IntermediateBaseDirectory, ObjFile);
    }

    StringLocal(FullPath, MAX_PATH_LENGTH);
    String_BuildPath(&FullPath, Params->RootDirectory, Params->SourceDirectory, RelativePath);

    StringLocal(FullSourcePath, MAX_PATH_LENGTH+2);
    String_AppendChar(&FullSourcePath, '"');
    String_Append(&FullSourcePath, FullPath);
    String_AppendChar(&FullSourcePath, '"');

    // build cmd line string
    StringLocal(CmdLine, UINT16_MAX);
    {
        String_AppendChar(&CmdLine, '"');
        String_Append(&CmdLine, Params->AsmPath);
        String_AppendChar(&CmdLine, '"');

        String_BuildSeparator(&CmdLine, ' ', FullSourcePath, Params->AssemblerFlags, Params->AssemblerIncludes, Params->AssemblerDefines);
        xx String_EatSpacesInlineFromEnd(&CmdLine);
        String_Append(&CmdLine, S(" -o \""));
        String_Append(&CmdLine, ObjectPath);
        String_Append(&CmdLine, S("\""));
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

    PlatformHandle Handle = Platform_RunProcess(Params->AsmPath, CmdLine, Params->RootDirectory, String_Null());
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
    }

    return true;
}

bool C_DoCompile(CompileData* Data, const String RelativePath)
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

    StringLocal(ObjectPath, MAX_PATH_LENGTH);
    {
        u32 LastDot = 0;
        bool bHasDot = String_IndexOfLastChar(RelativePath, '.', &LastDot);

        StringLocal(ObjFile, MAX_PATH_LENGTH);

        if (!Params->bDumpObjFilesInOneDirectory)
        {
            String_Append(&ObjFile, bHasDot ? StrSlice(RelativePath.Data, LastDot) : RelativePath);
        }

        if (Params->Type == AssemblyType_PCH)
        {
            String_Append(&ObjFile, S(".gch"));
        }
        else
        {
            const String Ext = String_IsValid(Params->CompilerObjectExt) ? Params->CompilerObjectExt : S(".o");
            if (!String_IsFirst(Ext, '.'))
            {
                String_AppendChar(&ObjFile, '.');
            }

            String_Append(&ObjFile, Ext);
        }

        if (Params->Type == AssemblyType_PCH)
        {
            String_BuildPath(&ObjectPath, Params->RootDirectory, Params->BuildDirectory, ObjFile);
        }
        else
        {
            String_BuildPath(&ObjectPath, Params->IntermediateBaseDirectory, ObjFile);
        }
    }

    StringLocal(FullPath, MAX_PATH_LENGTH);
    String_BuildPath(&FullPath, Params->RootDirectory, Params->SourceDirectory, RelativePath);

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
    if (Params->Type == AssemblyType_CompilerObject)
    {
        String_Empty(&ObjectPath);

        // this is the format we're going for:
        // int/relativepath/assmeblyprefix|filename.no_ext|assemblypostfix|ext

        String RelativePathNoFile = String_Null();
        u32 LastSlash = 0;
        if (String_IndexOfLastPathSlash(RelativePath, &LastSlash))
        {
            RelativePathNoFile = StrSlice(RelativePath.Data, LastSlash);
        }

        const String FileName = Filesystem_ExtractFileName(RelativePath, false);

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
    }

    return true;
}

static void Internal_AppendObjSourceFiles(const BuildParams* Params, String* CmdLine, String DefaultObjExt)
{
    for each_string_in_list (Params->SourceFiles)
    {
        // ignore .manifest files
        if (String_EndsWith(It.String, S(".manifest"), false))
        {
            continue;
        }

        StringLocal(ObjectPath, MAX_PATH_LENGTH);

        const String RelativePath = It.String;

        u32 LastDot = 0;
        bool bHasDot = String_IndexOfLastChar(RelativePath, '.', &LastDot);

        #if PLATFORM_WINDOWS
        if (String_EndsWith(RelativePath, S(".rc"), false))
        {
            if (String_EndsWith(RelativePath, S("icon.rc"), false))
            {
                continue;
            }

            StringLocal(ResPath, MAX_PATH_LENGTH);
            String_Append(&ResPath, bHasDot ? StrSlice(RelativePath.Data, LastDot) : RelativePath);
            String_Append(&ResPath, S(".res"));

            String_BuildPath(&ObjectPath, ResPath);
        }
        else
        #endif
        {
            // make the object file string
            // example: some_file.c now becomes some_file.o
            StringLocal(ObjFile, MAX_PATH_LENGTH);
            {
                String_Append(&ObjFile, bHasDot ? StrSlice(RelativePath.Data, LastDot) : RelativePath);

                const String Ext = String_IsValid(Params->CompilerObjectExt) ? Params->CompilerObjectExt : DefaultObjExt;
                if (!String_IsFirst(Ext, '.'))
                {
                    String_AppendChar(&ObjFile, '.');
                }

                String_Append(&ObjFile, Ext);
            }

            String_BuildPath(&ObjectPath, Params->IntermediateDirectory, ObjFile);
        }

        String_AppendChar (CmdLine, '"');
        String_Append     (CmdLine, ObjectPath);
        String_AppendChar (CmdLine, '"');
        String_AppendSpace(CmdLine);
    }
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

        Internal_AppendObjSourceFiles(Params, &CmdLine, S(".o"));

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

        #if PLATFORM_WINDOWS
        String_Append(&CmdLine, S(" r "));
        #else
        String_Append(&CmdLine, S(" ar rcs "));
        #endif

        String_AppendChar(&CmdLine, '"');

        StringLocal(BuildPath, MAX_PATH_LENGTH);
        String_BuildPath(&BuildPath, Params->RootDirectory, Params->BuildDirectory);
        String_AppendPathSeparator(&BuildPath);

        String_Append(&CmdLine, BuildPath);

        StringLocal(LibFile, MAX_PATH_LENGTH);
        {
            String_Append(&LibFile, Params->Assembly);

            if (Params->Type == AssemblyType_Library)
            {
                String_AppendChar(&LibFile, 'S');
            }

            #if PLATFORM_WINDOWS
            String_Append(&LibFile, S(".lib"));
            #else
            String_Append(&LibFile, S(".a"));
            #endif
        }

        String_Append(&CmdLine, LibFile);
        String_Append(&CmdLine, S("\" "));

        Internal_AppendObjSourceFiles(Params, &CmdLine, S(".o"));

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
    // TODO: get the full path to this
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







LinearAllocator GMSVCFindAllocator = {0};

#if PLATFORM_WINDOWS

/// TODO: if multithreaded and more than on soruce file. use /MP and call cl.exe only once

bool MSVC_DoCompile(CompileData* Data, const String RelativePath);
bool MSVC_ASM_DoCompile(CompileData* Data, const String RelativePath);

bool MSVC_Compile(const BuildParams* Params, u32* OutNumCompiled)
{
    if (NEVER(Params == NULL)) { return false; }

    // TODO: get rid of early returns

    // compile all source files
    {
        CompileData UserData = { 0 };
        UserData.Params = Params;
        UserData.NumCompiled = OutNumCompiled;
        UserData.Index = 0;
        for each_string_in_list (Params->SourceFiles)
        {
            bool bSuccess;
            if (String_EndsWith(It.String, S(".asm"), false))
            {
                bSuccess = MSVC_ASM_DoCompile(&UserData, It.String);
            }
            #if PLATFORM_WINDOWS
            else if (String_EndsWith(It.String, S(".manifest"), false))
            {
                UserData.Index++;

                // TODO
                bSuccess = true;
            }
            else if (String_EndsWith(It.String, S(".rc"), false))
            {
                UserData.Index++;

                StringLocal(FullPath, MAX_PATH_LENGTH);
                String_BuildPath(&FullPath, Params->RootDirectory, Params->SourceDirectory, It.String);

                // TODO: make async version
                bSuccess = RC_Compile(Params, FullPath, NULL);
                if (!bSuccess)
                {
                    LOG_ERROR("Failed to build resource file \"%S\" for %S. Aborting build...", It.String, Params->AssemblyWithExt);
                    return false;
                }
            }
            #endif
            else
            {
                bSuccess = MSVC_DoCompile(&UserData, It.String);
            }

            if (!bSuccess)
            {
                return false;
            }
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

    return true;
}

bool MSVC_ASM_DoCompile(CompileData* Data, const String RelativePath)
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

    u32 LastDot = 0;
    xx String_IndexOfLastChar(RelativePath, '.', &LastDot);

    StringLocal(ObjFile, MAX_PATH_LENGTH);
    String_Append(&ObjFile, LastDot > 0 ? StrSlice(RelativePath.Data, LastDot) : RelativePath);
    String_Append(&ObjFile, S(".obj"));

    StringLocal(ObjectPath, MAX_PATH_LENGTH);
    String_BuildPath(&ObjectPath, Params->IntermediateBaseDirectory, ObjFile);

    StringLocal(FullPath, MAX_PATH_LENGTH);
    String_BuildPath(&FullPath, Params->RootDirectory, Params->SourceDirectory, RelativePath);

    StringLocal(FullSourcePath, MAX_PATH_LENGTH+2);
    String_AppendChar(&FullSourcePath, '"');
    String_Append(&FullSourcePath, FullPath);
    String_AppendChar(&FullSourcePath, '"');

    // build cmd line string
    StringLocal(CmdLine, UINT16_MAX);
    {
        String_AppendChar(&CmdLine, '"');
        String_Append(&CmdLine, Params->AsmPath);
        String_AppendChar(&CmdLine, '"');

        String_Append(&CmdLine, S(" /nologo /c /Fo\""));
        String_Append(&CmdLine, ObjectPath);
        String_Append(&CmdLine, S("\" "));

        String_BuildSeparator(&CmdLine, ' ', FullSourcePath, Params->AssemblerFlags, Params->AssemblerIncludes, Params->AssemblerDefines);
        xx String_EatSpacesInlineFromEnd(&CmdLine);
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

    if (bQuietBuild) { Logging_Enable(); }

    if (Params->bShouldWaitPerCompileProcess)
    {
        LogCompilingFile(Data->Index, Params->NumSources, FullPath);
    }

    if (bQuietBuild) { Logging_Disable(); }

    if (Params->bVerbose)
    {
        LOG("\n    %S\n", CmdLine);
    }

    PlatformHandle Handle = Platform_RunProcess(Params->AsmPath, CmdLine, Params->RootDirectory, String_Null());
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
    }

    return true;
}

// todo: compiling with cl makes this run serially? (happens on release mode only)

bool MSVC_DoCompile(CompileData* Data, const String RelativePath)
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

    StringLocal(FullPath, MAX_PATH_LENGTH);
    String_BuildPath(&FullPath, Params->RootDirectory, Params->SourceDirectory, RelativePath);
    
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
    String_Copy(&RelativePathCopy, bHasDot ? StrSlice(RelativePath.Data, LastDot) : RelativePath);
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

        Internal_AppendObjSourceFiles(Params, &CmdLine, S(".obj"));

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

        Internal_AppendObjSourceFiles(Params, &CmdLine, S(".obj"));

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
    // TODO: get the full path to this
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
