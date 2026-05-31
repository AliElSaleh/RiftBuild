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
    TArray(String)* PendingObjectRenames;
    u32 Index;
    u32 Padding;
};

// Record a single produced path into an artifact list, normalized to absolute (relative paths are
// anchored to RootDirectory, then canonicalized). Shared by the backend's per-output recording and the
// frontend's generated-helper-file recording so both build the manifest identically. A NULL list or an
// empty path is a no-op.
void RecordArtifactPath(TArray(String)* Artifacts, LinearAllocator* Arena, const String RootDirectory, const String Path)
{
    if (Artifacts && *Artifacts && Path.Length > 0)
    {
        StringLocal(AbsolutePath, MAX_PATH_LENGTH);
        if (Filesystem_IsPathRelative(Path))
        {
            String_BuildPath(&AbsolutePath, RootDirectory, Path);
        }
        else
        {
            String_Copy(&AbsolutePath, Path);
        }

        xx Filesystem_ConvertRelativeToAbsolutePath(&AbsolutePath);

        String Dup = String_Duplicate(Arena, AbsolutePath);
        Array_Add(*Artifacts, Dup);
    }
}

static void Internal_RecordLinkArtifacts(const BuildParams* Params, const String BaseDir, const String OutputName)
{
    StringLocal(OutputPath, MAX_PATH_LENGTH);
    String_Append(&OutputPath, BaseDir);
    String_Append(&OutputPath, OutputName);
    RecordArtifactPath(Params->GeneratedArtifacts, Params->Arena, Params->RootDirectory,OutputPath);

    #if PLATFORM_WINDOWS
    {
        const String Exts[5] = { S(".pdb"), S(".ilk"), S(".exp"), S(".lib"), S(".def") };
        const u32 NumExts = SArray_Capacity(Exts);

        for (u32 i = 0; i < NumExts; i++)
        {
            StringLocal(Path, MAX_PATH_LENGTH);
            String_Append(&Path, BaseDir);
            String_Append(&Path, Filesystem_StripFileExtension(OutputName));
            String_Append(&Path, Exts[i]);
            RecordArtifactPath(Params->GeneratedArtifacts, Params->Arena, Params->RootDirectory,Path);
        }
    }
    #endif
}

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

static void RC_Compile(const BuildParams* Params, const String FullRCPath, String* OutResPath, String* OutCmdLine)
{
    #if PLATFORM_WINDOWS
    StringLocal(CmdLine, 1024);

    const bool bWindres = String_IsEqual(Params->RCProgram, S("windres"), false);

    // we are intentionally adding a backslash here for the windres compiler specifically, because
    // the developers behind this are so incompetent that they don't know how to properly handle
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

        // TODO: use enum instead
        if (String_EndsWith(Params->RCProgramPath, S("rc.exe"), false))
        {
            // TODO: this is duplicated code, collapse this in one place
            StringLocal(WinSDKInclude, MAX_PATH_LENGTH*7); // 7 paths
            if (!bWasVCVarsBatchExecuted)
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
    String_Append(&ResPath, bHasDot ? StrSlice(FullRCPath.Data, LastDot) : FullRCPath);
    String_Append(&ResPath, S(".res"));

    if (OutResPath)
    {
        String_Copy(OutResPath, ResPath);
    }

    // emit to a throwaway ".tmp" path so an interrupted/killed resource compile can't leave a corrupt
    // .res under the real name; Internal_DoCompile renames it onto ResPath once the compile succeeds
    StringLocal(ResTempPath, MAX_PATH_LENGTH);
    String_Copy(&ResTempPath, ResPath);
    String_Append(&ResTempPath, S(".tmp"));

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

        String_Append(&CmdLine, S(" -o \""));
        String_Append(&CmdLine, ResTempPath);
        String_Append(&CmdLine, S("\""));
    }
    else
    {
        String_Append(&CmdLine, S(" /fo \""));
        String_Append(&CmdLine, ResTempPath);
        String_Append(&CmdLine, S("\""));

        String_Append(&CmdLine, S(" \""));
        String_Append(&CmdLine, FullRCPath);
        String_AppendChar(&CmdLine, '"');
    }

    if (OutCmdLine)
    {
        String_Copy(OutCmdLine, CmdLine);
    }
    #endif
}

static bool Internal_DoCompile(CompileData* Data, const String RelativePath)
{
    if (NEVER(Data == NULL))         { return false; }
    if (NEVER(Data->Params == NULL)) { return false; }

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
    bool bInsideIntermediatePath = String_StartsWith(RelativePathCopy, Params->IntermediateDirectory, false);
    String_BuildPath(&FullPath, Params->RootDirectory, bInsideIntermediatePath ? String_Null() : Params->SourceDirectory, RelativePathCopy);

    StringLocal(FullSourcePath, MAX_PATH_LENGTH+2);
    String_WrapPath(&FullSourcePath, FullPath);

    String DefaultObjExtension = S(".o");

    bool bIsMicrosoftCompiler  = Params->CompilerVendor == Compiler_MSVC;
    bool bIsMicrosoftAssembler = Params->AssemblerVendor == Assembler_Masm;

    if (bIsMicrosoftCompiler || bIsMicrosoftAssembler)
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
            String Name    = Filesystem_ExtractFileName(RelativePath, true);
            String Prefix  = String_Null();
            String Postfix = String_Null();
            String ObjExt  = String_IsValid(Params->CompilerObjectExt) ? Params->CompilerObjectExt : DefaultObjExtension;

            if (Params->Type == AssemblyType_CustomCompilerObject)
            {
                Prefix  = Params->AssemblyPrefix;
                Postfix = Params->AssemblyPostfix;
                //ObjExt  = Params->Extension;
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

        String ObjDestinationDirectory = Params->IntermediateDirectory;
        if (String_IsValid(Params->CompilerObjectDirectory))
        {
            ObjDestinationDirectory = Params->CompilerObjectDirectory;
        }

        // handle a special case where we are compiling something in the intermediate directory
        if (String_StartsWith(ObjDestinationDirectory, PathOfObj, false))
        {
            PathOfObj = String_Null();
        }

        String_BuildPath(&ObjectPath, Params->RootDirectory, ObjDestinationDirectory, PathOfObj, ObjFile);
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

    // The compiler writes to a throwaway ".tmp" path; on success we atomically rename it onto the real
    // object path. That way an interrupted/killed/crashed compile can never leave a partial object under
    // the real name (which the timestamp check would then wrongly treat as up-to-date and skip).
    StringLocal(ObjectTempPath, MAX_PATH_LENGTH + 8);
    String_Copy(&ObjectTempPath, ObjectPath);
    String_Append(&ObjectTempPath, S(".tmp"));

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
            String_Append(&CmdLine, ObjectTempPath);
            String_Append(&CmdLine, S("\" "));
            String_BuildSeparator(&CmdLine, ' ', FullSourcePath, Params->AssemblerFlags, Params->AssemblerDefines, Params->AssemblerIncludes);
            xx String_EatSpacesInlineFromEnd(&CmdLine);
        }
        else
        {
            String_BuildSeparator(&CmdLine, ' ', FullSourcePath, Params->AssemblerFlags, Params->AssemblerDefines, Params->AssemblerIncludes);
            xx String_EatSpacesInlineFromEnd(&CmdLine);
            String_Append(&CmdLine, S(" -o \""));
            String_Append(&CmdLine, ObjectTempPath);
            String_Append(&CmdLine, S("\""));
        }
    }
    else if (String_EndsWith(RelativePath, S(".manifest"), false))
    {
        return true;
    }
    else if (String_EndsWith(RelativePath, S(".rc"), false))
    {
        ProgramPath = Params->RCProgramPath;

        String_Empty(&ObjectPath);

        RC_Compile(Params, RelativePath, &ObjectPath, &CmdLine);

        String_Copy(&ObjectTempPath, ObjectPath);
        String_Append(&ObjectTempPath, S(".tmp"));
    }
    else
    {
        String OutputFlag  = S("-o");
        String CompileFlag = S("-c");

        String AdditionalFlags = String_Null();

        StringLocal(PCHFlags, MAX_PATH_LENGTH*2);

        String_AppendChar(&CmdLine, '"');
        String_Append    (&CmdLine, Params->CompilerPath);
        String_AppendChar(&CmdLine, '"');

        if (Params->Type == AssemblyType_CustomCompilerObject)
        {
            if (String_IsValid(Params->CompilerOutputFlag))
            {
                OutputFlag = Params->CompilerOutputFlag;
            }

            if (String_IsValid(Params->CompilerCompileFlag))
            {
                CompileFlag = Params->CompilerCompileFlag;
            }
            else
            {
                CompileFlag = String_Null();
            }
        }
        else
        {
            #if PLATFORM_WINDOWS
            if (bIsMicrosoftCompiler)
            {
                OutputFlag  = S("/Fo:");
                CompileFlag = S(" /nologo /c ");

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
            #endif

            // using a pch
            if (Params->PCHPath.Length)
            {
                if (Params->CompilerVendor == Compiler_Clang)
                {
                    const String Trimmed = Filesystem_StripFileExtension(Params->PCHPath);

                    String_Concat(&PCHFlags, S("-include-pch \""), Trimmed, S(".h.gch\""));
                }
            }
        }

        String CompilerFlagsLeft  = Params->bCompilerFlagsFirst ? Params->CompilerFlags : String_Null();
        String CompilerFlagsRight = Params->bCompilerFlagsFirst ? String_Null() : Params->CompilerFlags;

        String_BuildSeparator(&CmdLine, ' ', CompilerFlagsLeft, CompileFlag, FullSourcePath, CompilerFlagsRight, Params->IncludeFlags, Params->DefineFlags, AdditionalFlags, PCHFlags, OutputFlag);
        xx String_EatSpacesInlineFromEnd(&CmdLine);

        String_Append(&CmdLine, S(" \""));
        String_Append(&CmdLine, ObjectTempPath);
        String_Append(&CmdLine, S("\""));
    }

    // ===============================================================================================

    // Record the object this source maps to, its in-flight ".tmp", and the compiler's sibling ".d"
    // dependency file (e.g. Foo.c.o.d) that lands next to it.
    RecordArtifactPath(Params->GeneratedArtifacts, Params->Arena, Params->RootDirectory, ObjectPath);
    RecordArtifactPath(Params->GeneratedArtifacts, Params->Arena, Params->RootDirectory, ObjectTempPath);
    {
        StringLocal(DepPath, MAX_PATH_LENGTH + 4);
        String_Copy(&DepPath, ObjectPath);
        String_Append(&DepPath, S(".d"));
        RecordArtifactPath(Params->GeneratedArtifacts, Params->Arena, Params->RootDirectory, DepPath);
    }
    if (Params->Type == AssemblyType_PCH)
    {
        RecordArtifactPath(Params->GeneratedArtifacts, Params->Arena, Params->RootDirectory, PCHObjectPath);
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
    }
    else
    {
        xx Filesystem_NewFile(ObjectTempPath);

        if (bQuietBuild) { Logging_Enable(); }

        bool bHideLog = (bIsMicrosoftCompiler && !IsAsmSource(Ext)) ||
                        (bIsMicrosoftAssembler && IsAsmSource(Ext));

        if (!bHideLog || Params->bShouldWaitPerCompileProcess)
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
                    xx Filesystem_DeleteFile(ObjectTempPath);
                    LOG_ERROR("Compiler errors detected. See above errors to fix. Exit code for process: %u. Aborting build...", ExitCode);
                    return false;
                }

                // compile succeeded, so atomically publish the finished object onto its real path
                if (!Filesystem_Move(ObjectTempPath, ObjectPath, true))
                {
                    LOG_ERROR("Failed to finalize object file \"%S\". Aborting build...", ObjectPath);
                    return false;
                }
            }
            else
            {
                // parallel build: defer publishing until the whole batch has compiled (see C_Compile)
                String Pending = String_Duplicate(Params->Arena, ObjectPath);
                Array_Add(*Data->PendingObjectRenames, Pending);
            }
        }
        else
        {
            xx Filesystem_DeleteFile(ObjectTempPath);
            LOG_ERROR("Failed to spawn compiler process: \"%S\"", ProgramPath);
            return false;
        }
    }

    return true;
}

bool C_Compile(const BuildParams* Params, u32* OutNumCompiled)
{
    if (NEVER(Params == NULL))         { return false; }
    if (NEVER(OutNumCompiled == NULL)) { return false; }

    bool bSuccess = true;

    ArrayLocal_Arena(String, PendingObjectRenames, Params->NumSources, Params->Arena);

    // compile all source files
    {
        CompileData UserData = { 0 };
        UserData.Params = Params;
        UserData.NumCompiled = OutNumCompiled;
        UserData.PendingObjectRenames = &PendingObjectRenames;
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
            if (!bQuietBuild)
            {
                //TODO: say how long ago the last build was like -> (5.3 secs ago)
                #ifndef HOOD
                LOG("\nNothing to compile - source files unchanged since last build");
                #else
                LOG("\nno work to do homie");
                #endif
            }
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

    // every compile in the batch finished cleanly: atomically publish each deferred object by renaming
    // its ".tmp" onto the real path. If anything failed above we skip this, leaving the real objects
    // untouched (and the stray ".tmp" files inert) so the next build recompiles them.
    if (bSuccess)
    {
        Array_For(PendingObjectRenames)
        {
            const String FinalPath = PendingObjectRenames[i];

            StringLocal(TempPath, MAX_PATH_LENGTH + 8);
            String_Copy(&TempPath, FinalPath);
            String_Append(&TempPath, S(".tmp"));

            if (!Filesystem_Move(TempPath, FinalPath, true))
            {
                LOG_ERROR("Failed to finalize object file \"%S\". Aborting build...", FinalPath);
                bSuccess = false;
                break;
            }
        }
    }

    Array_Destroy(PendingObjectRenames);

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

        #if PLATFORM_WINDOWS
        if (String_EndsWith(RelativePath, S(".rc"), false))
        {
            // TCC doesn't recognize .res files as of 0.9.28
            if (Params->CompilerVendor == Compiler_TCC)
            {
                continue;
            }

            bool bInsideIntermediatePath = String_StartsWith(RelativePath, Params->IntermediateDirectory, false);

            u32 LastDot = 0;
            bool bHasDot = String_IndexOfLastChar(RelativePath, '.', &LastDot);

            StringLocal(ResPath, MAX_PATH_LENGTH);
            String_Append(&ResPath, bHasDot ? StrSlice(RelativePath.Data, LastDot) : RelativePath);
            String_Append(&ResPath, S(".res"));

            String_BuildPath(&ObjectPath, bInsideIntermediatePath ? String_Null() : Params->SourceDirectory, ResPath);
        }
        else
        #endif
        {
            // make the object file string
            // example: some_file.c now becomes some_file.o
            StringLocal(ObjFile, MAX_PATH_LENGTH);
            {
                // String_Append(&ObjFile, bHasDot ? StrSlice(RelativePath.Data, LastDot) : RelativePath);
                String_Append(&ObjFile, RelativePath);

                const String Ext = String_IsValid(Params->CompilerObjectExt) ? Params->CompilerObjectExt : DefaultObjExt;
                if (!String_IsFirst(Ext, '.'))
                {
                    String_AppendChar(&ObjFile, '.');
                }

                String_Append(&ObjFile, Ext);
            }

            String ObjDestinationDirectory = Params->IntermediateDirectory;
            if (String_IsValid(Params->CompilerObjectDirectory))
            {
                ObjDestinationDirectory = Params->CompilerObjectDirectory;
            }

            String_BuildPath(&ObjectPath, ObjDestinationDirectory, ObjFile);
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

bool AssemblyTypeStringIsExecutable(String Type)
{
    return (String_IsEqual(Type, S("app"), false) ||
            String_IsEqual(Type, S("application"), false) ||
            String_IsEqual(Type, S("exe"), false) ||
            String_IsEqual(Type, S("executable"), false) ||
            String_IsEqual(Type, S("bin"), false) ||
            String_IsEqual(Type, S("binary"), false));
}

String AssemblyTypeStringToExtension(String Type)
{
    String Extension = String_Null();
    
    if (String_IsEqual(Type, S("lib"), false) ||
        String_IsEqual(Type, S("library"), false))
    {
        #if PLATFORM_WINDOWS
            Extension = S(".dll .lib");
        #elif PLATFORM_APPLE
            Extension = S(".dylib .a");
        #else
            Extension = S(".so .a");
        #endif
    }
    else if (String_IsEqual(Type, S("static"), false) ||
             String_IsEqual(Type, S("static_lib"), false) ||
             String_IsEqual(Type, S("static_library"), false))
    {
        #if PLATFORM_WINDOWS
            Extension = S(".lib");
        #elif PLATFORM_APPLE
            Extension = S(".a");
        #else
            Extension = S(".a");
        #endif
    }
    else if (String_IsEqual(Type, S("shared"), false) ||
             String_IsEqual(Type, S("shared_lib"), false) ||
             String_IsEqual(Type, S("shared_library"), false) ||
             String_IsEqual(Type, S("dynamic"), false) ||
             String_IsEqual(Type, S("dynamic_lib"), false) ||
             String_IsEqual(Type, S("dynamic_library"), false))
    {
        #if PLATFORM_WINDOWS
            Extension = S(".dll");
        #elif PLATFORM_APPLE
            Extension = S(".dylib");
        #else
            Extension = S(".so");
        #endif
    }
    else if (AssemblyTypeStringIsExecutable(Type))
    {
        #if PLATFORM_WINDOWS
            Extension = S(".exe");
        #elif PLATFORM_APPLE
            Extension = String_Null();
        #else
            Extension = String_Null();
        #endif
    }
    else if (String_IsEqual(Type, S("gch"), false))
    {
        Extension = S(".gch");
    }
    else if (String_IsEqual(Type, S("pch"), false) ||
             String_IsEqual(Type, S("pre_compiled_header"), false))
    {
        Extension = S(".pch");
    }
    else
    {
        // no action required
    }

    return Extension;
}

EAssemblyType StringToAssemblyTypeEnum(String Type)
{
    EAssemblyType AssemblyType = AssemblyType_None;

    if (String_IsEqual(Type, S("lib"), false) ||
        String_IsEqual(Type, S("library"), false))
    {
        AssemblyType = AssemblyType_Library;
    }
    else if (String_IsEqual(Type, S("static"), false) || 
             String_IsEqual(Type, S("static_lib"), false) || 
             String_IsEqual(Type, S("static_library"), false))
    {
        AssemblyType = AssemblyType_StaticLibrary;
    }
    else if (String_IsEqual(Type, S("dynamic"), false) || 
             String_IsEqual(Type, S("dynamic_lib"), false) || 
             String_IsEqual(Type, S("dynamic_library"), false) ||
             String_IsEqual(Type, S("shared"), false) || 
             String_IsEqual(Type, S("shared_lib"), false) || 
             String_IsEqual(Type, S("shared_library"), false))
    {
        AssemblyType = AssemblyType_DynamicLibrary;
    }
    else if (AssemblyTypeStringIsExecutable(Type))
    {
        AssemblyType = AssemblyType_Executable;
    }
    else if (String_IsEqual(Type, S("pch"), false) || 
             String_IsEqual(Type, S("gch"), false) || 
             String_IsEqual(Type, S("pre_compiled_header"), false))
    {
        AssemblyType = AssemblyType_PCH;
    }
    else if (String_IsEqual(Type, S("object"), false) ||
             String_IsEqual(Type, S("compiler_object"), false))
    {
        AssemblyType = AssemblyType_CustomCompilerObject;
    }
    else if (String_IsEqual(Type, S("null"), false) ||
             String_IsEqual(Type, S("none"), false) ||
             String_IsEqual(Type, S("phony"), false))
    {
        AssemblyType = AssemblyType_Null;
    }
    else
    {
        // no action required
    }

    return AssemblyType;
}

bool ExtensionStringIsExecutable(String Ext)
{
    const String Trimmed = String_EatChar(Ext, '.');

    return Trimmed.Length == 0 || 
           String_IsEqual(Trimmed, S("elf"), false) ||
           String_IsEqual(Trimmed, S("out"), false) ||
           String_IsEqual(Trimmed, S("exe"), false) ||
           String_IsEqual(Trimmed, S("com"), false);
}

bool ExtensionStringIsSharedLibrary(String Ext)
{
    const String Trimmed = String_EatChar(Ext, '.');

    return String_IsEqual(Trimmed, S("dll"), false) ||
           String_IsEqual(Trimmed, S("so"), false) ||
           String_IsEqual(Trimmed, S("dylib"), false);
}

bool ExtensionStringIsStaticLibrary(String Ext)
{
    const String Trimmed = String_EatChar(Ext, '.');

    return String_IsEqual(Trimmed, S("lib"), false) ||
           String_IsEqual(Trimmed, S("a"), false);
}

bool ExtensionStringIsPCH(String Ext)
{
    const String Trimmed = String_EatChar(Ext, '.');

    return String_IsEqual(Trimmed, S("pch"), false) ||
           String_IsEqual(Trimmed, S("gch"), false);
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
                                        // LOG("                          %S", SecondPart);
                                        LOG_MUTE("                          %S", SecondPart);
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


bool C_Link(const BuildParams* Params)
{
    if (NEVER(Params == NULL)) { return false; }

    if (Params->Type == AssemblyType_PCH ||
        Params->Type == AssemblyType_Null)
    {
        return true;
    }

    if (Params->Type == AssemblyType_CustomCompilerObject)
    {
        String ProgramPath = Params->LinkerPath;
        String OutputFlag = Params->LinkerOutputFlag;

        StringLocal(BuildPath, MAX_PATH_LENGTH);
        String_BuildPath(&BuildPath, Params->RootDirectory, Params->BuildDirectory);
        String_AppendPathSeparator(&BuildPath);

        StringLocal(CmdLine, UINT16_MAX);
        String_AppendChar(&CmdLine, '"');
        String_Append    (&CmdLine, ProgramPath);
        String_AppendChar(&CmdLine, '"');
        String_AppendSpace(&CmdLine);

        String LinkerFlagsLeft  = Params->bLinkerFlagsFirst ? Params->LinkerFlags : String_Null();
        String LinkerFlagsRight = Params->bLinkerFlagsFirst ? String_Null() : Params->LinkerFlags;

        String_Append(&CmdLine, LinkerFlagsLeft);
        xx String_EatSpacesInlineFromEnd(&CmdLine);
        String_AppendSpace(&CmdLine);

        String ObjExt  = String_IsValid(Params->CompilerObjectExt) ? Params->CompilerObjectExt : S(".o");
        Internal_AppendObjSourceFiles(Params, &CmdLine, ObjExt);

        // These must come after obj files because on some operating systems
        // the linker is sensitive to the order of how the flags are positioned
        // 
        String_BuildSeparator(&CmdLine, ' ', LinkerFlagsRight,
                                             Params->LinkerDefineFlags,
                                             Params->Libraries,
                                             Params->LibraryDirectories);

        xx String_EatSpacesInlineFromEnd(&CmdLine);
        String_AppendSpace(&CmdLine);

        String_Concat(&CmdLine, OutputFlag, S(" \""), BuildPath, Params->AssemblyWithExt, S("\""));

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

        Internal_RecordLinkArtifacts(Params, BuildPath, Params->AssemblyWithExt);

        return true;
    }

    StringLocal(SourceDir, MAX_PATH_LENGTH);
    String_BuildPath(&SourceDir, Params->RootDirectory, Params->SourceDirectory);

    StringLocal(BuildPath, MAX_PATH_LENGTH);
    String_BuildPath(&BuildPath, Params->RootDirectory, Params->BuildDirectory);
    String_AppendPathSeparator(&BuildPath);

    bool bIsMicrosoftLinker   = String_EndsWith(Params->LinkerPath, S("link.exe"), false);
    bool bIsMicrosoftArchiver = String_EndsWith(Params->ArchiverPath, S("lib.exe"), false);
    bool bIsUnixArchiver      = String_EndsWith(Params->ArchiverPath, S("ar"), false);

    bool bIsExe = Params->Type == AssemblyType_Executable;
    bool bIsDLL = Params->Type == AssemblyType_Library || Params->Type == AssemblyType_DynamicLibrary;
    bool bIsLib = Params->Type == AssemblyType_Library || Params->Type == AssemblyType_StaticLibrary;

    String ProgramPath = Params->LinkerPath;
    String VerboseFlag = S("-v");

    StringLocal(CmdLine, UINT16_MAX);
    String_AppendChar(&CmdLine, '"');
    String_Append    (&CmdLine, ProgramPath);
    String_AppendChar(&CmdLine, '"');
    String_AppendSpace(&CmdLine);

    if (bIsMicrosoftLinker || bIsMicrosoftArchiver)
    {
        String_Append(&CmdLine, S("/nologo "));
    }

    if (Params->bVerbose)
    {
        if (bIsMicrosoftLinker || bIsMicrosoftArchiver || bIsUnixArchiver)
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
        const String OutputFlag = Params->LinkerOutputFlag;
        String DefaultObjExtension = bIsMicrosoftLinker ? S(".obj") : S(".o");
        
        String_BuildSeparator(&CmdLine, ' ', VerboseFlag);

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

        String LinkerFlagsLeft  = Params->bLinkerFlagsFirst ? Params->LinkerFlags : String_Null();
        String LinkerFlagsRight = Params->bLinkerFlagsFirst ? String_Null() : Params->LinkerFlags;

        String_Append(&CmdLine, LinkerFlagsLeft);
        xx String_EatSpacesInlineFromEnd(&CmdLine);
        String_AppendSpace(&CmdLine);

        Internal_AppendObjSourceFiles(Params, &CmdLine, DefaultObjExtension);

        // These must come after obj files because on some operating systems
        // the linker is sensitive to the order of how the flags are positioned
        // 
        String_BuildSeparator(&CmdLine, ' ', //AdditionalFlags,
                                             //RunPathLinkFlag,
                                             LinkerFlagsRight,
                                             Params->LinkerDefineFlags,
                                             Params->Libraries,
                                             Params->LibraryDirectories);

        xx String_EatSpacesInlineFromEnd(&CmdLine);
        String_AppendSpace(&CmdLine);

        String SpaceAfterOut = S(" ");
        if (bIsMicrosoftLinker)
        {
            SpaceAfterOut = String_Null();
        }
        
        String_Concat(&CmdLine, OutputFlag, SpaceAfterOut, S("\""), BuildPath, Params->AssemblyWithExt, S("\""));


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
        Handle = Platform_RunProcess_Ex(ProgramPath, CmdLine, Params->RootDirectory, &StdOutHandle);
        #else
        Handle = Platform_RunProcess(ProgramPath, CmdLine, Params->RootDirectory, String_Null());
        #endif
        /*
        if (bIsMicrosoftLinker)
        {
            Handle = Platform_RunProcess_Ex(ProgramPath, CmdLine, Params->RootDirectory, &StdOutHandle);
        }
        */
        /*
        else
        #endif
        {
            Handle = Platform_RunProcess(ProgramPath, CmdLine, Params->RootDirectory, String_Null());
        }
        */

        if (Platform_IsValidHandle(Handle))
        {
            // TODO: switch between fancy and non fancy logging
            #if PLATFORM_WINDOWS
            // if (bIsMicrosoftLinker)
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

        Internal_RecordLinkArtifacts(Params, BuildPath, Params->AssemblyWithExt);
    }


    if (bIsLib)
    {
        const String OutputFlag = Params->ArchiverOutputFlag;

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

        String_BuildSeparator(&CmdLine, ' ', VerboseFlag, Params->ArchiverFlags);

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

        StringLocal(LibOutputPath, MAX_PATH_LENGTH);
        String_Append(&LibOutputPath, BuildPath);
        String_Append(&LibOutputPath, LibFile);
        RecordArtifactPath(Params->GeneratedArtifacts, Params->Arena, Params->RootDirectory, LibOutputPath);
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
