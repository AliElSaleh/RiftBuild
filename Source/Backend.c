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

static void LogCompilingFile(u32 Index, u32 NumSources, String FullPath)
{
    if (bQuietBuild) { Logging_Enable(); }
    #ifndef HOOD
    u8 NumDigits1 = Integer_CountDigits(NumSources);
    u8 NumDigits2 = Integer_CountDigits(Index);
    u8 Diff = (u8)(NumDigits1 - NumDigits2) + 1;

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

    const bool bWindres = String_IsEqual(Params->RCProgram, S("windres"), false);

    // we are intentionally adding a backslash here for the windres compiler specifically, because
    // the developers behind this are incompetent assholes who don't know how to properly handle
    // spaces within paths... like holy shit man... so depressing
    // https://sourceware.org/bugzilla/show_bug.cgi?id=4933
    // https://sourceware.org/bugzilla/show_bug.cgi?id=4356
    // https://github.com/msys2/MINGW-packages/issues/1035
    // https://github.com/msys2/MINGW-packages/issues/1035#issuecomment-3208735163
    if (bWindres) { String_Append(&CmdLine, S("\\")); }
    String_Append(&CmdLine, S("\""));
    String_Append(&CmdLine, Params->RCProgramPath);
    if (bWindres) { String_Append(&CmdLine, S("\\")); }
    String_Append(&CmdLine, S("\""));
    
    if (Params->RCProgramFlags.Length > 0)
    {
        String_AppendSpace(&CmdLine);
        String_Append(&CmdLine, Params->RCProgramFlags);
        xx String_EatSpacesInlineFromEnd(&CmdLine);
        String_AppendSpace(&CmdLine);

        // ergghh i hate this... TODO: something better
        #if PLATFORM_WINDOWS

        if (String_EndsWith(Params->RCProgramPath, S("rc.exe"), false))
        {
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
        }
        #endif
    }

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

    // todo: resource defines

    if (bWindres)
    {
        // for windres specifically, args must be in this form
        // <FLAGS> -O coff <DEFINES> -i <SOURCE> -o <OBJECT>

        String_Append(&CmdLine, S(" -O coff"));

        String_Append(&CmdLine, S(" -i"));
        String_Append(&CmdLine, S(" \""));
        String_Append(&CmdLine, FullRCPath);
        String_AppendChar(&CmdLine, '"');

        String_Append(&CmdLine, S(" -o "));
        String_Append(&CmdLine, ResPath);
    }
    else
    {
        String_Append(&CmdLine, S(" \""));
        String_Append(&CmdLine, FullRCPath);
        String_AppendChar(&CmdLine, '"');
    }

    LOG("Compiling resource %S", FullRCPath);
    
    if (Params->bVerbose) { LOG("\n    %S\n", CmdLine); }

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

static bool Internal_DoCompile(CompileData* Data, const String RelativePath)
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

    const String Ext = Filesystem_ExtractFileExtension(RelativePath, true);

    StringLocal(RelativePathCopy, MAX_PATH_LENGTH);
    if (Params->Type == AssemblyType_PCH)
    {
        bool bIsCppHeader = String_EndsWith(RelativePath, S(".hh"), false)  ||
                            String_EndsWith(RelativePath, S(".hpp"), false) ||
                            String_EndsWith(RelativePath, S(".hxx"), false) ||
                            String_EndsWith(RelativePath, S(".h++"), false);

        // generate a source file for the precompiled header (if it doesnt exist yet)
        StringLocal(PchSourceFile, MAX_PATH_LENGTH);
        String_Copy(&RelativePathCopy, Filesystem_StripFileExtension(RelativePath));

        const String Exts[5] = { S(".c"), S(".cc"), S(".cxx"), S(".c++"), S(".cpp") };
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

            FileHandle f = FileHandle_Null();
            if (Filesystem_Open(PchSourceFile, FileMode_Write, &f))
            {
                xx Filesystem_WriteLineFormatted(f, S("#include \"%S\"\n"), NULL, RelativePath);
                Filesystem_Close(&f);
            }
        }
    }
    else
    {
        RelativePathCopy = RelativePath;
    }

    StringLocal(FullPath, MAX_PATH_LENGTH);
    String_BuildPath(&FullPath, Params->RootDirectory, Params->SourceDirectory, RelativePathCopy);

    StringLocal(FullSourcePath, MAX_PATH_LENGTH+2);
    String_AppendChar(&FullSourcePath, '"');
    String_Append    (&FullSourcePath, FullPath);
    String_AppendChar(&FullSourcePath, '"');

    String DefaultObjExtension = S(".o");

    bool bIsMicrosoftCompiler  = String_EndsWith(Params->CompilerPath, S("cl.exe"), false);
    bool bIsMicrosoftAssembler = String_EndsWith(Params->AsmPath, S("ml.exe"), false) ||
                                 String_EndsWith(Params->AsmPath, S("ml64.exe"), false);

    if ((bIsMicrosoftAssembler &&  IsAsmSource(Ext)) ||
        (bIsMicrosoftCompiler  && !IsAsmSource(Ext)))
    {
        DefaultObjExtension = S(".obj");
    }

    // this is the format we're going for:
    // int/relativepath/assmeblyprefix|filename.no_ext|assemblypostfix|ext
    StringLocal(ObjectPath, MAX_PATH_LENGTH);
    StringLocal(PCHObjectPath, MAX_PATH_LENGTH);
    {
        StringLocal(ObjFile, MAX_PATH_LENGTH);
        {
            String Name    = Filesystem_ExtractFileName(RelativePath, false);
            String Prefix  = String_Null();
            String Postfix = String_Null();
            String ObjExt  = String_IsValid(Params->CompilerObjectExt) ? Params->CompilerObjectExt : DefaultObjExtension;

            if (Params->Type == AssemblyType_CustomCompilerObject)
            {
                Prefix  = Params->AssemblyPrefix;
                Postfix = Params->AssemblyPostfix;
                ObjExt  = Params->Extension;
            }

            String_Append(&ObjFile, Prefix);
            String_Append(&ObjFile, Name);
            String_Append(&ObjFile, Postfix);
            if (ObjExt.Length > 0 && !String_IsFirst(ObjExt, '.'))
            {
                String_AppendChar(&ObjFile, '.');
            }
            String_Append(&ObjFile, ObjExt);
        }

        String PathOfObj = Filesystem_ExtractFilePath(RelativePath, false);

        if (Params->bDumpObjFilesInOneDirectory)
        {
            PathOfObj = String_Null();
        }

        String_BuildPath(&ObjectPath, Params->RootDirectory, Params->IntermediateDirectory, PathOfObj, ObjFile);
        xx Filesystem_ConvertRelativeToAbsolutePath(&ObjectPath);
        
        if (Params->Type == AssemblyType_PCH)
        {
            String_Empty(&ObjFile);
            String_Append(&ObjFile, Params->Assembly);

            String ObjExt = S(".gch");

            #if PLATFORM_WINDOWS
            if (bIsMicrosoftCompiler)
            {
                ObjExt = S(".pch");
            }
            #endif

            String_Append(&ObjFile, ObjExt);

            String_BuildPath(&PCHObjectPath, Params->RootDirectory, Params->BuildDirectory, PathOfObj, ObjFile);
            xx Filesystem_ConvertRelativeToAbsolutePath(&PCHObjectPath);
        }
    }

    // ===============================================================================================

    String ProgramPath = Params->CompilerPath;
    StringLocal(CmdLine, UINT16_MAX);

    // switch on the source file type
    if (IsAsmSource(Ext))
    {
        ProgramPath = Params->AsmPath;

        String_AppendChar(&CmdLine, '"');
        String_Append    (&CmdLine, Params->AsmPath);
        String_AppendChar(&CmdLine, '"');
        String_AppendSpace(&CmdLine);

        if (bIsMicrosoftAssembler)
        {
            String_Append(&CmdLine, S("/nologo /c /Fo\""));
            String_Append(&CmdLine, ObjectPath);
            String_Append(&CmdLine, S("\" "));
            String_BuildSeparator(&CmdLine, ' ', FullSourcePath, Params->AssemblerFlags, Params->AssemblerDefines, Params->AssemblerIncludes);
            xx String_EatSpacesInlineFromEnd(&CmdLine);
        }
        else
        {
            String_BuildSeparator(&CmdLine, ' ', FullSourcePath, Params->AssemblerFlags, Params->AssemblerDefines, Params->AssemblerIncludes);
            xx String_EatSpacesInlineFromEnd(&CmdLine);
            String_Append(&CmdLine, S(" -o \""));
            String_Append(&CmdLine, ObjectPath);
            String_Append(&CmdLine, S("\""));
        }
    }
    else if (String_EndsWith(RelativePath, S(".manifest"), false))
    {
        return true;
    }
    else if (String_EndsWith(RelativePath, S(".rc"), false))
    {
        // TODO: make async version
        bool bSuccess = RC_Compile(Params, FullPath, NULL);
        if (!bSuccess)
        {
            LOG("Failed to build resource file \"%S\" for %S. Aborting build...", RelativePath, Params->AssemblyWithExt);
        }

        return bSuccess;
    }
    else
    {
        String OutputFlag  = S("-o");
        String CompileFlag = S("-c");

        String AdditionalFlags = String_Null();

        StringLocal(PCHFlags, MAX_PATH_LENGTH*2);

        #if PLATFORM_WINDOWS
        StringLocal(WinSDKInclude, MAX_PATH_LENGTH*7); // 7 paths
        #else
        String WinSDKInclude = String_Null();
        #endif

        String_AppendChar(&CmdLine, '"');
        String_Append    (&CmdLine, Params->CompilerPath);
        String_AppendChar(&CmdLine, '"');

        if (Params->Type == AssemblyType_CustomCompilerObject)
        {
            OutputFlag = Params->CompilerOutputFlag;
            CompileFlag = String_Null();
        }
        else
        {
            #if PLATFORM_WINDOWS
            if (bIsMicrosoftCompiler)
            {
                OutputFlag  = S("/Fo:");
                CompileFlag = S(" /nologo /c ");


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

                if (Params->Type == AssemblyType_PCH)
                {
                    // creating a pch
                    String_Append(&PCHFlags, S("/Yc\""));
                    String_Append(&PCHFlags, RelativePath);
                    String_Append(&PCHFlags, S("\" "));

                    String_Append(&PCHFlags, S("/Fp\""));
                    String_Append(&PCHFlags, PCHObjectPath);
                    String_Append(&PCHFlags, S("\""));
                }
                else
                {
                    // using a pch
                    if (Params->PCHPath.Length)
                    {
                        const String Trimmed = Filesystem_StripFileExtension(Params->PCHPath);

                        String_Append(&PCHFlags, S("/Yu\""));
                        if (Params->PCHHeaderPath.Length)
                        {
                            String_Append(&PCHFlags, Params->PCHHeaderPath);
                        }
                        else
                        {
                            String_Append(&PCHFlags, Trimmed);

                            // find a header that exists
                            bool bAnyFound = false;
                            const String Exts[5] = { S(".h"), S(".hh"), S(".hpp"), S(".hxx"), S(".h++") };
                            for (u8 i = 0; i < SArray_Capacity(Exts); i++)
                            {
                                StringLocal(Test, MAX_PATH_LENGTH);
                                String_Append(&Test, Trimmed);
                                String_Append(&Test, Exts[i]);
                                if (Filesystem_DoesFileExist(Test))
                                {
                                    String_Append(&PCHFlags, Exts[i]);
                                    bAnyFound = true;
                                    break;
                                }
                            }

                            // hardcode the extension as failsafe
                            if (!bAnyFound)
                            {
                                String_Append(&PCHFlags, S(".h"));
                            }
                        }

                        String_Append(&PCHFlags, S("\" "));

                        String_Append(&PCHFlags, S("/Fp\""));
                        String_Append(&PCHFlags, Trimmed);
                        String_Append(&PCHFlags, S(".pch\""));
                    }
                }

            }
            #else
            if (Params->Type == AssemblyType_Library ||
                Params->Type == AssemblyType_DynamicLibrary)
            {
                AdditionalFlags = S("-fPIC -fvisibility=default");
            }
            else if (Params->Type == AssemblyType_Library ||
                     Params->Type == AssemblyType_StaticLibrary)
            {
                AdditionalFlags = S("-fPIC");
            }
            else if (Params->Type == AssemblyType_Executable)
            {
                AdditionalFlags = S("-fPIE");
            }
            else
            {
                // no action required
            }
            #endif

            // using a pch
            if (Params->PCHPath.Length)
            {
                // TODO: no hardcoded string compiler
                if (String_IsEqual(Params->CompilerProgram, S("clang"), false) ||
                    String_IsEqual(Params->CompilerProgram, S("clang++"), false))
                {
                    const String Trimmed = Filesystem_StripFileExtension(Params->PCHPath);

                    String_Concat(&PCHFlags, S("-include-pch \""), Trimmed, S(".h.gch\""));
                }
            }
        }

        String_BuildSeparator(&CmdLine, ' ', CompileFlag, FullSourcePath, Params->CompilerFlags, Params->IncludeFlags, Params->DefineFlags, WinSDKInclude, AdditionalFlags, PCHFlags, OutputFlag);
        xx String_EatSpacesInlineFromEnd(&CmdLine);

        String_Append(&CmdLine, S(" \""));
        String_Append(&CmdLine, ObjectPath);
        String_Append(&CmdLine, S("\""));
    }

    // ===============================================================================================

    u64 ObjectFileWriteTime = Filesystem_GetLastWriteTime(ObjectPath);
    u64 SourceFileWriteTime = Filesystem_GetLastWriteTime(FullPath);

    if (ObjectFileWriteTime >= SourceFileWriteTime)
    {
        #ifndef HOOD
        LOG("[Skipping] %S", FullPath);
        #else
        LOG("skip'n dis shit %S", FullPath);
        #endif
    }
    else
    {
        xx Filesystem_NewFile(ObjectPath);

        if (bQuietBuild) { Logging_Enable(); }

        if (!(bIsMicrosoftCompiler || bIsMicrosoftAssembler) || Params->bShouldWaitPerCompileProcess)
        {
            LogCompilingFile(Data->Index, Params->NumSources, FullPath);
        }

        if (bQuietBuild) { Logging_Disable(); }

        if (Params->bVerbose)
        {
            LOG("\n    %S\n", CmdLine);
        }

        PlatformHandle Handle = Platform_RunProcess(ProgramPath, CmdLine, Params->RootDirectory, String_Null());
        if (Platform_IsValidHandle(Handle))
        {
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
        }
        else
        {
            LOG_ERROR("Failed to spawn compiler process: \"%S\"", ProgramPath);
            return false;
        }
    }

    return true;
}

bool C_Compile(const BuildParams* Params, u32* OutNumCompiled)
{
    if (NEVER(Params == NULL)) { return false; }
    if (NEVER(OutNumCompiled == NULL)) { return false; }

    bool bSuccess = true;

    // compile all source files
    {
        CompileData UserData = { 0 };
        UserData.Params = Params;
        UserData.NumCompiled = OutNumCompiled;
        UserData.Index = 0;

        for each_string_in_list (Params->SourceFiles)
        {
            bSuccess = Internal_DoCompile(&UserData, It.String);
            if (!bSuccess)
            {
                break;
            }
        }
    }

    if (bSuccess)
    {
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

                bSuccess = false;
            }
        }
    }

    return bSuccess;
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

            // TCC doesn't recognize .res files as of 0.9.28
            if (!String_IsEqual(Params->CompilerProgram, S("tcc"), false))
            {
                StringLocal(ResPath, MAX_PATH_LENGTH);
                String_Append(&ResPath, bHasDot ? StrSlice(RelativePath.Data, LastDot) : RelativePath);
                String_Append(&ResPath, S(".res"));

                String_BuildPath(&ObjectPath, Params->SourceDirectory, ResPath);
            }
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
            String_IsEqual(Extension, S(".s"), false)
            #if PLATFORM_WINDOWS
            || String_IsEqual(Extension, S(".rc"), false)
            || String_IsEqual(Extension, S(".manifest"), false)
            #elif PLATFORM_APPLE
            || String_IsEqual(Extension, S(".m"), false)
            || String_IsEqual(Extension, S(".mm"), false)
            #endif
            ;
}

bool IsAsmSource(const String Extension)
{
    return  String_IsEqual(Extension, S(".asm"), false) ||
            String_IsEqual(Extension, S(".s"), true);
}

bool IsAsmCSource(const String Extension)
{
    return  String_IsEqual(Extension, S(".S"), true);
}

bool IsCSource(const String Extension)
{
    return  String_IsEqual(Extension, S(".c"), false);
}

bool IsObjCSource(const String Extension)
{
    return  String_IsEqual(Extension, S(".m"), false) ||
            String_IsEqual(Extension, S(".mm"), false);
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

/// TODO: compiling with cl makes this run serially? (happens on release mode only)
/// TODO: if multithreaded and more than on soruce file. use /MP and call cl.exe only once

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
#endif // PLATFORM_WINDOWS


static void GetAdditionalLinkerFlags(const BuildParams* Params, String* AdditionalFlags)
{
    bool bIsMicrosoftLinker = String_EndsWith(Params->LinkerPath, S("link.exe"), false);

    // additional linker settings that are annoying to specify in the build file for all 3 major compilers
    // as clang, gcc and msvc have different ways of doing this
    // (and for all the different platforms as well)
    if (Params->Type == AssemblyType_Executable)
    {
        String NoDefaultLibs;// = String_Null();
        String NoStd         = String_Null();

        if (bIsMicrosoftLinker)
        {
            // todo: support
            // /NODEFAULTLIB:somelibrary /NODEFAULTLIB:anotherlibrary etc..
            // from this syntax: Linker.NoDefaultLibs somelibrary anotherlibrary            
            NoDefaultLibs = Params->bLinkerNoDefaultLibs ? S("/NODEFAULTLIB ") : String_Null();

            // TODO: what is the nostd flag for msvc?
        }
        else
        {
            NoStd         = Params->bLinkerNoStd ? S("-nostdlib") : String_Null();
            NoDefaultLibs = Params->bLinkerNoDefaultLibs ? S("-nodefaultlibs") : String_Null();
        }

        bool bCustomEntry     = String_IsValid(Params->LinkerEntryPoint);
        bool bCustomSubsystem = String_IsValid(Params->LinkerSubsystem);
        bool bCustomStack     = String_IsValid(Params->LinkerStack);
        bool bAnyValid        = bCustomEntry || bCustomSubsystem || bCustomStack;

        // TODO: linux, macos and bsd
        // --entry=entry
        // -Wl,-stack_size,0x800000
        #if PLATFORM_WINDOWS
        if (bIsMicrosoftLinker)
        {
            if (bAnyValid)
            {
                if (bCustomEntry)
                {
                    String_AppendF(AdditionalFlags, S("/ENTRY:%S "), Params->LinkerEntryPoint);
                }

                if (bCustomSubsystem)
                {
                    String_AppendF(AdditionalFlags, S("/SUBSYSTEM:%S "), Params->LinkerSubsystem);
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

                    String_AppendF(AdditionalFlags, S("/STACK:%S,%S "), Reserve, Commit);
                }
            }
        }
        else
        {
            StringLocal(WlFlags, 256);
            StringLocal(XlinkerFlags, 256);
            if (bAnyValid)
            {
                bool bIsClang = String_IsEqual(Params->CompilerProgram, S("clang"), false) ||
                                String_IsEqual(Params->CompilerProgram, S("clang++"), false);

                bool bIsTCC   = String_IsEqual(Params->CompilerProgram, S("tcc"), false);

                String_Append(&WlFlags, S("-Wl,"));

                if (bCustomEntry)
                {
                    if (bIsClang)
                    {
                        String_AppendF(&WlFlags, S("-entry:%S,"), Params->LinkerEntryPoint);
                    }
                    else if (bIsTCC)
                    {
                        String_AppendF(&WlFlags, S("-entry=%S,"), Params->LinkerEntryPoint);
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
                    else if (bIsTCC)
                    {
                        // need to lower it, cos fuck you i guess
                        // fixes this error -> ld: invalid subsystem type Console
                        StringLocal(Lowered, 32);
                        String_Copy(&Lowered, Params->LinkerSubsystem);
                        String_ToLower(&Lowered);

                        String_AppendF(&WlFlags, S("-subsystem=%S,"), Lowered);
                    }
                    else // GCC
                    {
                        // need to lower it, cos fuck you i guess
                        // fixes this error -> ld: invalid subsystem type Console
                        StringLocal(Lowered, 32);
                        String_Copy(&Lowered, Params->LinkerSubsystem);
                        String_ToLower(&Lowered);

                        String_AppendF(&WlFlags, S("--subsystem,%S,"), Lowered);
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
                    else if (bIsTCC)
                    {
                        String_AppendF(&WlFlags, S("-stack=%S"), Reserve);
                    }
                    else // GCC
                    {
                        // apparently you cant specify a commit here
                        String_AppendF(&WlFlags, S("--stack,%S"), Reserve);
                    }
                }
            }

            xx String_EatCharInlineFromEnd(&WlFlags, ',');

            String_BuildSeparator(AdditionalFlags, ' ', NoStd, NoDefaultLibs, WlFlags, XlinkerFlags);
        }
        #else
        xx bAnyValid;
        String_BuildSeparator(AdditionalFlags, ' ', NoStd, NoDefaultLibs);
        #endif
    }

    xx String_EatSpacesInlineFromEnd(AdditionalFlags);
}

bool C_Link(const BuildParams* Params)
{
    if (NEVER(Params == NULL)) { return false; }

    if (Params->Type == AssemblyType_PCH || Params->Type == AssemblyType_CustomCompilerObject)
    {
        return true;
    }

    StringLocal(SourceDir, MAX_PATH_LENGTH);
    String_BuildPath(&SourceDir, Params->RootDirectory, Params->SourceDirectory);

    StringLocal(BuildPath, MAX_PATH_LENGTH);
    String_BuildPath(&BuildPath, Params->RootDirectory, Params->BuildDirectory);
    String_AppendPathSeparator(&BuildPath);

    bool bIsMicrosoftLinker   = String_EndsWith(Params->LinkerPath, S("link.exe"), false);
    bool bIsMicrosoftArchiver = String_EndsWith(Params->ArchiverPath, S("lib.exe"), false);

    bool bIsExe = Params->Type == AssemblyType_Executable;
    bool bIsDLL = Params->Type == AssemblyType_Library || Params->Type == AssemblyType_DynamicLibrary;
    bool bIsLib = Params->Type == AssemblyType_Library || Params->Type == AssemblyType_StaticLibrary;

    #if PLATFORM_WINDOWS
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
    #else
    String WinSDKLibPaths = String_Null();
    #endif


    String ProgramPath = Params->LinkerPath;
    String OutputFlag = S("-o ");
    String VerboseFlag = S("-v");

    StringLocal(RunPathLinkFlag, MAX_PATH_LENGTH);

    StringLocal(CmdLine, UINT16_MAX);
    String_AppendChar(&CmdLine, '"');
    String_Append    (&CmdLine, ProgramPath);
    String_AppendChar(&CmdLine, '"');
    String_AppendSpace(&CmdLine);

    if (bIsMicrosoftLinker || bIsMicrosoftArchiver)
    {
        String_Append(&CmdLine, S("/nologo "));

        OutputFlag = S("/OUT:");
    }

    if (Params->bVerbose)
    {
        if (bIsMicrosoftLinker || bIsMicrosoftArchiver)
        {
            VerboseFlag = String_Null();
        }
    }
    else
    {
        VerboseFlag = String_Null();
    }

    if (bIsExe || bIsDLL)
    {
        String DefaultObjExtension = bIsMicrosoftLinker ? S(".obj") : S(".o");
        
        String SharedFlag = String_Null();

        if (bIsDLL)
        {
            if (bIsMicrosoftLinker)
            {
                SharedFlag = S("/DLL");
            }
            else
            {
                SharedFlag = S("-shared");
            }
        }

        #if !PLATFORM_WINDOWS
        if (bIsExe)
        {
            String ChosenRPath = S("$ORIGIN");
            if (String_IsValid(Params->RPath))
            {
                ChosenRPath = Params->RPath;
            }
            String_AppendF(&RunPathLinkFlag, S("-Wl,-rpath,'%S'"), ChosenRPath);
        }
        #endif

        StringLocal(AdditionalFlags, 512);
        GetAdditionalLinkerFlags(Params, &AdditionalFlags);

        String_BuildSeparator(&CmdLine, ' ', VerboseFlag,
                                             SharedFlag,
                                             bIsMicrosoftLinker ? WinSDKLibPaths : String_Null());

        // TCC doesn't recognize .res files as of v0.9.28
        if (!String_IsEqual(Params->CompilerProgram, S("tcc"), false))
        {
            String_BuildSeparator(&CmdLine, ' ', Params->IconResFilePath,
                                                 Params->VersionResFilePath);
        }

        xx String_EatSpacesInlineFromEnd(&CmdLine);
        String_AppendSpace(&CmdLine);

        if (Params->PCHPath.Length > 0 || Params->PCHHeaderPath.Length > 0)
        {
            String_Append(&CmdLine, S("\""));

            if (Params->PCHHeaderPath.Length > 0)
            {
                const String Trimmed = Filesystem_StripFileExtension(Params->PCHHeaderPath);
                String_Append(&CmdLine, Trimmed);
            }
            else
            {
                const String Trimmed = Filesystem_StripFileExtension(Params->PCHPath);
                String_Append(&CmdLine, Trimmed);
            }

            String_Append(&CmdLine, DefaultObjExtension);
            String_Append(&CmdLine, S("\" "));
        }

        Internal_AppendObjSourceFiles(Params, &CmdLine, DefaultObjExtension);

        // These must come after obj files because on some operating systems
        // the linker is sensitive to the order of how the flags are positioned
        // 
        String_BuildSeparator(&CmdLine, ' ', AdditionalFlags,
                                             RunPathLinkFlag,
                                             Params->LinkerFlags,
                                             Params->LinkerDefineFlags,
                                             Params->Libraries,
                                             Params->LibraryDirectories);

        xx String_EatSpacesInlineFromEnd(&CmdLine);
        String_AppendSpace(&CmdLine);

        String_Concat(&CmdLine, OutputFlag, S("\""), BuildPath, Params->AssemblyWithExt, S("\""));


        if (bQuietBuild) { Logging_Enable(); }

        #ifndef HOOD
        LOG("Linking %S", Params->AssemblyWithExt);
        #else
        LOG("linkn' shit up %S", Params->AssemblyWithExt);
        #endif

        if (bQuietBuild) { Logging_Disable(); }

        if (Params->bVerbose)
        {
            if (bNoWordWrapLogging)
            {
                LOG("\n    %S\n", CmdLine);
            }
            else
            {
                LogString_WordWrapped(*Params->Arena, S("    "), CmdLine, false);
            }
        }


        PlatformHandle Handle = {0};
        #if PLATFORM_WINDOWS
        PlatformPipe StdOutHandle = {0};
        if (bIsMicrosoftLinker)
        {
            Handle = Platform_RunProcess_Ex(ProgramPath, CmdLine, Params->RootDirectory, &StdOutHandle);
        }
        else
        #endif
        {
            Handle = Platform_RunProcess(ProgramPath, CmdLine, Params->RootDirectory, String_Null());
        }

        if (Platform_IsValidHandle(Handle))
        {
            // TODO: switch between fancy and non fancy logging
            #if PLATFORM_WINDOWS
            if (bIsMicrosoftLinker)
            {
                Internal_ProcessLinkerOutput_MSVC(StdOutHandle);
            }
            #endif

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
        else
        {
            return false;
        }
    }


    if (bIsLib)
    {
        String DefaultObjExtension = bIsMicrosoftArchiver ? S(".obj") : S(".o");

        ProgramPath = Params->ArchiverPath;

        String_Empty(&CmdLine);
        String_AppendChar(&CmdLine, '"');
        String_Append    (&CmdLine, ProgramPath);
        String_AppendChar(&CmdLine, '"');
        String_AppendSpace(&CmdLine);

        if (bIsMicrosoftArchiver)
        {
            String_Append(&CmdLine, S("/nologo "));

            // TODO: archiver.flags /machine:x64

            OutputFlag = S("/OUT:");
        }
        else
        {
            #if PLATFORM_WINDOWS
            OutputFlag = S("r ");
            #else
            OutputFlag = S("rcs ");
            #endif
        }

        StringLocal(LibFile, MAX_PATH_LENGTH);
        {
            String_Append(&LibFile, Params->Assembly);

            if (Params->Type == AssemblyType_Library)
            {
                String_AppendChar(&LibFile, 'S'); // todo: rename to _static ?
            }

            #if PLATFORM_WINDOWS
            String_Append(&LibFile, S(".lib"));
            #else
            String_Append(&LibFile, S(".a"));
            #endif
        }

        String_Concat(&CmdLine, OutputFlag, S("\""), BuildPath, LibFile, S("\" "));


        /*
        if (bIsMicrosoftArchiver)
        {
            String_BuildSeparator(&CmdLine, ' ', VerboseFlag, Params->LinkerFlags, Params->Libraries, Params->LibraryDirectories, Params->VersionResFilePath, WinSDKLibPaths);
        }
        else
        */
        {
            String_BuildSeparator(&CmdLine, ' ', VerboseFlag, Params->ArchiverFlags, Params->VersionResFilePath);
        }

        xx String_EatSpacesInlineFromEnd(&CmdLine);
        String_AppendSpace(&CmdLine);

        Internal_AppendObjSourceFiles(Params, &CmdLine, DefaultObjExtension);


        if (bQuietBuild) { Logging_Enable(); }

        #ifndef HOOD
        LOG("Linking %S [static]", LibFile);
        #else
        LOG("linkn' shit up %S [static]", LibFile);
        #endif

        if (bQuietBuild) { Logging_Disable(); }

        if (Params->bVerbose)
        {
            if (bNoWordWrapLogging)
            {
                LOG("\n    %S\n", CmdLine);
            }
            else
            {
                LogString_WordWrapped(*Params->Arena, S("    "), CmdLine, false);
            }
        }

        PlatformHandle Handle = Platform_RunProcess(ProgramPath, CmdLine, Params->RootDirectory, String_Null());
        if (Platform_IsValidHandle(Handle))
        {
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
        else
        {
            return false;
        }
    }


    // =========================================================================================


    // generate a .def file if we are building a dll file (windows only)
    #if PLATFORM_WINDOWS
    if (bIsDLL && bIsMicrosoftLinker) // todo: for clang and gcc as well?
    {
        String_Empty(&CmdLine);
        String_AppendChar(&CmdLine, '"');
        String_Append(&CmdLine, Params->DumpBinPath);
        String_AppendChar(&CmdLine, '"');
        String_Append(&CmdLine, S(" /EXPORTS /NOLOGO /OUT:\""));

        String_Append(&CmdLine, BuildPath);
        String_Append(&CmdLine, Params->Assembly);
        String_Append(&CmdLine, S(".def\" "));

        String_Append(&CmdLine, S("\""));
        String_Append(&CmdLine, BuildPath);
        String_Append(&CmdLine, Params->Assembly);
        String_Append(&CmdLine, S(".dll\""));

        PlatformHandle H = Platform_RunProcess(Params->DumpBinPath, CmdLine, Params->RootDirectory, String_Null());
        if (Platform_IsValidHandle(H))
        {
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
        else
        {
            return false;
        }
    }
    #endif // PLATFORM_WINDOWS

    return true;
}
