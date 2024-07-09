#pragma once

#include "EngineTypes.h"
#include "String/BaseString.h"
#include "Memory/LinearAllocator.h"

STRUCT(FileVariable)
{
    String Name;
    String Value;
    bool   bHasSpecial;
};

STRUCT(InternalVariable)
{
    String Name;
    String Value;
};

global TArray(InternalVariable) InternalVariablesDB;
global bool bHasWrittenJSON;
global bool bQuietBuild;
global bool bNoWordWrapLogging;

STRUCT(SourceFileData)
{
    String FullPath;
    String RelativePath;
};

STRUCT(CmdOption)
{
    String Name;
    String Value;
    bool bEqualsToSomething;
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

    String Assembly;
    String AssemblyWithExt;
    String Extension;
    String Extension_Og;
    String CompilerProgram;
    String CompilerPath;
    String RCProgram;
    String RCProgramPath;
    String RCProgramFlags;
    String CompilerFlags;
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

    LinearAllocator* Arena;

    #if PLATFORM_WINDOWS
    TArray(void*)* Processes;
    #else
    TArray(i32)* Processes;
    #endif

    u32 NumSources;
    u32 NumHeaders;
    u32 NumRcSources;

    u8 MaxCompilersAtOnce;
    u8 MaxErrors;

    bool bHasRCProgram;
    bool bShouldWaitPerCompileProcess;
    bool bIsAssemblyExe;
    bool bVerbose;
};

STRUCT(CompileData)
{
    bool (*Callback)(CompileData* Data, const String FullPath, const String RelativePath);

    const BuildParams* Params;
    u32* NumCompiled;
    u32 Index;
    bool bSuccess;

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

#if PLATFORM_WINDOWS
bool RC_Compile(const BuildParams* Params, const String FullRCPath, String* OutRestPath);
#endif

bool IsSource(const String Extension);
bool IsHeader(const String Extension);


// Util functions --------------------

bool ExtensionHas(LinearAllocator Scratch, const String ExtensionString, const String Ext);

bool DoesCmdVarExist(TArray(CmdOption) CmdOptionsDB, const String Name);
bool DoesBuildVarExist(TArray(FileVariable) VariablesDB, const String Name);

String GetCmdOptionValue(TArray(CmdOption) CmdOptionsDB, const String Name);
String GetVariableValue(TArray(FileVariable) Variables, const String Name);
String* GetVariableValue_Ref(TArray(FileVariable) Variables, const String Name);
StringList GetVariableValueList(LinearAllocator* Arena, TArray(FileVariable) VariablesDB, const String Name);

bool FilterSourceFile(const String WorkingDirectory, const String SourceDirectory, 
                      const String FullPath, const String RelativePath,
                      StringList WhitelistFiles, StringList BlacklistFiles,
                      StringList WhitelistDirectories, StringList BlacklistDirectories);

bool LogStringList_WordWrapped(LinearAllocator Arena, const String Name, const StringList List);
bool LogString_WordWrapped    (LinearAllocator Arena, const String Name, const String Value, const bool bAddNewLine);

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

bool ExpandBuildVariable(LinearAllocator Scratch, TArray(FileVariable) VariablesDB, TArray(CmdOption) CmdOptionsDB,
                         String* Dest, const String Key, const String Value, const String Root, const String WorkingDirectory,
                         bool bLowerStrings, bool bIsAssemblyExe);


// Export functions --------------------

bool ExportCompileCommands(const BuildParams* Params,
                           const String CompileFlags, const String IncludeFlags,
                           const String DefineFlags, const String UnDefineFlags,
                           const bool bIsLastBuild);


bool ExportInfoPlist(LinearAllocator Arena, const BuildParams* Params, const String Path, TArray(FileVariable) ExpandedVariablesDB, bool bRawMode);
bool ExportVersionPlist(LinearAllocator Arena, const BuildParams* Params, const String Path, TArray(FileVariable) ExpandedVariablesDB, bool bRawMode);
bool ExportPkgInfo(const BuildParams* Params, const String Path);
bool ExportVersionRC(const BuildParams* Params, const String Path);
bool ExportIconRC(const BuildParams* Params, const String Path, const String IconFilePath);

bool SourceFileDirectoryIterator(const String FullPath, const String RelativePath, const String FileName, u64 FileSize, bool bIsDirectory, void* UserData);
