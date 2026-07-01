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

void RecordArtifactPath(const FileHandle ManifestHandle, const String RootDirectory, const String Path)
{
    if (IsValidFileHandle(ManifestHandle) && Path.Length > 0)
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

        StringLocal(Line, MAX_PATH_LENGTH + 2);
        String_Copy(&Line, AbsolutePath);
        String_AppendChar(&Line, '\n');
        xx Filesystem_WriteLine(ManifestHandle, Line, NULL);
    }
}

static void Internal_RecordLinkArtifacts(const BuildParams* Params, const String BaseDir, const String OutputName)
{
    StringLocal(OutputPath, MAX_PATH_LENGTH);
    String_Append(&OutputPath, BaseDir);
    String_Append(&OutputPath, OutputName);
    RecordArtifactPath(Params->ArtifactManifestHandle, Params->RootDirectory, OutputPath);

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
            RecordArtifactPath(Params->ArtifactManifestHandle, Params->RootDirectory,Path);
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

    StringLocal(Spaces, 16);
    Spaces.Length = Min(Diff, 16);
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
        String_Append(&CmdLine, ResPath);
        String_Append(&CmdLine, S("\""));
    }
    else
    {
        String_Append(&CmdLine, S(" /fo \""));
        String_Append(&CmdLine, ResPath);
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

// True if this source file's per-file override changed since the last build (its bare name is in the diff's
// dirty set), meaning it must recompile even though its source timestamp is unchanged.
static bool Internal_IsForcedRecompile(const BuildParams* Params, const String RelativePath)
{
    if (!Params->ForceRecompileFiles || Array_Num(Params->ForceRecompileFiles) == 0)
    {
        return false;
    }

    const String FileName = Filesystem_ExtractFileName(RelativePath, true);

    for each (String, DirtyFile, Params->ForceRecompileFiles)
    {
        if (String_IsEqual(FileName, DirtyFile, false))
        {
            return true;
        }
    }

    return false;
}

// Append the extra compiler flags, include flags, defines and undefines from every per-file override that
// names this source file. Matching is by bare filename (case-insensitive, extension included) - no paths,
// no globs - so two files with the same name in different directories share an override by design.
static void Internal_AppendFileOverrideFlags(const BuildParams* Params, const String RelativePath, String* Dest)
{
    if (Params->NumFileOverrides == 0)
    {
        return;
    }

    const String FileName = Filesystem_ExtractFileName(RelativePath, true);

    for (u32 i = 0; i < Params->NumFileOverrides; i++)
    {
        const FileOverride* Override = &Params->FileOverrides[i];

        if (String_IsEqual(FileName, Override->FileName, false))
        {
            String_BuildSeparator(Dest, ' ', Override->CompilerFlags, Override->IncludeFlags, Override->DefineFlags, Override->UnDefineFlags);
        }
    }

    xx String_EatSpacesInlineFromEnd(Dest);
}

// A relative source path counts as "already inside the intermediate directory" only when that
// directory name is a full leading path component -- i.e. the whole path, or followed by a
// separator. A plain string prefix wrongly matches, e.g., "intra_edge.c" against an intermediate
// directory named "int", which then drops the source directory and loses the file.
static bool Internal_IsPathUnderDirectory(const String Path, const String Dir)
{
    if (!String_IsValid(Dir) || Dir.Length == 0)
    {
        return false;
    }

    if (!String_StartsWith(Path, Dir, false))
    {
        return false;
    }

    // Exact match: the path is the directory itself.
    if (Path.Length == Dir.Length)
    {
        return true;
    }

    // Dir already ends in a separator, so it is a full leading path component.
    const uchar Last = Dir.Data[Dir.Length - 1];
    if (Last == '/' || Last == '\\')
    {
        return true;
    }

    // Otherwise Dir is only a real component when a separator follows it in Path.
    const uchar Next = Path.Data[Dir.Length];
    return Next == '/' || Next == '\\';
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
    bool bInsideIntermediatePath = Internal_IsPathUnderDirectory(RelativePathCopy, Params->IntermediateDirectory);
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

    String ObjDestinationDirectory = Params->IntermediateDirectory;
    if (String_IsValid(Params->CompilerObjectDirectory))
    {
        ObjDestinationDirectory = Params->CompilerObjectDirectory;
    }

    String SourceFileName = Filesystem_ExtractFileName(RelativePath, true);
    String PathOfObj = Filesystem_ExtractFilePath(RelativePath, false);

    if (Params->bDumpObjFilesInOneDirectory)
    {
        PathOfObj = String_Null();
    }

    // handle a special case where we are compiling something in the intermediate directory
    if (String_StartsWith(ObjDestinationDirectory, PathOfObj, false))
    {
        PathOfObj = String_Null();
    }

    // this is the format we're going for:
    // int/relativepath/assmeblyprefix|filename.no_ext|assemblypostfix|ext
    StringLocal(ObjectPath, MAX_PATH_LENGTH);
    StringLocal(PCHObjectPath, MAX_PATH_LENGTH);
    {
        StringLocal(ObjFile, MAX_PATH_LENGTH);
        {
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
            String_Append(&ObjFile, SourceFileName);
            String_Append(&ObjFile, Postfix);
            if (ObjExt.Length > 0 && !String_IsFirst(ObjExt, '.'))
            {
                String_AppendChar(&ObjFile, '.');
            }
            String_Append(&ObjFile, ObjExt);
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
        ProgramPath = Params->RCProgramPath;

        String_Empty(&ObjectPath);

        RC_Compile(Params, RelativePath, &ObjectPath, &CmdLine);
    }
    else
    {
        String OutputFlag  = S("-o");
        String CompileFlag = S("-c");

        // extra flags/includes/defines from any per-file override naming this source file
        StringLocal(AdditionalFlags, 8192);
        Internal_AppendFileOverrideFlags(Params, RelativePath, &AdditionalFlags);

        StringLocal(PCHFlags, MAX_PATH_LENGTH*2);

        String_AppendChar(&CmdLine, '"');
        String_Append    (&CmdLine, Params->CompilerPath);
        String_AppendChar(&CmdLine, '"');

        if (Params->Type == AssemblyType_NoCompilerObject)
        {
            OutputFlag = String_Null();
            String_Empty(&ObjectPath);

            if (String_IsValid(Params->CompilerCompileFlag))
            {
                CompileFlag = Params->CompilerCompileFlag;
            }
            else
            {
                CompileFlag = String_Null();
            }
        }
        else if (Params->Type == AssemblyType_CustomCompilerObject)
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

        if (String_IsValid(ObjectPath))
        {
            String_Append(&CmdLine, S(" \""));
            String_Append(&CmdLine, ObjectPath);
            String_Append(&CmdLine, S("\""));
        }
    }

    // Record the object this source maps to, and the compiler's sibling ".d"
    // dependency file (e.g. Foo.c.d) that lands next to it.
    // ===============================================================================================
    RecordArtifactPath(Params->ArtifactManifestHandle, Params->RootDirectory, ObjectPath);
    if (Params->Type == AssemblyType_PCH)
    {
        RecordArtifactPath(Params->ArtifactManifestHandle, Params->RootDirectory, PCHObjectPath);
    }

    struct MiscArtifactTable
    {
        String Extension;
        b64 bRecord;
    };
    struct MiscArtifactTable MiscArtifacts[2] =
    {
        { .Extension = S(".d"),                      .bRecord = !bIsMicrosoftCompiler },
        { .Extension = S(".nativecodeanalysis.xml"), .bRecord = bIsMicrosoftCompiler },
    };
    for (u32 i = 0; i < SArray_Capacity(MiscArtifacts); i++)
    {
        struct MiscArtifactTable Entry = MiscArtifacts[i];
        if (Entry.bRecord)
        {
            StringLocal(MiscPath, MAX_PATH_LENGTH);
            String_BuildPath(&MiscPath, Params->RootDirectory, ObjDestinationDirectory, PathOfObj, SourceFileName);
            String_Append(&MiscPath, Entry.Extension);
            RecordArtifactPath(Params->ArtifactManifestHandle, Params->RootDirectory, MiscPath);
        }
    }
    // ===============================================================================================

    u64 ObjectFileWriteTime = Filesystem_GetLastWriteTime(ObjectPath);
    u64 SourceFileWriteTime = Filesystem_GetLastWriteTime(FullPath);

    // The diff system flags a file when its per-file override changed but its source didn't - the object is
    // stale even though timestamps say otherwise, so force it to recompile.
    bool bForcedRecompile = Internal_IsForcedRecompile(Params, RelativePath);

    if (ObjectFileWriteTime >= SourceFileWriteTime && !bForcedRecompile)
    {
        #ifndef HOOD
        LOG("[Skipping] %S", FullPath);
        #else
        LOG("skip'n dis shit %S", FullPath);
        #endif
    }
    else
    {
        xx Filesystem_OpenDirectory(Filesystem_ExtractFilePath(ObjectPath, false));

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
                    xx Filesystem_DeleteFile(ObjectPath);
                    LOG_ERROR("Compiler errors detected. See above errors to fix. Exit code for process: %u. Aborting build...", ExitCode);
                    return false;
                }
            }
        }
        else
        {
            xx Filesystem_DeleteFile(ObjectPath);
            LOG_ERROR("Failed to spawn compiler process: \"%S\"", ProgramPath);
            return false;
        }
    }

    return true;
}

// Spawn (and throttle) compile processes for this module's source files onto the shared
// Params->Processes pool, WITHOUT waiting for them to finish. Pair with C_Compile_Wait.
//
// Splitting spawn from wait is what lets multiple modules enqueue their source files into one
// shared process pool: the build executor calls C_Compile_Spawn for every module (throttled by a
// single global MaxCompilersAtOnce), then a single C_Compile_Wait barrier drains them all. That is
// how source files across independent modules end up compiling in one cross-module parallel batch.
bool C_Compile_Spawn(const BuildParams* Params, u32* OutNumCompiled)
{
    if (NEVER(Params == NULL))
    {
        return false;
    }

    if (NEVER(OutNumCompiled == NULL))
    {
        return false;
    }

    CompileData UserData = { 0 };
    UserData.Params = Params;
    UserData.NumCompiled = OutNumCompiled;
    UserData.Index = 0;

    for each_string_in_list (Params->SourceFiles)
    {
        if (!Internal_DoCompile(&UserData, It.String))
        {
            return false;
        }
    }

    return true;
}

// Wait-barrier for the shared Params->Processes pool: block until every spawned compile process has
// finished and check their exit codes. In batch mode this is called once, after all modules have
// spawned, so NumCompiled is the total across everything that was enqueued.
bool C_Compile_Wait(const BuildParams* Params, u32 NumCompiled)
{
    if (NEVER(Params == NULL))
    {
        return false;
    }

    bool bSuccess = true;

    if (NumCompiled == 0)
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

    return bSuccess;
}

bool C_Compile(const BuildParams* Params, u32* OutNumCompiled)
{
    if (NEVER(Params == NULL))
    {
        return false;
    }

    if (NEVER(OutNumCompiled == NULL))
    {
        return false;
    }

    // On spawn failure the original code broke out and returned without waiting on the already-spawned
    // processes; preserve that by short-circuiting here.
    if (!C_Compile_Spawn(Params, OutNumCompiled))
    {
        return false;
    }

    return C_Compile_Wait(Params, *OutNumCompiled);
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

            bool bInsideIntermediatePath = Internal_IsPathUnderDirectory(RelativePath, Params->IntermediateDirectory);

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
    else if (String_IsEqual(Type, S("no_object"), false) ||
             String_IsEqual(Type, S("no_compiler_object"), false))
    {
        AssemblyType = AssemblyType_NoCompilerObject;
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


// Given a C compiler/linker driver path, return the path to its C++ counterpart in the same
// directory (clang -> clang++, gcc -> g++, cc -> c++) if that binary exists, otherwise a null string.
// Linking a C++ target through the C++ driver is what pulls in the correct C++ runtime for the
// platform (libc++ or libstdc++), instead of guessing the standard-library flag ourselves.
static String Internal_DeriveCppDriverPath(LinearAllocator* Arena, const String DriverPath, ECompiler Vendor)
{
    const String Name = Filesystem_ExtractFileName(DriverPath, false);

    String CppName = String_Null();
    if (Vendor == Compiler_Clang)
    {
        if (String_IsEqual(Name, S("clang"), false))
        {
            CppName = S("clang++");
        }
    }
    else if (Vendor == Compiler_GCC || Vendor == Compiler_MINGW)
    {
        if (String_IsEqual(Name, S("gcc"), false))
        {
            CppName = S("g++");
        }
        else if (String_IsEqual(Name, S("cc"), false))
        {
            CppName = S("c++");
        }
    }

    if (!String_IsValid(CppName))
    {
        return String_Null();
    }

    u32 LastSlash = 0;
    xx String_IndexOfLastPathSlash(DriverPath, &LastSlash);
    const String Dir = StrSlice(DriverPath.Data, LastSlash);

    StringLocal(CppPath, MAX_PATH_LENGTH);
    String_BuildPath(&CppPath, Dir, CppName);

    #if PLATFORM_WINDOWS
    String_Append(&CppPath, S(".exe"));
    #endif

    if (!Filesystem_DoesFileExist(CppPath))
    {
        return String_Null();
    }

    return String_Create(Arena, CppPath);
}

bool C_Link(const BuildParams* Params)
{
    if (NEVER(Params == NULL))
    {
        return false;
    }

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

        Internal_RecordLinkArtifacts(Params, BuildPath, Params->AssemblyWithExt);

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

    // A target with C++ translation units must link through the C++ compiler driver (clang++/g++) so
    // the correct C++ runtime is pulled in. If we can resolve one next to the C driver, use it; if not
    // (e.g. an explicit non-driver linker), fall back to adding the standard-library flag ourselves.
    bool bUsingCppDriver = false;
    if (Params->bHasCppFiles && !bIsMicrosoftLinker)
    {
        const String CppDriver = Internal_DeriveCppDriverPath(Params->Arena, Params->LinkerPath, Params->CompilerVendor);
        if (String_IsValid(CppDriver))
        {
            ProgramPath = CppDriver;
            bUsingCppDriver = true;
        }
    }

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

        // Fallback for a C++ target when we could not switch to the C++ driver above (see
        // bUsingCppDriver): add the C++ standard library to the C driver's link ourselves, after the
        // objects (link order matters). MSVC/clang-cl link their C++ runtime automatically.
        if (Params->bHasCppFiles && !bIsMicrosoftLinker && !bUsingCppDriver)
        {
            if (Params->CompilerVendor == Compiler_GCC || Params->CompilerVendor == Compiler_MINGW)
            {
                String_BuildSeparator(&CmdLine, ' ', S("-lstdc++"));
            }
            else if (Params->CompilerVendor == Compiler_Clang)
            {
                String_BuildSeparator(&CmdLine, ' ', S("-lc++"));
            }
        }

        xx String_EatSpacesInlineFromEnd(&CmdLine);
        String_AppendSpace(&CmdLine);

        String SpaceAfterOut = S(" ");
        if (bIsMicrosoftLinker)
        {
            SpaceAfterOut = String_Null();
        }
        
        String_Concat(&CmdLine, OutputFlag, SpaceAfterOut, S("\""), BuildPath, Params->AssemblyWithExt, S("\""));

        Internal_RecordLinkArtifacts(Params, BuildPath, Params->AssemblyWithExt);

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

        StringLocal(LibOutputPath, MAX_PATH_LENGTH);
        String_Append(&LibOutputPath, BuildPath);
        String_Append(&LibOutputPath, LibFile);
        RecordArtifactPath(Params->ArtifactManifestHandle, Params->RootDirectory, LibOutputPath);

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
