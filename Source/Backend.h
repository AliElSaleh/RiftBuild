#ifndef BACKEND_H
#define BACKEND_H

#ifndef UNITY_BUILD
#include "Core/EngineTypes.h"
#include "Core/Filesystem.h"
#endif

STRUCT(FileVariable)
{
    String Name;
    String Value;
    String SpecialData;
    bool   bHasSpecial;
    u8     Padding[7];
};

STRUCT(InternalVariable)
{
    String Name;
    String Value;
};

global TArray(InternalVariable) InternalVariablesDB;
global bool bQuietBuild;
global bool bNoWordWrapLogging;

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
    bool bHasCppFiles;
    bool bIsPCHBuild;
    u8 Padding[6];
};

STRUCT(CmdOption)
{
    String Name;
    String Value;
    bool   bEqualsToSomething;
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
    AssemblyType_CompilerObject
};

ENUM(ECompiler)
{
    Compiler_Clang,
    Compiler_GCC,
    Compiler_MSVC
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
    String RootDirectory; // absolute
    String SourceDirectory; // relative
    String BuildDirectory; // relative
    String IntermediateDirectory; // relative
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
    String LinkerFlags;
    String IncludeFlags;
    String DefineFlags;
    String UnDefineFlags;
    String LinkerDefineFlags;
    String Libraries;
    String LibraryDirectories;

    String IconResFilePath;
    String VersionResFilePath;

    String TitleName;
    String InternalName;
    String CompanyName;
    String Description;
    String Copyright;
    String Version;

    StringList WhitelistFiles, WhitelistDirectories;
    StringList BlacklistFiles, BlacklistDirectories;
    StringList SourceFileExtensions;

    LinearAllocator* Arena;

    TArray(PlatformHandle)* Processes;
    TArray(PlatformPipe)* Pipes;

    u32 NumSources;
    u32 NumHeaders;
    u32 NumRcSources;

    EAssemblyType Type;

    u8 MaxCompilersAtOnce;
    u8 MaxErrors;

    bool bHasRCProgram;
    bool bShouldWaitPerCompileProcess;
    bool bIsAssemblyExe;
    bool bVerbose;
    bool bHasCppFiles;
    bool bDumpObjFilesInOneDirectory;

    bool bPadding1;
    bool bPadding2;
    bool bPadding3;
};

STRUCT(CompileData)
{
    bool (*Callback)(CompileData* Data, const String FullPath, const String RelativePath);

    const BuildParams* Params;
    u32* NumCompiled;
    u32 Index;
    b32 bSuccess;

    void* AdditionalData;
};

STRUCT(LinkData)
{
    const BuildParams* Params;
    String* CmdLine;
};

// Compiler/Building functions --------------------

bool MSVC_Compile(const BuildParams* Params, u32* OutNumCompiled);
bool MSVC_Link(const BuildParams* Params);
bool C_Compile(const BuildParams* Params, u32* OutNumCompiled);
bool C_Link(const BuildParams* Params);

bool RC_Compile(const BuildParams* Params, const String FullRCPath, String* OutResPath);

bool IsSource(const String Extension);
bool IsCppSource(const String Extension);
bool IsSourceCustom(const String Extension, const StringList CustomExtensions);
bool IsHeader(const String Extension);
bool IsCppHeader(const String Extension);


// Util functions --------------------

bool DoesCmdVarExist(TArray(CmdOption) CmdOptionsDB, const String Name);
bool DoesBuildVarExist(TArray(FileVariable) VariablesDB, const String Name);

String GetCmdOptionValue(TArray(CmdOption) CmdOptionsDB, const String Name);
FileVariable GetVariable(TArray(FileVariable) Variables, const String Name);
String GetVariableValue(TArray(FileVariable) Variables, const String Name);
String* GetVariableValue_Ref(TArray(FileVariable) Variables, const String Name);
StringList GetVariableValueList(LinearAllocator* Arena, TArray(FileVariable) VariablesDB, const String Name);

bool FilterSourceFile(const String WorkingDirectory, const String SourceDirectory, 
                      const String FullPath, const String RelativePath,
                      StringList WhitelistFiles, StringList BlacklistFiles,
                      StringList WhitelistDirectories, StringList BlacklistDirectories);

bool LogStringList_WordWrapped(LinearAllocator Scratch, const String Name, const StringList List);
void LogString_WordWrapped    (LinearAllocator Scratch, const String Name, const String Value, const bool bAddNewLine);

bool LogCustomErrorMessage(TArray(FileVariable) VariablesDB, const String Context, const String Key, const bool bLineBreak);

void LogPathEnvVarTutorialSteps(void);
void LogRegularEnvVarTutorialSteps(void);

// Parsing functions --------------------

bool ParseBuildFile(LinearAllocator* Arena,
                    const FileHandle H,
                    const String BuildFilePath,
                    const String WorkingDirectory,
                    TArray(FileVariable) VariablesDB,
                    TArray(FileVariable) ExpandedVariablesDB,
                    TArray(CmdOption) CmdOptionsDB,
                    TArray(String) Messages,
                    TArray(FileHandle) IncludeFiles,
                    u32* ReturnCode,
                    bool bIsIncludeFile,
                    StringList* Includes,
                    bool bIsAssemblyExe);

bool ParseBuildFileV2(LinearAllocator* Arena,
                    const FileHandle H,
                    const String BuildFilePath,
                    const String WorkingDirectory,
                    TArray(FileVariable) VariablesDB,
                    TArray(FileVariable) ExpandedVariablesDB,
                    TArray(CmdOption) CmdOptionsDB,
                    TArray(String) Messages,
                    TArray(FileHandle) IncludeFiles,
                    u32* ReturnCode,
                    bool bIsIncludeFile,
                    StringList* Includes,
                    bool bIsAssemblyExe);


bool ExpandBuildVariable(LinearAllocator Scratch, TArray(FileVariable) VariablesDB, TArray(CmdOption) CmdOptionsDB,
                         String* Dest, const String Key, const String Value, const String Root, const String WorkingDirectory,
                         bool bLowerStrings, bool bIsAssemblyExe);


// Export functions --------------------

bool Export_CompileCommands(const BuildParams* Params, const bool bIsLastBuild, const bool bKeepOneLine);
bool Export_InfoPlist(LinearAllocator Arena, const BuildParams* Params, const String Path, TArray(FileVariable) ExpandedVariablesDB, bool bRawMode);
bool Export_VersionPlist(LinearAllocator Arena, const BuildParams* Params, const String Path, TArray(FileVariable) ExpandedVariablesDB, bool bRawMode);
bool Export_PkgInfo(const String AssemblyName, const String Path);
bool Export_VersionRC(const BuildParams* Params, const String Path);
bool Export_IconRC(const String Path, const String IconFilePath);

bool Export_License(const String LicenseType, const BuildParams* Params, const String Path);
bool Export_License_BSD2(const BuildParams* Params, const String Path);
bool Export_License_BSD3(const BuildParams* Params, const String Path);
bool Export_License_MIT(const BuildParams* Params, const String Path);
bool Export_License_DoWhatTheFuckYouWantTo(const BuildParams* Params, const String Path);
bool Export_License_TheUnlicense(const String Path);

bool SourceFileDirectoryIterator(const String FullPath, const String RelativePath, const String FileName, u64 FileSize, bool bIsDirectory, void* UserData);

global LinearAllocator GMSVCFindAllocator;
void* MSVC_Find_Allocate(usize Size);
void MSVC_Find_Release(void* Memory);

#endif // _BACKEND_H_
