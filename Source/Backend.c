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
#include "Core/Memory.h"
#include "Core/HashUtils.h"
#endif

// Cached header write times for one module's compile loop (see "Header dependency tracking" below).
// Open-addressed, keyed by the FNV1a hash of the path exactly as the dependency file spells it (a
// collision could reuse a wrong timestamp, but at ~10^4 entries the odds are ~10^-12 - accepted).
// Sized to the module and carved from the build arena in C_Compile_Spawn. Lookups tolerate a NULL
// Entries table (they fall through to the filesystem), but that is pure defense - the table is
// always allocated.
STRUCT(DepTimeCacheEntry)
{
    u64 PathHash; // 0 = empty slot
    u64 WriteTime;
};

STRUCT(DepTimeCache)
{
    DepTimeCacheEntry* Entries;
    u64 NumEntries; // power of two (index mask)
};

STRUCT(CompileData)
{
    const BuildParams* Params;
    u32* NumCompiled;
    u32 Index;
    DepTimeCache DepCache;
};



void RecordArtifactPath(const FileHandle ManifestHandle, const String Path)
{
    if (IsValidFileHandle(ManifestHandle) && Path.Length > 0)
    {
        if (Filesystem_IsPathRelative(Path))
        {
            LOG_WARNING("Not recording relative artifact path \"%S\" in the manifest. This is a bug: artifact paths must be absolute.", Path);
        }
        else
        {
            // Still normalized: absolute paths routinely carry embedded "..\" segments (e.g. a
            // "BuildDirectory ../bin"), which the manifest's path-traversal defense would reject
            // when read back.
            StringLocal(AbsolutePath, MAX_PATH_LENGTH);
            String_Copy(&AbsolutePath, Path);
            xx Filesystem_ConvertRelativeToAbsolutePath(&AbsolutePath);

            StringLocal(Line, MAX_PATH_LENGTH + 2);
            String_Copy(&Line, AbsolutePath);
            String_AppendChar(&Line, '\n');
            xx Filesystem_WriteLine(ManifestHandle, Line, NULL);
        }
    }
}

static void Internal_RecordLinkArtifacts(const BuildParams* Params, const String BaseDir, const String OutputName)
{
    StringLocal(OutputPath, MAX_PATH_LENGTH);
    String_Append(&OutputPath, BaseDir);
    String_Append(&OutputPath, OutputName);
    RecordArtifactPath(Params->ArtifactManifestHandle, OutputPath);

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
            RecordArtifactPath(Params->ArtifactManifestHandle, Path);
        }
    }
    #endif
}

// The artifact manifest is rewritten from scratch on every build, but the linker/archiver output
// paths are only recorded inside C_Link. When an incremental build skips the link step entirely,
// this re-records those paths so a later "clean" still knows about the exe/lib in the build
// directory. Mirrors the per-type recording that C_Link performs.
void RecordSkippedLinkArtifacts(const BuildParams* Params)
{
    if (Params != NULL &&
        Params->Type != AssemblyType_PCH &&
        Params->Type != AssemblyType_Null &&
        Params->Type != AssemblyType_NoCompilerObject)
    {
        StringLocal(BuildPath, MAX_PATH_LENGTH);
        String_BuildPath(&BuildPath, Params->RootDirectory, Params->BuildDirectory);
        String_AppendPathSeparator(&BuildPath);

        const bool bIsCustomObject = Params->Type == AssemblyType_CustomCompilerObject;
        const bool bIsExe          = Params->Type == AssemblyType_Executable;
        const bool bIsDLL          = Params->Type == AssemblyType_Library || Params->Type == AssemblyType_DynamicLibrary;
        const bool bIsLib          = Params->Type == AssemblyType_Library || Params->Type == AssemblyType_StaticLibrary;

        if (bIsCustomObject || bIsExe || bIsDLL)
        {
            Internal_RecordLinkArtifacts(Params, BuildPath, Params->AssemblyWithExt);
        }

        if (bIsLib)
        {
            StringLocal(LibFile, MAX_PATH_LENGTH);
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

            StringLocal(LibOutputPath, MAX_PATH_LENGTH);
            String_Append(&LibOutputPath, BuildPath);
            String_Append(&LibOutputPath, LibFile);
            RecordArtifactPath(Params->ArtifactManifestHandle,LibOutputPath);
        }
    }
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

static void Internal_FlushCompileOutput(CompileProcess* Job)
{
    Platform_ConsoleWrite_CustomLength((const char*)Job->Output.Data, Job->Output.Length, 0, false);

    // keep blocks line-separated even when a compiler's last line has no newline
    if (!String_IsLast(Job->Output, '\n'))
    {
        Platform_ConsoleWrite_CustomLength("\n", 1, 0, false);
    }

    Job->Output.Length = 0;
}

static bool Internal_DrainCompileOutput(CompileProcess* Job)
{
    bool bReadAny = false;

    if (String_IsDataValid(Job->Output) && Platform_IsValidHandle(Job->Pipe[0]))
    {
        bool bPipeAlive = true;
        usize BytesRead = 1;

        while (bPipeAlive && BytesRead > 0)
        {
            if (Job->Output.Length == Job->Output.Capacity)
            {
                Internal_FlushCompileOutput(Job);
            }

            BytesRead = 0;
            bPipeAlive = Filesystem_ReadPipe(Job->Pipe,
                                            Job->Output.Capacity - Job->Output.Length,
                                            Job->Output.Data + Job->Output.Length,
                                            &BytesRead);
            if (BytesRead > 0)
            {
                Job->Output.Length += (u32)BytesRead;
                bReadAny = true;
            }
        }

        if (!bPipeAlive)
        {
            // completely drained now, close the "read" pipe
            Platform_ClosePipeEnd(&Job->Pipe[0]);
        }
    }

    return bReadAny;
}

static u32 Internal_WaitForCompileProcess(CompileProcessPool* Pool, i32 WantIndex, u32* OutExitCode)
{
    u32 FinishedIndex = 0;
    bool bFinished = false;

    while (!bFinished)
    {
        bool bReadAny = false;
        const u32 Num = (u32)Array_Num(Pool->Jobs);

        if (NEVER(Num == 0))
        {
            *OutExitCode = 0;
            bFinished = true;
        }

        for (u32 i = 0; i < Num; i++)
        {
            if (Internal_DrainCompileOutput(&Pool->Jobs[i]))
            {
                bReadAny = true;
            }
        }

        const u32 First = WantIndex >= 0 ? (u32)WantIndex : 0;
        const u32 Last  = WantIndex >= 0 ? (u32)WantIndex + 1 : Num;

        for (u32 i = First; i < Last && !bFinished; i++)
        {
            u32 ExitCode = 0;
            if (Platform_GetExitCodeForProcess(Pool->Jobs[i].Handle, &ExitCode))
            {
                FinishedIndex = i;
                *OutExitCode = ExitCode;
                bFinished = true;
            }
        }

        // without the sleep this loop busy-spins a full core, a core stolen from the very
        // compilers we are waiting on.
        if (!bFinished && !bReadAny)
        {
            Platform_Sleep(1);
        }
    }

    return FinishedIndex;
}

static void Internal_FinishCompileProcess(CompileProcessPool* Pool, u32 Index)
{
    CompileProcess* Job = &Pool->Jobs[Index];

    xx Internal_DrainCompileOutput(Job);
    if (Job->Output.Length)
    {
        Internal_FlushCompileOutput(Job);
    }

    Platform_ClosePipeEnd(&Job->Pipe[0]);

    if (Job->Output.Data != NULL)
    {
        Array_Add(Pool->FreeBuffers, Job->Output);
    }

    Array_RemoveAt(Pool->Jobs, NULL, Index);
}

// Where a .rc's compiled .res lands, relative to the root: under the intermediate directory (or
// Compiler.ObjectDirectory when set), mirroring the source's relative path the same way objects do,
// so cleans, rebuilds and the .build diff can always delete it - never next to the source, which the
// clean system refuses to touch. A .rc that already lives inside the intermediate directory (the
// generated icon/version resources) keeps its .res next to itself.
static void Internal_MakeResPath(const BuildParams* Params, const String RelativePath, String* OutResPath)
{
    String DestinationDirectory = Params->IntermediateDirectory;
    if (String_IsValid(Params->CompilerObjectDirectory))
    {
        DestinationDirectory = Params->CompilerObjectDirectory;
    }

    String PathOfRes = Filesystem_ExtractFilePath(RelativePath, false);

    if (Params->bDumpObjFilesInOneDirectory)
    {
        PathOfRes = String_Null();
    }

    // handle the special case where the .rc itself sits in the destination directory
    if (String_StartsWith(DestinationDirectory, PathOfRes, false))
    {
        PathOfRes = String_Null();
    }

    StringLocal(ResFile, MAX_PATH_LENGTH);
    String_Append(&ResFile, Filesystem_StripFileExtension(Filesystem_ExtractFileName(RelativePath, true)));
    String_Append(&ResFile, S(".res"));

    String_BuildPath(OutResPath, DestinationDirectory, PathOfRes, ResFile);
}

static void RC_Compile(const BuildParams* Params, const String FullRCPath, const String ResPath, String* OutCmdLine)
{
    #if PLATFORM_WINDOWS
    StringLocal(CmdLine, UINT16_MAX);

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
    
    // Resource.Flags from the .build file (plus the automatic /nologo) come pre-merged in RCProgramFlags
    if (Params->RCProgramFlags.Length > 0)
    {
        String_AppendSpace(&CmdLine);
        String_Append(&CmdLine, Export_TryWriteFlagsAndReturnThisValue(S("ResourceCompilerFlags"), Params->RCProgramFlags));
    }

    // Resource.Includes from the .build file, plus the Windows SDK paths rc.exe can't find on its own
    {
        StringLocal(RCIncludes, MAX_PATH_LENGTH*8); // user includes + up to 7 sdk paths
        String_Append(&RCIncludes, Params->RCIncludeFlags);

        // ergghh i hate this... TODO: something better
        // TODO: use enum instead
        if (String_EndsWith(Params->RCProgramPath, S("rc.exe"), false))
        {
            // TODO: this is duplicated code, collapse this in one place
            StringLocal(WinSDKInclude, MAX_PATH_LENGTH*7); // 7 paths
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

            if (WinSDKInclude.Length > 0)
            {
                String_AppendSpace(&RCIncludes);
                String_Append(&RCIncludes, WinSDKInclude);
            }
        }

        xx String_EatSpacesInlineFromEnd(&RCIncludes);

        if (RCIncludes.Length > 0)
        {
            String_AppendSpace(&CmdLine);
            String_Append(&CmdLine, Export_TryWriteFlagsAndReturnThisValue(S("ResourceCompilerIncludes"), RCIncludes));
        }
    }

    // Resource.Defines / Resource.UnDefines from the .build file, already expanded with the prefix
    // the resolved RC program wants (-D for windres, /D otherwise)
    String RCDefines   = Export_TryWriteFlagsAndReturnThisValue(S("ResourceCompilerDefines"),   Params->RCDefineFlags);
    String RCUnDefines = Export_TryWriteFlagsAndReturnThisValue(S("ResourceCompilerUnDefines"), Params->RCUnDefineFlags);

    if (bWindres)
    {
        // for windres specifically, args must be in this form
        // <FLAGS> -O coff <DEFINES> -i <SOURCE> -o <OBJECT>

        String_Append(&CmdLine, S(" -O coff"));

        if (RCDefines.Length > 0)
        {
            String_AppendSpace(&CmdLine);
            String_Append(&CmdLine, RCDefines);
        }

        if (RCUnDefines.Length > 0)
        {
            String_AppendSpace(&CmdLine);
            String_Append(&CmdLine, RCUnDefines);
        }

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
        if (RCDefines.Length > 0)
        {
            String_AppendSpace(&CmdLine);
            String_Append(&CmdLine, RCDefines);
        }

        if (RCUnDefines.Length > 0)
        {
            String_AppendSpace(&CmdLine);
            String_Append(&CmdLine, RCUnDefines);
        }

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

static u64 Internal_GetDepWriteTimeCached(DepTimeCache* Cache, const String Path)
{
    u64 Result = 0;

    if (Cache->Entries == NULL)
    {
        Result = (u64)Filesystem_GetLastWriteTime(Path);
    }
    else
    {
        u64 Hash = FNV1a_Hash(Path.Data, Path.Length);
        if (Hash == 0)
        {
            Hash = 1;
        }

        bool bResolved = false;

        u32 Index = (u32)(Hash & (Cache->NumEntries - 1));
        for (u32 Probe = 0; Probe < 32 && !bResolved; Probe++)
        {
            DepTimeCacheEntry* Entry = &Cache->Entries[Index];

            if (Entry->PathHash == Hash)
            {
                Result = Entry->WriteTime;
                bResolved = true;
            }
            else if (Entry->PathHash == 0)
            {
                Result = (u64)Filesystem_GetLastWriteTime(Path);
                Entry->PathHash  = Hash;
                Entry->WriteTime = Result;
                bResolved = true;
            }
            else
            {
                Index = (Index + 1) & (Cache->NumEntries - 1);
            }
        }

        // probe window exhausted (table effectively full) - resolve without caching
        if (!bResolved)
        {
            Result = (u64)Filesystem_GetLastWriteTime(Path);
        }
    }

    return Result;
}

STRUCT(DepTimeFoldData)
{
    DepTimeCache* Cache;
    String* NewestPath;
    u64 NewestTime;
};

static void Internal_FoldDepTime(const String DepPath, void* UserData)
{
    DepTimeFoldData* Data = UserData;

    u64 Time = Internal_GetDepWriteTimeCached(Data->Cache, DepPath);

    if (Time == 0)
    {
        Time = UINT64_MAX;
    }

    if (Time > Data->NewestTime)
    {
        Data->NewestTime = Time;
        String_Copy(Data->NewestPath, DepPath);
    }
}

static bool Internal_TryGetNewestDepTime(DepTimeCache* Cache, const String DepFilePath, bool bIsMSVCJson, u64* OutNewestTime, String* OutNewestPath)
{
    DepTimeFoldData Fold = {0};
    Fold.Cache      = Cache;
    Fold.NewestPath = OutNewestPath;

    bool bParsedOk = false;
    bool bHaveDepFile;

    if (bIsMSVCJson)
    {
        bHaveDepFile = ParseDependencyFile_MSVCJson(DepFilePath, &Internal_FoldDepTime, &Fold, &bParsedOk);
    }
    else
    {
        bHaveDepFile = ParseDependencyFile_Makefile(DepFilePath, &Internal_FoldDepTime, &Fold, &bParsedOk);
    }

    if (bHaveDepFile)
    {
        *OutNewestTime = bParsedOk ? Fold.NewestTime : UINT64_MAX;
    }

    return bHaveDepFile;
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
    CompileProcessPool* Pool = Params->Processes;

    if (Params->MaxCompilersAtOnce > 0 && !Params->bShouldWaitPerCompileProcess)
    {
        u32 Num = (u32)Array_Num(Pool->Jobs);
        if (Num >= Params->MaxCompilersAtOnce)
        {
            u32 ExitCode = 0;
            u32 Index = Internal_WaitForCompileProcess(Pool, -1, &ExitCode);
            Internal_FinishCompileProcess(Pool, Index);

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
            String Name    = SourceFileName;
            String Prefix  = String_Null();
            String Postfix = String_Null();
            String ObjExt  = String_IsValid(Params->CompilerObjectExt) ? Params->CompilerObjectExt : DefaultObjExtension;

            if (Params->Type == AssemblyType_CustomCompilerObject)
            {
                Prefix  = Params->AssemblyPrefix;
                Postfix = Params->AssemblyPostfix;

                if (String_IsValid(Params->Extension))
                {
                    Name   = Filesystem_ExtractFileName(RelativePath, false);
                    ObjExt = Params->Extension;
                }
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

    StringLocal(DepFilePath, MAX_PATH_LENGTH);
    String_BuildPath(&DepFilePath, Params->RootDirectory, ObjDestinationDirectory, PathOfObj, SourceFileName);
    String_Append(&DepFilePath, bIsMicrosoftCompiler ? S(".json") : S(".d"));

    bool bUseDepFile = false;

    // ===============================================================================================

    String ProgramPath = Params->CompilerPath;
    StringLocal(CmdLine, UINT16_MAX);

    // switch on the source file type
    if (IsAsmSource(Ext))
    {
        ProgramPath = Params->AsmPath;

        String AssemblerFlags    = Export_TryWriteFlagsAndReturnThisValue(S("AssemblerFlags"),    Params->AssemblerFlags);
        String AssemblerDefines  = Export_TryWriteFlagsAndReturnThisValue(S("AssemblerDefines"),  Params->AssemblerDefines);
        String AssemblerIncludes = Export_TryWriteFlagsAndReturnThisValue(S("AssemblerIncludes"), Params->AssemblerIncludes);

        String_AppendChar(&CmdLine, '"');
        String_Append    (&CmdLine, Params->AsmPath);
        String_AppendChar(&CmdLine, '"');
        String_AppendSpace(&CmdLine);

        if (bIsMicrosoftAssembler)
        {
            String_Append(&CmdLine, S("/nologo /c /Fo\""));
            String_Append(&CmdLine, ObjectPath);
            String_Append(&CmdLine, S("\" "));
            String_BuildSeparator(&CmdLine, ' ', FullSourcePath, AssemblerFlags, AssemblerDefines, AssemblerIncludes);
            xx String_EatSpacesInlineFromEnd(&CmdLine);
        }
        else
        {
            String_BuildSeparator(&CmdLine, ' ', FullSourcePath, AssemblerFlags, AssemblerDefines, AssemblerIncludes);
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

        StringLocal(ResRelativePath, MAX_PATH_LENGTH);
        Internal_MakeResPath(Params, RelativePath, &ResRelativePath);

        String_Empty(&ObjectPath);
        String_BuildPath(&ObjectPath, Params->RootDirectory, ResRelativePath);
        xx Filesystem_ConvertRelativeToAbsolutePath(&ObjectPath);

        RC_Compile(Params, FullPath, ObjectPath, &CmdLine);
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

        // ask the compiler to record which headers this translation unit includes, so the next build
        // can skip the object unless one of them changed (see "Header dependency tracking" above).
        // Custom compiler/no-object modules run arbitrary tools that would choke on these flags.
        StringLocal(DepFlags, MAX_PATH_LENGTH + 32);
        if (Params->Type != AssemblyType_CustomCompilerObject && Params->Type != AssemblyType_NoCompilerObject)
        {
            if (bIsMicrosoftCompiler)
            {
                // VS2019 16.7+; older cl warns D9002 and ignores it, and the skip check falls back
                String_Append(&DepFlags, S("/sourceDependencies \""));
                String_Append(&DepFlags, DepFilePath);
                String_Append(&DepFlags, S("\""));
                bUseDepFile = true;
            }
            else if (Params->CompilerVendor == Compiler_Clang_MSVC)
            {
                // clang-cl: -MD means "dynamic CRT" in cl mode, so the gnu-style dep flags must go
                // through /clang:. The path token is quoted whole - it lands as a single argument.
                String_Append(&DepFlags, S("/clang:-MMD \"/clang:-MF"));
                String_Append(&DepFlags, DepFilePath);
                String_Append(&DepFlags, S("\""));
                bUseDepFile = true;
            }
            else if (Params->CompilerVendor == Compiler_Clang ||
                     Params->CompilerVendor == Compiler_GCC   ||
                     Params->CompilerVendor == Compiler_MINGW)
            {
                String_Append(&DepFlags, S("-MMD -MF \""));
                String_Append(&DepFlags, DepFilePath);
                String_Append(&DepFlags, S("\""));
                bUseDepFile = true;
            }
            else if (Params->CompilerVendor == Compiler_TCC)
            {
                // tcc understands -MD/-MF but not -MMD; system headers get recorded too - harmless
                String_Append(&DepFlags, S("-MD -MF \""));
                String_Append(&DepFlags, DepFilePath);
                String_Append(&DepFlags, S("\""));
                bUseDepFile = true;
            }
            else
            {
                // unknown vendor - no dependency info; the skip check below falls back to
                // Params->NewestHeaderWriteTime
            }
        }

        // Compiler output is captured through a pipe, which makes compilers
        // silently drop colored diagnostics: they no longer see a terminal. When our own stdout is
        // a real console, ask for color explicitly. MSVC and tcc have no colored output to ask
        // for, and exported scripts must not hardcode a color choice for whatever runs them later.
        // TODO: option to disable this?
        String ColorFlags = String_Null();
        if (Params->Type != AssemblyType_CustomCompilerObject &&
            Params->Type != AssemblyType_NoCompilerObject)
        {
            if (!Export_IsCapturingCommands() && Platform_IsConsoleOutput())
            {
                if (Params->CompilerVendor == Compiler_GCC ||
                    Params->CompilerVendor == Compiler_MINGW)
                {
                    ColorFlags = S("-fdiagnostics-color=always");
                }
                else if (Params->CompilerVendor == Compiler_Clang ||
                         Params->CompilerVendor == Compiler_Clang_MSVC)
                {
                    // -fansi-escape-codes makes clang emit ANSI sequences instead of calling the Win32
                    // console API, which it cannot do through a pipe
                    ColorFlags = S("-fcolor-diagnostics -fansi-escape-codes");
                }
            }
        }

        String CompilerFlags = Export_TryWriteFlagsAndReturnThisValue(S("CompilerFlags"), Params->CompilerFlags);
        String IncludeFlags  = Export_TryWriteFlagsAndReturnThisValue(S("IncludeFlags"),  Params->IncludeFlags);
        String Defines       = Export_TryWriteFlagsAndReturnThisValue(S("Defines"),       Params->DefineFlags);
        String UnDefines     = Export_TryWriteFlagsAndReturnThisValue(S("UnDefines"),     Params->UnDefineFlags);

        String CompilerFlagsLeft  = Params->bCompilerFlagsFirst ? CompilerFlags : String_Null();
        String CompilerFlagsRight = Params->bCompilerFlagsFirst ? String_Null() : CompilerFlags;

        String_BuildSeparator(&CmdLine, ' ', CompilerFlagsLeft, CompileFlag, FullSourcePath, CompilerFlagsRight, IncludeFlags, Defines, UnDefines, AdditionalFlags, PCHFlags, DepFlags, ColorFlags, OutputFlag);
        xx String_EatSpacesInlineFromEnd(&CmdLine);

        if (String_IsValid(ObjectPath))
        {
            String_Append(&CmdLine, S(" \""));
            String_Append(&CmdLine, ObjectPath);
            String_Append(&CmdLine, S("\""));
        }
    }

    // Record the object this source maps to, and the compiler's sibling dependency file
    // (e.g. Foo.c.d / Foo.c.json, see "Header dependency tracking" above) that lands next to it.
    // ===============================================================================================
    RecordArtifactPath(Params->ArtifactManifestHandle,ObjectPath);
    if (Params->Type == AssemblyType_PCH)
    {
        RecordArtifactPath(Params->ArtifactManifestHandle,PCHObjectPath);
    }

    // TODO: maybe record this only if they exist after the compiler is successful?
    struct MiscArtifactTable
    {
        String Extension;
        b64 bRecord;
    };
    struct MiscArtifactTable MiscArtifacts[3] =
    {
        { .Extension = S(".d"),                      .bRecord = !bIsMicrosoftCompiler },
        { .Extension = S(".json"),                   .bRecord = bIsMicrosoftCompiler },
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
            RecordArtifactPath(Params->ArtifactManifestHandle,MiscPath);
        }
    }
    // ===============================================================================================

    u64 ObjectFileWriteTime = Filesystem_GetLastWriteTime(ObjectPath);
    u64 SourceFileWriteTime = Filesystem_GetLastWriteTime(FullPath);

    // The diff system flags a file when its per-file override changed but its source didn't - the object is
    // stale even though timestamps say otherwise, so force it to recompile.
    bool bForcedRecompile = Internal_IsForcedRecompile(Params, RelativePath);

    // The object must be newer than the source file AND every header the previous compile of it
    // recorded (see "Header dependency tracking" above). Without dependency info, fall back to the
    // newest header under the source directory, which over-rebuilds but never under-rebuilds.
    u64 NewestInputTime = SourceFileWriteTime;
    StringLocal(NewestDepPath, MAX_PATH_LENGTH);

    if (ObjectFileWriteTime > 0 && !bForcedRecompile)
    {
        bool bHaveDepInfo = false;

        if (bUseDepFile)
        {
            u64 NewestDepTime = 0;
            if (Internal_TryGetNewestDepTime(&Data->DepCache, DepFilePath, bIsMicrosoftCompiler, &NewestDepTime, &NewestDepPath))
            {
                if (NewestDepTime > NewestInputTime)
                {
                    NewestInputTime = NewestDepTime;
                }

                bHaveDepInfo = true;
            }
        }

        if (!bHaveDepInfo && Params->NewestHeaderWriteTime > NewestInputTime)
        {
            NewestInputTime = Params->NewestHeaderWriteTime;
        }
    }

    bool bSuccess = true;

    if (Export_IsCapturingCommands())
    {
        // Emit unconditionally - the script must be able to build from a clean tree, so the
        // timestamp skip below does not apply. The object directory still has to exist when the
        // script runs; create it now exactly like a real build would.
        xx Filesystem_OpenDirectory(Filesystem_ExtractFilePath(ObjectPath, false));

        String ToolVarName = S("Compiler");
        if (IsAsmSource(Ext))
        {
            ToolVarName = S("Assembler");
        }
        else if (String_EndsWith(RelativePath, S(".rc"), false))
        {
            ToolVarName = S("ResourceCompiler");
        }

        StringLocal(EchoText, MAX_PATH_LENGTH + 16);
        String_Format(&EchoText, S("Compiling %S"), FullPath);

        bSuccess = Export_EmitScriptCommand(EchoText, ToolVarName, ProgramPath, CmdLine);
    }
    else if (ObjectFileWriteTime >= NewestInputTime && !bForcedRecompile)
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
            // recompiling even though the source itself is unchanged - say which dependency did it
            if (ObjectFileWriteTime > 0 && ObjectFileWriteTime >= SourceFileWriteTime && NewestDepPath.Length > 0)
            {
                LOG("    dependency modified: %S", NewestDepPath);
            }

            LOG("\n    %S\n", CmdLine);
        }

        // Capture the compiler's stdout+stderr into a per-job buffer so parallel compilers cannot
        // interleave their diagnostics on the console; the block prints atomically when the job
        // finishes. With no buffer left to take, the job degrades to writing the console directly.
        CompileProcess Job = {0};
        Platform_PipeInit(Job.Pipe);

        const u32 NumFreeBuffers = (u32)Array_Num(Pool->FreeBuffers);
        if (NumFreeBuffers > 0)
        {
            Job.Output = Pool->FreeBuffers[NumFreeBuffers - 1];
            Array_RemoveAt(Pool->FreeBuffers, NULL, NumFreeBuffers - 1);

            Job.Handle = Platform_RunProcess_Ex(ProgramPath, CmdLine, Params->RootDirectory, &Job.Pipe);

            // Close our "write" handle, we only care about reading.
            Platform_ClosePipeEnd(&Job.Pipe[1]);

            // Clean up if we failed to spawn our compiler process
            if (!Platform_IsValidHandle(Job.Handle))
            {
                Platform_ClosePipe(Job.Pipe);

                Array_Add(Pool->FreeBuffers, Job.Output);
            }
        }
        else
        {
            Job.Handle = Platform_RunProcess(ProgramPath, CmdLine, Params->RootDirectory, String_Null());
        }

        if (Platform_IsValidHandle(Job.Handle))
        {
            Array_Add(Pool->Jobs, Job);
            (*Data->NumCompiled) += 1;

            if (UNLIKELY(Params->bShouldWaitPerCompileProcess))
            {
                // we cant do a plain "wait for exit" because of the way we are capturing output now.
                // we're redirecting the output to a pipe and we have to keep reading until exit,
                // so that it does not deadlock and hang forever.

                u32 ExitCode = 0;
                const u32 Index = (u32)Array_Num(Pool->Jobs) - 1;
                xx Internal_WaitForCompileProcess(Pool, (i32)Index, &ExitCode);
                Internal_FinishCompileProcess(Pool, Index);

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

    return bSuccess;
}

// Spawn (and throttle) compile processes for this module's source files onto the shared
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

    if (NEVER(Params->Arena == NULL))
    {
        return false;
    }

    CompileData UserData = { 0 };
    UserData.Params = Params;
    UserData.NumCompiled = OutNumCompiled;
    UserData.Index = 0;

    {
        u32 NumEntries = 128;
        const u32 Target = Params->NumSources > 256 ? 4096 : Params->NumSources * 16;

        // round up
        while (NumEntries < Target)
        {
            NumEntries *= 2;
        }

        const usize SizeNeeded = NumEntries * sizeof(DepTimeCacheEntry);

        UserData.DepCache.Entries    = LinearAllocator_Allocate(Params->Arena, SizeNeeded);
        UserData.DepCache.NumEntries = NumEntries;
        MemZero(UserData.DepCache.Entries, SizeNeeded);
    }

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

    CompileProcessPool* Pool = Params->Processes;

    while (Pool && Array_Num(Pool->Jobs) > 0)
    {
        u32 ExitCode = 0;
        u32 Index = Internal_WaitForCompileProcess(Pool, -1, &ExitCode); // wait for any to finish
        Internal_FinishCompileProcess(Pool, Index);

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

void CompileProcessPool_InitOutputBuffers(LinearAllocator* Arena, CompileProcessPool* Pool)
{
    if (NEVER(Arena == NULL)) { return; }
    if (NEVER(Pool == NULL))  { return; }

    usize NumBuffers = Array_Capacity(Pool->FreeBuffers);
    usize PerJobSize = Kibibytes(8);

    const usize Budget = (Arena->TotalSize - Arena->Allocated) / 4;

    // if we are over budget, keep reducing until we're below budget
    while (PerJobSize > Kibibytes(2) && NumBuffers * PerJobSize > Budget)
    {
        PerJobSize /= 2;
    }

    if (NumBuffers * PerJobSize > Budget)
    {
        NumBuffers = Budget / PerJobSize;
    }

    for (usize i = 0; i < NumBuffers; i++)
    {
        String Buffer = String_Reserve(Arena, (u32)PerJobSize);
        Array_Add(Pool->FreeBuffers, Buffer);
    }
}

static void Internal_ReapAbandoned(CompileProcessPool* Pool)
{
    if (NEVER(Pool == NULL)) { return; }

    while (Array_Num(Pool->Jobs) > 0)
    {
        u32 ExitCode = 0;
        u32 Index = Internal_WaitForCompileProcess(Pool, -1, &ExitCode);
        Internal_FinishCompileProcess(Pool, Index);
    }
}

bool C_Compile(const BuildParams* Params, u32* OutNumCompiled)
{
    if (NEVER(Params == NULL))         { return false; }
    if (NEVER(OutNumCompiled == NULL)) { return false; }

    bool bSuccess = C_Compile_Spawn(Params, OutNumCompiled);
    if (bSuccess)
    {
        bSuccess = C_Compile_Wait(Params, *OutNumCompiled);
    }

    Internal_ReapAbandoned(Params->Processes);

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

            Internal_MakeResPath(Params, RelativePath, &ObjectPath);
        }
        else
        #endif
        {
            String ObjDestinationDirectory = Params->IntermediateDirectory;
            if (String_IsValid(Params->CompilerObjectDirectory))
            {
                ObjDestinationDirectory = Params->CompilerObjectDirectory;
            }

            // the object must be looked up where Internal_DoCompile wrote it: next to the source's
            // relative path, unless the objects were dumped flat into one directory
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

            // make the object file string
            // example: some_file.c now becomes some_file.o
            StringLocal(ObjFile, MAX_PATH_LENGTH);
            {
                String_Append(&ObjFile, Filesystem_ExtractFileName(RelativePath, true));

                const String Ext = String_IsValid(Params->CompilerObjectExt) ? Params->CompilerObjectExt : DefaultObjExt;
                if (!String_IsFirst(Ext, '.'))
                {
                    String_AppendChar(&ObjFile, '.');
                }

                String_Append(&ObjFile, Ext);
            }

            String_BuildPath(&ObjectPath, ObjDestinationDirectory, PathOfObj, ObjFile);
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

// GNU ld reports undefined references as groups: a context line naming the object file and
// function, then one line per missing symbol - and every message is prefixed with the linker's
// own (very long) path:
//
//   <ld path>: Intermediate/Program.c.o: in function `main':
//   C:/GoldenDecoder/Program.c:308: undefined reference to `InitWindow'
//   <ld path>: C:/GoldenDecoder/Program.c:309: undefined reference to `SetTargetFPS'
//   collect2.exe: error: ld returned 1 exit status
//
// Reshape that into the same look the MSVC parser produces: the object file once as a header,
// one [ERROR] line per symbol with a short file:line, and the function context muted underneath.
static void Internal_ParseAndLogLinkerOutput_GNU(String StdOutData)
{
    String LastObjFile = String_Null();
    String CurrentFunction = String_Null();
    u32 NumUndefined = 0;
    u32 LastErrorIndent = 26;

    u32 Offset = 0;
    while (1)
    {
        if (Offset >= StdOutData.Length)
        {
            break;
        }

        String PipeDataSlice = StrShiftF(StdOutData, Offset);

        u32 NewLineIndex = 0;
        if (!String_IndexOfFirstNewline(PipeDataSlice, &NewLineIndex))
        {
            break;
        }

        Offset += NewLineIndex+1;
        if (PipeDataSlice.Data[NewLineIndex] == '\r')
        {
            Offset++;
        }

        String Line = String_EatSpaces(StrSlice(PipeDataSlice.Data, NewLineIndex));

        // every ld message starts with the linker's own path - drop it, it is pure noise
        u32 PrefixIndex = 0;
        if (String_IndexOfSubstring(Line, S("ld.exe: "), true, &PrefixIndex))
        {
            Line = String_EatSpaces(StrShiftF(Line, PrefixIndex + 8));
        }
        else if (String_IndexOfSubstring(Line, S("ld: "), true, &PrefixIndex))
        {
            Line = String_EatSpaces(StrShiftF(Line, PrefixIndex + 4));
        }

        if (Line.Length > 0)
        {
            u32 Index = 0;
            if (String_IndexOfSubstring(Line, S(": in function `"), true, &Index))
            {
                // "<obj>: in function `<func>':" - remember both, print the object as a header
                String ObjFile = StrSlice(Line.Data, Index);

                CurrentFunction = StrShiftF(Line, Index + 15);
                u32 QuoteIndex = 0;
                if (String_IndexOfChar(CurrentFunction, '\'', &QuoteIndex))
                {
                    CurrentFunction = StrSlice(CurrentFunction.Data, QuoteIndex);
                }

                if (!(String_IsValid(LastObjFile) && String_IsEqual(LastObjFile, ObjFile, true)))
                {
                    LOG_INLINE_WARNING("\n%S\n", ObjFile);
                }

                LastObjFile = ObjFile;
            }
            else if (String_IndexOfSubstring(Line, S(": undefined reference to `"), true, &Index))
            {
                // "<file>:<line>: undefined reference to `<sym>'"
                const String Location = Filesystem_ExtractFileName(StrSlice(Line.Data, Index), true);
                const String Message  = StrShiftF(Line, Index+1);

                LOG_INLINE("    ");
                LOG_INLINE_ERROR("[ERROR] %S", Location);
                LOG(" |%S", Message);

                if (String_IsValid(CurrentFunction) && CurrentFunction.Length > 0)
                {
                    // line the context up under the message text: "    [ERROR] " + location + " | "
                    StringLocal(ContextIndent, 64);
                    ContextIndent.Length = (u32)Min(Location.Length + 15, ContextIndent.Capacity);
                    String_Fill(&ContextIndent, ' ');

                    LOG_MUTE("%Sreferenced in function %S", ContextIndent, CurrentFunction);
                }

                NumUndefined++;
            }
            else if (String_IndexOfSubstring(Line, S(": multiple definition of `"), true, &Index))
            {
                const String Location = Filesystem_ExtractFileName(StrSlice(Line.Data, Index), true);
                const String Message  = StrShiftF(Line, Index+1);

                LOG_INLINE("    ");
                LOG_INLINE_ERROR("[ERROR] %S", Location);
                LOG(" |%S", Message);

                LastErrorIndent = (u32)Min(Location.Length + 15, 64);
            }
            else if (String_EndsWith(Line, S("first defined here"), true))
            {
                StringLocal(ContextIndent, 64);
                ContextIndent.Length = LastErrorIndent;
                String_Fill(&ContextIndent, ' ');

                LOG_MUTE("%S%S", ContextIndent, Line);
            }
            else if (String_StartsWith(Line, S("warning: "), true))
            {
                LOG_INLINE_WARNING("[WARNING] ");
                LOG("%S", StrShiftF(Line, 9));
            }
            else if (String_StartsWith(Line, S("collect2"), true))
            {
                // "collect2.exe: error: ld returned 1 exit status" - noise; the caller already
                // reports the exit code and we print the reference count below
            }
            else
            {
                LOG("%S", Line);
            }
        }
    }

    // GNU ld has no closing summary like MSVC's "N unresolved externals" - provide one
    if (NumUndefined == 1)
    {
        LOG("\n1 undefined reference");
    }
    else if (NumUndefined > 1)
    {
        LOG("\n%u undefined references", NumUndefined);
    }
}

static void Internal_ParseAndLogLinkerOutput(ECompiler Vendor, String StdOutData)
{
    if (Vendor == Compiler_GCC || Vendor == Compiler_MINGW)
    {
        Internal_ParseAndLogLinkerOutput_GNU(StdOutData);
    }
    else
    {
        Internal_ParseAndLogLinkerOutput_MSVC(StdOutData);
    }
}

static void Internal_ProcessLinkerOutput(ECompiler Vendor, PlatformHandle Process, PlatformPipe StdOutHandle)
{
    Platform_ClosePipeEnd(&StdOutHandle[1]);

    StringLocal(StdOutData, UINT16_MAX);

    bool bLinkerExited = false;
    bool bKeepReading = true;

    while (bKeepReading)
    {
        // The output accumulates (reads land on arbitrary byte boundaries, and the parser needs
        // whole lines) and is parsed in one go at the end. Only a full buffer forces an early
        // parse to make room - that can split a line, but only past 64KB of linker output.
        if (StdOutData.Length == StdOutData.Capacity)
        {
            Internal_ParseAndLogLinkerOutput(Vendor, StdOutData);
            String_Empty(&StdOutData);
        }

        usize BytesRead = 0;
        bKeepReading = Filesystem_ReadPipe(StdOutHandle,
                                           StdOutData.Capacity - StdOutData.Length,
                                           StdOutData.Data + StdOutData.Length,
                                           &BytesRead);

        if (BytesRead > 0)
        {
            StdOutData.Length += (u32)BytesRead;
        }
        else if (bKeepReading)
        {
            if (bLinkerExited)
            {
                bKeepReading = false; // exited and drained - done
            }
            else
            {
                u32 ExitCode = 0;
                bLinkerExited = Platform_GetExitCodeForProcess(Process, &ExitCode);
                if (!bLinkerExited)
                {
                    Platform_Sleep(1); // without this we busy-spin a full core the linker could be using
                }
            }
        }
    }

    Internal_ParseAndLogLinkerOutput(Vendor, StdOutData);

    Platform_ClosePipeEnd(&StdOutHandle[0]);
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
        else if (String_IsEqual(Name, S("egcc"), false)) // OpenBSD ports gcc
        {
            CppName = S("eg++");
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

        String LinkerFlags   = Export_TryWriteFlagsAndReturnThisValue(S("LinkerFlags"),   Params->LinkerFlags);
        String LinkerDefines = Export_TryWriteFlagsAndReturnThisValue(S("LinkerDefines"), Params->LinkerDefineFlags);
        String Libraries     = Export_TryWriteFlagsAndReturnThisValue(S("Libraries"),     Params->Libraries);
        String LibraryPaths  = Export_TryWriteFlagsAndReturnThisValue(S("LibraryPaths"),  Params->LibraryDirectories);

        String LinkerFlagsLeft  = Params->bLinkerFlagsFirst ? LinkerFlags : String_Null();
        String LinkerFlagsRight = Params->bLinkerFlagsFirst ? String_Null() : LinkerFlags;

        String_Append(&CmdLine, LinkerFlagsLeft);
        xx String_EatSpacesInlineFromEnd(&CmdLine);
        String_AppendSpace(&CmdLine);

        String ObjExt  = String_IsValid(Params->CompilerObjectExt) ? Params->CompilerObjectExt : S(".o");
        Internal_AppendObjSourceFiles(Params, &CmdLine, ObjExt);

        // These must come after obj files because on some operating systems
        // the linker is sensitive to the order of how the flags are positioned
        //
        String_BuildSeparator(&CmdLine, ' ', LinkerFlagsRight,
                                             LinkerDefines,
                                             Libraries,
                                             LibraryPaths);

        xx String_EatSpacesInlineFromEnd(&CmdLine);
        String_AppendSpace(&CmdLine);

        String_Concat(&CmdLine, OutputFlag, S(" \""), BuildPath, Params->AssemblyWithExt, S("\""));

        if (Export_IsCapturingCommands())
        {
            StringLocal(EchoText, MAX_PATH_LENGTH + 16);
            String_Format(&EchoText, S("Linking %S"), Params->AssemblyWithExt);

            if (!Export_EmitScriptCommand(EchoText, S("Linker"), ProgramPath, CmdLine))
            {
                return false;
            }
        }
        else
        {
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

    bool bUsingCppDriver = false;
    if (Params->bHasCppFiles && !bIsMicrosoftLinker)
    {
        const String LinkerName = Filesystem_ExtractFileName(Params->LinkerPath, false);
        if (String_EndsWith(LinkerName, S("++"), false))
        {
            // the configured linker already is the C++ driver, nothing to derive
            bUsingCppDriver = true;
        }
        else
        {
            const String CppDriver = Internal_DeriveCppDriverPath(Params->Arena, Params->LinkerPath, Params->CompilerVendor);
            if (String_IsValid(CppDriver))
            {
                ProgramPath = CppDriver;
                bUsingCppDriver = true;
            }
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

        String LinkerFlags   = Export_TryWriteFlagsAndReturnThisValue(S("LinkerFlags"),   Params->LinkerFlags);
        String LinkerDefines = Export_TryWriteFlagsAndReturnThisValue(S("LinkerDefines"), Params->LinkerDefineFlags);
        String Libraries     = Export_TryWriteFlagsAndReturnThisValue(S("Libraries"),     Params->Libraries);
        String LibraryPaths  = Export_TryWriteFlagsAndReturnThisValue(S("LibraryPaths"),  Params->LibraryDirectories);

        String LinkerFlagsLeft  = Params->bLinkerFlagsFirst ? LinkerFlags : String_Null();
        String LinkerFlagsRight = Params->bLinkerFlagsFirst ? String_Null() : LinkerFlags;

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
                                             LinkerDefines,
                                             Libraries,
                                             LibraryPaths);

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

        if (Export_IsCapturingCommands())
        {
            StringLocal(EchoText, MAX_PATH_LENGTH + 16);
            String_Format(&EchoText, S("Linking %S"), Params->AssemblyWithExt);

            if (!Export_EmitScriptCommand(EchoText, S("Linker"), ProgramPath, CmdLine))
            {
                return false;
            }
        }
        else
        {
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
            PlatformPipe StdOutHandle;
            Platform_PipeInit(StdOutHandle);
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
                    Internal_ProcessLinkerOutput(Params->CompilerVendor, Handle, StdOutHandle);
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

        String_BuildSeparator(&CmdLine, ' ', VerboseFlag, Export_TryWriteFlagsAndReturnThisValue(S("ArchiverFlags"), Params->ArchiverFlags));

        xx String_EatSpacesInlineFromEnd(&CmdLine);
        String_AppendSpace(&CmdLine);

        Internal_AppendObjSourceFiles(Params, &CmdLine, DefaultObjExtension);

        if (Export_IsCapturingCommands())
        {
            StringLocal(EchoText, MAX_PATH_LENGTH + 16);
            String_Format(&EchoText, S("Linking %S [static]"), LibFile);

            if (!Export_EmitScriptCommand(EchoText, S("Archiver"), ProgramPath, CmdLine))
            {
                return false;
            }
        }
        else
        {
            StringLocal(LibOutputPath, MAX_PATH_LENGTH);
            String_Append(&LibOutputPath, BuildPath);
            String_Append(&LibOutputPath, LibFile);
            RecordArtifactPath(Params->ArtifactManifestHandle,LibOutputPath);

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

        if (Export_IsCapturingCommands())
        {
            StringLocal(EchoText, MAX_PATH_LENGTH + 16);
            String_Format(&EchoText, S("Generating %S.def"), Params->Assembly);

            if (!Export_EmitScriptCommand(EchoText, S("DumpBin"), Params->DumpBinPath, CmdLine))
            {
                return false;
            }
        }
        else
        {
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
    }
    #endif // PLATFORM_WINDOWS

    return true;
}
