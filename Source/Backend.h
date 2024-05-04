#pragma once

#include "EngineTypes.h"
#include "String/BaseString.h"
#include "Memory/LinearAllocator.h"

STRUCT(FileVariable)
{
    String Name;
    String Value;
};

STRUCT(InternalVariable)
{
    String Name;
    String Value;
};

extern TArray(InternalVariable) InternalVariablesDB;

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
};

/*
ENUM_TYPED(ECompiler, u8)
{
    Compiler_Clang,
    Compiler_GCC,
    Compiler_MSVC,
    Compiler_Other
};
*/

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
    String CompilerFlags;
    String LinkerFlags;
    String IncludeFlags;
    String DefineFlags;
    String LinkerDefineFlags;
    String Libraries;
    String LibraryDirectories;

    String IconResFilePath;
    String VersionResFilePath;

    StringList WhitelistFiles, WhitelistDirectories;
    StringList BlacklistFiles, BlacklistDirectories;

    LinearAllocator* Arena;

    TArray(SourceFileData*) SourceFiles;
    TArray(SourceFileData) SourceFiles_Unfiltered;
    
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

    bool bShouldWaitPerCompileProcess;
    bool bIsAssemblyExe;
    bool bVerbose;
};

bool MSVC_Compile(const BuildParams* Params, u32* OutNumCompiled);
bool MSVC_CompileV2(const BuildParams* Params, u32* OutNumCompiled);
bool MSVC_Link(const BuildParams* Params);
bool MSVC_LinkV2(const BuildParams* Params);
bool C_Compile(const BuildParams* Params, u32* OutNumCompiled);
bool C_CompileV2(const BuildParams* Params, u32* OutNumCompiled);
bool C_Link(const BuildParams* Params);
bool C_LinkV2(const BuildParams* Params);


bool IsSource(const String Extension);
bool IsHeader(const String Extension);
bool C_IsSource(const String Extension);
bool C_IsHeader(const String Extension);

// Utils

bool ExtensionHas(const String ExtensionString, const String Ext);

/*
bool DoesCmdVarExist(const String Name);
bool DoesBuildVarExist(const String Name);

String GetCmdOptionValue(const String Name);
String GetExpandedVariableValue(const String Name);
StringList GetVariableValueList(LinearAllocator* Arena, const String Name);
*/

bool DoesCmdVarExist(TArray(CmdOption) CmdOptionsDB, const String Name);
bool DoesBuildVarExist(TArray(FileVariable) VariablesDB, const String Name);

String GetCmdOptionValue(TArray(CmdOption) CmdOptionsDB, const String Name);
String GetExpandedVariableValue(TArray(FileVariable) ExpandedVariablesDB, const String Name);
StringList GetVariableValueList(LinearAllocator* Arena, TArray(FileVariable) VariablesDB, const String Name);

bool FilterSourceFile(const String WorkingDirectory, const String SourceDirectory, 
                      const String FullPath, const String RelativePath,
                      StringList WhitelistFiles, StringList BlacklistFiles,
                      StringList WhitelistDirectories, StringList BlacklistDirectories);

bool LogStringList_WordWrapped(const String Name, const StringList List);
bool LogString_WordWrapped(const String Name, const String Value, const bool bAddNewLine);


bool ParseBuildFile(LinearAllocator* Arena,
                    FileHandle* H,
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

bool ExpandBuildVariable(TArray(FileVariable) VariablesDB, TArray(CmdOption) CmdOptionsDB,
                                  String* Dest, const String Key, const String Value, const String Root, bool bLowerStrings,
                                  bool bIsAssemblyExe);
