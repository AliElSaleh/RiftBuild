#pragma once

#include "EngineTypes.h"
#include "String/BaseString.h"
#include "Memory/LinearAllocator.h"

STRUCT(FileVariable)
{
    String Name;
    String Value;
};

STRUCT(SourceFileData)
{
    String FullPath;
    String RelativePath;
};

STRUCT(CmdOption)
{
    String Name;
    String Value;
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

ENUM(ECompiler)
{
    Compiler_Clang,
    Compiler_GCC,
    Compiler_MSVC,
    Compiler_Other
};

STRUCT(BuildParams)
{
    String RootDirectory; // absolute
    String SourceDirectory; // relative
    String BuildDirectory; // relative
    String IntermediateDirectory; // relative
    String IntermediateBaseDirectory; // absolute (it is root + intermediate combined)

    String Assembly;
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

    TArray(SourceFileData*) SourceFiles;
    TArray(SourceFileData) SourceFiles_Unfiltered;
    #if PLATFORM_WINDOWS
    TArray(void*)* Processes;
    #else
    TArray(i32)* Processes;
    #endif

    u8 MaxCompilersAtOnce;
    u8 MaxErrors;

    bool bShouldWaitPerCompileProcess;
    bool bIsAssemblyExe;
};

bool MSVC_Compile(const BuildParams* Params, u32* OutNumCompiled);
bool MSVC_Link(const BuildParams* Params);
bool C_Compile(const BuildParams* Params, u32* OutNumCompiled);
bool C_Link(const BuildParams* Params);

bool C_IsSource(const String Extension);
bool C_IsHeader(const String Extension);


// Utils

bool ExtensionHas(const String ExtensionString, const String Ext);

bool DoesCmdVarExist(const String Name);
bool DoesBuildVarExist(const String Name);

String GetCmdOptionValue(const String Name);
String GetExpandedVariableValue(const String Name);
StringList GetVariableValueList(LinearAllocator* Arena, const String Name);

