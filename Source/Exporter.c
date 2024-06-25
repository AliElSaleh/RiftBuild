// Copyright (c) 2024 Ali El Saleh

#include "Backend.h"

#include "Platform/Platform.h"
#include "Platform/Filesystem.h"
#include "String/StringUtils.h"
#include "Structures/Array.h"
#include "Uuid.h"
#include "Log.h"

bool bHasWrittenJSON = false;

STRUCT(ExportData)
{
    FileHandle File;
    bool bIsLastBuild;
};

internal void WriteFlags(LinearAllocator Scratch, const FileHandle File, const String Flags, bool bConvertSlashes)
{
    u16 i = 0;
    StringArray List = String_ParseIntoArray(&Scratch, Flags, ' ', 0, 256);
    if (List.Num > 0)
    {
        Filesystem_WriteLine(File, S(",\n"), NULL);
    }

    for each_str_i (i, Flag, List)
    {
        StringLocal(FlagCopy, 4096);
        String_Copy(&FlagCopy, *Flag);
        if (bConvertSlashes)
            String_BackSlashToForwardSlash(&FlagCopy);

        String Comma = i != List.Num-1 ? S(",\n") : S("");
        Filesystem_WriteLineFormatted(File, S("            \"%S\"%S"), NULL, FlagCopy, Comma);
    }
}

internal bool GenCommandObject(CompileData* Data, const String FullPath, const String RelativePath)
{
    const ExportData* ExportData = Data->AdditionalData;
    const BuildParams* Params = Data->Params;

    String AdditionalPlatformFlags = String_Null();

    #if PLATFORM_UNIX
    if (String_IsEqual(Params->Extension, S(".so"), false) ||
        String_IsEqual(Params->Extension, S(".dylib"), false) ||
        String_IsEqual(Params->Extension, S(".a"), false))
    {
        AdditionalPlatformFlags = S("-fPIC -fvisibility=default");
    }
    else if (Params->bIsAssemblyExe)
    {
        AdditionalPlatformFlags = S("-fPIE");
    }
    #endif

    StringLocal(RootDirectory, MAX_PATH_LENGTH);
    String_Copy(&RootDirectory, Params->RootDirectory);
    String_BackSlashToForwardSlash(&RootDirectory);

    StringLocal(RelativePathCopy, MAX_PATH_LENGTH);
    String_BuildPath(&RelativePathCopy, Params->SourceDirectory, RelativePath);
    String_BackSlashToForwardSlash(&RelativePathCopy);

    Filesystem_WriteLine         (ExportData->File, S("    {\n"), NULL);
    Filesystem_WriteLineFormatted(ExportData->File, S("        \"directory\": \"%S\",\n"), NULL, RootDirectory);
    Filesystem_WriteLineFormatted(ExportData->File, S("        \"file\": \"%S\",\n"), NULL, RelativePathCopy);
    Filesystem_WriteLine         (ExportData->File, S("        \"arguments\": [\n"), NULL);
    Filesystem_WriteLineFormatted(ExportData->File, S("            \"-c\""), NULL);
    
    WriteFlags(*Params->Arena, ExportData->File, Params->CompilerFlags, false);
    WriteFlags(*Params->Arena, ExportData->File, AdditionalPlatformFlags, false);
    WriteFlags(*Params->Arena, ExportData->File, Params->IncludeFlags, true);
    WriteFlags(*Params->Arena, ExportData->File, Params->DefineFlags, false);
    WriteFlags(*Params->Arena, ExportData->File, Params->UnDefineFlags, false);
    
    Filesystem_WriteLine         (ExportData->File, S("\n        ]\n"), NULL);
    Filesystem_WriteLineFormatted(ExportData->File, S("    }%S"), NULL, Data->Index != Params->NumSources-1 || !ExportData->bIsLastBuild ? S(",\n") : S("\n"));

    Data->Index++;
    
    return true;
}

bool ExportCompileCommands(const BuildParams* Params,
                           const String CompileFlags, const String IncludeFlags,
                           const String DefineFlags, const String UnDefineFlags,
                           const bool bIsLastBuild)
{
    if (NEVER(Params == NULL)) return false;

    FileHandle f = FileHandle_Null();
    if (Filesystem_Open(S("compile_commands.json"), !bHasWrittenJSON ? FileMode_Write : FileMode_Read|FileMode_Write, &f))
    {
        if (!bHasWrittenJSON) Filesystem_WriteLine(f, S("[\n"), NULL);

        ExportData Data = { f, bIsLastBuild };
        CompileData UserData = { GenCommandObject, Params, NULL, 0, true, &Data};
        Filesystem_IterateDirectory_Ex(Params->SourceDirectory, SourceFileDirectoryIterator, true, &UserData);

        if (bIsLastBuild) Filesystem_WriteLine(f, S("]\n"), NULL);

        bHasWrittenJSON = true;

        Filesystem_Close(&f);
        return true;
    }

    return false;
}

// -----------------------------------------------------------
// Visual Studio Solution Generator

/*
bool GenerateSolutionFile(const String ProjectName, const String ProjectPath)
{
    FileHandle file = FileHandle_Null();
    if (!Filesystem_Open(S("solution.sln"), FileMode_Write, &file))
    {
        LOG("Failed to create solution file");
        return false;
    }

    Filesystem_WriteLine(&file, S("Microsoft Visual Studio Solution File, Format Version 12.00\n"), NULL);
    Filesystem_WriteLine(&file, S("# Visual Studio 15\n"), NULL);
    Filesystem_WriteLine(&file, S("VisualStudioVersion = 15.0.28010.2046\n"), NULL);
    Filesystem_WriteLine(&file, S("MinimumVisualStudioVersion = 10.0.40219.1\n"), NULL);
    Filesystem_WriteLine(&file, S("Project(\"{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}\") = \"%S\", \"%S\", \"{GUID}\"\n", ProjectName, ProjectPath), NULL);
    Filesystem_WriteLine(&file, S("EndProject\n"), NULL);
    Filesystem_WriteLine(&file, S("Global\n"), NULL);
    Filesystem_WriteLine(&file, S("    GlobalSection(SolutionProperties) = preSolution\n"), NULL);
    Filesystem_WriteLine(&file, S("        HideSolutionNode = FALSE\n"), NULL);
    Filesystem_WriteLine(&file, S("    EndGlobalSection\n"), NULL);
    Filesystem_WriteLine(&file, S("    GlobalSection(ProjectConfigurationPlatforms) = postSolution\n"), NULL);
    Filesystem_WriteLine(&file, S("        {GUID}.Debug|Win32.ActiveCfg = Debug|Win32\n"), NULL);
    Filesystem_WriteLine(&file, S("        {GUID}.Debug|Win32.Build.0 = Debug|Win32\n"), NULL);
    Filesystem_WriteLine(&file, S("        {GUID}.Release|Win32.ActiveCfg = Release|Win32\n"), NULL);
    Filesystem_WriteLine(&file, S("        {GUID}.Release|Win32.Build.0 = Release|Win32\n"), NULL);
    Filesystem_WriteLine(&file, S("    EndGlobalSection\n"), NULL);
    Filesystem_WriteLine(&file, S("EndGlobal\n"), NULL);

    Filesystem_Close(&file);

    return true;
}
*/
