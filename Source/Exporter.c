// Copyright (c) Artisan Softworks
// Licensed under the BSD 3-Clause License. See the LICENSE file for details.

#ifndef UNITY_BUILD
#include "Backend.h"

#include "Core/Allocators.h"
#include "Core/StringUtils.h"
#include "Core/Array.h"
#include "Core/Clock.h"
#include "Core/Log.h"
#endif


// IDEAS: export pkg-config. better than shitty meson
/*
#
# Generate pkg-config .pc file
#
pkg_mod = import('pkgconfig')
pkg_mod.generate(libraries: libdav1d,
    version: meson.project_version(),
    name: 'libdav1d',
    filebase: 'dav1d',
    description: 'AV1 decoding library'
)
*/

STRUCT(ExportData)
{
    FileHandle File;

    bool bIsLastBuild;
    bool bKeepOneLine;

    u8 Padding[6];
};

static void WriteFlags(LinearAllocator Scratch, const FileHandle File, const String Flags, bool bConvertSlashes, bool bOneLine)
{
    u16 i = 0;
    StringArray List = String_ParseIntoArray(&Scratch, Flags, ' ', 0, 256);
    if (List.Num > 0)
    {
        Filesystem_WriteLine(File, bOneLine ? S(", ") : S(",\n"), NULL);
    }

    for each_str_i (i, Flag, List)
    {
        StringLocal(FlagCopy, 4096);
        String_Copy(&FlagCopy, *Flag);

        if (bConvertSlashes)
        {
            String_BackSlashToForwardSlash(&FlagCopy);
        }

        String Comma = i != List.Num-1 ? (bOneLine ? S(", ") : S(",\n")) : S("");
        Filesystem_WriteLineFormatted(File, S("%S\"%S\"%S"), NULL, bOneLine ? S("") : S("            "), FlagCopy, Comma);
    }
}

static bool Internal_GenCommandObject(CompileData* Data, const String FullPath, const String RelativePath)
{
    UNUSED_PARAM(FullPath);

    const ExportData* Export = Data->AdditionalData;
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
    else
    {
        // no action required
    }
    #endif

    StringLocal(RootDirectory, MAX_PATH_LENGTH);
    String_Copy(&RootDirectory, Params->RootDirectory);
    String_BackSlashToForwardSlash(&RootDirectory);

    StringLocal(RelativePathCopy, MAX_PATH_LENGTH);
    String_BuildPath(&RelativePathCopy, Params->SourceDirectory, RelativePath);
    String_BackSlashToForwardSlash(&RelativePathCopy);

    StringLocal(CompilerPathCopy, MAX_PATH_LENGTH);
    String_Copy(&CompilerPathCopy, Params->CompilerPath);
    String_BackSlashToForwardSlash(&CompilerPathCopy);

    Filesystem_WriteLine         (Export->File, S("    {\n"), NULL);
    Filesystem_WriteLineFormatted(Export->File, S("        \"directory\": \"%S\",\n"), NULL, RootDirectory);
    Filesystem_WriteLineFormatted(Export->File, S("        \"file\": \"%S\",\n"), NULL, RelativePathCopy);
    Filesystem_WriteLineFormatted(Export->File, S("        \"arguments\": [%S"), NULL, Export->bKeepOneLine ? S("") : S("\n"));
    Filesystem_WriteLineFormatted(Export->File, S("%S\"%S\", \"-c\""), NULL, Export->bKeepOneLine ? S(" ") : S("            "), CompilerPathCopy);
    
    WriteFlags(*Params->Arena, Export->File, Params->CompilerFlags, false, Export->bKeepOneLine);
    WriteFlags(*Params->Arena, Export->File, AdditionalPlatformFlags, false, Export->bKeepOneLine);
    WriteFlags(*Params->Arena, Export->File, Params->IncludeFlags, true, Export->bKeepOneLine);
    WriteFlags(*Params->Arena, Export->File, Params->DefineFlags, false, Export->bKeepOneLine);
    WriteFlags(*Params->Arena, Export->File, Params->UnDefineFlags, false, Export->bKeepOneLine);
    
    Filesystem_WriteLineFormatted(Export->File, S("%S]\n"), NULL, Export->bKeepOneLine ? S(" ") : S("\n        "));
    Filesystem_WriteLineFormatted(Export->File, S("    }%S"), NULL, Data->Index != Params->NumSources-1 || !Export->bIsLastBuild ? S(",\n") : S("\n"));

    Data->Index++;
    
    return true;
}

bool Export_CompileCommands(const BuildParams* Params, const bool bIsLastBuild, const bool bKeepOneLine)
{
    bool bSuccess = false;

    if (Params != NULL)
    {
        static bool bHasWrittenJSON = false;

        FileHandle f = FileHandle_Null();
        EFileMode FileMode = !bHasWrittenJSON ? FileMode_Write : FileMode_Read|FileMode_Write;
        if (Filesystem_Open(S("compile_commands.json"), FileMode, &f))
        {
            if (!bHasWrittenJSON) { Filesystem_WriteLine(f, S("[\n"), NULL); }

            ExportData Data = {0};
            Data.File = f;
            Data.bIsLastBuild = bIsLastBuild;
            Data.bKeepOneLine = bKeepOneLine;

            CompileData UserData = { &Internal_GenCommandObject, Params, NULL, 0, true, &Data};
            Filesystem_IterateDirectory_Ex(Params->SourceDirectory, &SourceFileDirectoryIterator, true, &UserData);

            if (bIsLastBuild) { Filesystem_WriteLine(f, S("]\n"), NULL); }

            bHasWrittenJSON = true;
            bSuccess = true;

            Filesystem_Close(&f);
        }
    }

    return bSuccess;
}

static void Internal_PlistWrite(LinearAllocator Scratch, const FileHandle f, const String Key, const String Value)
{
    Filesystem_WriteLineFormatted(f, S("    <key>%S</key>\n"), NULL, Key);
    
    // determine if it's a string, array or an integer
    if (String_StartsWith(Value, S("("), false) &&
        String_EndsWith(Value, S(")"), false))
    {
        Filesystem_WriteLine(f, S("    <array>\n"), NULL);

        {
            StringArray Values = String_ParseIntoArray(&Scratch, StrSlice(Value.Data+1, Value.Length-2), ' ', 0, 128);
            for each_str (e, Values)
            {
                String Slice = String_EatSpaces(String_EatSpacesFromEnd(*e));
                if (Slice.Length > 0)
                {
                    if (String_IsInteger32(Slice))
                    {
                        Filesystem_WriteLineFormatted(f, S("        <integer>%S</integer>\n"), NULL, Slice);
                    }
                    else
                    {
                        Filesystem_WriteLineFormatted(f, S("        <string>%S</string>\n"), NULL, Slice);
                    }
                }
            }
        }

        Filesystem_WriteLine(f, S("    </array>\n"), NULL);
    }
    else if (String_IsInteger32(Value))
    {
        Filesystem_WriteLineFormatted(f, S("    <integer>%S</integer>\n"), NULL, Value);
    }
    else
    {
        Filesystem_WriteLineFormatted(f, S("    <string>%S</string>\n"), NULL, Value);
    }
}

bool Export_InfoPlist(LinearAllocator Arena, const BuildParams* Params, const String Path, TArray(FileVariable) ExpandedVariablesDB, bool bRawMode)
{
    if (NEVER(Params == NULL)) { return false; }
    if (NEVER(ExpandedVariablesDB == NULL)) { return false; }

    bool bSuccess = false;

    FileHandle f = FileHandle_Null();
    if (Filesystem_Open(Path, FileMode_Write, &f))
    {
        Filesystem_WriteLine(f, S("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"), NULL);
        Filesystem_WriteLine(f, S("<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"), NULL);
        Filesystem_WriteLine(f, S("<plist version=\"1.0\">\n"), NULL);
        Filesystem_WriteLine(f, S("<dict>\n"), NULL);

        if (bRawMode)
        {
            const String RawValue = GetVariableValue(ExpandedVariablesDB, S("Info.plist"));

            if (RawValue.Length > 0)
            {
                Filesystem_WriteLine(f, RawValue, NULL);
                Filesystem_WriteLine(f, S("\n"), NULL);
            }
        }
        else
        {
            StringLocal(CompanyNameNoSpaces, 128);
            String_StripWhitespace(Params->CompanyName, &CompanyNameNoSpaces);

            if (CompanyNameNoSpaces.Length == 0)
            {
                CompanyNameNoSpaces = S("Unknown");
            }

            const String DisplayName = Params->TitleName.Length == 0 ? Params->Assembly : Params->TitleName;
            const String Version = Params->Version.Length == 0 ? S("1.0.0") : Params->Version;

            StringLocal(BundleIdentifer, 128);
            String_Append(&BundleIdentifer, S("com."));
            String_Append(&BundleIdentifer, CompanyNameNoSpaces);
            String_AppendChar(&BundleIdentifer, '.');
            String_Append(&BundleIdentifer, Params->Assembly);

            StringLocal(VersionLong, 128);
            String_Append(&VersionLong, S("Version "));
            String_Append(&VersionLong, Version);

            STRUCT(BundleTableEntry)
            {
                String Key;
                String Value;
                b64    bGiven;
            };

            BundleTableEntry BundleTable[12] = 
            {
                { .Key = S("CFBundleDevelopmentRegion"),     .Value = S("English"),        .bGiven = false },
                { .Key = S("CFBundleDisplayName"),           .Value = DisplayName,         .bGiven = false },
                { .Key = S("CFBundleExecutable"),            .Value = Params->Assembly,    .bGiven = false },
                { .Key = S("CFBundleGetInfoString"),         .Value = Params->Description, .bGiven = false },
                { .Key = S("CFBundleIconFile"),              .Value = Params->Assembly,    .bGiven = false },
                { .Key = S("CFBundleIdentifier"),            .Value = BundleIdentifer,     .bGiven = false },
                { .Key = S("CFBundleInfoDictionaryVersion"), .Value = S("6.0"),            .bGiven = false },
                { .Key = S("CFBundleName"),                  .Value = Params->Assembly,    .bGiven = false },
                { .Key = S("CFBundlePackageType"),           .Value = S("APPL"),           .bGiven = false },
                { .Key = S("CFBundleShortVersionString"),    .Value = Version,             .bGiven = false },
                { .Key = S("CFBundleSignature"),             .Value = Params->Assembly,    .bGiven = false },
                { .Key = S("CFBundleVersion"),               .Value = VersionLong,         .bGiven = false }
            };

            for each (FileVariable, v, ExpandedVariablesDB)
            {
                // TODO: change :: to .
                if (String_StartsWith(v.Name, S("Info.plist::"), false))
                {
                    const String Key = StrShiftF(v.Name, 12);

                    // mark the key as "given" in the table
                    for (u8 i = 0; i < SArray_Capacity(BundleTable); i++)
                    {
                        if (String_IsEqual(BundleTable[i].Key, Key, false))
                        {
                            BundleTable[i].bGiven = true;
                            break;
                        }
                    }

                    Internal_PlistWrite(Arena, f, Key, v.Value);
                }
            }

            for (u8 i = 0; i < SArray_Capacity(BundleTable); i++)
            {
                if (!BundleTable[i].bGiven)
                {
                    Internal_PlistWrite(Arena, f, BundleTable[i].Key, BundleTable[i].Value);
                }
            }
        }

        Filesystem_WriteLine(f, S("</dict>\n"), NULL);
        Filesystem_WriteLine(f, S("</plist>\n"), NULL);

        Filesystem_Close(&f);

        bSuccess = true;
    }

    return bSuccess;
}

bool Export_VersionPlist(LinearAllocator Arena, const BuildParams* Params, const String Path, TArray(FileVariable) ExpandedVariablesDB, bool bRawMode)
{
    if (NEVER(Params == NULL)) { return false; }
    if (NEVER(ExpandedVariablesDB == NULL)) { return false; }

    bool bSuccess = false;

    FileHandle f = {0};
    if (Filesystem_Open(Path, FileMode_Write, &f))
    {
        Filesystem_WriteLine(f, S("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"), NULL);
        Filesystem_WriteLine(f, S("<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"), NULL);
        Filesystem_WriteLine(f, S("<plist version=\"1.0\">\n"), NULL);
        Filesystem_WriteLine(f, S("<dict>\n"), NULL);

        if (bRawMode)
        {
            const String RawValue = GetVariableValue(ExpandedVariablesDB, S("Version.plist"));

            if (RawValue.Length > 0)
            {
                Filesystem_WriteLine(f, RawValue, NULL);
                Filesystem_WriteLine(f, S("\n"), NULL);
            }
        }
        else
        {
            STRUCT(BundleTableEntry)
            {
                String Key;
                String Value;
                b64    bGiven;
            };

            const String DisplayName = Params->TitleName.Length == 0 ? Params->Assembly : Params->TitleName;
            const String Version     = Params->Version.Length == 0 ? S("1.0.0") : Params->Version;

            BundleTableEntry BundleTable[4] = 
            {
                [0] = { .Key = S("BuildVersion"),                  .Value = S("1"),      .bGiven = false },
                [1] = { .Key = S("CFBundleShortVersionString"),    .Value = Version,     .bGiven = false },
                [2] = { .Key = S("CFBundleVersion"),               .Value = Version,     .bGiven = false },
                [3] = { .Key = S("ProjectName"),                   .Value = DisplayName, .bGiven = false }
            };

            for each (FileVariable, v, ExpandedVariablesDB)
            {
                if (String_StartsWith(v.Name, S("Version.plist::"), false))
                {
                    const String Key = StrShiftF(v.Name, 15);

                    // mark the key as "given" in the table
                    for (u8 i = 0; i < SArray_Capacity(BundleTable); i++)
                    {
                        if (String_IsEqual(BundleTable[i].Key, Key, false))
                        {
                            BundleTable[i].bGiven = true;
                            break;
                        }
                    }

                    Internal_PlistWrite(Arena, f, Key, v.Value);
                }
            }

            for (u8 i = 0; i < SArray_Capacity(BundleTable); i++)
            {
                if (!BundleTable[i].bGiven)
                {
                    Internal_PlistWrite(Arena, f, BundleTable[i].Key, BundleTable[i].Value);
                }
            }
        }

        Filesystem_WriteLine(f, S("</dict>\n"), NULL);
        Filesystem_WriteLine(f, S("</plist>\n"), NULL);

        Filesystem_Close(&f);

        bSuccess = true;
    }

    return bSuccess;
}

bool Export_PkgInfo(const String AssemblyName, const String Path)
{
    bool bSuccess = false;

    FileHandle f = FileHandle_Null();
    if (Filesystem_Open(Path, FileMode_Write, &f))
    {
        Filesystem_WriteLineFormatted(f, S("APPL%S"), NULL, AssemblyName);
        Filesystem_Close(&f);
        bSuccess = true;
    }

    return bSuccess;
}

bool Export_IconRC(const String Path, const String IconFilePath)
{
    bool bSuccess = false;

    FileHandle f = FileHandle_Null();
    if (Filesystem_Open(Path, FileMode_Write, &f))
    {
        u32 LastSlashIndex = 0;
        xx String_IndexOfLastPathSlash(IconFilePath, &LastSlashIndex);

        // TODO: think about error handling across all write line functions
        Filesystem_WriteLineFormatted(f, S("id ICON \"%S\""), NULL, StrShiftF(IconFilePath, LastSlashIndex == 0 ? 0 : LastSlashIndex+1));
        Filesystem_Close(&f);
        bSuccess = true;
    }

    return bSuccess;
}

bool Export_VersionRC(const BuildParams* Params, const String Path)
{
    if (NEVER(Params == NULL)) { return false; }

    bool bSuccess = false;

    FileHandle VersionRCFile = {0};
    if (Filesystem_Open(Path, FileMode_Write, &VersionRCFile))
    {
        StringLocal(AssemblyWithExt, 256);
        String_Append(&AssemblyWithExt, Params->Assembly);
        String_Append(&AssemblyWithExt, Params->Extension);

        // .rc files can only have 4 version numbers max. sigh...
        StringLocal(VersionCommas, 128);

        u8 NumParts = 0;
        StringLocal(VersionDigit, 6);
        for (u8 i = 0; i < Params->Version.Length; i++)
        {
            if (Params->Version.Data[i] == '.' ||
                Params->Version.Data[i] == '-' ||
                Params->Version.Data[i] == '_' ||
                Params->Version.Data[i] == ',')
            {
                if (NumParts == 3)
                {
                    break;
                }

                u64 VersionNumber = 0;
                if (String_ToU64(VersionDigit, &VersionNumber))
                {
                    if (VersionNumber > UINT16_MAX)
                    {
                        String_Append(&VersionCommas, S("65535"));
                    }
                    else
                    {
                        String_Append(&VersionCommas, VersionDigit);
                    }
                }

                String_Empty(&VersionDigit);

                // if previous character was a comma, meaning we didnt find any digits, add '0'
                if (VersionCommas.Length == 0 || 
                    String_IsLast(VersionCommas, ','))
                {
                    String_AppendChar(&VersionCommas, '0');
                }

                String_AppendChar(&VersionCommas, ',');

                NumParts++;
            }
            else
            {
                if (!IsDigit(Params->Version.Data[i]))
                {
                    continue;
                }

                String_AppendChar(&VersionDigit, Params->Version.Data[i]);
            }
        }

        if (VersionDigit.Length > 0)
        {
            u64 VersionNumber = 0;
            if (String_ToU64(VersionDigit, &VersionNumber))
            {
                if (VersionNumber > UINT16_MAX)
                {
                    String_Append(&VersionCommas, S("65535"));
                }
                else
                {
                    String_Append(&VersionCommas, VersionDigit);
                }
            }

            String_Empty(&VersionDigit);
        }

        xx String_EatCharInlineFromEnd(&VersionCommas, ',');

        // we must have at least 4 parts otherwise llvm-rc will complain
        u32 NumCommas = String_CountChar(VersionCommas, ',');
        if (NumCommas < 3)
        {
            for (u8 i = 0; i < 3-NumCommas; i++)
            {
                String_Append(&VersionCommas, S(",0"));
            }
        }

        xx String_EatCharInlineFromEnd(&VersionCommas, ',');

        STRUCT(FileFlagsEntry)
        {
            u32 HexValue;
            u32 Padding1;
            String Name;
        };

        const FileFlagsEntry FileFlags[6] = 
        {
            { .HexValue = 0x1,  .Name = S("VS_FF_DEBUG") },
            { .HexValue = 0x2,  .Name = S("VS_FF_PRERELEASE") },
            { .HexValue = 0x4,  .Name = S("VS_FF_PATCHED") },
            { .HexValue = 0x8,  .Name = S("VS_FF_PRIVATEBUILD") },
            { .HexValue = 0x10, .Name = S("VS_FF_INFOINFERRED") },
            { .HexValue = 0x20, .Name = S("VS_FF_SPECIALBUILD") }
        };

        const FileFlagsEntry FileOSFlags[5] = 
        {
            { .HexValue = 0x00, .Name = S("VOS__BASE") },
            { .HexValue = 0x01, .Name = S("VOS__WINDOWS16") },
            { .HexValue = 0x02, .Name = S("VOS__PM16") },
            { .HexValue = 0x03, .Name = S("VOS__PM32") },
            { .HexValue = 0x04, .Name = S("VOS__WINDOWS32") }
        };

        const FileFlagsEntry FileTypeFlags[7] = 
        {
            { .HexValue = 0x00, .Name = S("VFT_UNKNOWN") },
            { .HexValue = 0x01, .Name = S("VFT_APP") },
            { .HexValue = 0x02, .Name = S("VFT_DLL") },
            { .HexValue = 0x03, .Name = S("VFT_DRV") },
            { .HexValue = 0x04, .Name = S("VFT_FONT") },
            { .HexValue = 0x05, .Name = S("VFT_VXD") },
            { .HexValue = 0x07, .Name = S("VFT_STATIC_LIB") }
        };

        FileFlagsEntry FileType = FileTypeFlags[0];

        // todo: use params->type, so this can export on non-windows platforms
        if (String_IsEqual(Params->Extension, S(".exe"), false) || 
            String_IsEqual(Params->Extension, S(".com"), false))
        {
            FileType = FileTypeFlags[1];
        }
        else if (String_IsEqual(Params->Extension, S(".dll"), false))
        {
            FileType = FileTypeFlags[2];
        }
        else if (String_IsEqual(Params->Extension, S(".lib"), false))
        {
            FileType = FileTypeFlags[6];
        }
        else
        {
            // no action required
        }

        // is major version at 0? if so, it's a pre-release build
        u32 FirstDot = 0;
        xx String_IndexOfChar(Params->Version, '.', &FirstDot);
        u64 MajorVersionNumber = 0;
        xx String_ToU64(FirstDot == 0 ? Params->Version : StrSlice(Params->Version.Data, FirstDot), &MajorVersionNumber);

        const FileFlagsEntry FileFlag = MajorVersionNumber == 0 ? FileFlags[1] : FileFlags[4];
        const FileFlagsEntry FileOS   = FileOSFlags[4];

        StringLocal(FileData, 2048);
        String_Format(&FileData, S("1 VERSIONINFO\n"
                                    "FILEVERSION      %S  // this can only have 4 parts\n"
                                    "PRODUCTVERSION   %S  // same here\n\n"

                                    // TODO: check for this specific define
                                    /*
                                    "#ifdef _DEBUG\n"
                                    "FILEFLAGS        %X  // %S\n"
                                    "#else\n"
                                    "FILEFLAGS        %X  // %S\n"
                                    "#endif\n"
                                    */
                                    "FILEFLAGS        %X  // %S\n"
                                    "FILEOS           %X  // %S\n"
                                    "FILETYPE         %X  // %S\n"
                                    "FILESUBTYPE      0  // VFT2_UNKNOWN\n\n"

                                    "BEGIN\n"
                                    "    BLOCK \"StringFileInfo\"\n"
                                    "    BEGIN\n"
                                    "        BLOCK \"040904E4\"\n"
                                    "        BEGIN\n"
                                    "            VALUE \"CompanyName\",      \"%S\"\n"
                                    "            VALUE \"FileDescription\",  \"%S\"\n"
                                    "            VALUE \"FileVersion\",      \"%S\"\n"
                                    "            VALUE \"LegalCopyright\",   \"%S\"\n"
                                    "            VALUE \"OriginalFilename\", \"%S\"\n"
                                    "            VALUE \"InternalName\",     \"%S\"\n"
                                    "            VALUE \"ProductName\",      \"%S\"\n"
                                    "            VALUE \"ProductVersion\",   \"%S\"\n"
                                    "        END\n"
                                    "    END\n\n"

                                    "    BLOCK \"VarFileInfo\"\n"
                                    "    BEGIN\n"
                                    "        VALUE \"Translation\", 0x409, 1252\n"
                                    "    END\n"
                                    "END\n"
                                    ),
                                    VersionCommas, VersionCommas,
                                    FileFlag.HexValue, FileFlag.Name,
                                    FileOS.HexValue, FileOS.Name,
                                    FileType.HexValue, FileType.Name,
                                    Params->CompanyName, 
                                    Params->Description, Params->Version, Params->Copyright,
                                    AssemblyWithExt, Params->InternalName, Params->TitleName, Params->Version);
                                    // TODO: add FileVersion to build file. if empty fallback to Version

        Filesystem_Write(VersionRCFile, FileData.Length, FileData.Data, NULL);
        Filesystem_Close(&VersionRCFile);
        bSuccess = true;
    }

    return bSuccess;
}

/* I am conflicted on this because on the one hand it's clean to have one procedure that calls into the
   appropriate one, but on the other hand, user code is now slightly more complicated compared to a direct call.
   it's just more code, struct/enum types, and extra typing for no real benefit.

   for example, this original code

        if ((bGenVersionRc && !Export_VersionRC(&p, RCPath)) ||
            (bGenIconRc && !Export_IconRC(RCPath, IconFilePath)))

    now becomes

        if ((bGenVersionRc && !Export(ExportType_VersionRC, *Arena, &p, RCPath, ExportMetaData_Null)) ||
            (bGenIconRc && !Export(ExportType_IconRC, *Arena, &p, RCPath, (ExportMetaData){.StringParam_1 = IconFilePath})))
    
    which is a lot more noisy, and looks like shit to read (and write)

   so this code is staying here until i decide what to do with it.
   - Ali (20/04/2025)

bool Export(EExportType Type, LinearAllocator Scratch, const BuildParams* Params, const String OutputPath, ExportMetaData MetaData)
{
    bool bSuccess = false;

    switch (Type)
    {
        case ExportType_None: { } break;

        case ExportType_CompileCommands:
        {
            bSuccess = Export_CompileCommands(Params, MetaData.bIsLastBuild, MetaData.bKeepOneLine);
        }
        break;

        case ExportType_InfoPList:
        {
            bSuccess = Export_InfoPlist(Scratch, Params, OutputPath, MetaData.ExpandedVariablesDB, MetaData.bRawMode);
        }
        break;

        case ExportType_VersionPList:
        {
            bSuccess = Export_VersionPlist(Scratch, Params, OutputPath, MetaData.ExpandedVariablesDB, MetaData.bRawMode);
        }
        break;

        case ExportType_PkgInfo:
        {
            bSuccess = Export_PkgInfo(Params->Assembly, OutputPath);
        }
        break;

        case ExportType_VersionRC:
        {
            bSuccess = Export_VersionRC(Params, OutputPath);
        }
        break;

        case ExportType_IconRC:
        {
            bSuccess = Export_IconRC(OutputPath, MetaData.StringParam_1);
        }
        break;

        case ExportType_License:
        {
            bSuccess = Export_License(MetaData.StringParam_1, Params, OutputPath);
        }
        break;
    }

    return bSuccess;
}
*/


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


// -----------------------------------------------------------
// License Generator
// todo: if no copyright key was specified, make one

static bool Internal_Export_License_BSD2(const BuildParams* Params, const String Path)
{
    FileHandle f = FileHandle_Null();
    bool bSuccess = false;
    if (Filesystem_Open(Path, FileMode_Write, &f))
    {
        static const String Text = SC("BSD 3-Clause License\n\
\n\
%S\n\
\n\
Redistribution and use in source and binary forms, with or without\n\
modification, are permitted provided that the following conditions are met:\n\
\n\
1. Redistributions of source code must retain the above copyright notice, this\n\
   list of conditions and the following disclaimer.\n\
\n\
2. Redistributions in binary form must reproduce the above copyright notice,\n\
   this list of conditions and the following disclaimer in the documentation\n\
   and/or other materials provided with the distribution.\n\
\n\
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS \"AS IS\"\n\
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE\n\
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE\n\
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE\n\
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL\n\
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR\n\
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER\n\
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,\n\
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE\n\
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.\n\
");

        Filesystem_WriteLineFormatted(f, Text, NULL, Params->Copyright);
        Filesystem_Close(&f);
        bSuccess = true;
    }

    return bSuccess;
}

static bool Internal_Export_License_BSD3(const BuildParams* Params, const String Path)
{
    FileHandle f = FileHandle_Null();
    bool bSuccess = false;
    if (Filesystem_Open(Path, FileMode_Write, &f))
    {
        static const String Text = SC("BSD 3-Clause License\n\
\n\
%S\n\
\n\
Redistribution and use in source and binary forms, with or without\n\
modification, are permitted provided that the following conditions are met:\n\
\n\
1. Redistributions of source code must retain the above copyright notice, this\n\
   list of conditions and the following disclaimer.\n\
\n\
2. Redistributions in binary form must reproduce the above copyright notice,\n\
   this list of conditions and the following disclaimer in the documentation\n\
   and/or other materials provided with the distribution.\n\
\n\
3. Neither the name of the copyright holder nor the names of its\n\
   contributors may be used to endorse or promote products derived from\n\
   this software without specific prior written permission.\n\
\n\
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS \"AS IS\"\n\
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE\n\
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE\n\
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE\n\
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL\n\
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR\n\
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER\n\
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,\n\
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE\n\
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.\n\
");

        Filesystem_WriteLineFormatted(f, Text, NULL, Params->Copyright);
        Filesystem_Close(&f);
        bSuccess = true;
    }

    return bSuccess;
}

static bool Internal_Export_License_MIT(const BuildParams* Params, const String Path)
{
    FileHandle f = FileHandle_Null();

    bool bSuccess = false;
    if (Filesystem_Open(Path, FileMode_Write, &f))
    {
        static const String Text = SC("\
MIT License\n\
\n\
%S\n\
\n\
Permission is hereby granted, free of charge, to any person obtaining a copy\n\
of this software and associated documentation files (the \"Software\"), to deal\n\
in the Software without restriction, including without limitation the rights\n\
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell\n\
copies of the Software, and to permit persons to whom the Software is\n\
furnished to do so, subject to the following conditions:\n\
\n\
The above copyright notice and this permission notice shall be included in all\n\
copies or substantial portions of the Software.\n\
\n\
THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR\n\
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,\n\
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE\n\
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER\n\
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,\n\
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE\n\
SOFTWARE.\n\
");

        Filesystem_WriteLineFormatted(f, Text, NULL, Params->Copyright);
        Filesystem_Close(&f);
        bSuccess = true;
    }

    return bSuccess;
}

static bool Internal_Export_License_DoWhatTheFuckYouWantTo(const BuildParams* Params, const String Path)
{
    FileHandle f = FileHandle_Null();
    bool bSuccess = false;
    if (Filesystem_Open(Path, FileMode_Write, &f))
    {
        static const String Text = SC("\
        DO WHAT THE FUCK YOU WANT TO PUBLIC LICENSE\n\
            Version 2, December 2004\n\
\n\
%S\n\
\n\
Everyone is permitted to copy and distribute verbatim or modified\n\
copies of this license document, and changing it is allowed as long\n\
as the name is changed.\n\
\n\
        DO WHAT THE FUCK YOU WANT TO PUBLIC LICENSE\n\
TERMS AND CONDITIONS FOR COPYING, DISTRIBUTION AND MODIFICATION\n\
\n\
0. You just DO WHAT THE FUCK YOU WANT TO.\n\
");

        Filesystem_WriteLineFormatted(f, Text, NULL, Params->Copyright);
        Filesystem_Close(&f);
        bSuccess = true;
    }

    return bSuccess;
}

static bool Internal_Export_License_TheUnlicense(const String Path)
{
    FileHandle f = FileHandle_Null();
    bool bSuccess = false;
    if (Filesystem_Open(Path, FileMode_Write, &f))
    {
        static const String Text = SC("\
This is free and unencumbered software released into the public domain.\n\
\n\
Anyone is free to copy, modify, publish, use, compile, sell, or\n\
distribute this software, either in source code form or as a compiled\n\
binary, for any purpose, commercial or non-commercial, and by any\n\
means.\n\
\n\
In jurisdictions that recognize copyright laws, the author or authors\n\
of this software dedicate any and all copyright interest in the\n\
software to the public domain. We make this dedication for the benefit\n\
of the public at large and to the detriment of our heirs and\n\
successors. We intend this dedication to be an overt act of\n\
relinquishment in perpetuity of all present and future rights to this\n\
software under copyright law.\n\
\n\
THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND,\n\
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF\n\
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.\n\
IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR\n\
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,\n\
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR\n\
OTHER DEALINGS IN THE SOFTWARE.\n\
\n\
For more information, please refer to <https://unlicense.org>\n\
");

        Filesystem_WriteLine(f, Text, NULL);
        Filesystem_Close(&f);
        bSuccess = true;
    }

    return bSuccess;
}

bool Export_License(const String LicenseType, const BuildParams* Params, const String Path)
{
    bool bSuccess = false;

    if (String_IsEqual(LicenseType, S("BSD2"), false))
    {
        bSuccess = Internal_Export_License_BSD2(Params, Path);
    }
    else if (String_IsEqual(LicenseType, S("BSD3"), false))
    {
        bSuccess = Internal_Export_License_BSD3(Params, Path);
    }
    else if (String_IsEqual(LicenseType, S("MIT"), false))
    {
        bSuccess = Internal_Export_License_MIT(Params, Path);
    }
    else if (String_IsEqual(LicenseType, S("FuckYou"), false))
    {
        bSuccess = Internal_Export_License_DoWhatTheFuckYouWantTo(Params, Path);
    }
    else if (String_IsEqual(LicenseType, S("Unlicense"), false))
    {
        bSuccess = Internal_Export_License_TheUnlicense(Path);
    }
    else
    {
    }

    return bSuccess;
}

bool Export_FromArg(LinearAllocator Scratch, const BuildParams* Params, const String Arg, TArray(FileVariable) ExpandedVariablesDB)
{
    bool bSuccess = true;

    StringArray Vars = String_ParseIntoArray(&Scratch, Arg, ',', 0, 128);

    bool bAnyExported = false;
    for each_str (var, Vars)
    {
        const bool bGenCompileCommandsJSON        = String_IsEqual(*var, S("compile_commands"), false) ||
                                                    String_IsEqual(*var, S("cc"), false);
        const bool bGenCompileCommandsJSONOneLine = String_IsEqual(*var, S("compile_commands_one_line"), false) ||
                                                    String_IsEqual(*var, S("cc_one_line"), false) ||
                                                    String_IsEqual(*var, S("compile_commands1"), false) ||
                                                    String_IsEqual(*var, S("cc1"), false);
        const bool bGenInfoPlist                  = String_IsEqual(*var, S("info.plist"), false);
        const bool bGenVersionPlist               = String_IsEqual(*var, S("version.plist"), false);
        const bool bGenPlist                      = String_IsEqual(*var, S("plist"), false);
        const bool bGenPkgInfo                    = String_IsEqual(*var, S("pkginfo"), false);
        const bool bGenVersionRc                  = String_IsEqual(*var, S("versionrc"), false) ||
                                                    String_IsEqual(*var, S("version.rc"), false);
        const bool bGenIconRc                     = String_IsEqual(*var, S("iconrc"), false) ||
                                                    String_IsEqual(*var, S("icon.rc"), false);
        //const bool bGenVisualStudio             = String_IsEqual(*var, S("export:visual_studio"), false);
        //const bool bGenXCode                    = String_IsEqual(*var, S("export:xcode"), false);

        const bool bLicense                       = String_IsEqual(*var, S("license"), false) ||
                                                    String_IsEqual(*var, S("license="), false);
        const bool bLicenseMIT                    = String_IsEqual(*var, S("license=MIT"), false) ||
                                                    String_IsEqual(*var, S("MIT"), false);
        const bool bLicenseBSD2                   = String_IsEqual(*var, S("license=BSD2"), false) ||
                                                    String_IsEqual(*var, S("BSD2"), false);
        const bool bLicenseBSD3                   = String_IsEqual(*var, S("license=BSD3"), false) ||
                                                    String_IsEqual(*var, S("BSD3"), false);
        const bool bLicenseFuckYou                = String_IsEqual(*var, S("license=FuckYou"), false) ||
                                                    String_IsEqual(*var, S("FuckYou"), false);
        const bool bLicenseUnlicense              = String_IsEqual(*var, S("license=Unlicense"), false) ||
                                                    String_IsEqual(*var, S("unlicense"), false);

        if (bGenCompileCommandsJSON || bGenCompileCommandsJSONOneLine)
        {
            if (bQuietBuild) { Logging_Enable(); }

            LOG("Generating compile_commands.json ...");

            bool bLast = Params->CameFromBuildFile.Length == 0;

            Clock c;
            Clock_Start(&c);

            if (!Export_CompileCommands(Params, bLast, bGenCompileCommandsJSONOneLine))
            {
                bSuccess = false;
                break;
            }

            Clock_Tick(&c);

            if (bLast)
            {
                StringLocal(ExportTimeString, 32);
                Clock_GetElapsedTime_ToString(&c, true, &ExportTimeString);
                LOG("\nExport time: %S", ExportTimeString);

                StringLocal(CompileCommandsPath, MAX_PATH_LENGTH);
                String_BuildPath(&CompileCommandsPath, Params->RootDirectory, S("compile_commands.json"));
                LOG_SUCCESS("\n\"%S\"", CompileCommandsPath);
            }

            if (bQuietBuild) { Logging_Disable(); }

            bAnyExported = true;
        }
        else if (bGenPlist || bGenInfoPlist || bGenVersionPlist)
        {
            if (bQuietBuild) { Logging_Enable(); }

            StringLocal(ExportPath, MAX_PATH_LENGTH);
            String_BuildPath(&ExportPath, Params->RootDirectory, Params->IntermediateDirectory, S("__Exports"));

            if (!Filesystem_OpenDirectory(ExportPath))
            {
                bSuccess = false;
                break;
            }

            Clock c;
            Clock_Start(&c);

            if (bGenPlist || bGenInfoPlist)
            {
                LOG("Generating Info.plist ...");

                StringLocal(PlistPath, MAX_PATH_LENGTH);
                String_BuildPath(&PlistPath, ExportPath, S("Info.plist"));

                if (!Export_InfoPlist(Scratch, Params, PlistPath, ExpandedVariablesDB, DoesBuildVarExist(ExpandedVariablesDB, S("Info.plist"))))
                {
                    LOG_ERROR("Failed to export \"%S\". Aborting build...", PlistPath);

                    bSuccess = false;
                    break;
                }

                LOG_SUCCESS("\n\"%S\"", PlistPath);
            }

            if (bGenPlist || bGenVersionPlist)
            {
                LOG("\nGenerating Version.plist ...");

                StringLocal(PlistPath, MAX_PATH_LENGTH);
                String_BuildPath(&PlistPath, ExportPath, S("Version.plist"));

                if (!Export_VersionPlist(Scratch, Params, PlistPath, ExpandedVariablesDB, DoesBuildVarExist(ExpandedVariablesDB, S("Version.plist"))))
                {
                    LOG_ERROR("Failed to export \"%S\". Aborting build...", PlistPath);

                    bSuccess = false;
                    break;
                }

                LOG_SUCCESS("\n\"%S\"", PlistPath);
            }

            Clock_Tick(&c);

            StringLocal(ExportTimeString, 32);
            Clock_GetElapsedTime_ToString(&c, true, &ExportTimeString);
            LOG("\nExport time: %S", ExportTimeString);

            if (bQuietBuild) { Logging_Disable(); }

            bAnyExported = true;
        }
        else if (bGenPkgInfo)
        {
            if (bQuietBuild) { Logging_Enable(); } 

            LOG("Generating PkgInfo ...");

            StringLocal(ExportPath, MAX_PATH_LENGTH);
            String_BuildPath(&ExportPath, Params->RootDirectory, Params->IntermediateDirectory, S("__Exports"));

            if (!Filesystem_OpenDirectory(ExportPath))
            {
                bSuccess = false;
                break;
            }

            StringLocal(PkgInfoPath, MAX_PATH_LENGTH);
            String_BuildPath(&PkgInfoPath, ExportPath, S("PkgInfo"));

            Clock c;
            Clock_Start(&c);

            if (!Export_PkgInfo(Params->Assembly, PkgInfoPath))
            {
                LOG_ERROR("Failed to export \"%S\". Aborting build...", PkgInfoPath);

                bSuccess = false;
                break;
            }

            Clock_Tick(&c);

            StringLocal(ExportTimeString, 32);
            Clock_GetElapsedTime_ToString(&c, true, &ExportTimeString);
            LOG("\nExport time: %S", ExportTimeString);

            LOG_SUCCESS("\n\"%S\"", PkgInfoPath);

            if (bQuietBuild) { Logging_Disable(); }

            bAnyExported = true;
        }
        else if (bGenVersionRc || bGenIconRc)
        {
            if (bQuietBuild) { Logging_Enable(); }

            LOG("Generating resource file ...");

            StringLocal(ExportPath, MAX_PATH_LENGTH);
            String_BuildPath(&ExportPath, Params->RootDirectory, Params->IntermediateDirectory, S("__Exports"));

            if (!Filesystem_OpenDirectory(ExportPath))
            {
                bSuccess = false;
                break;
            }

            Clock c;
            Clock_Start(&c);

            StringLocal(RCPath, MAX_PATH_LENGTH);
            String_BuildPath(&RCPath, ExportPath, bGenVersionRc ? S("version.rc") : S("icon.rc"));

            if ((bGenVersionRc && !Export_VersionRC(Params, RCPath)) ||
                (bGenIconRc && !Export_IconRC(RCPath, Params->IconFilePath)))
            {
                LOG_ERROR("Failed to export \"%S\". Aborting build...", RCPath);

                bSuccess = false;
                break;
            }

            Clock_Tick(&c);

            StringLocal(ExportTimeString, 32);
            Clock_GetElapsedTime_ToString(&c, true, &ExportTimeString);
            LOG("\nExport time: %S", ExportTimeString);

            LOG_SUCCESS("\n\"%S\"", RCPath);

            if (bQuietBuild) { Logging_Disable(); }

            bAnyExported = true;
        }
        else if (bLicense || bLicenseBSD2 || bLicenseBSD3 || bLicenseMIT || bLicenseFuckYou || bLicenseUnlicense)
        {
            if (bQuietBuild) { Logging_Enable(); }

            // TODO export multiple licenses with comma like this:
            // export:license=MIT,BSD,Unlicense

            u32 EqualsIndex = 0;
            bool bHasEqual = String_IndexOfChar(*var, '=', &EqualsIndex);
            String LicenseType = String_Null();
            if (bHasEqual)
            {
                LicenseType = StrShiftF(*var, EqualsIndex+1);
            }
            else
            {
                if (!String_IsEqual(*var, S("license"), false)) // ignore this
                {
                    LicenseType = *var;
                }
            }

            if (LicenseType.Length > 0)
            {
                LOG("Generating %S license file ...", LicenseType);

                Clock c;
                Clock_Start(&c);

                StringLocal(ExportPath, MAX_PATH_LENGTH);
                String_BuildPath(&ExportPath, Params->RootDirectory, Params->IntermediateDirectory, S("__Exports"), S("Licenses"));

                if (!Filesystem_OpenDirectory(ExportPath))
                {
                    bSuccess = false;
                    break;
                }

                StringLocal(FileName, 128);
                String_Format(&FileName, S("LICENSE_%S"), LicenseType);
                String_ToUpper(&FileName);

                StringLocal(LicensePath, MAX_PATH_LENGTH);
                String_BuildPath(&LicensePath, ExportPath, FileName);

                if (!Export_License(LicenseType, Params, LicensePath))
                {
                    LOG_ERROR("Failed to export \"%S\". Aborting...", LicensePath);

                    bSuccess = false;
                    break;
                }

                Clock_Tick(&c);

                StringLocal(ExportTimeString, 32);
                Clock_GetElapsedTime_ToString(&c, true, &ExportTimeString);
                LOG("\nExport time: %S", ExportTimeString);

                LOG_SUCCESS("\n\"%S\"", LicensePath);

                bAnyExported = true;
            }
            else
            {
                bAnyExported = true; // to disable error message that is not meaningful

                // log all valid license exports
                const String LicenseTypes[5] =
                {
                    S("MIT"),
                    S("BSD2"),
                    S("BSD3"),
                    S("FuckYou"),
                    S("Unlicense"),
                };

                LOG("Here is a list of all supported license export types:");

                for (u8 j = 0; j < SArray_Capacity(LicenseTypes); j++)
                {
                    LOG("  %i. %S", j+1, LicenseTypes[j]);
                }


                LOG_INLINE_WARNING("\nUsage");
                LOG("\n     riftbuild export:license=MIT");
                LOG("  or riftbuild export:license=MIT,BSD3,WhateverElse,YouWant (not yet implemented)");
            }

            if (bQuietBuild) { Logging_Disable(); }
        }
        else
        {
            // no action required
        }
    }
    
    if (!bAnyExported)
    {
        LOG_INLINE("Nothing happened. No export logic was implemented for ");

        u32 i = 0;
        for each_str_i (i, var, Vars)
        {
            if (i == 0)
            {
                LOG_INLINE("\"%S\"", *var);
            }
            else
            {
                String Prefix = i == Vars.Num-1 ? S("or") : S(",");
                LOG_INLINE("%S \"%S\" ", Prefix, *var);
            }
        }

        LOG_LINE_BREAK();

        //TODO: list all export types
    }

    return bSuccess;
}
