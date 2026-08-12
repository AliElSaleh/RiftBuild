#ifndef BACKEND_H
#define BACKEND_H

#ifndef UNITY_BUILD
#include "Core/EngineTypes.h"
#include "Core/Filesystem.h"
#endif

#define MAX_KEY_LENGTH 128

global bool bQuietBuild;
global bool bNoWordWrapLogging;
global bool bHelp;
global bool bOptions;
global bool bVerboseLog;

STRUCT(FileVariable)
{
    String Params;
    String Name;
    String Value;
    String Content; // used for WriteFile/AppendFile commands, otherwise it's empty
};

global FileVariable FileVariable_Empty;

STRUCT(FileVariableList)
{
    FileVariable Var;
    struct FileVariableList* Next;
};

STRUCT(InternalVariable)
{
    String Name;
    String Value;
};

global TArray(InternalVariable) InternalVariablesDB;

STRUCT(SourceFileData)
{
    String FullPath;
    String RelativePath;
};

STRUCT(SourceCountData)
{
    // u64 AssemblyFileTime;
    u32 NumSources;
    u32 NumAsmSources;
    u32 NumHeaders;
    u32 NumRcSources;
    u64 NewestHeaderWriteTime;
    String* FirstSourceFileName;
    String WorkingDirectory;
    String SourceDirectory;
    String IntermediateBaseDirectory;
    String IntermediateDirectory;
    String BuildDirectory;
    StringList WhitelistArray;
    StringList BlacklistArray;
    StringList WhitelistDirArray;
    StringList BlacklistDirArray;
    StringList CustomSourceExtensions;
    StringList* FilteredFiles;
    StringList** FilteredFilesNext;
    LinearAllocator* ArenaForFilterList;
    bool bHasCppFiles;
    bool bIsPCHBuild;
    u8 Padding[6];
};

STRUCT(CmdOption)
{
    String Name;
    String Value;
    bool   bIsBuiltin;
    u8     Padding[7];
};

#define ASSEMBLY_TYPE_LIST(X)                      \
    X(None,                 "None")                \
    X(Executable,           "Executable")          \
    X(StaticLibrary,        "Static Library")      \
    X(DynamicLibrary,       "Shared Library")      \
    X(PCH,                  "Pre Compiled Header") \
    X(CustomCompilerObject, "Compiler Object")     \
    X(NoCompilerObject,     "No Compiler Object")  \
    X(NoAssembly,           "Source Transform")    \
    X(Null,                 "Null")

ENUM(EAssemblyType)
{
    #define X(Name, DisplayName) AssemblyType_##Name,
    ASSEMBLY_TYPE_LIST(X)
    #undef X
};

static const String AssemblyTypeStringTable[] =
{
    #define X(Name, DisplayName) SC(DisplayName),
    ASSEMBLY_TYPE_LIST(X)
    #undef X
};

ENUM(ECompiler)
{
    Compiler_Generic, // an unknown compiler, will be treated the same as clang and gcc
    Compiler_Clang,
    Compiler_Clang_MSVC,
    Compiler_GCC,
    Compiler_MINGW,
    Compiler_MSVC,
    Compiler_TCC
};

ENUM(EAssembler)
{
    Assembler_Generic,
    Assembler_Nasm,
    Assembler_Yasm,
    Assembler_Masm
};

// TODO: implement
ENUM(EResourceCompiler)
{
    ResourceCompiler_Generic,
    ResourceCompiler_RC,
    ResourceCompiler_Windres,
    ResourceCompiler_LLVM_RC
};

ENUM(EComparisonType)
{
    Cmp_None,
    Cmp_Equal,
    Cmp_NotEqual,
    Cmp_GreaterThan,
    Cmp_LessThan,
    Cmp_GreaterThanOrEqual,
    Cmp_LessThanOrEqual,
    Cmp_StartsWith,
    Cmp_EndsWith,
    Cmp_Contains
};

ENUM(EGenerator)
{
    Generator_None,
    Generator_CompileCommandsJSON,
    Generator_VisualStudioSolution,
    Generator_XCodeProject,
    Generator_Plist,
    Generator_PkgInfo,
    Generator_IconRC,
    Generator_VersionRC,
    Generator_Sh,
    Generator_Bat,
    Generator_Unity,
    Generator_DotDesktop,
    Generator_Mime,
    Generator_GNOMEMime,
    Generator_KDEMime
};

ENUM(EBuildMode)
{
    BuildMode_Build,
    BuildMode_Export
};

ENUM(EBuildKeyImpact)
{
    BuildKeyImpact_None = 0,
    BuildKeyImpact_Relink,
    BuildKeyImpact_Recompile
};

// Extra compiler settings that apply to specific source files, declared in the build file either as a
// block:
//     Parse.c
//     {
//         Compiler.Flags   -O0
//         Includes         thirdparty/pcre
//         Defines          PCRE_STATIC
//     }
// or in the flat form "Parse.c.Compiler.Flags -O0". Files are matched by bare filename (case-insensitive,
// extension included) - no paths, no globs. A file gets the union of every block naming it; the values
// here are already expanded/prefixed exactly like their global counterparts, ready for the command line.
STRUCT(FileOverride)
{
    String FileName;
    String CompilerFlags;
    String IncludeFlags;
    String DefineFlags;
    String UnDefineFlags;
};

STRUCT(CompileProcess)
{
    PlatformHandle Handle;
    PlatformPipe   Pipe;
    String         Output;
};

STRUCT(CompileProcessPool)
{
    TArray(CompileProcess) Jobs;
    TArray(String)         FreeBuffers;
};

STRUCT(BuildParams)
{
    String RootDirectory;             // absolute
    String SourceDirectory;           // relative
    String BuildDirectory;            // relative
    String IntermediateDirectory;     // relative
    String IntermediateBaseDirectory; // absolute (it is root + intermediate combined)

    String BuildFileName;

    String PCHPath;
    String PCHHeaderPath;

    String Assembly;
    String AssemblyWithExt;
    String AssemblyPrefix;
    String AssemblyPostfix;
    String Extension;
    String Extension_Og;
    String CompilerProgram;
    String CompilerPath;
    String CompilerObjectExt;
    String CompilerObjectDirectory;
    String LinkerPath;
    String ArchiverPath;
    String ArchiverOutputFlag;
    String DumpBinPath;
    String CompilerOutputFlag;
    String CompilerCompileFlag;
    String AsmProgram;
    String AsmPath;
    String TargetArchString;
    String RCProgram;
    String RCProgramPath;
    String RCProgramFlags;
    String RCIncludeFlags;
    String RCDefineFlags;
    String RCUnDefineFlags;
    String CompilerFlags;
    String AssemblerFlags;
    String AssemblerIncludes;
    String AssemblerDefines;
    String LinkerFlags;
    String ArchiverFlags;
    String IncludeFlags;
    String DefineFlags;
    String UnDefineFlags;
    String LinkerDefineFlags;
    String Libraries;
    String LibraryDirectories;

    String LinkerEntryPoint;
    String LinkerSubsystem;
    String LinkerStack;
    String LinkerOutputFlag;

    String CameFromBuildFile;

    String IconFilePath;

    String TitleName;
    String InternalName;
    String CompanyName;
    String Description;
    String Copyright;
    String Version;

    String RPathOrigin;
    String RPaths;

    String WindowsSDKIncludePath;
    String WindowsSDKLibUmPath;
    String WindowsSDKLibUcrtPath;
    String VisualStudioIncludePath;
    String VisualStudioLibraryPath;

    String Timestamp;

    StringList SourceFiles;

    const FileOverride* FileOverrides; // per-file compiler setting overrides (see FileOverride), or NULL

    TArray(String) ForceRecompileFiles; // bare filenames to recompile even if unchanged (per-file overrides changed), or NULL

    u64 NewestHeaderWriteTime;

    LinearAllocator* Arena;

    CompileProcessPool* Processes;

    FileHandle ArtifactManifestHandle;

    u32 NumSources;
    u32 NumFileOverrides;
    u32 MaxCompilersAtOnce;

    EAssemblyType Type;
    ECompiler CompilerVendor;
    EAssembler AssemblerVendor;

    bool bShouldWaitPerCompileProcess;
    bool bIsAssemblyExe;
    bool bVerbose;
    bool bHasCppFiles;
    bool bDumpObjFilesInOneDirectory;
    bool bLinkerNoStd;
    bool bLinkerNoDefaultLibs;
    bool bCompilerFlagsFirst;
    bool bLinkerFlagsFirst;
    bool bCanLink; // false for codegen modules (custom objects, no_object, no_assembly), they have no link stage

    bool bPadding[7];
};

STRUCT(CompilerPaths)
{
    String CompilerPath;
    String AssemblerPath;
    String LinkerPath;
    String ArchiverPath;
    String InstallPath;
    String ToolPath;
    String BasePath;
    String IncludePath;
    String LibraryPath;
};

// Compiler/Building functions --------------------

bool C_Compile(const BuildParams* Params, u32* OutNumCompiled);
bool C_Compile_Spawn(const BuildParams* Params, u32* OutNumCompiled);
bool C_Compile_Wait(const BuildParams* Params, u32 NumCompiled);
bool C_Link(const BuildParams* Params);

void CompileProcessPool_InitOutputBuffers(LinearAllocator* Arena, CompileProcessPool* Pool);

bool IsSource(const String Extension);
bool IsAsmSource(const String Extension);
bool IsAsmCSource(const String Extension);
bool IsCSource(const String Extension);
bool IsObjCSource(const String Extension);
bool IsCppSource(const String Extension);
bool IsSourceCustom(const String Extension, const StringList CustomExtensions);
bool IsHeader(const String Extension);
bool IsCppHeader(const String Extension);


// Util functions --------------------

bool DoesBuildVarExist(TArray(FileVariable) VariablesDB, const String Name);
bool DoesBuildVarExist_StartingWith(TArray(FileVariable) VariablesDB, const String Name);
CmdOption* FindCmdOption(TArray(CmdOption) CmdOptionsDB, const String Name);
CmdOption* FindCmdOptionOfKind(TArray(CmdOption) CmdOptionsDB, const String Name, bool bIsBuiltin);
bool DoesCmdOptionExist(TArray(CmdOption) CmdOptionsDB, const String Name);

String GetCmdOptionValue(TArray(CmdOption) CmdOptionsDB, const String Name);
FileVariable GetVariable(TArray(FileVariable) Variables, const String Name);
String GetVariableValue(TArray(FileVariable) Variables, const String Name);
String* GetVariableValue_Ref(TArray(FileVariable) Variables, const String Name);
StringList GetVariableValueList(LinearAllocator* Arena, TArray(FileVariable) VariablesDB, const String Name);

bool LogStringList_WordWrapped(LinearAllocator Scratch, const String Name, const StringList List);
void LogString_WordWrapped    (LinearAllocator Scratch, const String Name, const String Value, const bool bAddNewLine);

bool LogCustomErrorMessage(TArray(FileVariable) VariablesDB, const String Context, const String Key, const bool bLineBreak);

void LogPathEnvVarTutorialSteps(void);
void LogRegularEnvVarTutorialSteps(void);

bool FindFirstCompilerAvailable(const String CompilerToFind, const String AssemblerToFind, const String LinkerToFind, const String ArchiverToFind, const String WorkingPath, bool bTarget32Bit, CompilerPaths* OutCompilerPaths);
ECompiler DetermineCompilerVendor(String CompilerPath);
EAssembler DetermineAssemblerVendor(String CompilerPath);

// Parsing functions --------------------

STRUCT(IncludeFile)
{
    FileHandle Handle;
    String     Path;
};

STRUCT(ParsingContext)
{
    LinearAllocator*     PermanentArena;
    LinearAllocator*     TempArena;
    TArray(FileVariable) VariablesDB;
    TArray(CmdOption)    CmdOptionsDB;
    TArray(String)       Messages;
    TArray(IncludeFile)  IncludeFiles;
    FileVariableList*    VarListHead;
    FileVariableList**   VarListTail;
    String               WorkingDirectory;
    bool                 bNoFail;
    u8                   Level;
    u8                   Padding[6];
};

NO_DISCARD bool ParseBuildFile(
                    const FileHandle H,
                    const String BuildFilePath,
                    ParsingContext Context,
                    bool bIsIncludeFile);

// Called for every dependency path recorded in a compiler-written dependency file.
typedef void (*DependencyPathIterator)(const String Path, void* UserData);

NO_DISCARD bool ParseDependencyFile_Makefile(const String DepFilePath, DependencyPathIterator Iterator, void* UserData, bool* bOutParsedOk);
NO_DISCARD bool ParseDependencyFile_MSVCJson(const String DepFilePath, DependencyPathIterator Iterator, void* UserData, bool* bOutParsedOk);

bool ExpandVariable(LinearAllocator Scratch, FileVariableList* VariablesDB, TArray(CmdOption) CmdOptionsDB,
                    String* Dest, const String Key, const String Value, const String Root, const String WorkingDirectory,
                    bool* bFailed);

f64 ConsumeCommandExpansionTime(void);

u32 GetMaxValueLengthForReservedKey(const String Key);
EBuildKeyImpact GetBuildKeyImpact(const String Key);

bool IsPerFileOverrideKey(const String Key, String* OutFileName, String* OutSetting);
bool IsPlistEntryKey(const String Key);

void AddVariable(LinearAllocator* Arena,
                TArray(FileVariable) VariablesDB,
                const String Name,
                const String Value,
                const String Params,
                const String Content,
                u32 MaxValueLength);

void AddOrAppendVariable(LinearAllocator* Arena,
                        TArray(FileVariable) VariablesDB,
                        const String Name,
                        const String Value,
                        const String Params,
                        u32 MaxValueLength);

void AddCmdOption(TArray(CmdOption) CmdOptionsDB, const String Name, const String Value);
void AddBuiltinOption(TArray(CmdOption) CmdOptionsDB, const String Name, const String Value);
void AddInternalVariable(const String Name, const String Value);
InternalVariable* FindInternalVariable(const String Name);

String AssemblyTypeStringToExtension(String Type);
bool AssemblyTypeStringIsExecutable(String Type);
EAssemblyType StringToAssemblyTypeEnum(String Type);
bool ExtensionStringIsExecutable(String Ext);
bool ExtensionStringIsSharedLibrary(String Ext);
bool ExtensionStringIsStaticLibrary(String Ext);
bool ExtensionStringIsPCH(String Ext);

// Export functions --------------------

bool Export_IsCapturingCommands(void);
bool Export_EmitScriptCommand(const String EchoText, const String ToolVarName, const String ProgramPath, const String CmdLine);
String Export_TryWriteFlagsAndReturnThisValue(const String Name, const String Value);

//bool Export(EExportType Type, LinearAllocator Scratch, const BuildParams* Params, const String OutputPath, ExportMetaData MetaData);

bool Export_CompileCommands(const BuildParams* Params, const bool bIsLastBuild, const bool bKeepOneLine);
bool Export_InfoPlist(LinearAllocator Arena, const BuildParams* Params, const String Path, TArray(FileVariable) ExpandedVariablesDB, bool bRawMode);
bool Export_VersionPlist(LinearAllocator Arena, const BuildParams* Params, const String Path, TArray(FileVariable) ExpandedVariablesDB, bool bRawMode);
bool Export_PkgInfo(const String AssemblyName, const String Path);
bool Export_VersionRC(const BuildParams* Params, const String Path);
bool Export_IconRC(const String Path, const String IconFilePath);
bool Export_WindowsBatchScript(const BuildParams* Params);
bool Export_UnixShellScript(const BuildParams* Params);
bool Export_VisualStudioSolution(const BuildParams* Params, const String Path);
bool Export_VisualStudioProject(const BuildParams* Params, const String Path);

// LicenseType: BSD2, BSD3, MIT, FuckYou, Unlicense
bool Export_License(const String LicenseType, const BuildParams* Params, const String OutputPath);

bool Export_FromArg(LinearAllocator Scratch, const BuildParams* Params, const String Arg, TArray(FileVariable) ExpandedVariablesDB);
void Export_PrintAvailableTypes(void);

#if PLATFORM_LINUX || PLATFORM_BSD
bool TryBuildOrCleanUnixExeIcon(String IconFilePath, const BuildParams* Params);
#endif

#if PLATFORM_APPLE
bool TryBuildOrCleanMacExeIcon(String IconFilePath, const BuildParams* Params);
bool TryBuildMacBundle(LinearAllocator Scratch, const BuildParams* Params, TArray(FileVariable) VariablesDB);
#endif

void RecordArtifactPath(const FileHandle ManifestHandle, const String Path);
void RecordSkippedLinkArtifacts(const BuildParams* Params);

#endif // _BACKEND_H_
