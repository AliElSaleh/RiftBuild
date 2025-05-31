#ifndef BACKEND_H
#define BACKEND_H

#ifndef UNITY_BUILD
#include "Core/EngineTypes.h"
#include "Core/Filesystem.h"
#endif

#define MAX_KEY_LENGTH 64
#define MAX_VALUE_LENGTH 8192
#define MAX_META_KEY_LENGTH 64

global bool bQuietBuild;
global bool bNoWordWrapLogging;
global bool bHelp;
global bool bOptions;

STRUCT(FileVariable)
{
    String Params;
    String Name;
    String Value;
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
    u32 NumSources;
    u32 NumAsmSources;
    u32 NumHeaders;
    u32 NumRcSources;
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
    bool   bEqualsToSomething; // can i get rid of this??
    u8     Padding[7];
};

ENUM(EAssemblyType)
{
    AssemblyType_None,
    AssemblyType_Executable,
    AssemblyType_Library,
    AssemblyType_StaticLibrary,
    AssemblyType_DynamicLibrary,
    AssemblyType_PCH,
    AssemblyType_CustomCompilerObject
};

ENUM(ECompiler)
{
    Compiler_Clang,
    Compiler_GCC,
    Compiler_MSVC
};

// TODO: delete
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
    Cmp_Contains,
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
    Generator_KDEMime,
};

ENUM(EBuildMode)
{
    BuildMode_Build,
    BuildMode_Export,
};

STRUCT(BuildParams)
{
    String RootDirectory;             // absolute
    String SourceDirectory;           // relative
    String BuildDirectory;            // relative
    String IntermediateDirectory;     // relative
    String IntermediateBaseDirectory; // absolute (it is root + intermediate combined)

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
    String LinkerPath;
    String ArchiverPath;
    String DumpBinPath;
    String CompilerOutputFlag;
    String AsmProgram;
    String AsmPath;
    String RCProgram;
    String RCProgramPath;
    String RCProgramFlags;
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

    String CameFromBuildFile;

    String IconFilePath;
    String IconResFilePath;
    String VersionResFilePath;

    String TitleName;
    String InternalName;
    String CompanyName;
    String Description;
    String Copyright;
    String Version;

    String RPath;

    String WindowsSDKIncludePath;
    String WindowsSDKLibUmPath;
    String WindowsSDKLibUcrtPath;
    String VisualStudioIncludePath;
    String VisualStudioLibraryPath;

    StringList SourceFiles;

    LinearAllocator* Arena;

    TArray(PlatformHandle)* Processes;

    u32 NumSources;

    EAssemblyType Type;

    u8 MaxCompilersAtOnce;

    bool bHasRCProgram;
    bool bShouldWaitPerCompileProcess;
    bool bIsAssemblyExe;
    bool bVerbose;
    bool bHasCppFiles;
    bool bDumpObjFilesInOneDirectory;
    bool bLinkerNoStd;
    bool bLinkerNoDefaultLibs;
    bool bWasVCVarsBatchRan;
    bool _bPadding;
};

/*
ENUM(EExportType)
{
    ExportType_None,
    ExportType_CompileCommands,
    ExportType_InfoPList,
    ExportType_VersionPList,
    ExportType_PkgInfo,
    ExportType_VersionRC,
    ExportType_IconRC,
    ExportType_License
};

STRUCT(ExportMetaData)
{
    String StringParam_1;
    String StringParam_2;

    // for Export_*PList
    TArray(FileVariable) ExpandedVariablesDB;
    u32 bRawMode     : 1;

    // for Export_CompileCommands
    u32 bIsLastBuild : 1;
    u32 bKeepOneLine : 1;
};
*/

// Compiler/Building functions --------------------

bool C_Compile(const BuildParams* Params, u32* OutNumCompiled);
bool C_Link(const BuildParams* Params);

bool RC_Compile(const BuildParams* Params, const String FullRCPath, String* OutResPath);

bool IsSource(const String Extension);
bool IsAsmSource(const String Extension);
bool IsCSource(const String Extension);
bool IsObjCSource(const String Extension);
bool IsCppSource(const String Extension);
bool IsSourceCustom(const String Extension, const StringList CustomExtensions);
bool IsHeader(const String Extension);
bool IsCppHeader(const String Extension);


// Util functions --------------------

bool DoesBuildVarExist(TArray(FileVariable) VariablesDB, const String Name);
bool DoesCmdOptionExist(TArray(CmdOption) CmdOptionsDB, const String Name);

String GetCmdOptionValue(TArray(CmdOption) CmdOptionsDB, const String Name);
FileVariable GetVariable(TArray(FileVariable) Variables, const String Name);
String GetVariableValue(TArray(FileVariable) Variables, const String Name);
String* GetVariableValue_Ref(TArray(FileVariable) Variables, const String Name);
StringList GetVariableValueList(LinearAllocator* Arena, TArray(FileVariable) VariablesDB, const String Name);

bool LogStringList_WordWrapped(LinearAllocator Scratch, const String Name, const StringList List);
void LogString_WordWrapped    (LinearAllocator Scratch, const String Name, const String Value, const bool bAddNewLine);

bool LogCustomErrorMessage(TArray(FileVariable) VariablesDB, const String Context, const String Key, const bool bLineBreak);

//void LogPathEnvVarTutorialSteps(void);
void LogRegularEnvVarTutorialSteps(void);

// Parsing functions --------------------

STRUCT(ParsingContext)
{
    LinearAllocator*     TempArena;
    TArray(FileVariable) VariablesDB;
    TArray(CmdOption)    CmdOptionsDB;
    TArray(String)       Messages;
    TArray(FileHandle)   IncludeFiles;
    FileVariableList*    VarListHead;
    FileVariableList**   VarListTail;
    String               WorkingDirectory;
    bool                 bNoFail;
    bool                 bIgnoreDefaultOptions;
    u8                   Level;
    u8                   Padding[5];
};

NO_DISCARD bool ParseBuildFileV2(LinearAllocator* PermanentArena,
                    const FileHandle H,
                    const String BuildFilePath,
                    ParsingContext Context,
                    bool bIsIncludeFile,
                    StringList* Includes);

bool ExpandBuildVariableV2(LinearAllocator Scratch, FileVariableList* VariablesDB, TArray(CmdOption) CmdOptionsDB,
                         String* Dest, const String Key, const String Value, const String Root, const String WorkingDirectory,
                         bool bLowerStrings, bool bIsAssemblyExe, bool* bFailed);

void AddVariable(LinearAllocator* Arena,
                TArray(FileVariable) VariablesDB,
                const String Name,
                const String Value,
                const String Params);

void AddOrAppendVariable(LinearAllocator* Arena,
                        TArray(FileVariable) VariablesDB,
                        const String Name,
                        const String Value,
                        const String Params);

void AddCmdOption(TArray(CmdOption)* CmdOptionsDB, const String Name, const String Value);
void AddInternalVariable(const String Name, const String Value);

// Export functions --------------------

//bool Export(EExportType Type, LinearAllocator Scratch, const BuildParams* Params, const String OutputPath, ExportMetaData MetaData);

bool Export_CompileCommands(const BuildParams* Params, const bool bIsLastBuild, const bool bKeepOneLine);
bool Export_InfoPlist(LinearAllocator Arena, const BuildParams* Params, const String Path, TArray(FileVariable) ExpandedVariablesDB, bool bRawMode);
bool Export_VersionPlist(LinearAllocator Arena, const BuildParams* Params, const String Path, TArray(FileVariable) ExpandedVariablesDB, bool bRawMode);
bool Export_PkgInfo(const String AssemblyName, const String Path);
bool Export_VersionRC(const BuildParams* Params, const String Path);
bool Export_IconRC(const String Path, const String IconFilePath);

// LicenseType: BSD2, BSD3, MIT, FuckYou, Unlicense
bool Export_License(const String LicenseType, const BuildParams* Params, const String Path);

bool Export_FromArg(LinearAllocator Scratch, const BuildParams* Params, const String Arg, TArray(FileVariable) ExpandedVariablesDB);

global LinearAllocator GMSVCFindAllocator;
void* MSVC_Find_Allocate(usize Size);
void  MSVC_Find_Release(void* Memory);

#endif // _BACKEND_H_
