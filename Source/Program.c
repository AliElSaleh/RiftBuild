// Copyright (c) Artisan Softworks
// Licensed under the BSD 3-Clause License. See the LICENSE file for details.

#include "Core/EntryPoint.h"

#ifndef UNITY_BUILD
#include "Backend.h"

#include "Core/Allocators.h"
#include "Core/Clock.h"
#include "Core/StringUtils.h"
#include "Core/Array.h"
#include "Core/Uuid.h"
#endif

#if PLATFORM_WINDOWS
#include "MicrosoftCraziness.h"
#endif

// TODO:
// [ ] relink if a library file has changed, much like the source file change detection feature
//     that way we dont have to rebuild the host project again
// [ ] windows kits as internal variable?
// [ ] add dav1d to github examples
// [ ] change include to import and ensure it is only loaded once
// [ ] configure.file(config.build.config) config.h

const usize GEngineMemoryAmount  = Kibibytes(128);
const usize GEngineScratchAmount = Kibibytes(8);

TArray(InternalVariable) InternalVariablesDB = NULL;
bool bQuietBuild = false;
bool bNoWordWrapLogging = false;
bool bHelp = false;
bool bOptions = false;
bool bWasVCVarsBatchExecuted = false;
bool bVerboseLog = false;
FileVariable FileVariable_Empty = {0};

static bool bSingleThread = false;
// static bool bIsRebuild = false;
static bool bIsClean = false;

read_only String BuiltinOptions[] =
{
    SC("-h"),
    SC("-a"),
    SC("-b"),
    SC("-v"),
    SC("-q"),
    SC("-s"),
    SC("-t"),
    SC("-?"),
    SC("/?"),
    SC("-?"),
    SC("?"),
    SC("--help"),
    SC("--about"),
    SC("--buildfiles"),
    SC("--verbose"),
    SC("--singlethread"),
    SC("--quiet"),
    SC("--tutorial"), // todo: different types of tutorials like --tutorial:name
    SC("--from-desktop"),
    SC("--no-mutex"),
    SC("help"),
    SC("options"),
    SC("clean"),
    SC("rebuild"),
    SC("run"),
    SC("list:"),
    SC("override:"),
    SC("export:"),
    SC("preset:"),
};

STRUCT(BuildReceipt)
{
    String AssemblyName;
    String BuildDirectory;
    String WorkingPath;
    String Includes;
    String Defines;
    String Libraries;
    String LibraryPaths;
    String LinkerFlags;

    u32 ExitCode;
    u32 Padding;
    b64 bWorkWasDone;

    EAssemblyType AssemblyType;
    u8 blah[7];
};

STRUCT(BuildFileDirectoryIteratorData)
{
    String*     Name;
    String*     Path;
    StringArray Arguments;
    bool        bFoundBuildFile;
    bool        bNoBuildFileSpecifiedInCmd;
    bool        bSearchOnlyBuildBatch;
    i8          BuildFileIndex;
    i8          RootPathIndex;
    u8          NumBuildFilesFound;
    u8          Padding1[2];
};

STRUCT(CopyrightEnforceInfo)
{
    String Content;
    u32    FromLine;
    u32    ToLine;
};

static bool IsBuildFile(const String FilePath)
{
    return String_EndsWith(FilePath, S(".build"), false);
}

static bool IsBuildBatchFile(const String FilePath)
{
    return String_EndsWith(FilePath, S(".buildbatch"), false);
}

bool DoesCmdOptionExist(TArray(CmdOption) CmdOptionsDB, const String Name)
{
    bool bFound = false;

    for each (CmdOption, o, CmdOptionsDB)
    {
        if (String_IsEqual(o.Name, Name, false))
        {
            bFound = true;
            break;
        }
    }

    return bFound;
}

String GetCmdOptionValue(TArray(CmdOption) CmdOptionsDB, const String Name)
{
    String Value = String_Null();

    for each (CmdOption, o, CmdOptionsDB)
    {
        if (String_IsEqual(o.Name, Name, false))
        {
            Value = o.Value;
            break;
        }
    }

    return Value;
}

bool DoesBuildVarExist(TArray(FileVariable) VariablesDB, const String Name)
{
    bool bExists = false;

    for each (FileVariable, Var, VariablesDB)
    {
        if (String_IsEqual(Var.Name, Name, false))
        {
            bExists = true;
            break;
        }
    }

    return bExists;
}

StringList GetVariableValueList(LinearAllocator* Arena, TArray(FileVariable) VariablesDB, const String Name)
{
    StringList list = StringList_Null();

    for each (FileVariable, Var, VariablesDB)
    {
        if (String_IsEqual(Var.Name, Name, false))
        {
            StringList* This = &list;
            while (String_IsValid(This->String))
            {
                if ((!This->Next || This->Next == StringList_Null().Next))
                {
                    This->Next = LinearAllocator_Allocate(Arena, sizeof(StringList));
                    This->Next->String = String_Null();
                    This->Next->Next = StringList_Null().Next;
                    break;
                }

                This = This->Next;
            }

            This = &list;
            while (This->Next && This->Next != StringList_Null().Next)
            {
                This = This->Next;
            }

            This->String = Var.Value;
        }
    }

    return list;
}

FileVariable GetVariable(TArray(FileVariable) Variables, const String Name)
{
    FileVariable FoundVar = FileVariable_Empty;

    for each (FileVariable, Var, Variables)
    {
        if (String_IsEqual(Var.Name, Name, false))
        {
            FoundVar = Var;
            break;
        }
    }

    return FoundVar;
}

String GetVariableValue(TArray(FileVariable) Variables, const String Name)
{
    String Value = String_Null();

    for each (FileVariable, Var, Variables)
    {
        if (String_IsEqual(Var.Name, Name, false))
        {
            Value = Var.Value;
            break;
        }
    }

    return Value;
}

String* GetVariableValue_Ref(TArray(FileVariable) Variables, const String Name)
{
    String* Value = NULL;

    for each (FileVariable, Var, Variables)
    {
        if (String_IsEqual(Var.Name, Name, false))
        {
            Value = &Var_->Value;
        }
    }

    return Value;
}

static void PrefixVariables(String* Dest, String VariableValue, const String Prefix, bool bWrapWithQuotes)
{
    bool bInsideQuote = false;
    bool bSawSpace = false;

    bool bLastStringStartedWithQuote = false;

    u32 StartOffset = 0;

    if (VariableValue.Length > 0)
    {
        bLastStringStartedWithQuote = VariableValue.Data[0] == '"';

        #if 0
        if (bWrapWithQuotes && VariableValue.Data[0] != '"')
        {
            String_AppendChar(Dest, '"');
        }
        #else
        if (bWrapWithQuotes && bLastStringStartedWithQuote)
        {
            StartOffset = 1;
            bInsideQuote = true;
        }

        if (bWrapWithQuotes)
        {
            String_AppendChar(Dest, '"');
        }
        #endif

        if (!String_StartsWith(VariableValue, Prefix, false))
        {
            String_Append(Dest, Prefix);

            #if PLATFORM_LINUX
            if (String_StartsWith(VariableValue, S("lib"), false) && // TODO: only care about extension??
                String_IsEqual(Prefix, S("-l"), true))// &&
                //String_IndexOfChar(VariableValue, '.', NULL))
            {
                String_AppendChar(Dest, ':');
            }
            #endif
        }
    }

    for (u32 i = StartOffset; i < VariableValue.Length; i++)
    {
        uchar C = VariableValue.Data[i];

        // ignore trailing space
        if (IsWhitespace(C) && i == VariableValue.Length-1)
        {
            continue;
        }

        if (IsWhitespace(C))
        {
            bSawSpace = true;
        }
        else
        {
            if (bSawSpace)
            {
                bSawSpace = false;

                if (!bInsideQuote)
                {
                    bLastStringStartedWithQuote = C == '"';

                    if (bWrapWithQuotes)
                    {
                        String_AppendChar(Dest, '"');
                    }

                    if (!String_StartsWith(StrShiftF(VariableValue, i), Prefix, false))
                    {
                        String_Append(Dest, Prefix);

                        #if PLATFORM_LINUX
                        if (String_StartsWith(StrShiftF(VariableValue, i), S("lib"), false) &&
                            String_IsEqual(Prefix, S("-l"), true))
                        {
                            String_AppendChar(Dest, ':');
                        }
                        #endif
                    }

                    if (bWrapWithQuotes && C == '"')
                    {
                        bInsideQuote = true;

                        // skip appending this quote as we have already done so
                        continue;
                    }
                }
            }
        }

        if (C == '"')
        {
            bInsideQuote = !bInsideQuote;
        }

        if (!bInsideQuote)
        {
            if (IsWhitespace(C))
            {
                if (bWrapWithQuotes && !String_IsLast(*Dest, '"'))
                {
                    String_AppendChar(Dest, '"');
                }
            }

            if (C == '\\' || C == '/')
            {
                // is next char a whitespace? skip add path separator
                if (i+1 < VariableValue.Length && IsWhitespace(VariableValue.Data[i+1]))
                {
                    continue;
                }
            }
        }
        
        String_AppendChar(Dest, C);
    }

    if (Dest->Length > 0)
    {
        xx String_EatSpacesInlineFromEnd(Dest);

        if (bWrapWithQuotes && !bLastStringStartedWithQuote)
        {
            String_AppendChar(Dest, '"');
        }
    }
}

static void SuffixVariables(String* Dest, String VariableValue, const String Suffix)
{
    for (u32 i = 0; i < VariableValue.Length; i++)
    {
        u8 C = VariableValue.Data[i];

        bool bSawSpace = false;
        if (C == ' ')
        {
            bSawSpace = true;
        }

        if (bSawSpace)
        {
            if (!String_EndsWith(*Dest, Suffix, false))
            {
                String_Append(Dest, Suffix);
            }
        }

        String_AppendChar(Dest, C);
    }

    if (Dest->Length > 0)
    {
        if (!String_EndsWith(*Dest, Suffix, false))
        {
            String_Append(Dest, Suffix);
        }
    }
}

/*
bool LogCustomErrorMessage(TArray(FileVariable) VariablesDB, const String Context, const String Key, const bool bLineBreak)
{
    if (bQuietBuild) { Logging_Enable(); }

    bool bLogged = false;
    for each (FileVariable, Var, VariablesDB)
    {
        if (String_EndsWith(Var.Name, S(".errormessage"), false))
        {
            String Slice = StrSlice(Var.Name.Data, Var.Name.Length-13);

            u32 Dot = 0;
            if (String_IndexOfChar(Slice, '.', &Dot))
            {
                Slice = StrShiftF(Slice, Dot+1);
            }
            
            LinearAllocator Scratch = {0};
            i8 ScratchMemory[128] = {0};
            LinearAllocator_Create(128, ScratchMemory, &Scratch);
            StringArray Keys = String_ParseIntoArray(&Scratch, Slice, '|', 0, 8);
            for each_str (k, Keys)
            {
                if (String_IsEqual(*k, S("*"), false) ||
                   (String_IsEqual(*k, Key, false) &&
                   (Context.Length == 0 || String_StartsWith(Var.Name, Context, false))))
                {
                    if (bLineBreak) { LOG_LINE_BREAK(); }

                    LOG("%S", Var.Value);
                    bLogged = true;
                    break;
                }
            }
            LinearAllocator_Destroy(&Scratch);

            if (bLogged)
            {
                break;
            }
        }
    }

    if (bQuietBuild) { Logging_Disable(); }

    return bLogged;
}
*/

static bool FilterSourceFile(const String WorkingDirectory, const String SourceDirectory,
                      const String FullPath, const String RelativePath,
                      StringList WhitelistFiles, StringList BlacklistFiles,
                      StringList WhitelistDirectories, StringList BlacklistDirectories)
{
    bool bIsAllowed = true;

    const String TrimmedFileName = Filesystem_ExtractFileName(RelativePath, true);
    const String TrimmedDirName  = Filesystem_ExtractFilePath(RelativePath, false);

    // blacklist check
    {
        bool bUsingBlacklist = false;
        bool bFound = false;

        for each_str_list (BlacklistFiles)
        {
            bUsingBlacklist = true;

            u32 Index = 0;
            if (String_IndexOfLastChar(It.String, '*', &Index))
            {
                String Left = StrSlice(It.String.Data, Index);
                String Right = StrShiftF(It.String, Index+1);
                if (Index < TrimmedFileName.Length)
                {
                    if ((Index == 0 || String_IsEqual(Left, StrSlice(TrimmedFileName.Data, Index), false)) &&
                        String_IsEqual(Right, StrSlice(TrimmedFileName.Data+TrimmedFileName.Length-Right.Length, Right.Length), false))
                    {
                        bFound = true;
                        break;
                    }
                }
            }

            // Note: see comment in the whitelist version of this code
            String File = It.String;
            if (String_StartsWith(File, SourceDirectory, false))
            {
                String a = StrShiftF(File, SourceDirectory.Length);
                if (String_EatPathSeparatorsInline(&a))
                {
                    File = a;
                }
            }

            // Depending if the "File" string has an extension specified by the user, choose a comparison path.
            // The true branch will do an exact comparison, preserving the extension.
            // The false branch will strip the extension of the string we are comparing "File" to

            // Reasoning:
            // This is so the user can specify if they want to exactly match with the extension
            // if they know that two files with the same name in the same path would conflict.
            // or if the user of the build file was too lazy to type some extra characters, like me :P

            bool bWantsExact = Filesystem_DoesPathHaveFileExtension(File);
            if (bWantsExact)
            {
                if (String_IsEqual(File, RelativePath, false))
                {
                    bFound = true;
                    break;
                }
            }
            else
            {
                String RelativePathNoExt = Filesystem_StripFileExtension(RelativePath);
                if (String_IsEqual(File, RelativePathNoExt, false))
                {
                    bFound = true;
                    break;
                }
            }
        }

        if (bUsingBlacklist)
        {
            bIsAllowed = !bFound;
        }

        if (bIsAllowed)
        {
            bUsingBlacklist = false;
            bFound = false;

            for each_str_list (BlacklistDirectories)
            {
                bUsingBlacklist = true;

                if (String_IsEqual(It.String, S("*"), false) && TrimmedDirName.Length > 0)
                {
                    bFound = true;
                    break;
                }

                StringLocal(TestPath, MAX_PATH_LENGTH);
                String_BuildPath(&TestPath, WorkingDirectory, SourceDirectory, It.String);
                String_ConvertSlashToPlatformSlash(&TestPath);

                u32 Index = 0;
                if (String_IndexOfLastChar(TestPath, '*', &Index))
                {
                    StringLocal(DirPath, MAX_PATH_LENGTH);
                    String_BuildPath(&DirPath, WorkingDirectory, SourceDirectory, TrimmedDirName);

                    String Left = StrSlice(TestPath.Data, Index);
                    String Right = StrShiftF(TestPath, Index+1);
                    if (Index < DirPath.Length)
                    {
                        String Left2 = StrSlice(DirPath.Data, Index);
                        String Right2 = StrShiftF(DirPath, Index);

                        if ((String_IsEqual(Left, Left2, false) || Left.Length == 0) &&
                            (String_Contains(Right2, Right, false) || Right.Length == 0))
                        {
                            if (Filesystem_ArePathsCommon(DirPath, FullPath))
                            {
                                bFound = true;
                                break;
                            }
                        }
                    }
                }

                if (Filesystem_ArePathsCommon(TestPath, FullPath))
                {
                    bFound = true;
                    break;
                }
            }

            if (bUsingBlacklist)
            {
                bIsAllowed = !bFound;
            }
        }
    }

    // whitelist check
    if (bIsAllowed)
    {
        bool bUsingWhitelist = false;
        bool bFound = false;
        for each_str_list (WhitelistFiles)
        {
            bUsingWhitelist = true;

            u32 Index = 0;
            if (String_IndexOfLastChar(It.String, '*', &Index))
            {
                String Left = StrSlice(It.String.Data, Index);
                String Right = StrShiftF(It.String, Index+1);

                u32 LeftLastSlash = 0;
                bool bHasSlash = String_IndexOfLastPathSlash(Left, &LeftLastSlash);
                String TrimmedLeft = bHasSlash ? StrShiftF(Left, LeftLastSlash+1) : Left;

                if (String_StartsWith(RelativePath, Left, false) &&
                    String_EndsWith(RelativePath, Right, false))
                {
                    bFound = true;
                    break;
                }

                if (String_IsEqual(TrimmedLeft, StrSlice(TrimmedFileName.Data, Index), false) &&
                    String_IsEqual(Right, StrSlice(TrimmedFileName.Data+TrimmedFileName.Length-Right.Length, Right.Length), false))
                {
                    bFound = true;
                    break;
                }
            }

            // Note(Ali): this is here for convenience when you specify a SourceDirectory key and
            //            when you specify a list of source files that start with the SourceDirectory's value,
            //            we can just chop that off and continue on with the check below.
            String File = It.String;
            if (String_StartsWith(File, SourceDirectory, false))
            {
                String a = StrShiftF(File, SourceDirectory.Length);
                if (String_EatPathSeparatorsInline(&a))
                {
                    File = a;
                }
            }

            // Depending if the "File" string has an extension specified by the user, choose a comparison path.
            // The true branch will do an exact comparison, preserving the extension.
            // The false branch will strip the extension of the string we are comparing "File" to

            // Reasoning:
            // This is so the user can specify if they want to exactly match with the extension
            // if they know that two files with the same name in the same path would conflict.
            // or if the user of the build file was too lazy to type some extra characters, like me :P

            bool bWantsExact = Filesystem_DoesPathHaveFileExtension(File);
            if (bWantsExact)
            {
                if (String_IsEqual(File, RelativePath, false))
                {
                    bFound = true;
                    break;
                }
            }
            else
            {
                String RelativePathNoExt = Filesystem_StripFileExtension(RelativePath);
                if (String_IsEqual(File, RelativePathNoExt, false))
                {
                    bFound = true;
                    break;
                }
            }
        }

        if (bUsingWhitelist)
        {
            bIsAllowed = bFound;
        }

        if (bIsAllowed)
        {
            bUsingWhitelist = false;
            bFound = false;

            for each_str_list (WhitelistDirectories)
            {
                bUsingWhitelist = true;

                StringLocal(TestPath, MAX_PATH_LENGTH);
                String_BuildPath(&TestPath, WorkingDirectory, SourceDirectory, It.String);
                String_ConvertSlashToPlatformSlash(&TestPath);
                xx Filesystem_ConvertRelativeToAbsolutePath(&TestPath);

                StringLocal(SourceDir, MAX_PATH_LENGTH);
                String_BuildPath(&SourceDir, WorkingDirectory, SourceDirectory);
                String_ConvertSlashToPlatformSlash(&SourceDir);
                xx Filesystem_ConvertRelativeToAbsolutePath(&SourceDir);

                if (String_IsEqual(TestPath, SourceDir, false))
                {
                    break;
                }

                u32 Index = 0;
                if (String_IndexOfLastChar(TestPath, '*', &Index))
                {
                    StringLocal(DirPath, MAX_PATH_LENGTH);
                    String_BuildPath(&DirPath, WorkingDirectory, SourceDirectory, TrimmedDirName);

                    u32 LastSlash = 0;
                    xx String_IndexOfLastPathSlash(TestPath, &LastSlash);
                    i32 Diff = (i32)(Index-LastSlash+(TestPath.Length-1-Index));
                    if (Diff <= 1 && TrimmedDirName.Length > 0)
                    {
                        break;
                    }

                    String Left = StrSlice(TestPath.Data, Index);
                    String Right = StrShiftF(TestPath, Index+1);
                    if (Index < DirPath.Length)
                    {
                        String Left2 = StrSlice(DirPath.Data, Index);
                        String Right2 = StrShiftF(DirPath, Index);

                        if ((String_IsEqual(Left, Left2, false) || Left.Length == 0) &&
                            (String_Contains(Right2, Right, false) || Right.Length == 0))
                        {
                            if (Filesystem_ArePathsCommon(DirPath, FullPath))
                            {
                                bFound = true;
                                break;
                            }
                        }
                    }
                }

                if (Filesystem_ArePathsCommon(TestPath, FullPath))
                {
                    bFound = true;
                    break;
                }
            }

            if (bUsingWhitelist)
            {
                bIsAllowed = bFound;
            }
        }
    }

    return bIsAllowed;
}

static bool IconFileDirectoryIterator(const String FullPath, const String RelativePath, const String FileName, u64 FileSize, bool bIsDirectory, void* UserData)
{
    UNUSED_PARAM(RelativePath);
    UNUSED_PARAM(bIsDirectory);

    if (FileSize > 0)
    {
        struct Data
        {
            TArray(FileVariable) VariablesDB;
            String* IconFilePath;
            bool bSuccess;
            u8 Padding[7];
        };

        struct Data* D = UserData;

        const String IconName = GetVariableValue(D->VariablesDB, S("Icon"));

        u32 LastSlash = 0;
        const bool bNameOnly = !String_IndexOfLastPathSlash(IconName, &LastSlash);

        bool bHasExtension = false;
        u32 LastDot = 0;
        if (String_IndexOfLastChar(IconName, '.', &LastDot))
        {
            bool bHasPathSeparator = String_IndexOfFirstPathSlash(StrShiftF(IconName, LastDot), NULL);
            if (!bHasPathSeparator)
            {
                bHasExtension = true;
            }
        }

        if (bHasExtension)
        {
            if (String_IsEqual(FileName, IconName, false))
            {
                String_Copy(D->IconFilePath, FullPath);
                D->bSuccess = true;
                return false;
            }
        }
        else
        {
            const String IconExtensions[1] = 
            {
                #if PLATFORM_WINDOWS
                S(".ico"),
                #elif PLATFORM_APPLE
                S(".png"),
                #else
                S(".png"),
                #endif
            };

            for (u8 i = 0; i < SArray_Capacity(IconExtensions); i++)
            {
                StringLocal(TestName, MAX_PATH_LENGTH);
                String_Append(&TestName, IconName);
                String_Append(&TestName, IconExtensions[i]);

                bool bMatch = (!bNameOnly && String_IsEqual(FileName, StrShiftF(TestName, LastSlash+1), false)) ||
                              (bNameOnly && String_IsEqual(FileName, TestName, false));

                if (bMatch)
                {
                    String_Copy(D->IconFilePath, FullPath);
                    D->bSuccess = true;
                    return false;
                }
            }
        }
    }

    return true;
}

static bool SourceFileCounterDirectoryIterator(const String FullPath, const String RelativePath, const String FileName, u64 FileSize, bool bIsDirectory, void* UserData)
{
    UNUSED_PARAM(bIsDirectory);

    if (FileSize > 0)
    {
        if (String_StartsWith(FileName, S("__"), false))
        {
            return true;
        }

        SourceCountData* Data = UserData;

        // ignore the intermediate and build directories
        if (String_IndexOfFirstPathSlash(RelativePath, NULL))
        {
            if (String_StartsWith(RelativePath, Data->IntermediateDirectory, false) ||
                String_StartsWith(RelativePath, Data->BuildDirectory, false))
            {
                return true;
            }
        }

        String Extension = Filesystem_ExtractFileExtension(FileName, true);

        const bool bIsSource = IsSource(Extension);
        const bool bIsCustomSource = IsSourceCustom(Extension, Data->CustomSourceExtensions);

        if (bIsSource || bIsCustomSource)
        {
            if (FilterSourceFile(Data->WorkingDirectory, Data->SourceDirectory, FullPath, RelativePath, Data->WhitelistArray, Data->BlacklistArray, Data->WhitelistDirArray, Data->BlacklistDirArray))
            {
                if (Data->FirstSourceFileName->Length == 0)
                {
                    String_Copy(Data->FirstSourceFileName, FileName);
                }

                if (IsAsmSource(Extension))
                {
                    Data->NumAsmSources += 1;
                }
                else if (String_IsEqual(Extension, S(".rc"), false))
                {
                    bool bIgnore = String_IsEqual(FileName, S("icon.rc"), false);

                    if (!bIgnore)
                    {
                        Data->NumRcSources++;
                    }
                }
                else
                {
                    Data->NumSources++;
                }

                if (!Data->bHasCppFiles)
                {
                    if (IsCppSource(Extension) || IsCppHeader(Extension))
                    {
                        Data->bHasCppFiles = true;
                    }
                }

                SLinkedList_Push(Data->FilteredFilesNext, StringList_CreateWithCopy(Data->ArenaForFilterList, RelativePath, NULL));
            }
        }
        else if (IsHeader(Extension))
        {
            if (FilterSourceFile(Data->WorkingDirectory, Data->SourceDirectory, FullPath, RelativePath, Data->WhitelistArray, Data->BlacklistArray, Data->WhitelistDirArray, Data->BlacklistDirArray))
            {
                Data->NumHeaders++;

                if (!Data->bHasCppFiles)
                {
                    if (IsCppHeader(Extension))
                    {
                        Data->bHasCppFiles = true;
                    }
                }
                
                if (Data->bIsPCHBuild)
                {
                    SLinkedList_Push(Data->FilteredFilesNext, StringList_CreateWithCopy(Data->ArenaForFilterList, RelativePath, NULL));

                    Data->NumSources++;
                    String_Copy(Data->FirstSourceFileName, FileName);
                    return false;
                }
            }
        }
        else
        {
            // no action required
        }
    }

    return true;
}

static bool HeaderFileRebuildCheckDirectoryIterator(const String FullPath, const String RelativePath, const String FileName, u64 FileSize, bool bIsDirectory, void* UserData)
{
    UNUSED_PARAM(RelativePath);
    UNUSED_PARAM(bIsDirectory);

    if (FileSize > 0)
    {
        String Extension = Filesystem_ExtractFileExtension(FileName, true);

        // Note: this is a problem, because it just will look for any header filer within the search
        //       directory, we need to filter this somehow...
        if (IsHeader(Extension))
        {
            // todo??
            //if (FilterSourceFile(Data->WorkingDirectory, Data->SourceDirectory, FullPath, RelativePath, Data->WhitelistArray, Data->BlacklistArray, Data->WhitelistDirArray, Data->BlacklistDirArray))
            {
                struct HeaderIterData
                {
                    u64 AssemblyFileTime;
                    bool* bShouldRebuild;
                };

                struct HeaderIterData* Data = UserData;

                u64 HeaderFileTime = Filesystem_GetLastWriteTime(FullPath);

                if (HeaderFileTime >= Data->AssemblyFileTime)
                {
                    *Data->bShouldRebuild = true;

                    #ifndef HOOD
                    LOG("Header file \"%S\" has been modified since last build. Forcing rebuild...", FullPath);
                    #else
                    LOG("yo homie, dis header file \"%S\" was recently changed. gon force a rebuild...", FullPath);
                    #endif

                    LOG_LINE_BREAK();

                    return false;
                }

            }
        }
    }

    return true;
}

static bool BuildFileDirectoryIterator_Args(const String FullPath, const String RelativePath, const String FileName, u64 FileSize, bool bIsDirectory, void* UserData)
{
    UNUSED_PARAM(RelativePath);
    UNUSED_PARAM(bIsDirectory);

    if (FileSize > 0)
    {
        BuildFileDirectoryIteratorData* Data = UserData;

        if ((IsBuildFile(FileName) && !Data->bSearchOnlyBuildBatch) || (Data->bSearchOnlyBuildBatch && IsBuildBatchFile(FileName)))
        {
            bool bFoundFromNameSearch = false;
            for (u32 i = 0; i < Data->Arguments.Num; i++)
            {
                if (Data->BuildFileIndex == (i8)i || Data->RootPathIndex == (i8)i)
                {
                    continue;
                }

                StringLocal(Temp, MAX_PATH_LENGTH);
                String_Copy(&Temp, Data->Arguments.List[i]);
                String_Append(&Temp, Data->bSearchOnlyBuildBatch ? S(".buildbatch") : S(".build"));
                if (String_IsEqual(FileName, Temp, false))
                {
                    bFoundFromNameSearch = true;
                    break;
                }
            }

            if (bFoundFromNameSearch)
            {
                Data->bFoundBuildFile = true;

                String_Copy(Data->Name, FileName);
                String_Copy(Data->Path, FullPath);

                return false;
            }
        }
    }

    return true;
}

/*
static bool LibraryDirectoryIterator(const String FullPath, const String RelativePath, const String FileName, u64 FileSize, bool bIsDirectory, void* UserData)
{
    struct _blah_
    {
        String* LibName;
        bool* bFound;
        bool bWithExtension;
    };

    struct _blah_ Data = *(struct _blah_*)UserData;

    if (Data.bWithExtension)
    {
        if (String_IsEqual(FileName, *Data.LibName, false))
        {
            *Data.bFound = true;
            return false;
        }
    
        return true;
    }

    bool bFileMatch = false;
    
    #if PLATFORM_WINDOWS
    bFileMatch = String_EndsWith(FileName, S(".lib"), false);
    #elif PLATFORM_APPLE
    bFileMatch = String_EndsWith(FileName, S(".dylib"), false) ||
                 String_EndsWith(FileName, S(".a"), false);
    #else
    bFileMatch = String_EndsWith(FileName, S(".so"), false) ||
                 String_EndsWith(FileName, S(".a"), false);
    #endif

    if (bFileMatch)
    {
        u32 LastDot = 0;
        String_IndexOfLastChar(FileName, '.', &LastDot);

        if (String_IsEqual(StrSlice(FileName.Data, LastDot), *Data.LibName, false))
        {
            *Data.bFound = true;
            return false;
        }
    }

    return true;
}
*/

static bool MultipleBuildFileDirectoryIterator(const String FullPath, const String RelativePath, const String FileName, u64 FileSize, bool bIsDirectory, void* UserData)
{
    UNUSED_PARAM(FullPath);
    UNUSED_PARAM(FileSize);
    UNUSED_PARAM(bIsDirectory);
    UNUSED_PARAM(UserData);

    if (IsBuildFile(FileName))
    {
        static u8 i = 0;
        LOG("    [%hhu] %S", i, RelativePath);
        i++;
    }

    return true;
}

static bool BuildFileDirectoryIterator(const String FullPath, const String RelativePath, const String FileName, u64 FileSize, bool bIsDirectory, void* UserData)
{
    UNUSED_PARAM(RelativePath);
    UNUSED_PARAM(bIsDirectory);
    
    if (FileSize > 0)
    {
        BuildFileDirectoryIteratorData* Data = UserData;

        if ((IsBuildFile(FileName) && !Data->bSearchOnlyBuildBatch) || (Data->bSearchOnlyBuildBatch && IsBuildBatchFile(FileName)))
        {
            if (NEVER(Data->Name == NULL       || Data->Path == NULL)) { return false; }
            if (NEVER(Data->Name->Data == NULL || Data->Path->Data == NULL)) { return false; }

            if (String_StartsWith(FileName, S("__"), false))
            {
                if (Data->bNoBuildFileSpecifiedInCmd) // maybe people wanna explicitly specify the build file if they type it in the command line, so don't ignore it
                {
                    return true;
                }
            }

            if (Data->bNoBuildFileSpecifiedInCmd ||
                Data->Name->Length == 0 ||
                String_IsEqual(FileName, *Data->Name, false))
            {
                Data->bFoundBuildFile = true; // found build file?

                if (Data->NumBuildFilesFound > 0)
                {
                    String_Empty(Data->Name);
                    String_Empty(Data->Path);
                }
                else
                {
                    String_Copy(Data->Name, FileName);
                    String_Copy(Data->Path, FullPath);
                }

                Data->NumBuildFilesFound++;

                if (!Data->bNoBuildFileSpecifiedInCmd)
                {
                    return false;
                }
            }
        }
    }

    return true;
}

static bool PathFlagDirectoryIterator(const String FullPath, const String RelativePath, const String FileName, u64 FileSize, bool bIsDirectory, void* UserData)
{
    UNUSED_PARAM(FullPath);
    UNUSED_PARAM(FileName);
    UNUSED_PARAM(FileSize);

    if (NEVER(UserData == NULL)) { return false; }

    if (bIsDirectory)
    {
        STRUCT(PathIterData)
        {
            String BaseDirectory;
            String Postfix;
            String* Flags;
        };

        PathIterData* Data = UserData;

        String_Append(Data->Flags, Data->BaseDirectory);
        String_BuildPath(Data->Flags, RelativePath, Data->Postfix);
        xx String_EatSpacesInlineFromEnd(Data->Flags);
        String_AppendSpace(Data->Flags);
    }

    return true;
}

static bool EnforceCopyright(const BuildParams* Params, CopyrightEnforceInfo* AuxData, const String RelativePath)
{
    StringLocal(FullPath, MAX_PATH_LENGTH);
    String_BuildPath(&FullPath, Params->RootDirectory, Params->SourceDirectory, RelativePath);

    u32 LineNum = 0;
    bool bSuccess = false;
    bool bFileSuccess = false;
    FileHandle f = FileHandle_Null();
    if (Filesystem_Open(FullPath, FileMode_Read, &f))
    {
        bFileSuccess = true;

        StringLocal(Line, 4096);
        while (Filesystem_ReadLine(f, &Line))
        {
            LineNum++;

            if (LineNum < AuxData->FromLine)
            {
                continue;
            }

            if (LineNum > AuxData->ToLine)
            {
                break;
            }

            if (String_Contains(Line, AuxData->Content, false))
            {
                bSuccess = true;
                break;
            }
        }

        Filesystem_Close(&f);
    }

    bool bContinueSearch = true;

    // if failed to open file, skip checking this file
    if (bFileSuccess && !bSuccess)
    {
        StringLocal(LineInfo, 32);
        if (AuxData->FromLine == AuxData->ToLine)
        {
            String_Format(&LineInfo, S("line %u"), AuxData->FromLine);
        }
        else
        {
            bool bEnd = AuxData->ToLine == UINT32_MAX;
            if (bEnd)  { String_Format(&LineInfo, S("lines %u - End Of File"), AuxData->FromLine); }
            if (!bEnd) { String_Format(&LineInfo, S("lines %u - %u"), AuxData->FromLine, AuxData->ToLine); }
        }

        LOG_ERROR("Source file \"%S\" does not contain the required copyright notice on %S\n", RelativePath, LineInfo);
        LOG("    Expected this on %S: %S", LineInfo, AuxData->Content);
        bContinueSearch = false;
    }

    return bContinueSearch;
}

static void LogOptionData_WordWrapped(LinearAllocator Scratch, const String Name, u32 NamePadding, const String Value, u32 ValuePadding, const String Params, u32 ParamPadding, const String Description)
{
    u32 Rows = 30, Cols = 1000;
    xx Platform_GetTerminalDimensions(&Rows, &Cols);
    Cols = ClampU32(Cols, 30, 1000);

    StringLocal(LogBuffer, 2048);

    StringLocal(Spaces, MAX_KEY_LENGTH);
    Spaces.Length = NamePadding;
    String_Fill(&Spaces, ' ');

    StringLocal(Spaces2, MAX_KEY_LENGTH);
    Spaces2.Length = ValuePadding;
    String_Fill(&Spaces2, ' ');

    u32 BaseLength = 4 + Name.Length + NamePadding + 3 + Value.Length + ValuePadding + 4;
    u32 FirstPipe  = 4 + Name.Length + NamePadding + 3;
    u32 SecondPipe = FirstPipe + 2 + Value.Length + ValuePadding;

    u32 TerminalWidth = (u32)((f32)Cols/1.2);

    if ((Description.Length + BaseLength) > TerminalWidth)
    {
        u32 Fit = (TerminalWidth-BaseLength);
        u32 Loops = (u32)((f32)Description.Length / (f32)Fit);

        String_Append(&LogBuffer, StrSlice(Description.Data, Fit));

        for (u32 i = 0; i < Loops; i++)
        {
            String_AppendNewline(&LogBuffer);
            StringLocal(Padding, 256);
            Padding.Length = BaseLength;
            String_Fill(&Padding, ' ');
            Padding.Data[FirstPipe] = '|';
            Padding.Data[SecondPipe] = '|';
            String_Append(&LogBuffer, Padding);
            String_Append(&LogBuffer, StrSlice(StrShiftF(Description, Fit*(i+1)).Data, Fit));
        }
    }
    else
    {
        LogBuffer = Description;
    }

    if (ValuePadding) // we have defaults to show...
    {
        if (Params.Length)
        {
            LOG("    %S=[%S]%S| %S%S| %S", Name, Params, Spaces, Value, Spaces2, LogBuffer);
        }
        else
        {
            LOG("    %S%S   | %S%S| %S", Name, Spaces, Value, Spaces2, LogBuffer);
        }
    }
    else
    {
        if (Params.Length)
        {
            LOG("    %S=[%S]%S| %S", Name, Params, Spaces, LogBuffer);
        }
        else
        {
            LOG("    %S%S| %S", Name, Spaces, LogBuffer);
        }
    }
}

bool LogStringList_WordWrapped(LinearAllocator Scratch, const String Name, const StringList List)
{
    StringList History = {0};
    u32 ParentCount = 0;

    u32 Rows = 0, Cols = 0;
    xx Platform_GetTerminalDimensions(&Rows, &Cols);
    Cols = ClampU32(Cols, 30, 1000);

    StringLocal(LogBuffer, UINT16_MAX);
    String_Append(&LogBuffer, Name);

    for each_str_list (List)
    {
        StringList ValueList = String_SplitIntoList(&Scratch, It.String, ' ', true);

        u32 Count = 0;
        u32 Spaces = 0;
        u32 Index = 0;

        u32 Num = 0;
        for each_str_list_it (_, ValueList) { Num += 1; }

        for each_str_list_it (v, ValueList)
        {
            if (v.String.Length + ParentCount + Spaces > (u32)((f32)Cols/1.5f))
            {
                StringList** Next = &History.Next;
                while (*Next)
                {
                    const String Slice = String_EatSpacesFromEnd((*Next)->String);
                    String_Append(&LogBuffer, Slice);
                    String_AppendSpace(&LogBuffer);

                    Next = &(*Next)->Next;
                }

                History = (StringList){0};

                String_AppendNewline(&LogBuffer);

                String NameCopy = String_Reserve(&Scratch, Name.Length);
                NameCopy.Length = Name.Length;
                String_Fill(&NameCopy, ' ');

                String_Append(&LogBuffer, NameCopy);

                Count = 0;
                ParentCount = 0;
                Spaces = 0;
            }

            Count += v.String.Length;
            ParentCount += v.String.Length;
            if (Index != Num-1)
            {
                Spaces++; // for the spaces in between
            }
            Index++;

            StringList* Entry = LinearAllocator_Allocate(&Scratch, sizeof(StringList));
            Entry->String = v.String;
            Entry->Next = NULL;

            StringList** Next = &History.Next;
            while (*Next)
            {
                Next = &(*Next)->Next;
            }

            *Next = Entry;
        }

        if (Count > 0)
        {
            StringList** Next = &History.Next;
            while (*Next)
            {
                const String Slice = String_EatSpacesFromEnd((*Next)->String);
                String_Append(&LogBuffer, Slice);
                String_AppendSpace(&LogBuffer);

                Next = &(*Next)->Next;
            }

            History = (StringList){0};
        }
    }

    bool bLogged = LogBuffer.Length > 0;
    if (bLogged)
    {
        LOG_INLINE("%S\n", LogBuffer);
    }

    return bLogged;
}

#if !NO_PRINT_BUILD_CONFIG
static void LogNameValuePair(LinearAllocator Scratch, const String Name, const String Value, const bool bWordWrap)
{
    if (Value.Length > 0)
    {
        if (bWordWrap)
        {
            LogString_WordWrapped(Scratch, Name, Value, true);
        }
        else
        {
            LOG("%S%S", Name, Value);
        }
    }
}
#endif

void LogString_WordWrapped(LinearAllocator Scratch, const String Name, const String Value, const bool bAddNewLine)
{
    if (Value.Length > 0)
    {
        const StringList l = {Value, NULL};
        if (LogStringList_WordWrapped(Scratch, Name, l))
        {
            if (bAddNewLine) { LOG_LINE_BREAK(); }
        }
    }
}

static void ListVariables(LinearAllocator Arena, const String Name, TArray(FileVariable) VariablesDB) 
{
    const String Exclusions[31] =
    {
        S("Assert.ProgramExists"),
        S("Assert.LibExists"),
        S("Assert.BuildVarExists"),
        S("Assert.WorkingDirectory"),
        S("Assert.Arg"),
        S("Assert.EnvVarExists"),
        S("Assert.Platform"),
        S("PreDepend"),
        S("PreBuild"),
        S("PostBuild"),
        S("PreCompile"),
        S("PostCompile"),
        S("PreLink"),
        S("PostLink"),
        S(".Run"),
        S("Depend"),
        S("Depends"),
        S("Assembly"),
        S("Extension"),
        S("Type"),
        S("TitleName"),
        S("Description"),
        S("CompanyName"),
        S("Version"),
        S("Copyright"),
        S("SourceDirectory"),
        S("IntermediateDirectory"),
        S("BuildDirectory"),
        S("Icon"),
        S("Option."),
        S(".Help"),
    };

    for each (FileVariable, v, VariablesDB)
    {
        if (Name.Length == 0 || String_IsEqual(v.Name, Name, false))
        {
            LOG_INLINE_WARNING("%S\n", v.Name);

            bool bOneLine = false;
            for (u32 j = 0; j < SArray_Capacity(Exclusions); j++)
            {
                if (String_StartsWith(v.Name, Exclusions[j], false))
                {
                    bOneLine = true;
                    break;
                }
            }

            if (!bOneLine)
            {
                bOneLine = String_EndsWith(v.Name, S(".errormessage"), false);
            }
            
            if (bOneLine)
            {
                LOG("    %S", v.Value);
            }
            else
            {
                StringList Values = String_SplitIntoList(&Arena, v.Value, ' ', true);
                for each_str_list (Values)
                {
                    LOG("    %S", It.String);
                }
            }
        
            LOG_LINE_BREAK();
        }
    }
}

static bool Internal_ExecuteBuildCmd(const String WorkingDirectory, const FileVariable Var, u32* ExitCode)
{
    bool bSuccess = true;

    const String Params = Var.Params;
    const String Name   = Var.Name;
    const String Value  = Var.Value;

    if (ALWAYS(String_IsValid(Name)) && ALWAYS(String_IsValid(Value)))
    {
        LinearAllocator Scratch = Memory_GetScratch();
        StringList ParamList    = String_SplitIntoList(&Scratch, Params, ' ', true);

        bool bIgnoreErrors  = false;
        bool bParam_NotExist = false;
        for each_string_in_list (ParamList)
        {
            if (String_IsEqual(It.String, S("ignore_exit_code"), false) ||
                String_IsEqual(It.String, S("ignore_error"), false) ||
                String_IsEqual(It.String, S("ignore_errors"), false))
            {
                bIgnoreErrors = true;
            }
            else if (String_IsEqual(It.String, S("if_not_exist"), false))
            {
                bParam_NotExist = true;
            }
        }

        if (String_EndsWith(Name, S("Cmd"), false) ||
            String_EndsWith(Name, S("Exec"), false) ||
            String_EndsWith(Name, S("Command"), false) ||
            String_EndsWith(Name, S("Execute"), false))
        {
            const String Cmd = Value;

            StringLocal(CmdLine, 8192);

            #if PLATFORM_WINDOWS
            String_Append(&CmdLine, S("cmd.exe /c \""));
            String_Append(&CmdLine, Cmd);
            String_AppendChar(&CmdLine, '"');
            #else
            String_Append(&CmdLine, Cmd);
            #endif

            #ifndef HOOD
            LOG(" > %S", Cmd);
            #else
            LOG("da cmd: %S", Cmd);
            #endif

            bool bNoWait = false;
            for each_string_in_list (ParamList)
            {
                if (String_IsEqual(It.String, S("no_wait"), false))
                {
                    bNoWait = true;
                    break;
                }
            }

            PlatformHandle Handle = Platform_RunCommand(CmdLine, WorkingDirectory, String_Null());
            u32 ProcessCode = 1;
            if (Platform_IsValidHandle(Handle))
            {
                if (!bNoWait)
                {
                    ProcessCode = Platform_WaitForProcessAndGetExitCode(Handle);
                }
            }

            if (ExitCode) { *ExitCode = ProcessCode; }

            if (ProcessCode != 0 && !bIgnoreErrors)
            {
                bSuccess = false;
            }
        }
        else if (String_EndsWith(Name, S("Copy"), false))
        {
            const String Cmd = Value;

            StringList ArgList          = String_SplitIntoList(&Scratch, Value, ' ', true);
            String SourceFile           = StringList_GetStringFromIndex(ArgList, 0);
            String DestinationDirectory = StringList_GetStringFromIndex(ArgList, 1);

            StringLocal(FullSourcePath, MAX_PATH_LENGTH);
            if (Filesystem_IsPathRelative(SourceFile))
            {
                String_BuildPath(&FullSourcePath, WorkingDirectory, SourceFile);
            }
            else
            {
                String_BuildPath(&FullSourcePath, SourceFile);
            }

            xx Filesystem_ConvertRelativeToAbsolutePath(&FullSourcePath);

            StringLocal(FullDestPath, MAX_PATH_LENGTH);
            String_BuildPath(&FullDestPath, WorkingDirectory, DestinationDirectory);

            // TODO: remove, handle on linux/mac/bsd. already done on windows
            if (String_IsLast(DestinationDirectory, '/') ||
                String_IsLast(DestinationDirectory, '\\'))
            {
                u32 LastSlash = 0;
                xx String_IndexOfLastPathSlash(SourceFile, &LastSlash);
                String_BuildPath(&FullDestPath, StrShiftF(SourceFile, LastSlash == 0 ? 0 : LastSlash+1));
            }

            xx Filesystem_ConvertRelativeToAbsolutePath(&FullDestPath);

            // only copy if dest does not exist
            bool bDestExist = false;
            if (bParam_NotExist)
            {
                u32 LastDot = 0;
                xx String_IndexOfLastChar(FullDestPath, '.', &LastDot);

                bool bHasExtension = false;
                bool bHasPathSeparator = String_IndexOfFirstPathSlash(StrShiftF(FullDestPath, LastDot), NULL);
                if (!bHasPathSeparator)
                {
                    bHasExtension = true;
                }

                if (bHasExtension)
                {
                    if (Filesystem_DoesFileExist(FullDestPath))
                    {
                        LOG("[Skipping] Copy: %S", Cmd);
                        bDestExist = true;
                    }
                }
                else
                {
                    if (Filesystem_DoesDirectoryExist(FullDestPath))
                    {
                        LOG("[Skipping] Copy: %S", Cmd);
                        bDestExist = true;
                    }
                }
            }

            bool bContinueWithCopy = !bDestExist;
            if (bContinueWithCopy)
            {
                LOG(" > Copy: %S", Cmd);

                if (!Filesystem_Copy(FullSourcePath, FullDestPath) && !bIgnoreErrors)
                {
                    LOG(
                    "\n    You can ignore this error by using .Copy(ignore_error)\n"
                    "    or you can use .Copy(if_not_exist) to check whether the file exists\n"
                    "    and gracefully skip the copy operation if they don't.\n");

                    if (ExitCode) { *ExitCode = 1; }
                    bSuccess = false;
                }
            }
        }
        else if (String_EndsWith(Name, S("Wait"), false) ||
                String_EndsWith(Name, S("Sleep"), false))
        {
            u32 Milliseconds = 0;
            xx String_ToU32(Value, &Milliseconds);

            LOG(" > Sleeping for %ums ...", Milliseconds);

            Platform_Sleep(Milliseconds);
        }
        else if (String_EndsWith(Name, S("Move"), false) ||
                String_EndsWith(Name, S("Rename"), false))
        {
            // TODO: only deal with relative paths?

            const String Cmd = Value;

            StringList ArgList          = String_SplitIntoList(&Scratch, Value, ' ', true);
            String SourceFile           = StringList_GetStringFromIndex(ArgList, 0);
            String DestinationDirectory = StringList_GetStringFromIndex(ArgList, 1);

            bool bIsRename = String_EndsWith(Name, S("Rename"), false);
            if (bIsRename)
            {
                LOG(" > Rename: %S", Cmd);
            }
            else
            {
                LOG(" > Move: %S", Cmd);
            }

            // only move if dest does not exist
            bool bDestExist = false;
            if (bParam_NotExist)
            {
                u32 LastSlash = 0;
                bool bHasSlash = String_IndexOfLastPathSlash(SourceFile, &LastSlash);

                bool bCanWe = true;
                
                StringLocal(FullPath, MAX_PATH_LENGTH);
                if (Filesystem_IsPathRelative(SourceFile))
                {
                    String_BuildPath(&FullPath, WorkingDirectory, bHasSlash ? StrSlice(SourceFile.Data, LastSlash) : SourceFile);
                }
                else
                {
                    String_BuildPath(&FullPath, SourceFile);
                }

                if (!Filesystem_DoesDirectoryExist(FullPath))
                {
                    bCanWe = false;
                }

                if (bCanWe)
                {
                    String_Empty(&FullPath);
                    if (Filesystem_IsPathRelative(SourceFile))
                    {
                        String_BuildPath(&FullPath, WorkingDirectory, SourceFile);
                    }
                    else
                    {
                        String_BuildPath(&FullPath, SourceFile);
                    }

                    if (!Filesystem_DoesFileExist(FullPath))
                    {
                        bCanWe = false;
                    }
                }
                    
                if (bCanWe)
                {
                    String_Empty(&FullPath);
                    String FileName = StrShiftF(SourceFile, LastSlash == 0 ? 0 : LastSlash+1);
                    String_BuildPath(&FullPath, WorkingDirectory, DestinationDirectory, FileName);

                    u32 LastDot = 0;
                    bool bHasDot = String_IndexOfLastChar(SourceFile, '.', &LastDot);

                    bool bHasExtension = false;
                    bool bHasPathSeparator = String_IndexOfFirstPathSlash(StrShiftF(SourceFile, LastDot), NULL);
                    if (bHasDot && !bHasPathSeparator)
                    {
                        bHasExtension = true;
                    }

                    if (bHasExtension)
                    {
                        if (Filesystem_DoesFileExist(FullPath))
                        {
                            bDestExist = true;
                        }
                    }
                    else
                    {
                        if (Filesystem_DoesDirectoryExist(FullPath))
                        {
                            bDestExist = true;
                        }
                    }
                }
                else
                {
                    bDestExist = true; // skip the move/rename if the given source file does not exist
                }
            }

            bool bContinueWithMove = !bDestExist;
            if (bContinueWithMove)
            {
                StringLocal(FullSourcePath, MAX_PATH_LENGTH);
                String_BuildPath(&FullSourcePath, WorkingDirectory, SourceFile);

                StringLocal(FullDestPath, MAX_PATH_LENGTH);
                String_BuildPath(&FullDestPath, WorkingDirectory, DestinationDirectory);

                if (bIgnoreErrors) { Logging_Disable(); }
                bool bResult = Filesystem_Move(FullSourcePath, FullDestPath, bIsRename);
                if (bIgnoreErrors) { Logging_Enable(); }

                if (!bResult && !bIgnoreErrors)
                {
                    LOG(
                    "\n    You can ignore this error by using %S instead\n"
                    "    or you can use %S to check whether the source files exist\n"
                    "    and gracefully skip the move operation if they don't.\n",
                    bIsRename ? S(".Rename(ignore_error)") : S(".Move(ignore_error)"),
                    bIsRename ? S(".Rename(if_not_exist)") : S(".Move(if_not_exist)"));

                    if (ExitCode) { *ExitCode = 1; }
                    bSuccess = false;
                }
            }
        }
        else if (String_EndsWith(Name, S("Delete"), false))
        {
            const String Cmd = Value;

            LOG(" > Delete: %S", Cmd);

            // todo: make sure we only delete stuff relative to the working directory

            if (String_IsEqual(Cmd, S("*"), false) ||
                String_IsEqual(Cmd, S("."), false) ||
                String_IsEqual(Cmd, S(".."), false) ||
                String_IsEqual(Cmd, S("/"), false))
            {
                bSuccess = false;
            }

            // todo: handle wildcards?

            if (bSuccess)
            {
                u32 LastDot = 0;
                bool bHasDot = String_IndexOfLastChar(Cmd, '.', &LastDot);

                bool bHasExtension = false;
                bool bHasPathSeparator = String_IndexOfFirstPathSlash(StrShiftF(Cmd, LastDot), NULL);
                if (bHasDot && !bHasPathSeparator)
                {
                    bHasExtension = true;
                }

                StringLocal(FullFilePath, MAX_PATH_LENGTH);
                String_BuildPath(&FullFilePath, WorkingDirectory, Cmd);

                if (bIgnoreErrors) { Logging_Disable(); }
                bool bResult = false;
                if (bHasExtension)
                {
                    if (Filesystem_DoesFileExist(FullFilePath))
                    {
                        bResult = Filesystem_DeleteFile(FullFilePath);
                    }
                    else
                    {
                        bResult = true;
                    }
                }
                else
                {
                    if (Filesystem_DoesDirectoryExist(FullFilePath))
                    {
                        bResult = Filesystem_DeleteDirectory(FullFilePath);
                    }
                    else
                    {
                        bResult = true;
                    }
                }
                if (bIgnoreErrors) { Logging_Enable(); }

                if (!bResult && !bIgnoreErrors)
                {
                    if (ExitCode) { *ExitCode = 1; }
                    bSuccess = false;
                }
            }
        }
        else if (String_EndsWith(Name, S("NewFile"), false))
        {
            const String Cmd = Value;

            LOG(" > New File: %S", Cmd);

            StringLocal(FullFilePath, MAX_PATH_LENGTH);
            String_BuildPath(&FullFilePath, WorkingDirectory, Cmd);

            if (bIgnoreErrors) { Logging_Disable(); }
            bool bResult = Filesystem_NewFile(FullFilePath);
            if (bIgnoreErrors) { Logging_Enable(); }

            if (!bResult && !bIgnoreErrors)
            {
                if (ExitCode) { *ExitCode = 1; }
                bSuccess = false;
            }
        }
        else if (String_EndsWith(Name, S("NewDirectory"), false) ||
                String_EndsWith(Name, S("NewDir"), false))
        {
            const String Cmd = Value;

            LOG(" > New Directory: %S", Cmd);

            StringLocal(FullDirPath, MAX_PATH_LENGTH);
            String_BuildPath(&FullDirPath, WorkingDirectory, Cmd);

            if (bIgnoreErrors) { Logging_Disable(); }
            bool bResult = Filesystem_OpenDirectory(FullDirPath);
            if (bIgnoreErrors) { Logging_Enable(); }

            if (!bResult && !bIgnoreErrors)
            {
                if (ExitCode) { *ExitCode = 1; }
                bSuccess = false;
            }
        }
        else if (String_EndsWith(Name, S("Log"), false))
        {
            if (Value.Length == 0)
            {
                LOG_LINE_BREAK();
            }
            else
            {
                LOG(" %S\n", Value);
            }
        }
        else if (String_EndsWith(Name, S("WriteFile"), false) ||
                String_EndsWith(Name, S("WriteFileLines"), false))
        {
            bSuccess = false;
            UNIMPLEMENTED;
        }
        else if (String_EndsWith(Name, S("Download"), false))
        {
            StringList ArgList = String_SplitIntoList(&Scratch, Value, ' ', true);
            String URL         = StringList_GetStringFromIndex(ArgList, 0);
            String Destination = StringList_GetStringFromIndex(ArgList, 1);

            // TODO:     PreBuild.Download https://github.com/glfw/glfw/releases/download/3.4/glfw-3.4.zip glfw.zip
            // make it so that we dont have to specify .zip. if we want directory, a slash will be necessary.
            // like this: ./deps or whatever/deps


            StringLocal(FinalDestinationPath, MAX_PATH_LENGTH);
            String_BuildPath(&FinalDestinationPath, WorkingDirectory, Destination);

            if (!Filesystem_DoesPathHaveFileExtension(Destination))
            {
                if (Filesystem_OpenDirectory(FinalDestinationPath))
                {
                    const String FileName = Filesystem_ExtractFileName(URL, true);
                    String_BuildPath(&FinalDestinationPath, FileName);
                }
                else
                {
                    bSuccess = false;
                }
            }

            if (bSuccess)
            {
                LOG(" > Download: %S\n -> Destination: %S", URL, FinalDestinationPath);

                if (Filesystem_DoesFileExist(FinalDestinationPath))
                {
                    LOG("    File already exists. Skipping download...\n");
                }
                else
                {
                    StringLocal(CmdLine, 8192);
                    
                    #if PLATFORM_WINDOWS
                    String_Concat(&CmdLine, S("powershell -Command \"(New-Object Net.WebClient).DownloadFile('"), URL, S("', '"), FinalDestinationPath, S("')\""));
                    #else
                    UNIMPLEMENTED;
                    #endif

                    if (bVerboseLog) { LOG("    %S", CmdLine); }

                    PlatformHandle H = Platform_RunCommand(CmdLine, WorkingDirectory, String_Null());
                    bSuccess = Platform_IsValidHandle(H);
                    if (bSuccess)
                    {
                        u32 ProcessCode = Platform_WaitForProcessAndGetExitCode(H);
                        if (ExitCode) { *ExitCode = ProcessCode; }

                        if (ProcessCode != 0 && !bIgnoreErrors)
                        {
                            bSuccess = false;
                        }
                        else
                        {
                            LOG_LINE_BREAK();
                        }
                    }
                }
            }
        }
        else if (String_EndsWith(Name, S(".Unzip"), false))
        {
            StringList ArgList = String_SplitIntoList(&Scratch, Value, ' ', true);
            String ZipFilePath = StringList_GetStringFromIndex(ArgList, 0);
            String Destination = StringList_GetStringFromIndex(ArgList, 1);

            if (!Filesystem_DoesFileExist(ZipFilePath))
            {
                LOG_ERROR("Zip file \"%S\" does not exist", ZipFilePath);
                bSuccess = false;
            }

            StringLocal(FinalDestinationPath, MAX_PATH_LENGTH);
            String_BuildPath(&FinalDestinationPath, WorkingDirectory, Destination);

            if (!Filesystem_OpenDirectory(FinalDestinationPath))
            {
                bSuccess = false;
            }

            if (bSuccess)
            {
                StringLocal(CmdLine, 8192);
                
                #if PLATFORM_WINDOWS
                String_Concat(&CmdLine, S("powershell -Command \"Expand-Archive -Force -Path \"\"\""), ZipFilePath, S("\"\"\" -DestinationPath \"\""), FinalDestinationPath, S("\"\"\""));
                #else
                UNIMPLEMENTED;
                #endif

                LOG(" > Unzip: %S\n -> Destination: %S", ZipFilePath, FinalDestinationPath);

                if (bVerboseLog) { LOG("    %S", CmdLine); }

                PlatformHandle H = Platform_RunCommand(CmdLine, WorkingDirectory, String_Null());
                bSuccess = Platform_IsValidHandle(H);
                if (bSuccess)
                {
                    u32 ProcessCode = Platform_WaitForProcessAndGetExitCode(H);
                    if (ExitCode) { *ExitCode = ProcessCode; }

                    if (ProcessCode != 0 && !bIgnoreErrors)
                    {
                        bSuccess = false;
                    }
                    else
                    {
                        LOG_LINE_BREAK();
                    }
                }
            }
        }
        else if (String_EndsWith(Name, S(".Zip"), false))
        {
            StringList ArgList = String_SplitIntoList(&Scratch, Value, ' ', true);
            String FilePath    = StringList_GetStringFromIndex(ArgList, 0);
            String Destination = StringList_GetStringFromIndex(ArgList, 1);

            xx String_EatPathSeparatorsInlineFromEnd(&FilePath);

            if (!Filesystem_DoesFileExist(FilePath) &&
                !Filesystem_DoesDirectoryExist(FilePath))
            {
                LOG_ERROR("File path \"%S\" does not exist", FilePath);
                bSuccess = false;
            }

            StringLocal(FinalDestinationPath, MAX_PATH_LENGTH);
            String_BuildPath(&FinalDestinationPath, WorkingDirectory, Destination);

            if (bSuccess)
            {
                if (!String_EndsWith(Destination, S(".zip"), false))
                {
                    String FileName = FilePath;
                    u32 LastSlash = 0;
                    if (String_IndexOfLastPathSlash(FilePath, &LastSlash))
                    {
                        FileName = StrShiftF(FilePath, LastSlash+1);
                    }

                    String_BuildPath(&FinalDestinationPath, FileName);
                    String_Append(&FinalDestinationPath, S(".zip"));
                }

                u32 LastSlash = 0;
                if (String_IndexOfLastPathSlash(FinalDestinationPath, &LastSlash))
                {
                    if (!Filesystem_OpenDirectory(StrSlice(FinalDestinationPath.Data, LastSlash)))
                    {
                        bSuccess = false;
                    }
                }
            }

            if (bSuccess)
            {
                StringLocal(CmdLine, 8192);
                
                #if PLATFORM_WINDOWS
                const bool bFilePathHasExtension = Filesystem_DoesPathHaveFileExtension(FilePath);
                String_Concat(&CmdLine, S("powershell -Command \"Compress-Archive -Force -Path \"\"\""), FilePath, bFilePathHasExtension ? String_Null() : S("\\*"),  S("\"\"\" -DestinationPath \"\""), FinalDestinationPath, S("\"\"\""));
                #else
                UNIMPLEMENTED;
                #endif

                LOG(" > Zip: %S\n -> Destination: %S", FilePath, FinalDestinationPath);

                if (bVerboseLog) { LOG("    %S", CmdLine); }

                PlatformHandle H = Platform_RunCommand(CmdLine, WorkingDirectory, String_Null());
                bSuccess = Platform_IsValidHandle(H);
                if (bSuccess)
                {
                    u32 ProcessCode = Platform_WaitForProcessAndGetExitCode(H);
                    if (ExitCode) { *ExitCode = ProcessCode; }

                    if (ProcessCode != 0 && !bIgnoreErrors)
                    {
                        bSuccess = false;
                    }
                    else
                    {
                        LOG_LINE_BREAK();
                    }
                }
            }
        }
        // TODO: implement this
        else if (String_EndsWith(Name, S(".Export"), false))
        {
            if (String_StartsWith(Name, S("PreDepend"), false) ||
                String_StartsWith(Name, S("PreBuild"), false))
            {
                u32 Dot = 0;
                xx String_IndexOfChar(Name, '.', &Dot);
                String Key = StrSlice(Name.Data, Dot);
                LOG_WARNING("Cannot execute the \".Export\" command under the \"%S\" context", Key);
                LOG("\".Export\" can only be executed under these contexts:");
                LOG("\n    PreCompile\n    PostCompile\n    PreLink\n    PostLink\n    PostBuild");

                bSuccess = false;
            }

            //Export_FromArg();
        }
        else
        {
            // no action required
        }
    }

    return bSuccess;
}

static bool TryRunBuildCommands(const String Key, const String WorkingPath, TArray(FileVariable) VariablesDB, Clock* Timer)
{
    bool bSuccess = true;

    FileVariable* Cmds[64] = {0}; // reasonable max limit

    u8 NumCmds = 0;
    for each (FileVariable, Var, VariablesDB)
    {
        if (String_StartsWith(Var.Name, Key, false))
        {
            Cmds[NumCmds] = Var_;
            NumCmds++;
            if (NumCmds >= 64)
            {
                break;
            }
        }
    }

    if (NumCmds > 0 && !bIsClean)
    {
        // Hack: dont wanna introduce a bool param right now...
        if (String_IsEqual(Key, S("PostBuild"), false)) { LOG_LINE_BREAK(); }

        #ifndef HOOD
        LOG("%S:", Key);
        #else
        LOG("cool mang, gonna run some %S cmds...", Key);
        #endif

        f64 ElapsedSoFar = 0;
        if (Timer)
        {
            ElapsedSoFar = Timer->ElapsedTime;
            Clock_Start(Timer);
        }

        for (u8 i = 0; i < NumCmds; i++)
        {
            const FileVariable* Var = Cmds[i];

            u32 ExitCode = 0;
            bool bResult = Internal_ExecuteBuildCmd(WorkingPath, *Var, &ExitCode);
            if (!bResult)
            {
                #ifndef HOOD
                LOG_ERROR("%S command exited with a failure result: %u", Key, ExitCode);
                #else
                LOG_ERROR("brah wtf, gon have to stop you there nigga. da command we jus run fuck'n failed on me nigga");
                #endif

                bSuccess = false;
                break;
            }
        }

        if (Timer)
        {
            Clock_Tick(Timer);
            Timer->ElapsedTime += ElapsedSoFar;
        }

        LOG_LINE_BREAK();
    }

    return bSuccess;
}

static void Internal_RemoveBuildVariable(TArray(FileVariable) VariablesDB, const String Name)
{
    u32 i = 0;
    for each_i (i, FileVariable, Var, VariablesDB)
    {
        if (String_IsEqual(Var.Name, Name, false))
        {
            Array_RemoveAt(VariablesDB, NULL, i);
            break;
        }
    }
}

static void Internal_AddOrUpdateBuildVariable(TArray(FileVariable) VariablesDB, FileVariable Expanded)
{
    Internal_RemoveBuildVariable(VariablesDB, Expanded.Name);
    Array_Add(VariablesDB, Expanded);
}

static void Internal_SetDefaultBuildVariables(LinearAllocator* Arena, const FileHandle BuildFileHandle, TArray(FileVariable) VariablesDB)//, TArray(FileVariable) VariablesDB)
{
    if (!DoesBuildVarExist(VariablesDB, S("Assembly")))
    {
        StringLocal(Path, MAX_PATH_LENGTH);

        String Name = S("Untitled");

        if (IsValidFileHandle(BuildFileHandle))
        {
            if (Filesystem_GetFilePath(BuildFileHandle, &Path))
            {
                u32 LastSlash = 0;
                bool bHasSlash = String_IndexOfLastPathSlash(Path, &LastSlash);

                String FileName = bHasSlash ? StrShiftF(Path, LastSlash+1) : Path;
                
                u32 LastDot = 0;
                bool bHasDot = String_IndexOfLastChar(FileName, '.', &LastDot);
                FileName = bHasDot ? StrSlice(FileName.Data, LastDot) : FileName;
                if (FileName.Length > 0)
                {
                    Name = FileName;
                }
            }
        }

        FileVariable Expanded;
        Expanded.Name = S("Assembly");
        Expanded.Value = String_Create(Arena, Name);

        Array_Add(VariablesDB, Expanded);
    }

    const String Type = GetVariableValue(VariablesDB, S("Type"));
    if (String_IsValid(Type))
    {
        String Extension = String_Null();
        
        FileVariable Expanded;
        Expanded.Name = S("Extension");
        // // Expanded.bHasSpecial = false;

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
        else if (String_IsEqual(Type, S("static_lib"), false) ||
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
        else if (String_IsEqual(Type, S("shared_lib"), false) ||
                 String_IsEqual(Type, S("shared_library"), false) ||
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
        else if (String_IsEqual(Type, S("app"), false) ||
                 String_IsEqual(Type, S("application"), false) ||
                 String_IsEqual(Type, S("exe"), false) ||
                 String_IsEqual(Type, S("executable"), false))
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

        Expanded.Value = Extension;

        Array_Add(VariablesDB, Expanded);
    }

    if (!DoesBuildVarExist(VariablesDB, S("Extension")))
    {
        FileVariable Expanded;
        Expanded.Name = S("Extension");
        // Expanded.bHasSpecial = false;

        #if PLATFORM_WINDOWS
            Expanded.Value = S(".exe");
        #elif PLATFORM_APPLE
            Expanded.Value = String_Null();
        #else
            Expanded.Value = String_Null();
        #endif

        Array_Add(VariablesDB, Expanded);
    }

    /*
    if (!DoesBuildVarExist(VariablesDB, S("Compiler")))
    {
        FileVariable Expanded;
        Expanded.Name = S("Compiler");
        Expanded.Value = String_Null();

        Array_Add(VariablesDB, Expanded);
    }
    */

    if (!DoesBuildVarExist(VariablesDB, S("Version")))
    {
        FileVariable Expanded;
        Expanded.Name = S("Version");
        Expanded.Value = S("1.0.0");

        Array_Add(VariablesDB, Expanded);
    }

    if (!DoesBuildVarExist(VariablesDB, S("BuildDirectory")))
    {
        FileVariable Expanded;
        Expanded.Name = S("BuildDirectory");
        Expanded.Value = S("Build");

        Array_Add(VariablesDB, Expanded);
    }

    if (!DoesBuildVarExist(VariablesDB, S("IntermediateDirectory")))
    {
        FileVariable Expanded;
        Expanded.Name = S("IntermediateDirectory");
        Expanded.Value = S("Intermediate");

        Array_Add(VariablesDB, Expanded);
    }

    if (!DoesBuildVarExist(VariablesDB, S("SourceDirectory")))
    {
        FileVariable Expanded;
        Expanded.Name = S("SourceDirectory");
        Expanded.Value = String_Null();

        Array_Add(VariablesDB, Expanded);
    }
}

void AddCmdOption(TArray(CmdOption) CmdOptionsDB, const String Name, const String Value)
{
    CmdOption c;
    c.Name = Name;
    c.Value = Value;

    #if RIFT_DEBUG
    ENSURE(Name.Length > 0);
    #endif

    if (Name.Length > 0)
    {
        Array_Add(CmdOptionsDB, c);
    }
}

void AddInternalVariable(const String Name, const String Value)
{
    if (Name.Length > 0)
    {
        InternalVariable c;
        c.Name = Name;
        c.Value = Value;

        Array_Add(InternalVariablesDB, c);
    }
}

static bool CheckForBuildVariableOverrides(TArray(FileVariable) VariablesDB, TArray(CmdOption) CmdOptionsDB)
{
    bool bAnyOverriden = false;

    // check if the user wants to override a build variable
    for each (CmdOption, o, CmdOptionsDB)
    {
        if (String_StartsWith(o.Name, S("override:"), false))
        {
            bool bOverriden = false;
            String VarToOverride = StrShiftF(o.Name, 9);

            for each (FileVariable, Var, VariablesDB)
            {
                if (String_IsEqual(Var.Name, VarToOverride, false))
                {
                    LOG("Overriding existing variable \"%S\" from \"%S\" to \"%S\"", Var.Name, Var.Value, o.Value);
                    Var_->Value = o.Value;
                    bOverriden = true;
                    bAnyOverriden = true;
                    break;
                }
            }

            // add it if not found
            if (!bOverriden)
            {
                LOG("New override: \"%S\" = \"%S\"", VarToOverride, o.Value);

                FileVariable NewOverride;
                NewOverride.Name = VarToOverride;
                NewOverride.Value = o.Value;
                // NewOverride.bHasSpecial = false;
                NewOverride.Params = String_Null();

                Internal_AddOrUpdateBuildVariable(VariablesDB, NewOverride);

                bAnyOverriden = true;
            }
        }
    }

    return bAnyOverriden;
}

void LogPathEnvVarTutorialSteps(void)
{
    #ifdef HOOD
    LOG_INLINE_WARNING("aight lisen up dawg, this is how you put a new entry to the path env var on yo system:\n");
    #else
    LOG_INLINE_WARNING("Here is how to add a new entry to the path environment variable:\n");
    #endif

    #if PLATFORM_WINDOWS
    LOG("    1. Open the start menu and type \"Environment Variables\"");
    LOG("    2. Click on \"Edit the system environment variables\"");
    LOG("    3. Click on \"Environment Variables\"");
    LOG("    4. In the \"System variables\" section, scroll down and select \"Path\"");
    LOG("    5. Click on Edit...");
    LOG("    6. Click on New");
    LOG("    7. Type in the path to the executable. For example: C:\\MyCompiler\\bin");
    LOG("    8. Click on OK until all windows are closed");
    LOG("    9. Restart the command prompt (by closing and opening it again) for changes to take effect");
    #elif PLATFORM_APPLE
    LOG("    1. Open the terminal");
    LOG("    2. Type in sudo nano /etc/paths");
    LOG("    3. Enter your password");
    LOG("    4. Go to the bottom of the list and add the path to the executable\n"
        "       For example: /Users/Bob/MyCompiler/bin");
    LOG("    5. Press Ctrl + X to exit");
    LOG("    6. Press Y to save changes");
    LOG("    7. Press Enter to confirm");
    LOG("    8. Restart the terminal (by closing and opening it again) for changes to take effect");
    #else
    LOG("    1. Open the terminal");
    LOG("    2. Type in nano ~/.bashrc");
    LOG("    3. Go to the bottom of the file and add the path to the executable\n"
        "       For example: export PATH=${PATH}:/Users/Bob/MyCompiler/bin");
    LOG("    4. Press Ctrl + X to exit");
    LOG("    5. Press Y to save changes");
    LOG("    6. Press Enter to confirm");
    LOG("    7. Restart the terminal (by closing and opening it again) for changes to take effect");
    #endif
}

void LogRegularEnvVarTutorialSteps(void)
{
    #ifdef HOOD
    LOG_INLINE_WARNING("aight lisen up dawg, this is how you put a new env var on yo system:\n");
    #else
    LOG_INLINE_WARNING("Here is how to add a new environment variable:\n");
    #endif

    #if PLATFORM_WINDOWS
    LOG("    1. Open the start menu and type \"Environment Variables\"");
    LOG("    2. Click on \"Edit the system environment variables\"");
    LOG("    3. Click on \"Environment Variables\"");
    LOG("    4. In either the \"User variables\" or \"System variables\" section, click on New");
    LOG("    5. Type in the name of the variable. For example: MY_COOL_VARIABLE");
    LOG("    6. Type in the value of the variable. For example: my_value");
    LOG("    7. Click on OK until all windows are closed");
    LOG("    8. Restart the command prompt (by closing and opening it again) for changes to take effect");
    #elif PLATFORM_APPLE
    LOG("    1. Open a terminal");
    LOG("    2. Type in sudo nano ~/.zshrc");
    LOG("    3. Enter your password");
    LOG("    4. Go to the bottom of the file and add the variable\n"
        "       For example: export MY_COOL_VARIABLE=Some_useful_value");
    LOG("    5. Press Ctrl + X to exit");
    LOG("    6. Press Y to save changes");
    LOG("    7. Press Enter to confirm");
    LOG("    8. Restart the terminal (by closing and opening it again) for changes to take effect");
    #else
    LOG("    1. Open a terminal");
    LOG("    2. Type in nano ~/.bashrc");
    LOG("    3. Go to the bottom of the file and add the variable\n"
        "       For example: export MY_COOL_VARIABLE=Some_useful_value");
    LOG("    4. Press Ctrl + X to exit");
    LOG("    5. Press Y to save changes");
    LOG("    6. Press Enter to confirm");
    LOG("    7. Restart the terminal (by closing and opening it again) for changes to take effect");
    #endif
}

static bool BuildFilesIterator(const String FullPath, const String RelativePath, const String FileName, u64 FileSize, bool bIsDirectory, void* UserData)
{
    UNUSED_PARAM(FullPath);
    UNUSED_PARAM(FileName);
    UNUSED_PARAM(bIsDirectory);
    UNUSED_PARAM(UserData);

    if (FileSize > 0)
    {
        if (IsBuildFile(RelativePath) || IsBuildBatchFile(RelativePath))
        {
            LOG("   %S", RelativePath);
            /*
            LOG("       Usage:       ");
            LOG("       Description: ");
            LOG("       Options:       ");
            LOG("       Presets:     ");
            LOG_LINE_BREAK();
            */
        }
    }

    return true;
}

static void PrintAbout(void)
{
    LOG_INLINE_WARNING("About\n");

    #if COMPILER_MSVC
    String CompiledWith = S("MSVC " STRINGIFY(_MSC_FULL_VER));
    #elif COMPILER_CLANG
    String CompiledWith = S("Clang " __clang_version__);
    #elif COMPILER_GCC
    String CompiledWith = S("GCC " STRINGIFY(__GNUC__) "." STRINGIFY(__GNUC_MINOR__) "." STRINGIFY(__GNUC_PATCHLEVEL__));
    #elif COMPILER_TCC
    String CompiledWith = S("TCC " STRINGIFY(__TINYC__));
    #else
    String CompiledWith = S("Unknown Compiler");
    #endif

    SystemTime TimeNow = Platform_GetSystemLocalTime();

    LOG("   A simpler build tool for C/C++, because fuck CMake.\n");
    LOG("   Copyright (c) %hu Artisan Softworks", TimeNow.Year);
    LOG("   Licensed under the BSD 3-Clause License. See the LICENSE file for details.\n");
    LOG("   Compiled with %S on %S", String_EatSpacesFromEnd(CompiledWith), S(__DATE__ " " __TIME__));
    LOG_LINE_BREAK();
    LOG("   Repository Link: https://github.com/AliElSaleh/RiftBuild");
    LOG("   Contact E-Mail:  elsaleh78@gmail.com");
    LOG_LINE_BREAK();
    LOG("   Submit a request, issue or bug report: https://github.com/AliElSaleh/RiftBuild/issues");
}

static void PrintInternals(void)
{
    LOG_INLINE_WARNING("Internals\n");

    u32 LongestName = 0;

    for each (InternalVariable, v, InternalVariablesDB)
    {
        if (v.Name.Length > LongestName)
        {
            LongestName = v.Name.Length;
        }
    }

    for each (InternalVariable, v, InternalVariablesDB)
    {
        LOG("   %S %*S", v.Name, (LongestName-v.Name.Length), v.Value);
    }

    LOG_INLINE_WARNING("\nYou can reference the above internal variables\ninside your .build file with this syntax:\n");
    LOG("    %%_UserDirectory\n    %%_AVX2\n    if _CacheLineSize == 64\n    if _FMA3");
}

static void PrintBuildFiles(const String WorkingDirectory)
{
    LOG_INLINE_WARNING("Build Files (found in: %S)\n", WorkingDirectory);
    Filesystem_IterateDirectory(WorkingDirectory, &BuildFilesIterator, true);
}

static void PrintUsage(const String WorkingDirectory)
{
    LOG_INLINE_WARNING("Usage\n");
    LOG("   riftbuild");
    LOG("   riftbuild [options]");
    LOG("   riftbuild [.build file] [options]");

    LOG_LINE_BREAK();

    PrintBuildFiles(WorkingDirectory);

    LOG_LINE_BREAK();

    // TODO: custom usage message from each build file
    // TODO: custom description message for each build file
    // TODO: log the preset options as well
    // TODO: log command options

    // TODO: unrelated to this func. consider supporting paths with spaces given on the cmd line without wrapping with quotes

    // TODO: expand tutorial to general stuff like how to setup a simple project
    // -t -> will display all the options and its breif description
    // -t:env
    // -t:pch
    // -t:general
    // -t:help

    // TODO: better documentation
    // TODO: use the BuiltinOptions global var. make some kinda structure out of this and not hardcoded
    LOG_INLINE_WARNING("Options\n");
    LOG(
      "   -h, --help, /?, -?, ? : Display this help message\n"
      "   -a, --about           : About this program\n"
      "   -b, --buildfiles      : List all .build files found under this working directory\n"
      "   -v, --verbose         : Enable verbose logging\n"
      "   -s, --singlethread    : Force single-threaded mode. Disables multi-process compilation\n"
      "   -q, --quiet           : Quiet mode. Disables logging but outputs necessary information, like errors\n"
      "   -t, --tutorial        : Display a tutorial on how to set environment variables\n"
      "   -i, --internals       : Display internal variables that can be referenced in a .build file\n"
    "\n   help                  : Print out custom help message from the build file\n"
    "\n   options               : Print out custom options from the build file\n"
    "\n   clean                 : Delete all intermediate and binary files\n"
    "\n   rebuild               : Clean all and build\n"
    "\n   run                   : If building an executable, immediately run it after the build has succeeded.\n"
    "\n   list                  : List variables given this syntax -> list:name\n"
    "                           special names like \"list:all\" will list all variables\n"
    "\n   override              : Override a build variable given this syntax -> override:Name=Value\n"
    "\n   export                : Generate a wide variety of files given this syntax -> export:type.\n"
    "                           for example export:compile_commands will export a compile_commands.json file.\n"
    "                           you can also generate .bat or .sh files, license files, .plist files, etc.\n"
    "                           type export: to view a list of all the things that you can export.\n"
    "                           Note: this only works if you are using a .build file\n"
    "\n   preset                : Build with a preset of command line arguments, given this syntax -> preset:name"
    );

    LOG_LINE_BREAK();

    LOG_INLINE_WARNING("Issue\n");
    LOG("   Submit a request, issue or bug report: https://github.com/AliElSaleh/RiftBuild/issues");
}

static void ExpandDefineFlags(String* Dest, const String Flags, const String FlagPrefix, bool bExportingSomething)
{
    // Note(Ali): on windows, wrap the define in quotes so that we can have spaces for the string defines
    //            like so: "-DVAR=\"va lue\"". the createprocess() argument parser splits up the arguments
    //            by default where as unix doesn't. if only there was a way to fucking bypass this shit. sigh... :(
    //            on unix we just do this: -DVAR="va lue". so we dont actually need to do any processing

    if (Flags.Length)
    {
        #if PLATFORM_WINDOWS
        // TODO: verify compile_commands.json export?
        ScratchLocal(Scratch, Kibibytes(8));
        StringList List = String_SplitIntoList(&Scratch, Flags, ' ', true);

        for each_string_in_list (List)
        {
            if (!bExportingSomething) { String_AppendChar(Dest, '"'); }
            String_Append(Dest, FlagPrefix);

            u32 Equals = 0;
            if (String_IndexOfChar(It.String, '=', &Equals))
            {
                // is this define a string?

                String Name = StrSlice(It.String.Data, Equals);
                String Value = StrShiftF(It.String, Equals+1);
                bool bIsString = String_IsFirst(Value, '"') && String_IsLast(Value, '"');

                String_Append    (Dest, Name);
                String_AppendChar(Dest, '=');
                
                // if its a string, insert a backslash so that the argument parser
                // for createprocess() understands that this is a nested string.

                if (bIsString)
                {
                    String_AppendChar(Dest, '\\');
                    String_Append    (Dest, Value);
                    Dest->Length--;
                    String_AppendChar(Dest, '\\');
                    String_AppendChar(Dest, '"');
                }
                else
                {
                    String_Append    (Dest, Value);
                }
            }
            else
            {
                String_Append(Dest, It.String);
            }

            if (!bExportingSomething) { String_AppendChar(Dest, '"'); }
            String_AppendSpace(Dest);
        }
        #else
        PrefixVariables(Dest, Flags, FlagPrefix, false);
        #endif
    }
}

static void ExpandPathFlags(LinearAllocator Scratch, String* Dest, const String Flags, const String FlagPrefix, bool bWrapWithQuotes)
{
    // expand include flags with * and ** wildcards
    StringLocal(WildcardFlags, 4096);
    StringLocal(NonWildcardFlags, 4096);

    StringList List = String_SplitIntoList(&Scratch, Flags, ' ', true);
    for each_str_list (List)
    {
        StringLocal(SearchDir, MAX_PATH_LENGTH);
        StringLocal(AfterStar, MAX_PATH_LENGTH);

        bool bWildcard = false;
        bool bRecursive = false;
        u32 Star = 0;
        if (String_IndexOfChar(It.String, '*', &Star))
        {
            bWildcard = true;

            u32 Offset = Star+1;
            if (String_GetCharFromIndex(It.String, Offset) == '*')
            {
                Offset += 1;
                bRecursive = true;
            }

            String_Copy(&SearchDir, StrSlice(It.String.Data, Star));
            String_Copy(&AfterStar, StrShiftF(It.String, Offset));
        }

        xx String_EatPathSeparatorsInlineFromEnd(&SearchDir);

        if (bWildcard)
        {
            STRUCT(PathIterData)
            {
                String BaseDirectory;
                String Postfix;
                String* Flags;
            };
            
            PathIterData Data = { SearchDir, AfterStar, &WildcardFlags };

            Filesystem_IterateDirectory_Ex(SearchDir, &PathFlagDirectoryIterator, bRecursive, &Data);
        }
        else
        {
            StringLocal(ItCopy, MAX_PATH_LENGTH);
            xx String_SanitizeQuotes(&ItCopy, It.String);

            // Fixes compiler being confused by this specific cmd line arg (on windows)
            // -I"..\" now becomes -I"..\\"
            if (String_EatPathSeparatorsInlineFromEnd(&ItCopy))
            {
                #if PLATFORM_WINDOWS
                String_Append(&ItCopy, S("\\\\"));
                #else
                String_Append(&ItCopy, S("/"));
                #endif
            }

            String_Append(&NonWildcardFlags, ItCopy);
            String_AppendSpace(&NonWildcardFlags);
        }
    }

    xx String_EatSpacesInlineFromEnd(&WildcardFlags);
    xx String_EatSpacesInlineFromEnd(&NonWildcardFlags);

    PrefixVariables(Dest, WildcardFlags, FlagPrefix, bWrapWithQuotes);
    if (WildcardFlags.Length > 0)
    {
        String_AppendSpace(Dest);
    }
    PrefixVariables(Dest, NonWildcardFlags, FlagPrefix, bWrapWithQuotes);
}

static void Internal_RunAssembly(LinearAllocator Scratch, const String WorkingPath, const String BuildDirectory, const String AssemblyNameWithExt, const String ArgString)
{
    u32 PipeIndex = 0;
    bool bFound = String_IndexOfChar(ArgString, '|', &PipeIndex);

    const String Args       = bFound ? StrSlice(ArgString.Data, PipeIndex) : ArgString;
    const String CustomPath = bFound ? String_EatSpaces(StrShiftF(ArgString, PipeIndex+1)) : String_Null();

    StringLocal(ProgramArgs, 4096);
    StringLocal(EnvArgs, 4096);

    StringList List = String_SplitIntoList(&Scratch, Args, ' ', true);
    for each_str_list (List)
    {
        if (String_StartsWith(It.String, S("env:"), false))
        {
            String_Append(&EnvArgs, StrShiftF(It.String, 4));
            xx String_EatSpacesInlineFromEnd(&EnvArgs);
            String_AppendChar(&EnvArgs, '\0');
        }
        else
        {
            String_Append(&ProgramArgs, It.String);
            xx String_EatSpacesInlineFromEnd(&ProgramArgs);
            String_AppendChar(&ProgramArgs, ' ');
        }
    }

    xx String_EatSpacesInlineFromEnd(&ProgramArgs);

    StringLocal(CmdLine, 8192);

    #if PLATFORM_WINDOWS
    String_Append(&CmdLine, S("cmd.exe /c \""));
    #endif

    StringLocal(ExecutableWorkingPath, MAX_PATH_LENGTH);

    if (CustomPath.Length > 0)
    {
        if (Filesystem_IsPathRelative(CustomPath))
        {
            String_BuildPath(&ExecutableWorkingPath, WorkingPath, CustomPath);
            xx String_EatPathSeparatorsInlineFromEnd(&ExecutableWorkingPath);
        }
    }
    else
    {
        String_Copy(&ExecutableWorkingPath, BuildDirectory);
    }

    xx Filesystem_ConvertRelativeToAbsolutePath(&ExecutableWorkingPath);

    String_Append(&CmdLine, S("cd \""));
    String_Append(&CmdLine, ExecutableWorkingPath);
    String_Append(&CmdLine, S("\" && "));

    StringLocal(ExePath, MAX_PATH_LENGTH);
    String_Append(&ExePath, BuildDirectory);
    String_Append(&ExePath, AssemblyNameWithExt);

    String_AppendChar(&CmdLine, '"');
    String_Append(&CmdLine, ExePath);
    String_AppendChar(&CmdLine, '"');

    String_AppendSpace(&CmdLine);
    String_Append(&CmdLine, ProgramArgs);

    xx String_EatSpacesInlineFromEnd(&CmdLine);

    #if PLATFORM_WINDOWS
    String_AppendChar(&CmdLine, '"');

    LOG_LINE_BREAK();
    #endif

    if (AssemblyNameWithExt.Length > 0 && Filesystem_DoesFileExist(ExePath))
    {
        if (!bQuietBuild)
        {
            LOG("Launching %S ...", AssemblyNameWithExt);
            LOG(" -> Working Directory: %S", ExecutableWorkingPath);

            if (ProgramArgs.Length > 0)
            {
                LOG(" -> Parameters: %S", ProgramArgs);
            }

            if (EnvArgs.Length > 0)
            {
                LOG(" -> Environment: %S", EnvArgs);
            }

            LOG_LINE_BREAK();
        }

        Platform_WaitForHandle(Platform_RunCommand(CmdLine, ExecutableWorkingPath, EnvArgs), -1);
    }
}

static void TimeAsPercentageOfTotal(String* Buffer, u32 Length, f64 ElapsedTime, f64 TotalTime)
{
    i32 SpacesToAppend = Max(0, 12 - (i32)Length) + 2;
    for (i32 i = 0; i < SpacesToAppend; i++)
    {
        String_AppendSpace(Buffer);
    }

    String_Append(Buffer, S("["));
    f64 Percentage = (ElapsedTime / TotalTime) * 100.0;
    StringLocal(PercentageString, 8);
    String_Format(&PercentageString, S("%.2f"), Percentage);
    String_Append(Buffer, PercentageString);
    String_Append(Buffer, S("%]"));
}

static void PrintClockTimeToBuffer(String* Buffer, Clock* Timer, Clock* MasterTimer, const String DisplayName, bool bOnlyElapsed)
{
    if (Timer->StartTime > 0 &&
        (!bOnlyElapsed || (bOnlyElapsed && Timer->ElapsedTime > 0)))
    {
        StringLocal(TimeString, 32);

        Clock_GetElapsedTime_ToString(Timer, true, &TimeString);
        String_Append(Buffer, DisplayName);
        String_Append(Buffer, TimeString);
        if (MasterTimer)
        {
            TimeAsPercentageOfTotal(Buffer, TimeString.Length, Timer->ElapsedTime, MasterTimer->ElapsedTime);
        }
        String_AppendNewline(Buffer);
    }
}

static bool Parameters_TryListVariables(LinearAllocator Scratch, const StringArray Parameters, TArray(FileVariable) VariablesDB, TArray(CmdOption) CmdOptionDB, const String BuildFilePath, u32* ExitCode)
{
    bool bTried = false;
    
    if (ExitCode)
    {
        *ExitCode = 0;
    }

    for (u8 i = 0; i < Parameters.Num; i++)
    {
        const String Arg = Parameters.List[i];

        if (String_StartsWith(Arg, S("list:"), false))
        {
            bTried = true;

            LOG_LINE_BREAK();

            // TODO: if we do this: list:somekey.   with a . at the end, then print out all keys that start with that
            u32 Colon = 0;
            if (String_IndexOfChar(Arg, ':', &Colon))
            {
                const String VarToList = StrShiftF(Arg, Colon+1);

                if (VarToList.Length == 0)
                {
                    LOG_ERROR("Failed to list build variable. No variable name was given after ':'");
                    LOG_INLINE_WARNING("\nUsage\n");
                    LOG("     list:all");
                    LOG("     list:varname");
                    LOG("     list:varname,othername,anotherone");
                    if (ExitCode)
                    {
                        *ExitCode = 1;
                    }
                    break;
                }

                StringArray Vars = String_ParseIntoArray(&Scratch, VarToList, ',', 0, 128);
            
                for each_str (var, Vars)
                {
                    if (String_IsEqual(*var, S("all"), false))
                    {
                        ListVariables(Scratch, String_Null(), VariablesDB);
                    }
                    else if (String_IsEqual(*var, S("internal_vars"), false))
                    {
                        PrintInternals();

                        LOG_INLINE_WARNING("\nInternal Options\n");

                        u32 LongestName = 0;

                        for each (CmdOption, c, CmdOptionDB)
                        {
                            if (c.Name.Length > LongestName)
                            {
                                LongestName = c.Name.Length;
                            }
                        }

                        for each (CmdOption, c, CmdOptionDB)
                        {
                            LOG("   %S %*S", c.Name, (LongestName-c.Name.Length), c.Value);
                        }

                        LOG_INLINE_WARNING("\nYou can reference the above internal variables\ninside your .build file with this syntax:\n");
                        LOG("    %%_Date\n    %%Compiler.InstallPath\n    if clang\n    if clang.version > 17.2.0");
                    }
                    else
                    {
                        if (!DoesBuildVarExist(VariablesDB, *var))
                        {
                            LOG_WARNING("Failed to list \"%S\". It does not exist in \"%S\" after expansion (within the context of the given build parameters)", *var, BuildFilePath);
                            if (ExitCode)
                            {
                                *ExitCode = 1;
                            }
                            break;
                        }

                        ListVariables(Scratch, *var, VariablesDB);
                    }
                }

                // todo: put in function? clean up code routine?
                // shouldnt be here. remove
                /*
                for each (FileHandle, File, IncludeFiles)
                {
                    Filesystem_Close(&File);
                }
                */

                break;
            }
        }
    }

    return bTried;
}

static BuildReceipt BuildTarget(LinearAllocator* Arena,
                        const FileHandle BuildFileHandle, PlatformMutex* BuildMutex,
                        const String WorkingPath, const StringArray Parameters, const String CameFromBuildFile,
                        i8 BuildFileIndex, i8 RootPathIndex)
{
    // BuildResult BuildOutputResult = {0};
    BuildReceipt Receipt = {0};

    if (!Platform_SetWorkingDirectory(WorkingPath))
    {
        #ifndef HOOD
        LOG_ERROR("Failed to set working directory to \"%S\"", WorkingPath);
        #else
        LOG_ERROR("nah cuh, couldnt set the workin directory to \"%S\"", WorkingPath);
        #endif

        Receipt.ExitCode = 1;
        return Receipt;
    }

    Receipt.WorkingPath = WorkingPath;

    // make sure we can get the path of the build file (if applicable)
    StringLocal(BuildFilePathFull, MAX_PATH_LENGTH);
    bool bFoundBuildFile = IsValidFileHandle(BuildFileHandle);
    bool bBuildFilePathSuccess = Filesystem_GetFilePath(BuildFileHandle, &BuildFilePathFull);
    if (bFoundBuildFile && (BuildFilePathFull.Length == 0 || !bBuildFilePathSuccess))
    {
        LOG_FATAL("Operating system error: Failed to retrieve build file path from its handle. Aborting...");

        LOG("   Something seriously went wrong here, before you proceed any further,");
        LOG("   please submit this issue over to: https://github.com/AliElSaleh/RiftBuild/issues");

        LOG("\n   Provide clear and detailed reproduction steps on how this issue had occured.");

        Receipt.ExitCode = 1;
        return Receipt;
    }

    // make sure no one else is building this target
    const bool bNoMutex = StringArray_Contains(Parameters, S("--no-mutex"), false);
    if (!bNoMutex)
    {
        String MutexString = String_Reserve(Arena, MAX_PATH_LENGTH);
        if (bFoundBuildFile)
        {
            String_Copy(&MutexString, BuildFilePathFull);
        }
        else
        {
            String_Copy(&MutexString, WorkingPath);
        }

        // mainly for windows, but keep it for other platforms just in case
        xx String_ReplaceNonAlphaNumericCharInline(&MutexString, '_');

        if (!Platform_CreateNamedMutex(MutexString, BuildMutex))
        {
            const String BuildPath = bFoundBuildFile ? BuildFilePathFull : WorkingPath;

            // u32 LastSlash = 0;
            // bool bHasSlash = String_IndexOfLastPathSlash(BuildPath, &LastSlash);
            // String BuildFileName = bHasSlash ? StrShiftF(BuildPath, LastSlash+1) : BuildPath;

            LOG_ERROR("Failed to acquire a build mutex. Aborting build...");
            
            if (BuildMutex->ID > 0)
            {
                LOG("\n    An existing riftbuild process is running for \"%S\" [Process ID: %i]", BuildPath, BuildMutex->ID);
            } 
            else
            {
                LOG("\n    An existing riftbuild process is running for \"%S\"", BuildPath);
            }

            LOG("    To prevent conflicts, please wait for the existing build to finish before trying again.\n");
            LOG("    This feature can be disabled with --no-mutex");

            Receipt.ExitCode = 1;
            return Receipt;
        }
    }

    Clock BuildRuntime;
    Clock_Start(&BuildRuntime);

    StringLocal(RiftCmdLine, 2048);
    for (u8 i = 0; i < Parameters.Num; i++)
    {
        const String Param = Parameters.List[i];

        // todo: allow the user to specify which args can be ignored for rebuild?
        bool bIsBuiltin = false;
        for (u8 j = 0; j < SArray_Capacity(BuiltinOptions); j++)
        {
            const String Option = BuiltinOptions[j];
            if (String_IsEqual(Option, S("run"), false))
            {
                // make an exception for "run"
                continue;
            }

            if (String_IsEqual(Param, Option, false))
            {
                bIsBuiltin = true;
                break;
            }
        }

        if (bIsBuiltin)
        {
            continue;
        }

        String_Append     (&RiftCmdLine, Param);
        String_AppendSpace(&RiftCmdLine);
    }

    xx String_EatSpacesInlineFromEnd(&RiftCmdLine);

    u32 MaxLogicalCores = Platform_GetNumLogicalProcessors();

    ArrayLocal_Arena(FileVariable,   VariablesDB,         256, Arena); // 8192 bytes

    ArrayLocal_Arena(FileHandle,     IncludeFiles,        64,  Arena); // 1024 bytes
    ArrayLocal_Arena(CmdOption,      CmdOptionsDB,        128, Arena); // 4608 bytes
    ArrayLocal_Arena(String,         Messages,            128, Arena); // 2048 bytes

    ArrayLocal_Arena(PlatformHandle, Processes,           MaxLogicalCores, Arena);

    // store custom command line options to be referenced inside a .build file
    for (u8 i = 0; i < Parameters.Num; i++)
    {
        const String Param = Parameters.List[i];

        if (String_IsEqual(Param, S("_args"), false))
        {
            continue;
        }

        // TODO: something better. this is duplicated code...
        bool bIsBuiltin = false;
        for (u8 j = 0; j < SArray_Capacity(BuiltinOptions); j++)
        {
            String Option = BuiltinOptions[j];
            if (String_IsEqual(Option, S("run"), false))
            {
                // make an exception for "run"
                continue;
            }

            if (String_IsEqual(Parameters.List[i], Option, false))
            {
                bIsBuiltin = true;
                break;
            }
        }

        if (bIsBuiltin)
        {
            continue;
        }

        // ignore .build file and paths as they're not gonna be referenced anywhere
        if (i != BuildFileIndex &&
            i != RootPathIndex)
        {
            u32 EqualIndex = 0;
            bool bFoundEqual = String_IndexOfChar(Param, '=', &EqualIndex);

            CmdOption c;
            if (bFoundEqual)
            {
                c.Name = StrSlice(Param.Data, EqualIndex);
                xx String_EatSpacesInlineFromEnd(&c.Name);
                c.Value = StrSlice(Param.Data+EqualIndex+1, Param.Length - (EqualIndex + 1));
                xx String_EatSpacesInline(&c.Value);
            }
            else
            {
                c.Name = Param;
                xx String_EatSpacesInlineFromEnd(&c.Name);
                c.Value = String_Null();
            }

            Array_Add(CmdOptionsDB, c);
        }
    }

    String BuildFileName;
    StringLocal(BuildFilePath, MAX_PATH_LENGTH);
    {
        u32 LastSlash = 0;
        bool bHasSlash = String_IndexOfLastPathSlash(BuildFilePathFull, &LastSlash);

        BuildFileName = bHasSlash ? StrShiftF(BuildFilePathFull, LastSlash+1) : BuildFilePathFull;
        
        const String NameCopy = String_Create(Arena, BuildFileName);

        u32 LastDot = 0;
        bool bHasDot = String_IndexOfLastChar(NameCopy, '.', &LastDot);

        AddCmdOption(CmdOptionsDB, S("_FileName"), bHasDot ? StrSlice(NameCopy.Data, LastDot) : NameCopy);
        AddCmdOption(CmdOptionsDB, S("_FileNameExt"), NameCopy);

        const String PathNoExt = bHasSlash ? StrSlice(BuildFilePathFull.Data, LastSlash) : BuildFilePathFull;
        const String PathFull = String_Create(Arena, PathNoExt);
        AddCmdOption(CmdOptionsDB, S("_FileDirectoryFull"), PathFull);

        const String PathRelative = StrShiftF(PathNoExt, WorkingPath.Length+1);

        String_BuildPath(&BuildFilePath, PathRelative, BuildFileName);

        AddCmdOption(CmdOptionsDB, S("_FileDirectory"), String_Create(Arena, PathRelative));
        
        u32 SecondLastSlash = 0;
        bHasSlash = String_IndexOfLastPathSlash(PathNoExt, &SecondLastSlash);

        AddCmdOption(CmdOptionsDB, S("_FolderName"), String_Create(Arena, bHasSlash ? StrShiftF(PathNoExt, SecondLastSlash+1) : PathNoExt));
        AddCmdOption(CmdOptionsDB, S("_DirectoryName"), String_Create(Arena, bHasSlash ? StrShiftF(PathNoExt, SecondLastSlash+1) : PathNoExt));

        AddCmdOption(CmdOptionsDB, S("_WorkingDirectory"), WorkingPath);
    }

    SystemTime TimeNow = Platform_GetSystemLocalTime();
    
    StringLocal(TimeZone, 64);
    if (!Platform_GetTimeZone(&TimeZone))
    {
        String_Copy(&TimeZone, S("Unknown"));
    }

    StringLocal(TimeStamp, 64);
    String_Format(&TimeStamp, S("%hu-%.2hu-%.2hu %.2hu:%.2hu:%.2hu [%S]"), TimeNow.Year, TimeNow.Month, TimeNow.Day, TimeNow.Hour, TimeNow.Minute, TimeNow.Second, TimeZone);

    {
        StringLocal(Temp, 64);
        String_Format(&Temp, S("%hu-%.2hu-%.2hu %.2hu:%.2hu:%.2hu"), TimeNow.Year, TimeNow.Month, TimeNow.Day, TimeNow.Hour, TimeNow.Minute, TimeNow.Second);
        String a = String_Create(Arena, Temp);
        AddCmdOption(CmdOptionsDB, S("_Timestamp"), a);

        // add another for time zone information
        String_Format(&Temp, S("%hu-%.2hu-%.2hu %.2hu:%.2hu:%.2hu [%S]"), TimeNow.Year, TimeNow.Month, TimeNow.Day, TimeNow.Hour, TimeNow.Minute, TimeNow.Second, TimeZone);
        a = String_Create(Arena, Temp);
        AddCmdOption(CmdOptionsDB, S("_Timestamp.Zone"), a);

        String_Format(&Temp, S("%hu%.2hu%.2hu%.2hu%.2hu%.2hu"), TimeNow.Year, TimeNow.Month, TimeNow.Day, TimeNow.Hour, TimeNow.Minute, TimeNow.Second);
        a = String_Create(Arena, Temp);
        AddCmdOption(CmdOptionsDB, S("_Timestamp.NoSep"), a);

        String_Format(&Temp, S("%hu-%.2hu-%.2hu"), TimeNow.Year, TimeNow.Month, TimeNow.Day);
        a = String_Create(Arena, Temp);
        AddCmdOption(CmdOptionsDB, S("_Date"), a);

        String_Format(&Temp, S("%hu"), TimeNow.Year);
        a = String_Create(Arena, Temp);
        AddCmdOption(CmdOptionsDB, S("_Date.Year"), a);
        String_Format(&Temp, S("%.2hu"), TimeNow.Month);
        a = String_Create(Arena, Temp);
        AddCmdOption(CmdOptionsDB, S("_Date.Month"), a);
        String_Format(&Temp, S("%hu"), TimeNow.Week);
        a = String_Create(Arena, Temp);
        AddCmdOption(CmdOptionsDB, S("_Date.Week"), a);
        String_Format(&Temp, S("%.2hu"), TimeNow.Day);
        a = String_Create(Arena, Temp);
        AddCmdOption(CmdOptionsDB, S("_Date.Day"), a);

        String_Format(&Temp, S("%S"), Platform_GetMonthName(TimeNow.Month));
        a = String_Create(Arena, Temp);
        AddCmdOption(CmdOptionsDB, S("_Date.MonthName"), a);

        String_Format(&Temp, S("%S"), Platform_GetDayName(TimeNow.DayOfWeek));
        a = String_Create(Arena, Temp);
        AddCmdOption(CmdOptionsDB, S("_Date.DayName"), a);

        String_Format(&Temp, S("%hu"), TimeNow.DayOfWeek);
        a = String_Create(Arena, Temp);
        AddCmdOption(CmdOptionsDB, S("_Date.DayOfWeek"), a);

        u16 DayOfYear = Platform_GetDayOfYear(TimeNow.Day, TimeNow.Month, TimeNow.Year);
        String_Format(&Temp, S("%hu"), DayOfYear);
        a = String_Create(Arena, Temp);
        AddCmdOption(CmdOptionsDB, S("_Date.DayOfYear"), a);

        String_Format(&Temp, S("%hu%.2hu%.2hu"), TimeNow.Year, TimeNow.Month, TimeNow.Day);
        a = String_Create(Arena, Temp);
        AddCmdOption(CmdOptionsDB, S("_Date.NoSep"), a);

        String_Format(&Temp, S("%.2hu:%.2hu:%.2hu"), TimeNow.Hour, TimeNow.Minute, TimeNow.Second);
        a = String_Create(Arena, Temp);
        AddCmdOption(CmdOptionsDB, S("_Time"), a);

        String_Format(&Temp, S("%.2hu"), TimeNow.Hour);
        a = String_Create(Arena, Temp);
        AddCmdOption(CmdOptionsDB, S("_Time.Hour"), a);
        String_Format(&Temp, S("%.2hu"), TimeNow.Minute);
        a = String_Create(Arena, Temp);
        AddCmdOption(CmdOptionsDB, S("_Time.Minute"), a);
        String_Format(&Temp, S("%.2hu"), TimeNow.Second);
        a = String_Create(Arena, Temp);
        AddCmdOption(CmdOptionsDB, S("_Time.Second"), a);
        String_Format(&Temp, S("%.3hu"), TimeNow.Millisecond);
        a = String_Create(Arena, Temp);
        AddCmdOption(CmdOptionsDB, S("_Time.Millisecond"), a);

        String_Format(&Temp, S("%.2hu%.2hu%.2hu"), TimeNow.Hour, TimeNow.Minute, TimeNow.Second);
        a = String_Create(Arena, Temp);
        AddCmdOption(CmdOptionsDB, S("_Time.NoSep"), a);
    }

    StringLocal(RiftBuildArgs, 4096);

    // add all arguments, except working directory and build file path
    {
        for (u8 i = 0; i < Parameters.Num; i++)
        {
            if (i == BuildFileIndex ||
                i == RootPathIndex)
            {
                continue;
            }

            String_Append(&RiftBuildArgs, Parameters.List[i]);
            String_AppendSpace(&RiftBuildArgs);
        }

        AddCmdOption(CmdOptionsDB, S("%"), RiftBuildArgs);
        AddCmdOption(CmdOptionsDB, S("_Args"), RiftBuildArgs);
    }

    if (bFoundBuildFile)
    {
        bool bHidden = Filesystem_IsHidden(BuildFilePath);

        #ifndef HOOD
        LOG("Using build file:  %S %S", BuildFilePath, bHidden ? S("[hidden]") : String_Null());
        #else
        LOG("alright sweet, using this build file btw: %S %S", BuildFilePath, bHidden ? S("[why the fuck is the hidden bro, idk dont care]") : String_Null());
        #endif
    }

    if (RiftCmdLine.Length > 0)
    {
        LOG("Arguments:         %S", RiftCmdLine);
    }

    #ifndef HOOD
    LOG("Working Directory: %S", WorkingPath);
    #else
    LOG("dis da work'n directory bro: %S", WorkingPath);
    #endif

    EAssemblyType AssemblyType = AssemblyType_None;

    #if PLATFORM_WINDOWS
    bool bFallbackVersion = false;
    #endif

    Clock BuildFileParseClock = {0};

    bool bAnyVarsOverriden = false;

    if (bFoundBuildFile)
    {
        ////////////////////////////////////////////////////////////////////////////////////////////////////////
        //
        //
        //                PARSE AND EXPAND START
        //
        //
        ////////////////////////////////////////////////////////////////////////////////////////////////////////

        Clock_Start(&BuildFileParseClock);
        {
            LinearAllocator Scratch = {0};
            i8 ScratchMemory[Kibibytes(512)] = {0};
            LinearAllocator_Create(Kibibytes(512), ScratchMemory, &Scratch);

            ParsingContext Context        = {0};
            Context.PermanentArena        = Arena;
            Context.TempArena             = &Scratch;
            Context.VariablesDB           = VariablesDB;
            Context.CmdOptionsDB          = CmdOptionsDB;
            Context.Messages              = Messages;
            Context.IncludeFiles          = IncludeFiles;
            Context.WorkingDirectory      = WorkingPath;

            if (!ParseBuildFile(BuildFileHandle, BuildFilePath, Context, false, NULL))
            {
                Receipt.ExitCode = 1;
                return Receipt;
            }
        }
        Clock_Tick(&BuildFileParseClock);

        if (bHelp)
        {
            const String HelpMessage = GetVariableValue(VariablesDB, S(".Help"));

            LOG_INLINE_WARNING("\nHelp\n");
            if (HelpMessage.Length > 0)
            {
                LOG("%S", HelpMessage);
            }
            else
            {
                LOG("    No help message provided. Use -h to view this program's usage help instead.");
            }

            Receipt.ExitCode = 0;
            return Receipt;
        }


        if (bOptions)
        {
            // TODO: fix long descriptions breaking layout

            LOG_INLINE_WARNING("\nOptions\n\n");

            SArray(FileVariable, OptionVars, 64) = {0};
            
            u32 LongestParams = 0;
            {
                for each (FileVariable, v, VariablesDB)
                {
                    if (String_StartsWith(v.Name, S("Option."), false))
                    {
                        String Trimmed = v.Name;
                        u32 LastDot = 0;
                        if (String_IndexOfLastChar(StrShiftF(v.Name, 7), '.', &LastDot))
                        {
                            Trimmed = StrSlice(Trimmed.Data, LastDot+7);
                        }
                        
                        // get the base key
                        FileVariable Base = GetVariable(VariablesDB, Trimmed);
                        if (Base.Params.Length > LongestParams)
                        {
                            LongestParams = Base.Params.Length;
                        }
                    }
                }
            }

            u32 LongestName = 8;
            bool bAnyOptions = false;
            for each (FileVariable, v, VariablesDB)
            {
                if (String_StartsWith(v.Name, S("Option."), false))
                {
                    bAnyOptions = true;

                    String Trimmed = v.Name;
                    u32 LastDot = 0;
                    if (String_IndexOfLastChar(StrShiftF(v.Name, 7), '.', &LastDot))
                    {
                        Trimmed = StrSlice(Trimmed.Data, LastDot+7);
                    }

                    // did we already add this?
                    bool bAlready = false;
                    for EachElement(i, OptionVars)
                    {
                        if (String_IsEqual(OptionVars[i].Name, Trimmed, false))
                        {
                            bAlready = true;
                            break;
                        }
                    }

                    if (bAlready)
                    {
                        continue;
                    }

                    FileVariable NewVar = v;
                    NewVar.Name = Trimmed;

                    SArray_Add(OptionVars, NewVar);

                    FileVariable Base = GetVariable(VariablesDB, Trimmed);

                    u32 NameLength = StrShiftF(Trimmed, 7).Length + Base.Params.Length;
                    if (NameLength > LongestName)
                    {
                        LongestName = NameLength;
                    }
                }
            }

            u32 LongestDefault = 7;
            bool bAnyDefault = false;
            for EachElement(i, OptionVars)
            {
                FileVariable Var = OptionVars[i];
                if (String_IsValid(Var.Name))
                {
                    // bool bValid = false;

                    // FileVariable Base = GetVariable(VariablesDB, Var.Name);

                    // if (Base.Value.Length > LongestDefault)
                    // {
                        // LongestDefault = Base.Value.Length;
                        // bAnyDefault = true;
                    // }

                    // get the default key
                    // StringLocal(Default, MAX_KEY_LENGTH);
                    // String_Append(&Default, Var.Name);
                    // String_Append(&Default, S(".Default"));
                    // FileVariable DefaultVar = GetVariable(VariablesDB, Default);

                    // if (DefaultVar.Value.Length > LongestDefault)
                    // {
                        // LongestDefault = DefaultVar.Value.Length;
                        // bAnyDefault = true;
                        // bValid = true;
                    // }

                    // look at the cmd option list if we couldnt find it in the regular list
                    // if (!bValid)
                    {
                        bool bExists = DoesCmdOptionExist(CmdOptionsDB, Var.Name);
                        if (bExists)
                        {
                            String FinalValue = GetCmdOptionValue(CmdOptionsDB, Var.Name);
                            if (FinalValue.Length > LongestDefault)
                            {
                                LongestDefault = FinalValue.Length;
                                bAnyDefault = true;
                            }
                        }
                    }
                }
            }

            StringLocal(Spaces, MAX_KEY_LENGTH);
            Spaces.Length = LongestName+1;
            String_Fill(&Spaces, ' ');

            StringLocal(Spaces2, MAX_KEY_LENGTH);
            Spaces2.Length = (LongestDefault-7)+1;
            String_Fill(&Spaces2, ' ');

            if (bAnyDefault)
            {
                //LOG("    %S  Default%S  Description\n", Spaces, Spaces2);
                //LOG("    %S  %S  ", Spaces, Spaces2);
            }
            else
            {
                //LOG("    %S  Description\n", Spaces);
                //LOG("    %S  ", Spaces);
            }

            for EachElement(i, OptionVars)
            {
                FileVariable Var = OptionVars[i];
                if (String_IsValid(Var.Name))
                {
                    String Trimmed = StrShiftF(Var.Name, 7);

                    String FinalValue = String_Null();

                    // get the base key
                    FileVariable Base = GetVariable(VariablesDB, Var.Name);
                    // FinalValue = Base.Value;

                    // get the default key
                    // StringLocal(Default, MAX_KEY_LENGTH);
                    // String_Append(&Default, Var.Name);
                    // String_Append(&Default, S(".Default"));
                    // FileVariable DefaultVar = GetVariable(VariablesDB, Default);

                    if (!String_IsValid(FinalValue))
                    {
                        bool bExists = DoesCmdOptionExist(CmdOptionsDB, Trimmed);
                        if (bExists)
                        {
                            FinalValue = GetCmdOptionValue(CmdOptionsDB, Trimmed);
                            if (!FinalValue.Length)
                            {
                                FinalValue = S("on");
                            }
                        }
                    }

                    // if (!String_IsValid(FinalValue))
                    // {
                        // FinalValue = DefaultVar.Value;
                    // }

                    if (!String_IsValid(FinalValue))
                    {
                        String First = Base.Params;
                        u32 Space = 0;
                        if (String_IndexOfFirstWhitespace(Base.Params, &Space))
                        {
                            First = StrSlice(Base.Params.Data, Space);
                        }

                        FinalValue = First;
                    }

                    if (!String_IsValid(FinalValue))
                    {
                        FinalValue = S("off");
                    }

                    // get the description key
                    // StringLocal(Desc, MAX_KEY_LENGTH);
                    // String_Append(&Desc, Var.Name);
                    // String_Append(&Desc, S(".Description"));
                    // FileVariable DescriptionVar = GetVariable(VariablesDB, Desc);

                    // String FinalDesc = DescriptionVar.Value;
                    String FinalDesc = Var.Value;
                    if (!String_IsValid(FinalDesc))
                    {
                        FinalDesc = S("No description provided");
                    }

                    u32 Actual = Trimmed.Length + Base.Params.Length ;
                    u32 NameLength  = (LongestName - Actual) + 1;
                    u32 ValueLength = (LongestDefault - FinalValue.Length) + 1;
                    u32 ParamLength = (LongestParams - Base.Params.Length) + 1;
                    /*
                    if (!bAnyDefault)
                    {
                        ValueLength = 0;
                    }
                    */

                    LogOptionData_WordWrapped(*Arena, Trimmed, NameLength, FinalValue, ValueLength, Base.Params, ParamLength, FinalDesc);
                }
            }

            if (!bAnyOptions)
            {
                LOG("    No options provided. Use -h to view this program's usage help instead.");
            }

            Receipt.ExitCode = 0;
            return Receipt;
        }

        bAnyVarsOverriden = CheckForBuildVariableOverrides(VariablesDB, CmdOptionsDB);

        const String Ext  = String_EatChar(GetVariableValue(VariablesDB, S("Extension")), '.');
        const String Type = GetVariableValue(VariablesDB, S("Type"));

        bool bIsAssemblyExe = Type.Length == 0 && Ext.Length == 0;
        if (bIsAssemblyExe)
        {
            FileVariable Var;
            Var.Name = S("Type");
            Var.Value = S("app");
            Array_Add(VariablesDB, Var);
        }

        if (!bIsAssemblyExe)
        {
            bIsAssemblyExe = String_IsEqual(Type, S("app"), false) ||
                             String_IsEqual(Type, S("application"), false) ||
                             String_IsEqual(Type, S("exe"), false) ||
                             String_IsEqual(Type, S("executable"), false);
        }

        if (!bIsAssemblyExe && Type.Length == 0)
        {
            bIsAssemblyExe = Ext.Length == 0 || 
                             String_IsEqual(Ext, S("elf"), false) ||
                             String_IsEqual(Ext, S("out"), false) ||
                             String_IsEqual(Ext, S("exe"), false) ||
                             String_IsEqual(Ext, S("com"), false);
        }

        if (bIsAssemblyExe)
        {
            AssemblyType = AssemblyType_Executable;
        }

        StringLocal(AssemblyNameUpper, 128);
        String_Copy(&AssemblyNameUpper, GetVariableValue(VariablesDB, S("Assembly")));
        xx String_ReplaceCharInline(&AssemblyNameUpper, '-', '_');
        String_ToUpper(&AssemblyNameUpper);

        String VersionKey = S("Version");
        bool bDoesVersionVarExist = DoesBuildVarExist(VariablesDB, VersionKey);

        #if PLATFORM_WINDOWS
        if (!bDoesVersionVarExist)
        {
            bFallbackVersion = true;
        }
        #endif

        // set defaults for a few key build variables
        bool bAnyOverriden = CheckForBuildVariableOverrides(VariablesDB, CmdOptionsDB);

        if (!bAnyVarsOverriden) { bAnyVarsOverriden = bAnyOverriden; }

        if (bDoesVersionVarExist)
        {
            FileVariable VersionVar = GetVariable(VariablesDB, VersionKey);
            String ExpandedVar = VersionVar.Value;

            // add the defines (if desired)

            LinearAllocator Scratch = *Arena;
            StringList ParamList = String_SplitIntoList(&Scratch, VersionVar.Params, ' ', false);
            if (StringList_FindIndex(ParamList, S("define"), false, StringCompare_Equal, NULL))
            {
                const String VersionLevels[3] = 
                {
                    S("MAJOR"),
                    S("MINOR"),
                    S("PATCH")
                };

                {
                    StringLocal(VersionDefineString, 256);
                    String_Format(&VersionDefineString, S("%S_VERSION_STRING=\"%S\""), AssemblyNameUpper, ExpandedVar);

                    AddOrAppendVariable(Arena, VariablesDB, S("Defines"), VersionDefineString, String_Null(), 8192); // TODO: read from reservedkeys table
                }

                xx String_ReplaceNonAlphaNumericCharInline(&ExpandedVar, '.');

                const u32 NumDots = String_CountChar(ExpandedVar, '.');
                if (NumDots > 0)
                {
                    StringArray Versions = String_ParseIntoArray(Arena, ExpandedVar, '.', 0, 128);

                    u8 i = 0;
                    for each_str (v, Versions)
                    {
                        if (v->Length > 0)
                        {
                            const bool bContainsNonDigit = String_ContainsNonDigits(*v);

                            StringLocal(VersionDefine, 256);
                            if (i < 3)
                            {
                                if (bContainsNonDigit)
                                {
                                    String_Format(&VersionDefine, S("%S_%S_VERSION=\"%S\""), AssemblyNameUpper, VersionLevels[i], *v);
                                }
                                else
                                {
                                    String_Format(&VersionDefine, S("%S_%S_VERSION=%S"), AssemblyNameUpper, VersionLevels[i], *v);
                                }
                            }
                            else
                            {
                                if (bContainsNonDigit)
                                {
                                    String_Format(&VersionDefine, S("%S_EXTRA_VERSION_%hhu=\"%S\""), AssemblyNameUpper, i-3, *v);
                                }
                                else
                                {
                                    String_Format(&VersionDefine, S("%S_EXTRA_VERSION_%hhu=%S"), AssemblyNameUpper, i-3, *v);
                                }
                            }

                            AddOrAppendVariable(Arena, VariablesDB, S("Defines"), VersionDefine, String_Null(), 8192); // TODO

                            i++;
                        }
                    }
                }
                else
                {
                    const bool bContainsNonDigit = String_ContainsNonDigits(ExpandedVar);

                    StringLocal(VersionDefine, 256);

                    if (bContainsNonDigit)
                    {
                        String_Format(&VersionDefine, S("%S_VERSION=\"%S\""), AssemblyNameUpper, ExpandedVar);
                    }
                    else
                    {
                        String_Format(&VersionDefine, S("%S_VERSION=%S"), AssemblyNameUpper, ExpandedVar);
                    }

                    AddOrAppendVariable(Arena, VariablesDB, S("Defines"), VersionDefine, String_Null(), 8192); // TODO
                }
            }
        }

        FileVariable CopyrightVar = GetVariable(VariablesDB, S("Copyright"));
        if (CopyrightVar.Value.Length)
        {
            LinearAllocator Scratch = *Arena;
            StringList ParamList = String_SplitIntoList(&Scratch, CopyrightVar.Params, ' ', false);
            if (StringList_FindIndex(ParamList, S("define"), false, StringCompare_Equal, NULL))
            {
                StringLocal(CopyrightDefine, 256);
                String_Format(&CopyrightDefine, S("%S_COPYRIGHT_STRING=\"%S\""), AssemblyNameUpper, CopyrightVar.Value);

                AddOrAppendVariable(Arena, VariablesDB, S("Defines"), CopyrightDefine, String_Null(), 8192); // TODO
            }
        }
    }
    else
    {
        #if PLATFORM_WINDOWS
        bFallbackVersion = true;
        #endif

        // find the first compiler available on this machine

        StringLocal(CompilerPath, MAX_PATH_LENGTH);
        StringLocal(LinkerPath, MAX_PATH_LENGTH);
        StringLocal(AssemblerPath, MAX_PATH_LENGTH);
        StringLocal(ArchiverPath, MAX_PATH_LENGTH);
        StringLocal(CompilerInstallPath, MAX_PATH_LENGTH);
        StringLocal(CompilerToolPath, MAX_PATH_LENGTH);
        StringLocal(CompilerBasePath, MAX_PATH_LENGTH);
        StringLocal(CompilerIncludePath, MAX_PATH_LENGTH);
        StringLocal(CompilerLibraryPath, MAX_PATH_LENGTH);

        CompilerPaths FoundCompilerPaths = {0};
        FoundCompilerPaths.CompilerPath  = CompilerPath;
        FoundCompilerPaths.LinkerPath    = LinkerPath;
        FoundCompilerPaths.AssemblerPath = AssemblerPath;
        FoundCompilerPaths.ArchiverPath  = ArchiverPath;
        FoundCompilerPaths.InstallPath   = CompilerInstallPath;
        FoundCompilerPaths.ToolPath      = CompilerToolPath;
        FoundCompilerPaths.BasePath      = CompilerBasePath;
        FoundCompilerPaths.IncludePath   = CompilerIncludePath;
        FoundCompilerPaths.LibraryPath   = CompilerLibraryPath;

        if (!FindFirstCompilerAvailable(String_Null(), String_Null(), String_Null(), String_Null(), &FoundCompilerPaths))
        {
            Receipt.ExitCode = 1;
            return Receipt;
        }

        AddCmdOption(CmdOptionsDB, S("Compiler.Path"), String_Create(Arena, FoundCompilerPaths.CompilerPath));
        AddCmdOption(CmdOptionsDB, S("Assembler.Path"), String_Create(Arena, FoundCompilerPaths.AssemblerPath));
        AddCmdOption(CmdOptionsDB, S("Linker.Path"), String_Create(Arena, FoundCompilerPaths.LinkerPath));
        AddCmdOption(CmdOptionsDB, S("Archiver.Path"), String_Create(Arena, FoundCompilerPaths.ArchiverPath));

        // set defaults for a few key build variables
        FileHandle f = {0};
        bool bAnyOverriden = CheckForBuildVariableOverrides(VariablesDB, CmdOptionsDB);
        Internal_SetDefaultBuildVariables(Arena, f, VariablesDB);

        bAnyVarsOverriden = bAnyOverriden;
    }

    if (bAnyVarsOverriden)
    {
        LOG_LINE_BREAK();
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////
    //
    //
    //                PARSE AND EXPAND END
    //
    //
    ////////////////////////////////////////////////////////////////////////////////////////////////////////

    // build file variable listing feature. list:all or list:varname
    u32 ListingSuccess = 0;
    if (Parameters_TryListVariables(*Arena, Parameters, VariablesDB, CmdOptionsDB, BuildFilePath, &ListingSuccess))
    {
        Receipt.ExitCode = ListingSuccess;
        return Receipt;
    }

    const String Assembly                   = GetVariableValue(VariablesDB, S("Assembly"));
    const String AssemblyPrefix             = GetVariableValue(VariablesDB, S("Assembly.Prefix"));
    const String AssemblyPostfix            = GetVariableValue(VariablesDB, S("Assembly.Postfix"));
    String Extension                        = GetVariableValue(VariablesDB, S("Extension"));
    String Type                             = GetVariableValue(VariablesDB, S("Type"));

    String CompilerPath                     = GetCmdOptionValue(CmdOptionsDB, S("Compiler.Path"));
    String CompilerProgram                  = Filesystem_ExtractFileName(CompilerPath, false);
    const String CompilerFlags              = GetVariableValue(VariablesDB, S("Compiler.Flags"));
    const String CompilerFlagsParams        = GetVariable(VariablesDB, S("Compiler.Flags")).Params;
    const String MaxConcurrentCompilations  = GetVariableValue(VariablesDB, S("Compiler.MaxCores"));
    const String CompilerOutputFlag         = GetVariableValue(VariablesDB, S("Compiler.OutputFlag"));
    const String CompilerCompileFlag        = GetVariableValue(VariablesDB, S("Compiler.CompileFlag"));
    const String CompilerObjectExt          = GetVariableValue(VariablesDB, S("Compiler.ObjectExtension"));
    const String CompilerObjectDirectory    = GetVariableValue(VariablesDB, S("Compiler.ObjectDirectory"));

    const String LinkerPath                 = GetCmdOptionValue(CmdOptionsDB, S("Linker.Path"));
    const String LinkerProgram              = Filesystem_ExtractFileName(LinkerPath, false);
    const String LinkerDefines              = GetVariableValue(VariablesDB, S("Linker.Defines"));

    const String AsmCompilerPath            = GetCmdOptionValue(CmdOptionsDB, S("Assembler.Path"));
    const String AsmProgram                 = Filesystem_ExtractFileName(AsmCompilerPath, false);
    const String AssemblerFlags             = GetVariableValue(VariablesDB, S("Assembler.Flags"));
    const String AssemblerIncludes          = GetVariableValue(VariablesDB, S("Assembler.Includes"));
    const String AssemblerDefines           = GetVariableValue(VariablesDB, S("Assembler.Defines"));

    const String ArchiverPath               = GetCmdOptionValue(CmdOptionsDB, S("Archiver.Path"));
    const String ArchiverProgram            = Filesystem_ExtractFileName(ArchiverPath, false);

    String CompilerFlagPrefixSymbol         = S("-");

    String IncludedSourceFiles              = GetVariableValue(VariablesDB, S("SourceFiles"));
    String ExcludedSourceFiles              = GetVariableValue(VariablesDB, S("SourceFiles.Exclude"));
    String IncludedSourceDir                = GetVariableValue(VariablesDB, S("SourceDirectories"));
    const String ExcludedSourceDir          = GetVariableValue(VariablesDB, S("SourceDirectories.Exclude"));
    String Icon                             = GetVariableValue(VariablesDB, S("Icon"));
    const String PCHPath                    = GetVariableValue(VariablesDB, S("PCH"));
    const String PCHHeaderPath              = GetVariableValue(VariablesDB, S("PCH.h"));
    const String RPath                      = GetVariableValue(VariablesDB, S(".RPathOrigin"));

    #if PLATFORM_APPLE
    const bool bBundleApp                   = DoesBuildVarExist(VariablesDB, S("Bundle"));
    #endif

    const String TitleName                  = GetVariableValue(VariablesDB, S("TitleName"));
    const String InternalName               = GetVariableValue(VariablesDB, S("InternalName"));
    const String Description                = GetVariableValue(VariablesDB, S("Description"));
    const String CompanyName                = GetVariableValue(VariablesDB, S("CompanyName"));
    const String Copyright                  = GetVariableValue(VariablesDB, S("Copyright"));

    String Version                          = GetVariableValue(VariablesDB, S("Version"));

    //const bool bNoRebuildOnDependencyChange = String_ToBool(GetVariableValue(VariablesDB, S("NoRebuildOnDependencyChange")));
    // todo: run pre build?
    const bool bRunPostBuildWhenWorkWasDone = String_ToBool(GetVariableValue(VariablesDB, S(".OnlyRunPostBuildOnChange")));


    String LinkerEntryPoint                 = GetVariableValue(VariablesDB, S("Linker.EntryPoint"));
    String LinkerSubsystem                  = GetVariableValue(VariablesDB, S("Linker.Subsystem"));
    String LinkerStack                      = GetVariableValue(VariablesDB, S("Linker.Stack"));
    const bool bLinkerNoStd                 = DoesBuildVarExist(VariablesDB, S("Linker.NoStdLib"));
    const bool bLinkerNoDefaultLibs         = DoesBuildVarExist(VariablesDB, S("Linker.NoDefaultLibs"));

    ECompiler CompilerVendor = DetermineCompilerVendor(CompilerPath);

    // For compilers that want flags first instead of "-c some/file"
    bool bCompilerFlagsFirst = false;
    {
        ScratchLocal(Temp, Kibibytes(1));
        StringList CFlagParamsList = String_SplitIntoList(&Temp, CompilerFlagsParams, ' ', false);
        bCompilerFlagsFirst = StringList_FindIndex(CFlagParamsList, S("first"), false, StringCompare_Equal, NULL);
    }

    #ifndef HOOD
    LOG("Timestamp:         %S\n", TimeStamp);
    #else
    LOG("stamp of da time yo:         %S\n", TimeStamp);
    #endif

    // bool bNoCompilerProgramExplicityGiven = false;
    // if (CompilerProgram.Length == 0)
    // {
    //     bNoCompilerProgramExplicityGiven = true;
    // }

    // bool bNoAsmCompilerProgramExplicityGiven = false;
    // if (AsmProgram.Length == 0)
    // {
        // bNoAsmCompilerProgramExplicityGiven = true;
    // }

    String_ConvertSlashToPlatformSlash(&CompilerProgram);
    // String_ConvertSlashToPlatformSlash(&LibraryDirectories);
    // String_ConvertSlashToPlatformSlash(&IncludeFlags);
    String_ConvertSlashToPlatformSlash(&Icon);
    String_ConvertSlashToPlatformSlash(&IncludedSourceFiles);
    String_ConvertSlashToPlatformSlash(&ExcludedSourceFiles);
    //String_ConvertSlashToPlatformSlash(&AssertWorkingDirectory);

    // Extension could have multiple options listed
    // for example: to allow for a .dll and a static lib to be generated. So the first one is always the real extension
    String Extension_Og = Extension;
    {
        u32 Index = 0;
        xx String_IndexOfFirstWhitespace(Extension, &Index);
        if (Index > 0)
        {
            Extension.Length = Index;
        }
    }

    bool bIsAssemblyExe = Type.Length == 0 && Extension.Length == 0;

    if (!bIsAssemblyExe)
    {
        bIsAssemblyExe = String_IsEqual(Type, S("app"), false) ||
                         String_IsEqual(Type, S("application"), false) ||
                         String_IsEqual(Type, S("exe"), false) || 
                         String_IsEqual(Type, S("executable"), false);
    }

    if (!bIsAssemblyExe && Type.Length == 0)
    {
        bIsAssemblyExe = Extension.Length == 0 || 
                         String_IsEqual(Extension, S(".elf"), false) ||
                         String_IsEqual(Extension, S(".out"), false) ||
                         String_IsEqual(Extension, S(".exe"), false) ||
                         String_IsEqual(Extension, S(".com"), false);
    }

    if (bIsAssemblyExe)
    {
        AssemblyType = AssemblyType_Executable;
    }
    else
    {
        if (String_IsEqual(Type, S("lib"), false) ||
            String_IsEqual(Type, S("library"), false))
        {
            AssemblyType = AssemblyType_Library;
        }
        else if (String_IsEqual(Type, S("static_lib"), false) || 
                 String_IsEqual(Type, S("static_library"), false))
        {
            AssemblyType = AssemblyType_StaticLibrary;
        }
        else if (String_IsEqual(Type, S("dynamic_lib"), false) || 
                 String_IsEqual(Type, S("dynamic_library"), false) ||
                 String_IsEqual(Type, S("shared_lib"), false) || 
                 String_IsEqual(Type, S("shared_library"), false))
        {
            AssemblyType = AssemblyType_DynamicLibrary;
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
        else
        {
            // no action required
        }

        if (AssemblyType == AssemblyType_None)
        {
            LinearAllocator Scratch = *Arena;
            StringArray Array = String_ParseIntoArray(&Scratch, Extension_Og, ' ', 0, 128);
            bool bHasDynamicLib = false;
            bool bHasStaticLib = false;
            bool bHasPCH = false;
            for each_str (e, Array)
            {
                if (!bHasDynamicLib)
                {
                    bHasDynamicLib = String_IsEqual(*e, S("dll"), false) ||
                                     String_IsEqual(*e, S("so"), false) ||
                                     String_IsEqual(*e, S("dylib"), false);
                }

                if (!bHasStaticLib)
                {
                    bHasStaticLib = String_IsEqual(*e, S("lib"), false) ||
                                    String_IsEqual(*e, S("a"), false);
                }

                if (!bHasPCH)
                {
                    bHasPCH = String_IsEqual(*e, S("pch"), false) ||
                              String_IsEqual(*e, S("gch"), false);
                }
            }

            if (bHasDynamicLib && bHasStaticLib)
            {
                AssemblyType = AssemblyType_Library;
            }
            else if (bHasDynamicLib)
            {
                AssemblyType = AssemblyType_DynamicLibrary;
            }
            else if (bHasStaticLib)
            {
                AssemblyType = AssemblyType_StaticLibrary;
            }
            else if (bHasPCH)
            {
                AssemblyType = AssemblyType_PCH;
            }
            else
            {
                AssemblyType = AssemblyType_CustomCompilerObject;
            }
        }
    }

    if (!String_IsValid(Version))
    {
        #if PLATFORM_WINDOWS
        bFallbackVersion = true;
        #endif

        Version = S("1.0.0");
    }

    StringLocal(FinalAssemblyName, 256);

    // to make life easier on linux because of case fuckjing sensitive commands
    #if !PLATFORM_WINDOWS
    if (bIsAssemblyExe || !bFoundBuildFile) // only do this for executables. people may want case sensitivity when building libraries
    {
        String_Copy(&FinalAssemblyName, AssemblyPrefix);
        String_Append(&FinalAssemblyName, Assembly);
        String_Append(&FinalAssemblyName, AssemblyPostfix);
        String_ToLower(&FinalAssemblyName);
    }
    else
    {
        if (AssemblyType != AssemblyType_CustomCompilerObject)
        {
            String_Append(&FinalAssemblyName, S("lib"));
        }

        String_Append(&FinalAssemblyName, AssemblyPrefix);
        String_Append(&FinalAssemblyName, Assembly);
        String_Append(&FinalAssemblyName, AssemblyPostfix);
    }
    #else
    String_Copy(&FinalAssemblyName, AssemblyPrefix);
    String_Append(&FinalAssemblyName, Assembly);
    String_Append(&FinalAssemblyName, AssemblyPostfix);
    #endif

    const String AssemblyName = FinalAssemblyName;

    //StringLocal(CompilerPath,    MAX_PATH_LENGTH);
    //StringLocal(AsmCompilerPath, MAX_PATH_LENGTH);
    // StringLocal(LinkerPath,      MAX_PATH_LENGTH);
    // StringLocal(ArchiverPath,    MAX_PATH_LENGTH);
    StringLocal(RCCompilerPath,  MAX_PATH_LENGTH);
    StringLocal(MTCompilerPath,  MAX_PATH_LENGTH);
    StringLocal(DumpBinPath,     MAX_PATH_LENGTH);

    #if PLATFORM_WINDOWS
    StringLocal(WindowsSDKBinaryPath,    MAX_PATH_LENGTH);
    StringLocal(WindowsSDKIncludePath,   MAX_PATH_LENGTH);
    StringLocal(WindowsSDKLibUmPath,     MAX_PATH_LENGTH);
    StringLocal(WindowsSDKLibUcrtPath,   MAX_PATH_LENGTH);

    if (!bWasVCVarsBatchExecuted)
    {
        // find the latest version of windows sdk and extract all the useful directories

        LinearAllocator Scratch = *Arena;

        MicrosoftWindowsSDKPaths SDKPaths = {0};
        bool bFoundSDK = FindWindowsSDK(&Scratch, &SDKPaths);
        if (bFoundSDK)
        {
            String_Copy(&WindowsSDKBinaryPath,  SDKPaths.BinPath);
            String_Copy(&WindowsSDKIncludePath, SDKPaths.IncludePath);
            String_Copy(&WindowsSDKLibUcrtPath, SDKPaths.UCRT_LibraryPath);
            String_Copy(&WindowsSDKLibUmPath,   SDKPaths.UM_LibraryPath);
        }

        // this is needed because link.exe tries to call mt.exe if you are using a manifest embed and it cant find it
        // if you have not ran the vcvarsall.bat file. so just add the directory where mt.exe lives to the path
        if (WindowsSDKBinaryPath.Length)
        {
            StringLocal(PathVar, INT16_MAX);
            xx Platform_GetEnvironmentVariableValue(S("PATH"), &PathVar);
            String_AppendF(&PathVar, S(";%S%S"), WindowsSDKBinaryPath, S("\\"CPU_ARCHITECTURE_STRING));
            xx Platform_SetEnvironmentVariableValue(S("PATH"), PathVar);
        }
    }
    #endif

    if (CompilerVendor == Compiler_MSVC || CompilerVendor == Compiler_Clang_MSVC)
    {
        CompilerFlagPrefixSymbol = S("/");
    }

    // convert prefix symbols (if appropriate)
    for (u32 i = 0; i < CompilerFlags.Length; i++)
    {
        if (i == 0 || IsWhitespace(CompilerFlags.Data[i-1]))
        {
            if (CompilerFlags.Data[i] == '-' || CompilerFlags.Data[i] == '/')
            {
                CompilerFlags.Data[i] = CompilerFlagPrefixSymbol.Data[0];
            }
        }
    }

    // run through the assert lists
    /*
    {
        LinearAllocator Scratch = *Arena;
        StringArray CompilersArray = String_ParseIntoArray(&Scratch, AssertCompilers, ' ', 0, 128);
        
        if (CompilersArray.Num > 0)
        {
            bool bAnyCompilerMatch = false;
            for each_str (Str, CompilersArray)
            {
                String Trimmed = String_EatSpaces(*Str);

                if (String_IsEqual(Trimmed, CompilerProgram, false) ||
                    String_IsEqual(Trimmed, CompilerPath, false))
                {
                    bAnyCompilerMatch = true;
                    break;
                }
            }

            StringLocal(CompilersLogString, 128);
            {
                u8 i = 0;
                for each_str_i (i, a, CompilersArray)
                {
                    String_Append(&CompilersLogString, *a);
                    if (CompilersArray.Num > 1 && i != CompilersArray.Num-1)
                    {
                        if (i == CompilersArray.Num-2)
                        {
                            String_Append(&CompilersLogString, S(" and "));
                        }
                        else
                        {
                            String_AppendChar (&CompilersLogString, ',');
                            String_AppendSpace(&CompilersLogString);
                        }
                    }
                }
            }

            if (!bAnyCompilerMatch)
            {
                #ifndef HOOD
                LOG_INLINE_ERROR("[ASSERTION FAILURE] %S can only be compiled with %S. First compiler found was \"%S\". Aborting build...\n\n", BuildFileName, CompilersLogString, CompilerProgram);
                LOG("    This can be fixed by explicity providing the compiler name inside of %S", BuildFileName);

                LOG("    For example:");
                {
                    u8 i = 0;
                    for each_str_i (i, a, CompilersArray)
                    {
                        if (i > 0)
                        {
                            LOG("      or Compiler %S", *a);
                        }
                        else
                        {
                            LOG("         Compiler %S", *a);
                        }
                    }
                }

                #else
                LOG_ERROR("yo dis compiler program \"%S\" cant be used cuh", CompilerProgram);
                #endif

                // if (LogCustomErrorMessage(VariablesDB, S("Compiler"), CompilerProgram, true))
                // {
                //     LOG_LINE_BREAK();
                // }

                Receipt.ExitCode = 1;
                return Receipt;
            }
        }


    }
    */

    bool bIsRebuild    = StringArray_Contains(Parameters, S("rebuild"), false);

    // force rebuild if we say so in the build file
    if (!bIsRebuild)
    {
        if (DoesBuildVarExist(VariablesDB, S("AlwaysRebuild")) ||
            DoesBuildVarExist(VariablesDB, S("AlwaysRebuildAll")))
        {
            LOG("\"AlwaysRebuild\" was specified in %S. Forcing rebuild...\n", BuildFileName);

            bIsRebuild = true;
            bIsClean = false;
        }
    }

    bool bExportingSomething = false;
    for (u8 i = 0; i < Parameters.Num; i++)
    {
        const String Arg = Parameters.List[i];

        if (String_StartsWith(Arg, S("export:"), false))
        {
            bExportingSomething = true;
            break;
        }
    }

    // pre depend
    // TODO: time this
    if (!bExportingSomething)
    {
        if (!TryRunBuildCommands(S("PreDepend"), WorkingPath, VariablesDB, NULL))
        {
            Receipt.ExitCode = 1;
            return Receipt;
        }
    }

    bool bDependenciesDoneWork = false;

    Clock DependencyBuildClock;
    Clock_Start(&DependencyBuildClock);

    // run build depenencies
    // if (!bExportingSomething)
    {
        SArray(FileVariable*, Depends, 256) = {0};

        bool bRanAnyDependencies = false;
        for each (FileVariable, Var, VariablesDB)
        {
            if (String_IsEqual(Var.Name, S("Depend"), false) ||
                String_IsEqual(Var.Name, S("Depends"), false))
            {
                SArray_Add(Depends, Var_);
            }
        }

        // Note(Ali): I would've liked to keep this allocation on the stack but it is very hard to make a 
        //            gurantee against stackover flow errors (currently the stack size for this program is
        //            set to 8MB). what happens if we are N "depends" deep? if i keep on allocating 1MB on
        //            the stack for each depends, then we will defintely overflow, subsequent functions/scopes
        //            will not have enough space on the stack for them to function properly thus causing a 
        //            crash :( I want to revist this in the future again to see how i can make a pretty good
        //            "gurantee" that the program won't get into that state. On the bright side, at least
        //            we aren't dynamically shitting memory out everytime we need some :P
        //            So just allocate one big chunk and let our allocator dish out the memory.
        //i8 ArenaMemory[Mebibytes(1)] = {0};
        usize TotalMem = Mebibytes(1);
        void* ArenaMemory = Platform_MemAlloc(TotalMem);

        if (!ArenaMemory)
        {
            LOG_FATAL("Failed to allocate memory from the operating system!");

            Receipt.ExitCode = 1;
            return Receipt;
        }

        LinearAllocator NewArena = {0};
        LinearAllocator_Create(TotalMem, ArenaMemory, &NewArena);

        for (u32 i = 0; i < Depends_Count; i++)
        {
            //MemZero(ArenaMemory, TotalMem);

            LinearAllocator_Reset(&NewArena, 0); // "free" the memory

            FileVariable Var = *Depends[i];

            String Value = Var.Value;

            String BuildFile;
            String SpecifiedParams = String_Null();
            String Directory = String_Null();

            u32 PipeIndex = 0;
            if (String_IndexOfChar(Value, '|', &PipeIndex))
            {
                // we have params that we need to pass in
                SpecifiedParams = String_EatSpaces(StrSlice(Value.Data+PipeIndex+1, Value.Length-PipeIndex-1));
            }

            bool bDirectoryOnly = false;

            u32 SpaceIndex = 0;
            if (PipeIndex)
            {
                BuildFile = StrSlice(Value.Data, PipeIndex);
            }
            else
            {
                if (String_IndexOfFirstWhitespace(Value, &SpaceIndex))
                {
                    BuildFile = StrSlice(Value.Data, SpaceIndex);
                }
                else
                {
                    BuildFile = Value;
                }
            }

            StringLocal(BuildFilePathCopy, MAX_PATH_LENGTH);
            {
                xx String_SanitizePath(&BuildFilePathCopy, BuildFile);
                BuildFile = BuildFilePathCopy;
            }

            // if someone wants to not specify a build file, they can specify the path instead
            if (String_CountPathSeparators(BuildFile) > 0)
            {
                Directory = BuildFile;
                bDirectoryOnly = true;
            }

            // circular dependency. abort, this is bad...
            if (String_IsValid(CameFromBuildFile))
            {
                u32 Dot = 0;
                bool bHasDot = String_IndexOfLastChar(CameFromBuildFile, '.', &Dot);
                if (String_IsEqual(BuildFile, bHasDot ? StrSlice(CameFromBuildFile.Data, Dot) : CameFromBuildFile, false))
                {
                    LOG_ERROR("Circular build dependency. We came from \"%S\" but \"%S\" is trying to build \"%S\", which is circular and doesn't make sense", CameFromBuildFile, BuildFileName, CameFromBuildFile);

                    Receipt.ExitCode = 1;
                    return Receipt;
                }
            }

            StringLocal(CustomWorkingPath_Full, MAX_PATH_LENGTH);

            String CustomRelativePath = String_Null();
            bool bUsingRelativePath = true;

            if (bDirectoryOnly)
            {
                if (Filesystem_IsPathRelative(Directory))
                {
                    CustomRelativePath = Directory;
                    if (String_EndsWith(Directory, S(".build"), false))
                    {
                        CustomRelativePath = Filesystem_ExtractFilePath(Directory, false);
                    }

                    String_BuildPath(&CustomWorkingPath_Full, WorkingPath, CustomRelativePath);
                    xx Filesystem_ConvertRelativeToAbsolutePath(&CustomWorkingPath_Full);
                }
                else
                {
                    bUsingRelativePath = false;
                    String_Copy(&CustomWorkingPath_Full, Directory);
                }
            }
            else
            {
                if (SpaceIndex > 0)
                {
                    String CustomPath;

                    if (PipeIndex > 0)
                    {
                        CustomPath = String_EatSpacesFromEnd(StrSlice(Value.Data+SpaceIndex+1, PipeIndex-SpaceIndex-1));
                    }
                    else
                    {
                        CustomPath = String_EatSpacesFromEnd(StrSlice(Value.Data+SpaceIndex+1, Value.Length-SpaceIndex-1));
                    }

                    xx String_EatPathSeparatorsInlineFromEnd(&CustomPath);
                    String_ConvertSlashToPlatformSlash(&CustomPath);

                    if (Filesystem_IsPathRelative(CustomPath))
                    {
                        CustomRelativePath = CustomPath;

                        String_BuildPath(&CustomWorkingPath_Full, WorkingPath, CustomPath);
                        xx Filesystem_ConvertRelativeToAbsolutePath(&CustomWorkingPath_Full);
                    }
                    else
                    {
                        bUsingRelativePath = false;
                        String_Copy(&CustomWorkingPath_Full, CustomPath);
                    }
                }
                else
                {
                    bUsingRelativePath = false;
                    String_Copy(&CustomWorkingPath_Full, WorkingPath);
                }
            }

            xx String_EatPathSeparatorsInlineFromEnd(&CustomWorkingPath_Full);

            StringLocal(BuildFileNameWithExt, 128);
            if (!bDirectoryOnly)
            {
                String_Append(&BuildFileNameWithExt, BuildFile);

                if (!String_EndsWith(BuildFile, S(".build"), false))
                {
                    String_Append(&BuildFileNameWithExt, S(".build"));
                }
            }

            StringList List = String_SplitIntoList(&NewArena, SpecifiedParams, ' ', true);
            u8 Num = 0;
            for each_str_list (List)
            {
                Num += 1;
            }

            StringArray NewParams = StringArray_Null();

            if (Num > 0)
            {
                NewParams.List = LinearAllocator_Allocate(&NewArena, sizeof(String) * Num);
                NewParams.Num = Num;

                u8 j = 0;
                for each_str_list (List)
                {
                    NewParams.List[j] = It.String;
                    j++;
                }
            }
            
            LOG("Depend -> %S\n", bDirectoryOnly ? Directory : BuildFileNameWithExt);
            bRanAnyDependencies = true;

            if (!Filesystem_DoesDirectoryExist(CustomWorkingPath_Full))
            {
                LOG_ERROR("Failed to find a .build in \"%S\" because the directory does not exist.", CustomWorkingPath_Full);

                Receipt.ExitCode = 1;
                return Receipt;
            }

            StringLocal(NewBuildFilePath, MAX_PATH_LENGTH);

            {
                StringLocal(Temp, MAX_PATH_LENGTH);
                String_BuildPath(&Temp, CustomWorkingPath_Full, Filesystem_ExtractFileName(Directory, true));
                if (bDirectoryOnly && Filesystem_DoesFileExist(Temp))
                {
                    String_Copy(&NewBuildFilePath, Temp);
                    String_Copy(&BuildFileNameWithExt, Filesystem_ExtractFileName(Directory, true));
                }
                else
                {
                    BuildFileDirectoryIteratorData Data = {0};
                    Data.bNoBuildFileSpecifiedInCmd = false;
                    Data.BuildFileIndex = -1;
                    Data.RootPathIndex = -1;
                    Data.Name = &BuildFileNameWithExt;
                    Data.Path = &NewBuildFilePath;
                    Data.Arguments = NewParams;

                    Filesystem_IterateDirectory_Ex(CustomWorkingPath_Full, &BuildFileDirectoryIterator, !bDirectoryOnly, &Data);

                    if (!Data.bFoundBuildFile)
                    {
                        if (!bDirectoryOnly)
                        {
                            LOG_ERROR("Failed to find %S in %S", BuildFileNameWithExt, CustomWorkingPath_Full);
                        }

                        Receipt.ExitCode = 1;
                        return Receipt;
                    }
                }
            }

            FileHandle f = {0};
            if (!Filesystem_Open(NewBuildFilePath, FileMode_Read, &f))
            {
                #ifndef HOOD
                LOG_ERROR("Failed to open build file \"%S\" for reading", NewBuildFilePath);
                #else
                LOG_ERROR("wtf, cant read this shit man, think the path to the build file is wrong or smthg homie. this is what i got: %S", NewBuildFilePath);
                #endif

                Receipt.ExitCode = 1;
                return Receipt;
            }

            String RelativeWorkingPathFromMe = bUsingRelativePath ? CustomRelativePath : CustomWorkingPath_Full;

            PlatformMutex NewMutex = {0};
            BuildReceipt FreshReceipt = BuildTarget(&NewArena, f, &NewMutex, CustomWorkingPath_Full, NewParams, BuildFileName, -1, -1);
            if (NewMutex.Handle) { xx Platform_ReleaseMutex(&NewMutex); }

            Filesystem_Close(&f);

            // Reset the working path
            xx Platform_SetWorkingDirectory(WorkingPath);

            // add all the public keys of what the dependency build file exposed, and append them to ours.
            // TODO: Depends(protected) param? to avoid nested depends from bubbling up to the parent?
            {
                // if we are depending on a libarary, automatically append the build directory
                // of the thing we just built
                if (FreshReceipt.AssemblyType == AssemblyType_Library ||
                    FreshReceipt.AssemblyType == AssemblyType_DynamicLibrary ||
                    FreshReceipt.AssemblyType == AssemblyType_StaticLibrary)
                {
                    StringLocal(LibBuildPath, MAX_PATH_LENGTH);
                    String_BuildPath(&LibBuildPath, S("\""), RelativeWorkingPathFromMe, FreshReceipt.BuildDirectory, S("\""));
                    AddOrAppendVariable(Arena, VariablesDB, S("Library.Paths"), LibBuildPath, String_Null(), GetMaxValueLengthForReservedKey(S("Library.Paths")));
                    AddOrAppendVariable(Arena, VariablesDB, S("Library.Paths.Public"), LibBuildPath, String_Null(), GetMaxValueLengthForReservedKey(S("Library.Paths")));

                    // remove lib prefix from assembly name so that linking does not fail
                    String AssemblyNameTrimmed = FreshReceipt.AssemblyName;
                    if (String_StartsWith(FreshReceipt.AssemblyName, S("lib"), false))
                    {
                        AssemblyNameTrimmed = String_Right(AssemblyNameTrimmed, 3);
                    }

                    AddOrAppendVariable(Arena, VariablesDB, S("Libraries"), AssemblyNameTrimmed, String_Null(), GetMaxValueLengthForReservedKey(S("Libraries")));
                    AddOrAppendVariable(Arena, VariablesDB, S("Libraries.Public"), AssemblyNameTrimmed, String_Null(), GetMaxValueLengthForReservedKey(S("Libraries")));
                }

                AddOrAppendVariable(Arena, VariablesDB, S("Defines"),             FreshReceipt.Defines, String_Null(),     GetMaxValueLengthForReservedKey(S("Includes")));
                AddOrAppendVariable(Arena, VariablesDB, S("Defines.Public"),      FreshReceipt.Defines, String_Null(),     GetMaxValueLengthForReservedKey(S("Includes")));
                AddOrAppendVariable(Arena, VariablesDB, S("Libraries"),           FreshReceipt.Libraries, String_Null(),   GetMaxValueLengthForReservedKey(S("Libraries")));
                AddOrAppendVariable(Arena, VariablesDB, S("Libraries.Public"),    FreshReceipt.Libraries, String_Null(),   GetMaxValueLengthForReservedKey(S("Libraries")));
                AddOrAppendVariable(Arena, VariablesDB, S("Linker.Flags"),        FreshReceipt.LinkerFlags, String_Null(), GetMaxValueLengthForReservedKey(S("Linker.Flags.Public")));
                AddOrAppendVariable(Arena, VariablesDB, S("Linker.Flags.Public"), FreshReceipt.LinkerFlags, String_Null(), GetMaxValueLengthForReservedKey(S("Linker.Flags.Public")));

                {
                    StringList Paths = String_SplitIntoList(Arena, FreshReceipt.LibraryPaths, ' ', true);
                    for each_string_in_list (Paths)
                    {
                        StringLocal(Temp, MAX_PATH_LENGTH);
                        if (Filesystem_IsPathRelative(It.String))
                        {
                            String_BuildPath(&Temp, S("\""), RelativeWorkingPathFromMe, It.String, S("\""),);
                        }
                        else
                        {
                            String_Copy(&Temp, It.String);
                        }
                        
                        AddOrAppendVariable(Arena, VariablesDB, S("Library.Paths"), Temp, String_Null(), GetMaxValueLengthForReservedKey(S("Library.Paths")));
                        AddOrAppendVariable(Arena, VariablesDB, S("Library.Paths.Public"), Temp, String_Null(), GetMaxValueLengthForReservedKey(S("Library.Paths")));
                    }
                }

                {
                    StringList Paths = String_SplitIntoList(Arena, FreshReceipt.Includes, ' ', true);
                    for each_string_in_list (Paths)
                    {
                        StringLocal(Temp, MAX_PATH_LENGTH);

                        if (Filesystem_IsPathRelative(It.String))
                        {
                            String_BuildPath(&Temp, S("\""), RelativeWorkingPathFromMe, It.String, S("\""),);
                        }
                        else
                        {
                            String_Copy(&Temp, It.String);
                        }

                        AddOrAppendVariable(Arena, VariablesDB, S("Includes"), Temp, String_Null(), GetMaxValueLengthForReservedKey(S("Includes")));
                        AddOrAppendVariable(Arena, VariablesDB, S("Includes.Public"), Temp, String_Null(), GetMaxValueLengthForReservedKey(S("Includes")));
                    }
                }

                // TODO: fix repeating duplicated build directory paths, if depends was called on the same build file multiple times

            }

            if (FreshReceipt.bWorkWasDone)
            {
                // TODO: fuck... i need a way to just link and not rebuild the child. Implement this...
                bDependenciesDoneWork = true;

                bool bIsSpecial = String_IsEqual(Var.Params, S("Rebuild_If_Done_Work"), false);
                if (!bIsRebuild && bIsSpecial)
                {
                    // LOG("\nDependency \"%S\" was modified. Forcing rebuild...", BuildFileNameWithExt);
                    // bIsRebuild = true;
                }
            }
            else if (FreshReceipt.ExitCode != 0)
            {
                #ifndef HOOD
                LOG_ERROR("Dependency build \"%S\" failed. Aborting build...", BuildFileNameWithExt);
                #else
                LOG_ERROR("brah wtf, depndncy buil faild nigga");
                #endif

                Receipt.ExitCode = 1;
                return Receipt;
            }
            else
            {
                // no action required
            }

            LOG_LINE_BREAK();
        }

        LinearAllocator_Destroy(&NewArena);
        Platform_MemFree(ArenaMemory);

        if (bRanAnyDependencies && !bIsClean)
        {
            Clock_Tick(&DependencyBuildClock);
            LOG("[All build dependencies complete. Continuing with %S]\n", BuildFileName);
        }
    }

    // TODO: time this

    if (!bExportingSomething)
    {
        if (!TryRunBuildCommands(S("PreBuild"), WorkingPath, VariablesDB, NULL))
        {
            Receipt.ExitCode = 1;
            return Receipt;
        }
    }

    // TODO: make use of this. for example. do a link only build instead of compile and link
    xx bDependenciesDoneWork;

    // assert libraries exist
    if (!bIsClean)
    {
        /*
        LinearAllocator Scratch = *Arena;
        // TODO: support native libraries. windows kits, msvc lib paths, etc
        StringArray LibsArray = String_ParseIntoArray(&Scratch, AssertLibs, ' ', 0, 128);
        for each_str (S, LibsArray)
        {
            const String Trimmed = String_EatSpaces(*S);

            StringLocal(TrimmedCopy, MAX_PATH_LENGTH);
            #if !PLATFORM_WINDOWS
            String_Append(&TrimmedCopy, S("lib"));
            String_Append(&TrimmedCopy, Trimmed);
            #else
            String_Copy(&TrimmedCopy, Trimmed);
            #endif

            bool bExactFile = false;
            u32 LastDot = 0;
            if (String_IndexOfLastChar(TrimmedCopy, '.', &LastDot))
            {
                bExactFile = true;
            }

            bool bAnyFound = false;

            struct _blah_
            {
                String* LibName;
                bool* bFound;
                bool bWithExtension;
            };

            struct _blah_ Packet = {.LibName = &TrimmedCopy, .bFound = &bAnyFound, .bWithExtension = bExactFile};

            StringList DirList = String_SplitIntoList(&Scratch, LibraryDirectories, ' ', true);
            for each_str_list (DirList)
            {
                StringLocal(DirPath, MAX_PATH_LENGTH);

                if (Filesystem_IsPathRelative(It.String))
                {
                    String_BuildPath(&DirPath, WorkingPath, It.String);
                }
                else
                {
                    String_Copy(&DirPath, It.String);
                }

                Filesystem_ConvertRelativeToAbsolutePath(&DirPath);

                if (Filesystem_DoesDirectoryExist(DirPath))
                {
                    Filesystem_IterateDirectory_Ex(DirPath, LibraryDirectoryIterator, false, &Packet);
                }
            }
            
            if (!bAnyFound)
            {
                if (bExactFile)
                {
                    StringLocal(Copy, 256);
                    String_Copy(&Copy, StrSlice(TrimmedCopy.Data, LastDot));
                    bAnyFound = Platform_FindFile(Copy, StrShiftF(TrimmedCopy, LastDot));
                }
                else
                {
                    #if PLATFORM_WINDOWS
                    bAnyFound = Platform_FindFile(TrimmedCopy, S(".lib"));
                    #else
                    bAnyFound = Platform_FindFile(TrimmedCopy, S(".a"));
                    if (!bAnyFound)
                        bAnyFound = Platform_FindFile(TrimmedCopy, S(".so"));
                    #endif
                }
            }

            if (!bAnyFound)
            {
                #ifndef HOOD
                if (bExactFile)
                {
                    LOG_INLINE_ERROR("[ASSERTION FAILURE] Library \"%S\" does not exist. Aborting build...\n\n", TrimmedCopy);
                }
                else
                {
                    #if PLATFORM_WINDOWS
                    LOG_INLINE_ERROR("[ASSERTION FAILURE] Library \"%S.lib\" does not exist. Aborting build...\n\n", TrimmedCopy);
                    #elif PLATFORM_APPLE
                    LOG_INLINE_ERROR("[ASSERTION FAILURE] Library \"%S(.dylib/.a)\" does not exist. Aborting build...\n\n", TrimmedCopy);
                    #else
                    LOG_INLINE_ERROR("[ASSERTION FAILURE] Library \"%S(.so/.a)\" does not exist. Aborting build...\n\n", TrimmedCopy);
                    #endif
                }
                #else
                LOG_ERROR("cant find this library nigga \"%S\". i searched fuckin everywhere bro\n", TrimmedCopy);
                #endif

                if (LogCustomErrorMessage(VariablesDB, S("Lib"), TrimmedCopy, false))
                {
                    LOG_LINE_BREAK();
                }

                StringLocal(PathValue, Kibibytes(32));
                #if PLATFORM_WINDOWS
                Platform_GetEnvironmentVariableValue(S("Path"), &PathValue);
                #else
                Platform_GetEnvironmentVariableValue(S("PATH"), &PathValue);
                #endif

                LOG("Here is a list of the directories that was searched through:\n");
                LOG_INLINE_WARNING("Library Directories\n");
                for each_str_list (DirList)
                {
                    StringLocal(DirPath, MAX_PATH_LENGTH);

                    if (Filesystem_IsPathRelative(It.String))
                    {
                        String_BuildPath(&DirPath, WorkingPath, It.String);
                    }
                    else
                    {
                        String_Copy(&DirPath, It.String);
                    }

                    Filesystem_ConvertRelativeToAbsolutePath(&DirPath);

                    LOG("    %S", DirPath);
                }
                LOG_LINE_BREAK();
                LOG_INLINE_WARNING("PATH environment\n");
                #if PLATFORM_WINDOWS
                StringArray Values = String_ParseIntoArray(&Scratch, PathValue, ';', 0, 128);
                #else
                StringArray Values = String_ParseIntoArray(&Scratch, PathValue, ':', 0, 128);
                #endif
                for each_str (v, Values)
                {
                    LOG("    %S", *v);
                }

                return 1;
            }
        }
        */
    }

    String SourceDirectory       = GetVariableValue(VariablesDB, S("SourceDirectory"));
    String BuildDirectory        = GetVariableValue(VariablesDB, S("BuildDirectory"));
    String IntermediateDirectory = GetVariableValue(VariablesDB, S("IntermediateDirectory"));

    String_ConvertSlashToPlatformSlash(&SourceDirectory);
    String_ConvertSlashToPlatformSlash(&IntermediateDirectory);
    String_ConvertSlashToPlatformSlash(&BuildDirectory);

    bool bDumpObjFilesInOneDirectory = String_EndsWith(IntermediateDirectory, S("/."), false) ||
                                       String_EndsWith(IntermediateDirectory, S("\\."), false);

    // these directories must be relative
    if (!Filesystem_IsPathRelative(SourceDirectory))
    {
        TODO();
    }

    if (!Filesystem_IsPathRelative(BuildDirectory))
    {
        TODO();
    }

    if (!Filesystem_IsPathRelative(IntermediateDirectory))
    {
        TODO();
    }

    StringLocal(BuildBaseDirectory, MAX_PATH_LENGTH);
    String_BuildPath(&BuildBaseDirectory, WorkingPath, BuildDirectory);
    String_AppendPathSeparator(&BuildBaseDirectory);
    xx Filesystem_ConvertRelativeToAbsolutePath(&BuildBaseDirectory);

    StringLocal(IntermediateBaseDirectory, MAX_PATH_LENGTH);
    String_BuildPath(&IntermediateBaseDirectory, WorkingPath, IntermediateDirectory);
    String_AppendPathSeparator(&IntermediateBaseDirectory);
    xx Filesystem_ConvertRelativeToAbsolutePath(&IntermediateBaseDirectory);

    StringLocal(SourceDir, MAX_PATH_LENGTH);
    String_BuildPath(&SourceDir, WorkingPath, SourceDirectory);
    String_AppendPathSeparator(&SourceDir);
    xx Filesystem_ConvertRelativeToAbsolutePath(&SourceDir);

    const bool bBuildDirSameAsSource        = String_IsEqual(BuildBaseDirectory, SourceDir, false);
    const bool bIntermediateDirSameAsSource = String_IsEqual(IntermediateBaseDirectory, SourceDir, false);

    const bool bDidIntermediateDirectoryExist = Filesystem_DoesDirectoryExist(IntermediateBaseDirectory);
    const bool bDidBuildDirectoryExist        = Filesystem_DoesDirectoryExist(BuildBaseDirectory);

    // actual source path cannot be inside of the build or intermediate directory, this is an error
    {
        StringLocal(Test, MAX_PATH_LENGTH);
        String_BuildPath(&Test, WorkingPath, BuildDirectory);
        String_AppendPathSeparator(&Test);
        xx Filesystem_ConvertRelativeToAbsolutePath(&Test);
        String_Append(&Test, StrShiftF(SourceDir, Test.Length));
        if (String_IsEqual(Test, SourceDir, false))
        {
            LOG_ERROR("%S: Source Directory '%S' must not be nested inside\n"
                        "        of the given Build Directory '%S'\n\n"
                        "        You must keep both of these paths separate.",
                        BuildFileName, SourceDir, BuildBaseDirectory);

            Receipt.ExitCode = 1;
            return Receipt;
        }

        String_Empty(&Test);
        String_BuildPath(&Test, WorkingPath, IntermediateDirectory);
        String_AppendPathSeparator(&Test);
        xx Filesystem_ConvertRelativeToAbsolutePath(&Test);
        String_Append(&Test, StrShiftF(SourceDir, Test.Length));
        if (String_IsEqual(Test, SourceDir, false))
        {
            LOG_ERROR("%S: Source Directory '%S' must not be nested inside\n"
                        "        of the given Intermediate Directory '%S'\n\n"
                        "        You must keep both of these paths separate.",
                        BuildFileName, SourceDir, IntermediateBaseDirectory);

            Receipt.ExitCode = 1;
            return Receipt;
        }
    }

    // assert that the given directories exist before proceeding with the build
    // ignoring build/intermediate since they will be created if they don't exist
    {
        LinearAllocator Scratch = *Arena;
        if (!Filesystem_DoesDirectoryExist(SourceDir))
        {
            #ifndef HOOD
            LOG_ERROR("%S: Given source directory \"%S\" does not exist. Aborting build...", BuildFileName, SourceDir);
            #else
            LOG_ERROR("yo dis source dir \"%S\" dont exist cuhh", SourceDir);
            #endif

            Receipt.ExitCode = 1;
            return Receipt;
        }

        // check if the given LibraryDirectories exist
        String LibraryDirectories = GetVariableValue(VariablesDB, S("Library.Paths"));
        StringList DirList = String_SplitIntoList(&Scratch, LibraryDirectories, ' ', true);
        for each_str_list (DirList)
        {
            StringLocal(DirPath, MAX_PATH_LENGTH);

            StringLocal(DirCopy, MAX_PATH_LENGTH);
            xx String_SanitizeQuotes(&DirCopy, It.String);
            DirCopy = String_TrimQuotes(DirCopy);

            if (String_IsEqual(It.String, BuildDirectory, false) ||
                String_IsEqual(It.String, IntermediateDirectory, false))
            {
                continue;
            }

            if (Filesystem_IsPathRelative(DirCopy))
            {
                String_BuildPath(&DirPath, WorkingPath, DirCopy);
            }
            else
            {
                String_BuildPath(&DirPath, DirCopy);
            }

            xx Filesystem_ConvertRelativeToAbsolutePath(&DirPath);

            if (!Filesystem_DoesDirectoryExist(DirPath))
            {
                #ifndef HOOD
                LOG_ERROR("%S: Given library directory \"%S\" does not exist. Aborting build...", BuildFileName, DirPath);
                #else
                LOG_ERROR("yo dis library path \"%S\" dont exist cuhh", DirPath);
                #endif

                Receipt.ExitCode = 1;
                return Receipt;
            }
        }
    }

    // Receipt.BuildDirectory  = String_Create(Arena, BuildDirectory);
    // Receipt.Includes        = String_Create(Arena, IncludeFlags_Public);
    // Receipt.Defines         = String_Create(Arena, Defines_Public);
    // Receipt.Libraries       = String_Create(Arena, Libraries_Public);
    // Receipt.LibraryPaths    = String_Create(Arena, LibraryDirectories_Public);
    Receipt.BuildDirectory  = GetVariableValue(VariablesDB, S("BuildDirectory"));
    Receipt.Includes        = GetVariableValue(VariablesDB, S("Includes.Public"));
    Receipt.Defines         = GetVariableValue(VariablesDB, S("Defines.Public"));
    Receipt.Libraries       = GetVariableValue(VariablesDB, S("Libraries.Public"));
    Receipt.LibraryPaths    = GetVariableValue(VariablesDB, S("Library.Paths.Public"));
    Receipt.LinkerFlags     = GetVariableValue(VariablesDB, S("Linker.Flags.Public"));
    Receipt.AssemblyName    = String_Create(Arena, AssemblyName);
    Receipt.AssemblyType    = AssemblyType;

    StringList WhitelistArray    = String_SplitIntoList(Arena, IncludedSourceFiles, ' ', true);
    StringList BlacklistArray    = String_SplitIntoList(Arena, ExcludedSourceFiles, ' ', true);
    StringList WhitelistDirArray = String_SplitIntoList(Arena, IncludedSourceDir, ' ', true);
    StringList BlacklistDirArray = String_SplitIntoList(Arena, ExcludedSourceDir, ' ', true);

    // any custom source files?
    StringList CustomExtensionsList = {0};
    if (AssemblyType == AssemblyType_CustomCompilerObject)
    {
        StringLocal(SourceFileExtensions, 128);
        for each_str_list (WhitelistArray)
        {
            u32 Index = 0;
            if (String_IndexOfLastChar(It.String, '.', &Index))
            {
                const String Ext = StrShiftF(It.String, Index);
                if (!IsSource(Ext))
                {
                    String_Append(&SourceFileExtensions, Ext);
                    String_AppendSpace(&SourceFileExtensions);
                }
            }
        }

        CustomExtensionsList = String_SplitIntoList(Arena, SourceFileExtensions, ' ', false);
    }

    StringLocal(FirstSourceFileName, 256);
    SourceCountData CountData             = {0};
    // CountData.AssemblyFileTime            = ;
    CountData.NumSources                  = 0;
    CountData.NumAsmSources               = 0;
    CountData.NumHeaders                  = 0;
    CountData.NumRcSources                = 0;
    CountData.FirstSourceFileName         = &FirstSourceFileName;
    CountData.WorkingDirectory            = WorkingPath;
    CountData.SourceDirectory             = SourceDirectory;
    CountData.IntermediateBaseDirectory   = IntermediateBaseDirectory;
    CountData.IntermediateDirectory       = IntermediateDirectory;
    CountData.BuildDirectory              = BuildDirectory;
    CountData.WhitelistArray              = WhitelistArray;
    CountData.BlacklistArray              = BlacklistArray;
    CountData.WhitelistDirArray           = WhitelistDirArray;
    CountData.BlacklistDirArray           = BlacklistDirArray;
    CountData.CustomSourceExtensions      = CustomExtensionsList;
    CountData.FilteredFilesNext           = &CountData.FilteredFiles;
    CountData.ArenaForFilterList          = Arena;
    CountData.bHasCppFiles                = false;
    CountData.bIsPCHBuild                 = AssemblyType == AssemblyType_PCH;

    Filesystem_IterateDirectory_Ex(SourceDir, &SourceFileCounterDirectoryIterator, true, &CountData);

    /*
    if (CountData.FilteredFiles)
    {
        for each_string_in_list (*CountData.FilteredFiles)
        {
            LOG("%S", It.String);
        }
        Clock_PrintElapsedTime(&c, true);
        return 1;
    }
    */

    const u32 NumSources = CountData.NumSources + CountData.NumAsmSources + CountData.NumRcSources;

    // use the first source file as the assembly name (if none provided or if "untitled" was set)
    if (CountData.NumSources == 1 || CountData.NumAsmSources == 1)
    {
        if (!String_IsValid(Assembly) ||
            String_IsEqual(Assembly, S("Untitled"), false))
        {
            String Trimmed = Filesystem_ExtractFileName(FirstSourceFileName, false);

            String_Copy(&FinalAssemblyName, AssemblyPrefix);
            String_Append(&FinalAssemblyName, Trimmed);
            String_Append(&FinalAssemblyName, AssemblyPostfix);
        }
    }

    StringLocal(AssemblyNameWithExt, 256);
    String_Copy(&AssemblyNameWithExt, FinalAssemblyName);
    if (Extension.Length > 0)
    {
        if (Extension.Data[0] != '.')
        {
            String_AppendChar(&AssemblyNameWithExt, '.');
        }

        String_Append(&AssemblyNameWithExt, Extension);
    }

    #if !PLATFORM_WINDOWS
    if (bIsAssemblyExe)
    {
        String_ToLower(&AssemblyNameWithExt);
    }
    #endif

    /*
    StringLocal(AssemblyPath, MAX_PATH_LENGTH);
    String_BuildPath(&AssemblyPath, WorkingPath, BuildDirectory, AssemblyNameWithExt);

    usize AssemblyFileTime = Filesystem_GetLastWriteTime(AssemblyPath);

    if (AssemblyFileTime > 0)
    {
        if (bAnyHeaderFileModified)
        {
            bIsRebuild = true;
        }
    }
    */


    u32 NumCompiled = 0;

    if (NumSources == 0)
    {
        if (bQuietBuild) { Logging_Enable(); }

        #ifndef HOOD
        LOG("Nothing to compile");
        #else
        LOG("no work to do homie");
        #endif

        goto End;
    }

    // save the directory state
    if (!bIsClean)
    {
        StringLocal(Name, 256);
        String_Append(&Name, BuildFileName);
        String_Append(&Name, S(".directory_state"));
        StringLocal(DirectoryStatePath, MAX_PATH_LENGTH);
        String_BuildPath(&DirectoryStatePath, WorkingPath, IntermediateDirectory, Name);
        FileHandle f = FileHandle_Null();
        
        if (Filesystem_DoesFileExist(DirectoryStatePath))
        {
            if (Filesystem_Open(DirectoryStatePath, FileMode_Read, &f))
            {
                SourceCountData CountData_File = {0};
                if (Filesystem_Read(f, sizeof(SourceCountData), &CountData_File, NULL))
                {
                    bool bAnyChange = CountData_File.NumSources    != CountData.NumSources ||
                                      CountData_File.NumAsmSources != CountData.NumAsmSources ||
                                      CountData_File.NumHeaders    != CountData.NumHeaders ||
                                      CountData_File.NumRcSources  != CountData.NumRcSources;

                    if (bAnyChange)
                    {
                        LOG("Directory state has changed. Forcing rebuild...");
                        if (CountData_File.NumSources > CountData.NumSources)
                        {
                            LOG("    %u source file(s) removed", CountData_File.NumSources - CountData.NumSources);
                        }
                        else if (CountData.NumSources > 0)
                        {
                            LOG("    %u source file(s) added", CountData.NumSources - CountData_File.NumSources);
                        }
                        else
                        {
                            // no action required
                        }

                        if (CountData_File.NumAsmSources > CountData.NumAsmSources)
                        {
                            LOG("    %u assembly file(s) removed", CountData_File.NumAsmSources - CountData.NumAsmSources);
                        }
                        else if (CountData.NumAsmSources > 0)
                        {
                            LOG("    %u assembly file(s) added", CountData.NumAsmSources - CountData_File.NumAsmSources);
                        }
                        else
                        {
                            // no action required
                        }

                        if (CountData_File.NumHeaders > CountData.NumHeaders)
                        {
                            LOG("    %u header file(s) removed", CountData_File.NumHeaders - CountData.NumHeaders);
                        }
                        else if (CountData.NumHeaders > 0)
                        {
                            LOG("    %u header file(s) added", CountData.NumHeaders - CountData_File.NumHeaders);
                        }
                        else
                        {
                            // no action required
                        }

                        if (CountData_File.NumRcSources > CountData.NumRcSources)
                        {
                            LOG("    %u resource file(s) removed", CountData_File.NumRcSources - CountData.NumRcSources);
                        }
                        else if (CountData.NumRcSources > 0)
                        {
                            LOG("    %u resource file(s) added", CountData.NumRcSources - CountData_File.NumRcSources);
                        }
                        else
                        {
                            // no action required
                        }

                        LOG_LINE_BREAK();

                        bIsRebuild = true;
                    }
                }

                Filesystem_Close(&f);
            }
        }

        if (Filesystem_Open(DirectoryStatePath, FileMode_Write, &f))
        {
            Filesystem_Write(f, sizeof(SourceCountData), &CountData, NULL);
            Filesystem_Close(&f);
        }
    }


    /*
    bool bExplicitAsmPath = false;
    if (String_IndexOfFirstPathSlash(AsmProgram, NULL))
    {
        StringLocal(CompilerPathCopy, MAX_PATH_LENGTH);
        String_Copy(&CompilerPathCopy, AsmProgram);

        #if PLATFORM_WINDOWS
        if (!String_EndsWith(AsmProgram, S(".exe"), false))
        {
            String_Copy(&CompilerPathCopy, AsmProgram);
            String_Append(&CompilerPathCopy, S(".exe"));
        }
        #endif

        if (Filesystem_DoesFileExist(CompilerPathCopy))
        {
            bExplicitAsmPath = true;
            String_Copy(&AsmCompilerPath, CompilerPathCopy);
        }
        else
        {
            LOG_ERROR("Assembler program \"%S\" does not exist", CompilerPathCopy);

            Receipt.ExitCode = 1;
            return Receipt;
        }
    }
    */

    // does the asm program exist on the user's machine
    if (!String_IsValid(AsmCompilerPath) && CountData.NumAsmSources > 0)
    {
        /*
        bool bCompilerProgramFound = Platform_FindProgram_Ex(AsmProgram, &AsmCompilerPath);

        if (!bCompilerProgramFound && bNoAsmCompilerProgramExplicityGiven)
        {
            const String AsmPrograms_Default[2] =
            {
                S("nasm"),
                S("yasm"),
            };

            const String AsmPrograms_MSVC[4] =
            {
                S("ml64"),
                S("ml"),
                S("nasm"),
                S("yasm"),
            };

            const String* AsmPrograms = AsmPrograms_Default;
            u32 Num = SArray_Capacity(AsmPrograms_Default);

            if (String_IsEqual(CompilerProgram, S("cl"), false) ||
                String_IsEqual(CompilerProgram, S("msvc"), false))
            {
                AsmPrograms = AsmPrograms_MSVC;
                Num = SArray_Capacity(AsmPrograms_MSVC);
            }

            for (u8 i = 0; i < Num; i++)
            {
                const bool bFound = Platform_FindProgram_Ex(AsmPrograms[i], &AsmCompilerPath);
                if (bFound)
                {
                    AsmProgram = AsmPrograms[i];
                    bCompilerProgramFound = true;
                    break;
                }
            }
        }
        */

        // if (!bCompilerProgramFound)
        {
            // if (bNoAsmCompilerProgramExplicityGiven)
            {
                #if PLATFORM_WINDOWS
                LOG_ERROR(
                    "You don't seem to have an assember installed on your machine.\n"
                    "        Install either \"nasm\", \"yasm\" or \"ml (MSVC)\" and add to the path environment\n"
                    "        before using RiftBuild, as we require a working assembler program to function properly. Aborting build...\n");
                #else
                LOG_ERROR(
                    "You don't seem to have an assmebler installed on your machine."
                    " Install either \"nasm\" or \"yasm\" and add to the PATH environment"
                    " before using RiftBuild, as we require a working assembler program to function properly. Aborting build...\n");

                #endif

                LogPathEnvVarTutorialSteps();
                    
                Receipt.ExitCode = 1;
                return Receipt;
            }

            /*
            if (String_IsEqual(AsmProgram, S("ml"), false) ||
                String_IsEqual(AsmProgram, S("ml64"), false))
            {
                #if PLATFORM_WINDOWS
                LOG_ERROR("Assembler program \"%S\" does not exist. Aborting build...", AsmProgram);
                
                LOG("\n    Make sure that you have the Visual Studio build tools installed and "
                    "\n    that you run riftbuild from a different terminal application named"
                    "\n    \"x64 (or x86) Native Tools Command Prompt for VS\".");

                LOG("\n    This can be found through Windows Search.");
                #else
                LOG_ERROR("Assembler program \"%S\" does not exist on non-Windows platforms. Use a different assembler. Aborting build...", AsmProgram);
                #endif
            }
            else
            {
                LOG_ERROR("Assembler program \"%S\" does not exist. Make sure that it is installed and added to the path environment.\n"
                          "        Alternatively, you can specify the full path to the assembler executable instead. Aborting build...\n", AsmProgram);

                LogPathEnvVarTutorialSteps();
            }

            Receipt.ExitCode = 1;
            return Receipt;
            */
        }
        
        /*
        // TODO: move to parse.c
        {
            LinearAllocator Scratch = *Arena;
            StringArray AssemblersArray = String_ParseIntoArray(&Scratch, AssertAssemblers, ' ', 0, 128);
            if (AssemblersArray.Num > 0)
            {
                bool bAnyAssemblerMatch = false;
                for each_str (str, AssemblersArray)
                {
                    String Trimmed = String_EatSpaces(*str);

                    // todo: asm path
                    if (String_IsEqual(Trimmed, AsmProgram, false))
                    {
                        bAnyAssemblerMatch = true;
                        break;
                    }
                }

                StringLocal(AssemblersLogString, 128);
                {
                    u8 i = 0;
                    for each_str_i (i, a, AssemblersArray)
                    {
                        String_Append(&AssemblersLogString, *a);
                        if (AssemblersArray.Num > 1 && i != AssemblersArray.Num-1)
                        {
                            if (i == AssemblersArray.Num-2)
                            {
                                String_Append(&AssemblersLogString, S(" and "));
                            }
                            else
                            {
                                String_AppendChar (&AssemblersLogString, ',');
                                String_AppendSpace(&AssemblersLogString);
                            }
                        }
                    }
                }

                if (!bAnyAssemblerMatch)
                {
                    #ifndef HOOD
                    LOG_INLINE_ERROR("[ASSERTION FAILURE] %S can only be compiled with %S. First assembler found was \"%S\". Aborting build...\n\n", BuildFileName, AssemblersLogString, AsmProgram);
                    LOG("    This can be fixed by explicity providing the Assembler name inside of %S", BuildFileName);

                    LOG("    For example:");
                    {
                        u8 i = 0;
                        for each_str_i (i, a, AssemblersArray)
                        {
                            if (i > 0)
                            {
                                LOG("      or Assembler %S", *a);
                            }
                            else
                            {
                                LOG("         Assembler %S", *a);
                            }
                        }
                    }

                    #else
                    LOG_ERROR("yo dis assembler program \"%S\" cant be used cuh", AsmProgram);
                    #endif

                    // if (LogCustomErrorMessage(VariablesDB, S("Assembler"), AsmProgram, true))
                    // {
                    //     LOG_LINE_BREAK();
                    // }

                    Receipt.ExitCode = 1;
                    return Receipt;
                }
            }
        }
        */
    }

    // automatically switch to a c++ compiler if we have c++ source code files
    // TODO
    /*
    if (bNoCompilerProgramExplicityGiven)
    {
        const String CppCompilers[3] =
        {
            S("clang++"),
            S("g++"),
            S("cl")
        };

        String CompilerToUse = CompilerProgram;
        StringLocal(NewCompilerPath, MAX_PATH_LENGTH);
        bool bFoundAny = false;
        for (u8 i = 0; i < SArray_Capacity(CppCompilers); i++)
        {
            const bool bFound = Platform_FindProgram_Ex(CppCompilers[i], &NewCompilerPath);
            if (bFound)
            {
                bFoundAny = true;
                CompilerToUse = CppCompilers[i];
                break;
            }
        }

        if (bFoundAny)
        {
            bool bHasCppFiles = CountData.bHasCppFiles;

            if (bHasCppFiles)
            {
                CompilerProgram = CompilerToUse;
                String_Copy(&CompilerPath, NewCompilerPath);
            }
        }
    }
    */

    #if PLATFORM_WINDOWS
    String RCProgramFlags = String_Null();
    #endif

    // resolve linker and archiver paths
    {
        u32 LastSlashIndex = 0;
        xx String_IndexOfLastPathSlash(CompilerPath, &LastSlashIndex);

        const String CompilerBasePath = StrSlice(CompilerPath.Data, LastSlashIndex);
        const String CompilerExe = StrShiftF(CompilerPath, LastSlashIndex+1);

        if (String_Contains(CompilerExe, S("clang"), false))
        {
            // String_Copy(&LinkerPath, CompilerPath);

            // #if PLATFORM_WINDOWS
            // String_BuildPath(&ArchiverPath, CompilerBasePath, S("llvm-ar"));
            // #else
            // xx Platform_FindProgram_Ex(S("ar"), &ArchiverPath);
            // #endif

            String_BuildPath(&DumpBinPath, CompilerBasePath, S("llvm-objdump"));
            String_BuildPath(&RCCompilerPath, CompilerBasePath, S("llvm-rc"));
            String_BuildPath(&MTCompilerPath, CompilerBasePath, S("llvm-mt"));

            #if PLATFORM_WINDOWS
            // String_Append(&ArchiverPath, S(".exe"));
            String_Append(&DumpBinPath, S(".exe"));
            String_Append(&RCCompilerPath, S(".exe"));
            String_Append(&MTCompilerPath, S(".exe"));
            #endif
        }
        else if (String_Contains(CompilerExe, S("gcc"), false) ||
                 String_Contains(CompilerExe, S("g++"), false))
        {
            // String_Copy(&LinkerPath, CompilerPath);

            // #if PLATFORM_WINDOWS
            // String_BuildPath(&ArchiverPath, CompilerBasePath, S("gcc-ar"));
            // #else
            // xx Platform_FindProgram_Ex(S("ar"), &ArchiverPath);
            // #endif

            String_BuildPath(&DumpBinPath, CompilerBasePath, S("objdump"));
            String_BuildPath(&RCCompilerPath, CompilerBasePath, S("windres"));

            #if PLATFORM_WINDOWS
            // String_Append(&ArchiverPath, S(".exe"));
            String_Append(&DumpBinPath, S(".exe"));
            String_Append(&RCCompilerPath, S(".exe"));
            #endif
        }
        #if PLATFORM_WINDOWS
        else if (String_Contains(CompilerExe, S("cl.exe"), false))
        {
            RCProgramFlags = S("/nologo");

            // String_BuildPath(&LinkerPath, CompilerBasePath, S("link.exe"));
            // String_BuildPath(&ArchiverPath, CompilerBasePath, S("lib.exe"));
            String_BuildPath(&DumpBinPath, CompilerBasePath, S("dumpbin.exe"));

            if (bWasVCVarsBatchExecuted)
            {
                xx Platform_FindProgram_Ex(S("rc"), &RCCompilerPath);
                xx Platform_FindProgram_Ex(S("mt"), &MTCompilerPath);
            }
            else
            {
                String_BuildPath(&RCCompilerPath, WindowsSDKBinaryPath, S("\\"CPU_ARCHITECTURE_STRING"\\rc.exe"));
                String_BuildPath(&MTCompilerPath, WindowsSDKBinaryPath, S("\\"CPU_ARCHITECTURE_STRING"\\mt.exe"));
            }
        }
        #endif
        else
        {
            // String_Copy(&LinkerPath, CompilerPath);

            #if PLATFORM_WINDOWS
            RCProgramFlags = S("/nologo");

            if (bWasVCVarsBatchExecuted)
            {
                xx Platform_FindProgram_Ex(S("rc"), &RCCompilerPath);
                xx Platform_FindProgram_Ex(S("mt"), &MTCompilerPath);
            }
            else
            {
                if (WindowsSDKBinaryPath.Length > 0)
                {
                    String_BuildPath(&RCCompilerPath, WindowsSDKBinaryPath, S("\\"CPU_ARCHITECTURE_STRING"\\rc.exe"));
                    String_BuildPath(&MTCompilerPath, WindowsSDKBinaryPath, S("\\"CPU_ARCHITECTURE_STRING"\\mt.exe"));
                }
            }
            #else
            // xx Platform_FindProgram_Ex(S("ar"), &ArchiverPath);
            String_Copy(&DumpBinPath, S("objdump"));
            #endif
        }
    }

    if (!Filesystem_DoesFileExist(RCCompilerPath))
    {
        RCCompilerPath.Length = 0;
    }

    const bool bHasRcProgram = RCCompilerPath.Length > 0;
    String RCProgram = String_Null();

    if (bHasRcProgram)
    {
        u32 Index = 0;
        xx String_IndexOfLastPathSlash(RCCompilerPath, &Index);
        RCProgram = StrShiftF(RCCompilerPath, Index+1);
        if (String_IndexOfLastChar(RCProgram, '.', &Index))
        {
            RCProgram = StrSlice(RCProgram.Data, Index);
        }
    }

    if (!bExportingSomething)
    {
        // force a rebuild if the .build file has been modified
        if (!bIsRebuild && !bIsClean && bFoundBuildFile && AssemblyType != AssemblyType_CustomCompilerObject)
        {
            // build the full assembly path
            StringLocal(AssemblyPath, MAX_PATH_LENGTH);

            bool bAnyExist = false;

            if (AssemblyType == AssemblyType_PCH)
            {
                const String PCHExts[11] =
                {
                    S(".pch"),
                    S(".h.pch"),
                    S(".h.gch"),
                    S(".hpp.pch"),
                    S(".hpp.gch"),
                    S(".h++.pch"),
                    S(".h++.gch"),
                    S(".hh.pch"),
                    S(".hh.gch"),
                    S(".hxx.pch"),
                    S(".hxx.gch"),
                };

                for (u32 i = 0; i < SArray_Capacity(PCHExts); i++)
                {
                    StringLocal(Name, 256);
                    String_Copy(&Name, AssemblyName);
                    String_Append(&Name, PCHExts[i]);

                    String_Empty(&AssemblyPath);
                    String_BuildPath(&AssemblyPath, WorkingPath, BuildDirectory, Name);

                    if (Filesystem_DoesFileExist(AssemblyPath))
                    {
                        bAnyExist = true;
                        break;
                    }
                }
            }
            else
            {
                String_BuildPath(&AssemblyPath, WorkingPath, BuildDirectory, AssemblyNameWithExt);

                if (Filesystem_DoesFileExist(AssemblyPath))
                {
                    bAnyExist = true;
                }
            }

            if (!bAnyExist)
            {
                /*
                bIsRebuild = true;
                
                StringLocal(Temp, MAX_PATH_LENGTH);
                String_BuildPath(&Temp, WorkingPath, BuildDirectory);
                if (Filesystem_DoesDirectoryExist(Temp))
                {
                    // only say this if we have a build directory but no assembly file
                    LOG("Assembly file \"%S\" does not exist. Forcing rebuild...\n", AssemblyPath);
                }
                */
            }

            if (!bIsRebuild)
            {
                u64 AssemblyFileTime = Filesystem_GetLastWriteTime(AssemblyPath);
                u64 BuildFileTime = Filesystem_GetLastWriteTimeH(BuildFileHandle);

                if (BuildFileTime >= AssemblyFileTime && AssemblyFileTime > 0)
                {
                    bIsRebuild = true;

                    #ifndef HOOD
                    LOG("Assembly file older than build file. Forcing rebuild...");
                    #else
                    LOG("dawwwg, da assembly file is older than da buil fil. gon force a rebuild...");
                    #endif

                    LOG_LINE_BREAK();
                }
            }
        }

        // force a rebuild if any of the included files have been modified
        if (!bIsRebuild && !bIsClean && bFoundBuildFile && Array_Num(IncludeFiles) > 0 && AssemblyType != AssemblyType_CustomCompilerObject)
        {
            StringLocal(AssemblyPath, MAX_PATH_LENGTH);
            String_BuildPath(&AssemblyPath, WorkingPath, BuildDirectory, AssemblyNameWithExt);

            u64 AssemblyFileTime = Filesystem_GetLastWriteTime(AssemblyPath);

            if (AssemblyFileTime > 0)
            {
                for each (FileHandle, Include, IncludeFiles)
                {
                    u64 IncludeFileTime = Filesystem_GetLastWriteTimeH(Include);

                    if (IncludeFileTime >= AssemblyFileTime)
                    {
                        bIsRebuild = true;

                        StringLocal(Path, MAX_PATH_LENGTH);
                        xx Filesystem_GetFilePath(Include, &Path);

                        #ifndef HOOD
                        LOG("Build variables file \"%S\" has been modified since last build. Forcing rebuild...", Path);
                        #else
                        LOG("dawwwg, dis build vars file \"%S\" has been modified since last build. gon force a rebuild...", Path);
                        #endif

                        LOG_LINE_BREAK();

                        break;
                    }
                }
            }
        }

        for each (FileHandle, File, IncludeFiles)
        {
            Filesystem_Close(&File);
        }

        // force a rebuild if either the intermediate directory is missing
        if (!bIsRebuild && !bIsClean)
        {
            // if (!bDidBuildDirectoryExist || !bDidIntermediateDirectoryExist)
            if (!bDidIntermediateDirectoryExist)
            {
                bIsRebuild = true;
            }
        }

        // force a rebuild if the cmd line given to this program was different than the previous run
        /// TODO: fix this, idk what to do
        if (!bIsRebuild && !bIsClean)// && !String_IsValid(CameFromBuildFile))
        {
            StringLocal(OutputDebugFile, MAX_PATH_LENGTH);
            StringLocal(GenFileName, 256);
            String_Append(&GenFileName, BuildFileName);
            String_Append(&GenFileName, S(".generated"));
            String_ToLower(&GenFileName);
            String_BuildPath(&OutputDebugFile, IntermediateBaseDirectory, GenFileName);

            bool bFileExists = Filesystem_DoesFileExist(OutputDebugFile);
            if (bFileExists)
            {
                FileHandle h = {0};
                if (Filesystem_Open(OutputDebugFile, FileMode_Read, &h))
                {
                    StringLocal(SavedCmdLine, 2048);
                    if (Filesystem_ReadLine(h, &SavedCmdLine))
                    {
                        if (!String_IsEqual(SavedCmdLine, RiftCmdLine, false))
                        {
                            LOG("Different command line given. Forcing rebuild...");
                            LOG("    Previous: %S", SavedCmdLine.Length == 0 ? S("<empty>") : SavedCmdLine);
                            LOG("    Current:  %S", RiftCmdLine.Length == 0 ? S("<empty>") : RiftCmdLine);
                            LOG_LINE_BREAK();

                            bIsRebuild = true;
                        }
                    }

                    Filesystem_Close(&h);
                }
            }
        }

        // force a rebuild if any of the .h files have been modified after a build
        // TODO: rewrite
        if (!bIsRebuild && !bIsClean)
        {
            StringLocal(AssemblyPath, MAX_PATH_LENGTH);
            String_BuildPath(&AssemblyPath, WorkingPath, BuildDirectory, AssemblyNameWithExt);

            usize AssemblyFileTime = Filesystem_GetLastWriteTime(AssemblyPath);

            if (AssemblyFileTime > 0)
            {
                struct HeaderIterData
                {
                    u64 AssemblyFileTime;
                    bool* bShouldRebuild;
                };

                struct HeaderIterData Data = { AssemblyFileTime, &bIsRebuild };
                Filesystem_IterateDirectory_Ex(SourceDir, &HeaderFileRebuildCheckDirectoryIterator, true, &Data);
            }
        }

        if (bIsClean)
        {
            bIsRebuild = false;
        }

        if (bIsClean || bIsRebuild)
        {
            bool bCleanedSomething = false;

            const String Exts[42] =
            {
                String_Null(),
                S(".o"),
                S(".obj"),
                S(".lib"),
                S(".a"),
                S(".so"),
                S(".dylib"),
                S(".dll"),
                S(".exe"),
                S(".app"),
                S(".out"),
                S(".bin"),
                S(".elf"),
                S(".pdb"),
                S(".ilk"),
                S(".res"),
                S(".rc"),
                S(".manifest"),
                S(".exp"),
                S(".def"),
                S(".map"),
                S(".suo"),
                S(".sdf"),
                S(".idb"),
                S(".ipch"),
                S(".pch"),
                S(".h.pch"),
                S(".h.gch"),
                S(".hpp.pch"),
                S(".hpp.gch"),
                S(".h++.pch"),
                S(".h++.gch"),
                S(".hxx.pch"),
                S(".hxx.gch"),
                S(".hh.pch"),
                S(".hh.gch"),
                S(".log"),
                S(".tmp"),
                S(".build.generated"),
                S(".build.directory_state"),
                S(".build_version.rc"),
                S(".build_version.res")
            };

            #if PLATFORM_WINDOWS
            String Wildcard = S(".*");
            const String WildcardS = S("S.*");
            #else
            String Wildcard = String_Null();
            const String WildcardS = S("S");
            #endif

            // Delete all [Assembly]*.* files
            if (AssemblyType != AssemblyType_CustomCompilerObject)
            {
                if (bDidBuildDirectoryExist)
                {
                    #ifndef HOOD
                    LOG("Cleaning %S%S%S", BuildBaseDirectory, AssemblyName, Wildcard);
                    #else
                    LOG("cleaning dis fuckin' shit %S%S%S", BuildBaseDirectory, AssemblyName, Wildcard);
                    #endif

                    if (bBuildDirSameAsSource)
                    {
                        for each_static (String, e, Exts)
                        {
                            StringLocal(AssemblyWildcard, MAX_PATH_LENGTH);
                            String_Append(&AssemblyWildcard, AssemblyName);
                            String_Append(&AssemblyWildcard, e);
                            xx Filesystem_DeleteFiles(BuildBaseDirectory, AssemblyWildcard, true);
                        }
                    }
                    else
                    {
                        StringLocal(AssemblyWildcard, MAX_PATH_LENGTH);
                        String_Append(&AssemblyWildcard, AssemblyName);
                        String_Append(&AssemblyWildcard, Wildcard);
                        xx Filesystem_DeleteFiles(BuildBaseDirectory, AssemblyWildcard, true);
                        String_Empty(&AssemblyWildcard);
                        String_Append(&AssemblyWildcard, AssemblyName);
                        String_Append(&AssemblyWildcard, WildcardS);
                        xx Filesystem_DeleteFiles(BuildBaseDirectory, AssemblyWildcard, true);
                    }

                    #if PLATFORM_APPLE
                    if (bBundleApp && bIsAssemblyExe)
                    {
                        StringLocal(AppBundleName, 256);
                        String_Append(&AppBundleName, TitleName);
                        String_Append(&AppBundleName, S(".app"));
                        StringLocal(AppBundlePath, MAX_PATH_LENGTH);
                        String_BuildPath(&AppBundlePath, WorkingPath, BuildDirectory, AppBundleName);
                        LOG("Cleaning %S", AppBundlePath);
                        xx Filesystem_DeleteDirectory(AppBundlePath);

                        String_Empty(&AppBundleName);
                        String_Append(&AppBundleName, AssemblyName);
                        String_Append(&AppBundleName, S(".app"));
                        String_Empty(&AppBundlePath);
                        String_BuildPath(&AppBundlePath, WorkingPath, BuildDirectory, AppBundleName);
                        LOG("Cleaning %S", AppBundlePath);
                        xx Filesystem_DeleteDirectory(AppBundlePath);
                    }
                    #endif

                    bCleanedSomething = true;
                }
            }

            // Delete intermediate directory based on given source directory
            if (bDidIntermediateDirectoryExist)
            {
                Wildcard = S("*");

                #ifndef HOOD
                LOG("Cleaning %S%S", IntermediateBaseDirectory, Wildcard);
                #else
                LOG("cleaning dis stoopid shit %S%S", IntermediateBaseDirectory, Wildcard);
                #endif

                if (bIntermediateDirSameAsSource)
                {
                    for each_static (String, e, Exts)
                    {
                        if (e.Length > 0)
                        {
                            StringLocal(AssemblyWildcard, MAX_PATH_LENGTH);
                            String_Append(&AssemblyWildcard, S("*"));
                            String_Append(&AssemblyWildcard, e);
                            xx Filesystem_DeleteFiles(IntermediateBaseDirectory, AssemblyWildcard, true);
                            xx Filesystem_DeleteFiles(IntermediateBaseDirectory, e, true);
                        }
                    }
                }
                else
                {
                    // TODO: should we even do this???
                    xx Filesystem_DeleteFiles(IntermediateBaseDirectory, Wildcard, true);
                }

                bCleanedSomething = true;
            }

            if (bCleanedSomething)
            {
                LOG_LINE_BREAK();
            }

            if (!bIsRebuild)
            {
                Receipt.ExitCode = 0;
                return Receipt;
            }
        }
    }

    if (bFoundBuildFile)
    {
        StringLocal(Name, 256);
        String_Append(&Name, BuildFileName);
        String_Append(&Name, S(".generated"));
        StringLocal(OutputDebugFile, MAX_PATH_LENGTH);
        String_BuildPath(&OutputDebugFile, IntermediateBaseDirectory, Name);

        FileHandle f = FileHandle_Null();
        if (Filesystem_Open(OutputDebugFile, FileMode_Write, &f))
        {
            // write the cmd line of this program to a file in the intermediate directory for comparison between subsequent runs
            Filesystem_Write(f, RiftCmdLine.Length, RiftCmdLine.Data, NULL);

            StringLocal(Spaces, 64);
            Spaces.Length = 64;
            String_Fill(&Spaces, ' ');

            StringLocal(Buffer, Kibibytes(32));
            String_AppendChar(&Buffer, '\n');

            u32 LongestName = 4;
            for each (FileVariable, v, VariablesDB)
            {
                u32 Length = v.Name.Length;
                if (Length > LongestName)
                {
                    LongestName = Length;
                }
            }

            for each (FileVariable, v, VariablesDB)
            {
                Spaces.Length = (LongestName - v.Name.Length) + 1; // +1 for extra space

                String_Append(&Buffer, v.Name);
                String_Append(&Buffer, Spaces);
                String_Append(&Buffer, v.Value);
                String_AppendChar(&Buffer, '\n');
            }

            Filesystem_WriteLine(f, Buffer, NULL);
            Filesystem_Close(&f);
        }
    }

    if (Array_Num(Messages) > 0)
    {
        for each (String, m, Messages)
        {
            LOG("%S", m);
        }

        LOG_LINE_BREAK();
    }

    bool bExplicitLinker = DoesCmdOptionExist(CmdOptionsDB, S("Linker.Explicit"));
    bool bCanLink = AssemblyType != AssemblyType_CustomCompilerObject ||
                    (AssemblyType == AssemblyType_CustomCompilerObject && bExplicitLinker);

    #if !NO_PRINT_BUILD_CONFIG
    if (!bExportingSomething)
    {
        if (bFoundBuildFile)
        {
            LOG("Build Configuration:");
            
            if (bCanLink)
            {
                u32 WhitespaceIndex = 0;
                bool bHasSpace = String_IndexOfFirstWhitespace(Extension_Og, &WhitespaceIndex);

                if (bIsAssemblyExe || !bHasSpace)
                {
                    LOG("    Assembly:             %S", AssemblyNameWithExt);
                }
                else
                {
                    String NextExt = StrShiftF(Extension_Og, WhitespaceIndex+1);
                    LOG("    Assembly:             %S and %S%S", AssemblyNameWithExt, AssemblyName, NextExt);
                }
            }

            const String AssemblyTypeStringTable[7] =
            {
                S("None"),
                S("Executable"),
                S("Library"),
                S("Static Library"),
                S("Shared Library"),
                S("Pre Compiled Header"),
                S("Compiler Object"),
            };

            StringLocal(ExtInfo, 32);
            String_Format(&ExtInfo, S(" (%S)"), Extension_Og);

            LOG("    Type:                 %S%S", AssemblyTypeStringTable[AssemblyType], Extension_Og.Length == 0 ? String_Null() : ExtInfo);

            if (AssemblyType != AssemblyType_CustomCompilerObject)
            {
                LOG("    Version:              %S", Version);
            }
            
            LOG("    Compiler:             %S -> \"%S\"", CompilerProgram, CompilerPath);

            if (bCanLink)
            {
                LOG("    Linker:               %S -> \"%S\"", LinkerProgram, LinkerPath);
            }

            if (AssemblyType == AssemblyType_Library ||
                AssemblyType == AssemblyType_StaticLibrary)
            {
                LOG("    Archiver:             %S -> \"%S\"", ArchiverProgram, ArchiverPath);
            }

            if (CountData.NumAsmSources > 0)
            {
                LOG("    Assembler:            %S -> \"%S\"", AsmProgram, AsmCompilerPath);
            }

            #if PLATFORM_WINDOWS
            if (RCCompilerPath.Length > 0 && (Icon.Length > 0 || CountData.NumRcSources > 0))
            {
                LOG("    Resource Compiler:    %S -> \"%S\"", RCProgram, RCCompilerPath);
            }
            #endif

            LOG_LINE_BREAK();
        }
    }
    #else
    xx bExplicitAsmPath;
    #endif

    StringLocal(IconFilePath, MAX_PATH_LENGTH);
    StringLocal(IconResFilePath, MAX_PATH_LENGTH);
    StringLocal(VersionResFilePath, MAX_PATH_LENGTH);

    StringLocal(ExpandedIncludeFlags, 4096);
    StringLocal(ExpandedLibraries, 2048);
    StringLocal(ExpandedLibraryDirectories, 4096);
    StringLocal(ExpandedDefineFlags, 2048);
    StringLocal(ExpandedUnDefineFlags, 1024);
    StringLocal(ExpandedLinkerDefineFlags, 1024);
    StringLocal(ExpandedAssemblerIncludeFlags, 4096);
    StringLocal(ExpandedAssemblerDefineFlags, 1024);
    StringLocal(ExpandedFrameworks, 2048);

    StringLocal(FlagPrefix, 4);
    String_Append(&FlagPrefix, CompilerFlagPrefixSymbol);
    String_Append(&FlagPrefix, S("I"));

    bool bWrapWithQuotes = !bExportingSomething;

    String IncludeFlags = GetVariableValue(VariablesDB, S("Includes"));
    String_ConvertSlashToPlatformSlash(&IncludeFlags);

    ExpandPathFlags(*Arena, &ExpandedIncludeFlags, IncludeFlags, FlagPrefix, bWrapWithQuotes);

    String Libraries = GetVariableValue(VariablesDB, S("Libraries"));

    FlagPrefix.Data[1] = 'l';
    if (String_IsEqual(CompilerProgram, S("cl"), false) ||
        String_IsEqual(CompilerProgram, S("msvc"), false)) // todo: something better
    {
        SuffixVariables(&ExpandedLibraries, Libraries, S(".lib"));
    }
    else
    {
        PrefixVariables(&ExpandedLibraries, Libraries, FlagPrefix, false);
    }

    String LibraryDirectories = GetVariableValue(VariablesDB, S("Library.Paths"));
    String_ConvertSlashToPlatformSlash(&LibraryDirectories);

    FlagPrefix.Data[1] = 'L';
    if (String_IsEqual(CompilerProgram, S("cl"), false) ||
        String_IsEqual(CompilerProgram, S("msvc"), false)) // todo: something better
    {
        ExpandPathFlags(*Arena, &ExpandedLibraryDirectories, LibraryDirectories, S("/LIBPATH:"), bWrapWithQuotes);
    }
    else
    {
        ExpandPathFlags(*Arena, &ExpandedLibraryDirectories, LibraryDirectories, FlagPrefix, bWrapWithQuotes);
    }

    String Defines = GetVariableValue(VariablesDB, S("Defines"));

    FlagPrefix.Data[1] = 'D';
    ExpandDefineFlags(&ExpandedDefineFlags, Defines, FlagPrefix, bExportingSomething);
    ExpandDefineFlags(&ExpandedLinkerDefineFlags, LinkerDefines, FlagPrefix, bExportingSomething);

    String UnDefines = GetVariableValue(VariablesDB, S("UnDefines"));

    FlagPrefix.Data[1] = 'U';
    ExpandDefineFlags(&ExpandedUnDefineFlags, UnDefines, FlagPrefix, bExportingSomething);
    
    // assembler stuff
    FlagPrefix.Data[0] = '-'; // todo: masm uses /
    FlagPrefix.Data[1] = 'I';
    ExpandPathFlags(*Arena, &ExpandedAssemblerIncludeFlags, AssemblerIncludes, FlagPrefix, bWrapWithQuotes);

    FlagPrefix.Data[1] = 'D';
    ExpandDefineFlags(&ExpandedAssemblerDefineFlags, AssemblerDefines, FlagPrefix, bExportingSomething);

    #if PLATFORM_APPLE
    String Frameworks = GetVariableValue(VariablesDB, S("Frameworks"));
    PrefixVariables(&ExpandedFrameworks, Frameworks, S("-framework "), false);
    #endif

    // TODO: move the rest down here
    const String LinkerFlags                = GetVariableValue(VariablesDB, S("Linker.Flags"));

    #if !NO_PRINT_BUILD_CONFIG
    if (!bExportingSomething)
    {
        LogNameValuePair(*Arena, S("    Compiler  Flags:      "), CompilerFlags,                 !bNoWordWrapLogging);
        LogNameValuePair(*Arena, S("    Include   Flags:      "), ExpandedIncludeFlags,          !bNoWordWrapLogging);
        LogNameValuePair(*Arena, S("    Linker    Flags:      "), LinkerFlags,                   !bNoWordWrapLogging);
        LogNameValuePair(*Arena, S("    Define    Flags:      "), ExpandedDefineFlags,           !bNoWordWrapLogging);
        LogNameValuePair(*Arena, S("    UnDefine  Flags:      "), ExpandedUnDefineFlags,         !bNoWordWrapLogging);
        LogNameValuePair(*Arena, S("    Linker  Defines:      "), ExpandedLinkerDefineFlags,     !bNoWordWrapLogging);
        LogNameValuePair(*Arena, S("    Library   Flags:      "), ExpandedLibraries,             !bNoWordWrapLogging);
        LogNameValuePair(*Arena, S("    Library   Paths:      "), ExpandedLibraryDirectories,    !bNoWordWrapLogging);
        LogNameValuePair(*Arena, S("    Assembler Flags:      "), AssemblerFlags,                !bNoWordWrapLogging);
        LogNameValuePair(*Arena, S("    Assembler Includes:   "), ExpandedAssemblerIncludeFlags, !bNoWordWrapLogging);
        LogNameValuePair(*Arena, S("    Assembler Defines:    "), ExpandedAssemblerDefineFlags,  !bNoWordWrapLogging);
    }
    #endif

    Clock IconClock = {0};
    Clock ResourceCompileClock = {0};
    Clock BundleCompileClock = {0};

    // TODO: should leave one core free? so as to not freeze/lag the entire computer?
    u8 MaxCompilersAtOnce = (u8)MaxLogicalCores; // bound by max logical processors on the user's machine
    //LOG_INFO("Max logical cores: %u", MaxLogicalCores);

    // clamp to the min amount of source files vs cores
    u32 MinResult = Min(NumSources, (u32)MaxCompilersAtOnce);
    MaxCompilersAtOnce = (u8)Min(MinResult, (u32)UINT8_MAX);

    if (String_IsValid(MaxConcurrentCompilations))
    {
        u8 Num = 0;
        xx String_ToU8(MaxConcurrentCompilations, &Num);
        MaxCompilersAtOnce = Min(Num, (u8)MaxLogicalCores);
    }

    if (bSingleThread)
    {
        MaxCompilersAtOnce = 1;
    }

    //LOG_INFO("Max compilers: %u", MaxCompilersAtOnce);

    if (!String_IsEqual(BuildDirectory, S("."), false))
    {
        StringLocal(FullBuildDirectory, MAX_PATH_LENGTH);
        String_BuildPath(&FullBuildDirectory, WorkingPath, BuildDirectory);
        if (!Filesystem_DoesDirectoryExist(FullBuildDirectory))
        {
            if (!Filesystem_OpenDirectory(FullBuildDirectory))
            {
                Receipt.ExitCode = 1;
                return Receipt;
            }
        }
    }

    StringLocal(IntSrcDir, MAX_PATH_LENGTH);
    String_BuildPath(&IntSrcDir, IntermediateBaseDirectory, SourceDirectory);
    if (!Filesystem_DoesDirectoryExist(IntermediateBaseDirectory))
    {
        if (!Filesystem_OpenDirectory(IntermediateBaseDirectory))
        {
            Receipt.ExitCode = 1;
            return Receipt;
        }
    }

    BuildParams p = {0};
    p.Arena                         = Arena;
    p.CompilerProgram               = CompilerProgram;
    p.CompilerPath                  = CompilerPath;
    p.CompilerOutputFlag            = CompilerOutputFlag;
    p.CompilerCompileFlag           = CompilerCompileFlag;
    p.CompilerObjectExt             = CompilerObjectExt;
    p.CompilerObjectDirectory       = CompilerObjectDirectory;
    p.LinkerPath                    = LinkerPath;
    p.ArchiverPath                  = ArchiverPath;
    p.DumpBinPath                   = DumpBinPath;
    p.AsmProgram                    = AsmProgram;
    p.AsmPath                       = AsmCompilerPath;
    p.BuildFileName                 = BuildFileName;
    #if PLATFORM_WINDOWS
    p.RCProgram                     = RCProgram;
    p.RCProgramPath                 = RCCompilerPath;
    p.RCProgramFlags                = RCProgramFlags;
    p.bHasRCProgram                 = bHasRcProgram;
    #endif
    p.Assembly                      = AssemblyName;
    p.AssemblyWithExt               = AssemblyNameWithExt;
    p.AssemblyPrefix                = AssemblyPrefix;
    p.AssemblyPostfix               = AssemblyPostfix;
    p.Extension                     = Extension;
    p.Extension_Og                  = Extension_Og;
    p.Type                          = AssemblyType;
    p.Processes                     = &Processes;
    p.RootDirectory                 = WorkingPath;
    p.SourceDirectory               = SourceDirectory;
    p.BuildDirectory                = BuildDirectory;
    p.IntermediateDirectory         = IntermediateDirectory;
    p.IntermediateBaseDirectory     = IntermediateBaseDirectory;
    p.PCHPath                       = PCHPath;
    p.PCHHeaderPath                 = PCHHeaderPath;
    p.MaxCompilersAtOnce            = MaxCompilersAtOnce;
    p.bShouldWaitPerCompileProcess  = bSingleThread;
    p.CompilerFlags                 = CompilerFlags;
    p.bCompilerFlagsFirst           = bCompilerFlagsFirst;
    p.AssemblerFlags                = AssemblerFlags;
    p.AssemblerIncludes             = ExpandedAssemblerIncludeFlags;
    p.AssemblerDefines              = ExpandedAssemblerDefineFlags;
    p.LinkerFlags                   = LinkerFlags;
    p.IncludeFlags                  = ExpandedIncludeFlags;
    p.DefineFlags                   = ExpandedDefineFlags;
    p.UnDefineFlags                 = ExpandedUnDefineFlags;
    p.LinkerDefineFlags             = ExpandedLinkerDefineFlags;
    p.Libraries                     = ExpandedLibraries;
    p.LibraryDirectories            = ExpandedLibraryDirectories;
    p.LinkerEntryPoint              = LinkerEntryPoint;
    p.LinkerSubsystem               = LinkerSubsystem;
    p.LinkerStack                   = LinkerStack;
    p.Frameworks                    = ExpandedFrameworks;
    p.bIsAssemblyExe                = bIsAssemblyExe;
    p.bVerbose                      = bVerboseLog;
    p.TitleName                     = TitleName;
    p.InternalName                  = InternalName;
    p.CompanyName                   = CompanyName;
    p.Description                   = Description;
    p.Copyright                     = Copyright;
    p.Version                       = Version;
    p.SourceFiles                   = *CountData.FilteredFiles;
    p.NumSources                    = NumSources;
    p.bHasCppFiles                  = CountData.bHasCppFiles;
    p.bDumpObjFilesInOneDirectory   = bDumpObjFilesInOneDirectory;
    p.CameFromBuildFile             = CameFromBuildFile;
    p.IconFilePath                  = IconFilePath;
    p.bLinkerNoStd                  = bLinkerNoStd;
    p.bLinkerNoDefaultLibs          = bLinkerNoDefaultLibs;
    p.RPath                         = RPath;
    p.Timestamp                     = TimeStamp;
    #if PLATFORM_WINDOWS
    p.WindowsSDKIncludePath         = WindowsSDKIncludePath;
    p.WindowsSDKLibUmPath           = WindowsSDKLibUmPath;
    p.WindowsSDKLibUcrtPath         = WindowsSDKLibUcrtPath;
    if (CompilerVendor == Compiler_MSVC)
    {
        p.VisualStudioLibraryPath       = GetCmdOptionValue(CmdOptionsDB, S("Compiler.LibraryPath"));
        p.VisualStudioIncludePath       = GetCmdOptionValue(CmdOptionsDB, S("Compiler.IncludePath"));
    }
    #endif

    // @export feature
    for (u8 i = 0; i < Parameters.Num; i++)
    {
        const String Arg = Parameters.List[i];

        if (String_StartsWith(Arg, S("export:"), false))
        {
            u32 Colon = 0;
            if (String_IndexOfChar(Arg, ':', &Colon))
            {
                const String VarToList = StrShiftF(Arg, Colon+1);

                if (VarToList.Length == 0)
                {
                    LOG_ERROR("Failed to export. No export type was given after ':'");
                    LOG_INLINE_WARNING("\nUsage\n");
                    LOG("     export:compile_commands");
                    LOG("     export:icon.rc");
                    LOG("     export:plist,bat,sh");

                    Receipt.ExitCode = 1;
                    return Receipt;
                }

                if (!Export_FromArg(*Arena, &p, VarToList, VariablesDB))
                {
                    Receipt.ExitCode = 1;
                    return Receipt;
                }

                Receipt.ExitCode = 0;
                return Receipt;
            }
        }
    }

    // enforce copyright in all source files
    {
        const FileVariable CopyrightVar = GetVariable(VariablesDB, S("Copyright"));
        if (CopyrightVar.Value.Length)
        {
            LinearAllocator Scratch = *Arena;
            StringList ParamList = String_SplitIntoList(&Scratch, CopyrightVar.Params, ' ', false);
            String EnforceParam = StringList_Find(ParamList, S("enforce"), false, StringCompare_Equal, NULL);
            if (!String_IsValid(EnforceParam))
            {
                EnforceParam = StringList_Find(ParamList, S("enforce:"), false, StringCompare_StartsWith, NULL);
            }

            if (String_IsValid(EnforceParam))
            {
                u32 FromLine = 1;
                u32 ToLine = 1;

                u32 ColonIndex = 0;
                if (String_IndexOfChar(EnforceParam, ':', &ColonIndex))
                {
                    String SpecialData = StrShiftF(EnforceParam, ColonIndex+1);

                    u32 DashIndex = 0;
                    if (String_IndexOfChar(SpecialData, '-', &DashIndex))
                    {
                        String From = StrSlice(SpecialData.Data, DashIndex);
                        String To   = StrShiftF(SpecialData, DashIndex+1);

                        xx String_ToU32(From, &FromLine);
                        bool bHasTo = String_ToU32(To, &ToLine);

                        FromLine = Max(1, FromLine);
                        ToLine   = Max(1, ToLine);
                        if (!bHasTo) { ToLine = UINT32_MAX; } // end of file

                        if (FromLine > ToLine)
                        {
                            u32 Temp = FromLine;
                            FromLine = ToLine;
                            ToLine = Temp;
                        }
                    }
                    else
                    {
                        xx String_ToU32(SpecialData, &FromLine);
                        ToLine = FromLine;
                    }
                }

                CopyrightEnforceInfo AuxData = {0};
                AuxData.Content  = CopyrightVar.Value;
                AuxData.FromLine = FromLine;
                AuxData.ToLine   = ToLine;
                
                for each_string_in_list (p.SourceFiles)
                {
                    const String Ext = Filesystem_ExtractFileExtension(It.String, true);
                    if (IsCSource(Ext)    ||
                        IsCppSource(Ext)  ||
                        IsObjCSource(Ext) ||
                        IsAsmSource(Ext))
                    {
                        if (!EnforceCopyright(&p, &AuxData, It.String))
                        {
                            Receipt.ExitCode = 1;
                            return Receipt;
                        }
                    }
                }
            }
        }
    }
    
    // generate a license file in the same directory as the .build file
    {
        const FileVariable LicenseVar = GetVariable(VariablesDB, S("License"));
        if (LicenseVar.Value.Length)
        {
            LinearAllocator Scratch = *Arena;
            StringList ParamList = String_SplitIntoList(&Scratch, LicenseVar.Params, ' ', false);
            if (StringList_FindIndex(ParamList, S("generate"), false, StringCompare_Equal, NULL))
            {
                const String CustomLicensePath = GetVariableValue(VariablesDB, S("License.Path"));
                const String CustomLicenseFileName = GetVariableValue(VariablesDB, S("License.FileName"));

                String LicenseFileName = S("LICENSE");
                if (String_IsValid(CustomLicenseFileName))
                {
                    LicenseFileName = CustomLicenseFileName;
                }

                StringLocal(OutputPath, MAX_PATH_LENGTH);
                if (Filesystem_IsPathRelative(CustomLicensePath))
                {
                    String_BuildPath(&OutputPath, Filesystem_ExtractFilePath(BuildFilePathFull, false), CustomLicensePath, LicenseFileName);
                }
                else
                {
                    String_BuildPath(&OutputPath, CustomLicensePath, LicenseFileName);
                }

                // make sure the file does not already exist
                // force re-gen if we are rebuilding or cleaning
                if (!Filesystem_DoesFileExist(OutputPath) || bIsRebuild || bIsClean)
                {
                    bool bLicenseExported = false;

                    if (String_IsEqual(LicenseVar.Value, S("BSD"), false) ||
                        String_StartsWith(LicenseVar.Value, S("BSD-3"), false) ||
                        String_StartsWith(LicenseVar.Value, S("BSD 3"), false) ||
                        String_StartsWith(LicenseVar.Value, S("BSD3"), false))
                    {
                        bLicenseExported = Export_License(S("BSD3"), &p, OutputPath);
                    }
                    else if (String_StartsWith(LicenseVar.Value, S("BSD-2"), false) ||
                            String_StartsWith(LicenseVar.Value, S("BSD 2"), false) ||
                            String_StartsWith(LicenseVar.Value, S("BSD2"), false))
                    {
                        bLicenseExported = Export_License(S("BSD2"), &p, OutputPath);
                    }
                    else if (String_IsEqual(LicenseVar.Value, S("MIT"), false))
                    {
                        bLicenseExported = Export_License(S("MIT"), &p, OutputPath);
                    }
                    else if (String_IsEqual(LicenseVar.Value, S("Fuck You"), false) ||
                            String_IsEqual(LicenseVar.Value, S("FuckYou"), false))
                    {
                        bLicenseExported = Export_License(S("FuckYou"), &p, OutputPath);
                    }
                    else if (String_IsEqual(LicenseVar.Value, S("Unlicense"), false) ||
                            String_IsEqual(LicenseVar.Value, S("Un-license"), false))
                    {
                        bLicenseExported = Export_License(S("Unlicense"), &p, OutputPath);
                    }
                    else
                    {
                        LOG_WARNING("Unable to generate a %S %S file. Skipping...", LicenseVar.Value, LicenseFileName);
                    }

                    if (!bLicenseExported)
                    {
                        LOG_WARNING("Failed to write a %S file to \"%S\". Skipping...", OutputPath, LicenseFileName);
                    }
                }
            }
        }
    }

    Clock ExternalClock = {0};
    Clock_Start(&ExternalClock);

    // precompile step
    if (!TryRunBuildCommands(S("PreCompile"), WorkingPath, VariablesDB, &ExternalClock))
    {
        Receipt.ExitCode = 1;
        return Receipt;
    }

    // find the icon path (if specified)
    if (Icon.Length > 0)
    {
        u32 LastSlashIndex = 0;
        xx String_IndexOfLastPathSlash(Icon, &LastSlashIndex);

        bool bHasExtension = false;
        u32 LastDot = 0;
        if (String_IndexOfLastChar(Icon, '.', &LastDot))
        {
            bool bHasPathSeparator = String_IndexOfFirstPathSlash(StrShiftF(Icon, LastDot), NULL);
            if (!bHasPathSeparator)
            {
                bHasExtension = true;
            }
        }

        if (bHasExtension && LastSlashIndex) // is this an exact file path? if so, no need to search
        {
            String_Copy(&IconFilePath, Icon);
        }
        else
        {
            struct Data
            {
                TArray(FileVariable) VariablesDB;
                String* IconFilePath;
                bool bSuccess;
                u8 Padding[7];
            };

            struct Data d = {0};
            d.VariablesDB = VariablesDB;
            d.IconFilePath = &IconFilePath;
            d.bSuccess = false;

            String SearchPath = WorkingPath;
            if (LastSlashIndex && !Filesystem_IsPathRelative(Icon))
            {
                SearchPath = StrSlice(Icon.Data, LastSlashIndex+1);
            }

            Filesystem_IterateDirectory_Ex(SearchPath, &IconFileDirectoryIterator, true, &d);

            if (!d.bSuccess)
            {
                LOG_WARNING("Failed to find icon file \"%S\" in \"%S\". Skipping icon build...\n", Icon, SearchPath);
                String_Empty(&IconFilePath);
            }
        }

        if (IconFilePath.Length > 0)
        {
            #if PLATFORM_WINDOWS
            const String IconExt = S(".ico");
            #else
            const String IconExt = S(".png");
            #endif

            if (!String_EndsWith(IconFilePath, IconExt, false))
            {
                LOG_WARNING("Icon file \"%S\" is not a %S file. Skipping icon build...\n", IconFilePath, IconExt);
                String_Empty(&IconFilePath);
            }
        }
    }
    // TODO: refactor the above code. this is ugly
    p.IconFilePath = IconFilePath;

    // log "Building (Assembly)" ui text
    {
        if (NumSources > 0)
        {
            if (AssemblyType == AssemblyType_CustomCompilerObject && !bCanLink)
            {
                LOG("Building *%S files [%S] (%u %S) (with %u %S max)\n", CompilerObjectExt, S(CPU_ARCHITECTURE_STRING), NumSources, NumSources == 1 ? S("source file") : S("source files"), MaxCompilersAtOnce, MaxCompilersAtOnce == 1 ? S("core") : S("cores"));
            }
            else
            {
                u32 WhitespaceIndex = 0;
                bool bHasSpace = String_IndexOfFirstWhitespace(Extension_Og, &WhitespaceIndex);

                #ifndef HOOD
                if (bIsAssemblyExe || !bHasSpace)
                {
                    LOG("Building %S [%S] (%u %S) (with %u %S max)\n", AssemblyNameWithExt, S(CPU_ARCHITECTURE_STRING), NumSources, NumSources == 1 ? S("source file") : S("source files"), MaxCompilersAtOnce, MaxCompilersAtOnce == 1 ? S("core") : S("cores"));
                }
                else
                {
                    String NextExt = StrShiftF(Extension_Og, WhitespaceIndex+1);
                    LOG("Building %S/%S [%S] (%u %S) (with %u %S max)\n", AssemblyNameWithExt, NextExt, S(CPU_ARCHITECTURE_STRING), NumSources, NumSources == 1 ? S("source file") : S("source files"), MaxCompilersAtOnce, MaxCompilersAtOnce == 1 ? S("core") : S("cores"));
                }
                #else
                if (bIsAssemblyExe || !bHasSpace)
                {
                    LOG("build'n dis fooo %S\n", AssemblyNameWithExt);
                }
                else
                {
                    String NextExt = StrShiftF(Extension_Og, WhitespaceIndex+1);
                    LOG("build'n dis fooo %S and %S%S\n", AssemblyNameWithExt, AssemblyName, NextExt);
                }
                #endif
            }
        }

        // compile executable icon just before we link (if specified)
        #if PLATFORM_WINDOWS
        if (IconFilePath.Length > 0)
        {
            if (bHasRcProgram)
            {
                Clock_Start(&IconClock);

                u32 LastSlashIndex = 0;
                bool bHasSlash = String_IndexOfLastPathSlash(IconFilePath, &LastSlashIndex);

                String BasePath = bHasSlash ? StrSlice(IconFilePath.Data, LastSlashIndex) : IconFilePath;

                StringLocal(RcFilePath, MAX_PATH_LENGTH);
                if (Filesystem_IsPathRelative(IconFilePath))
                {
                    String_BuildPath(&RcFilePath, WorkingPath, BasePath, S("icon.rc"));
                }
                else
                {
                    String_BuildPath(&RcFilePath, BasePath, S("icon.rc"));
                }

                if (Export_IconRC(RcFilePath, IconFilePath))
                {
                    if (!RC_Compile(&p, RcFilePath, &IconResFilePath))
                    {
                        LOG_WARNING("Failed to build icon \"%S\" for %S%S. Skipping icon build...", IconFilePath, AssemblyName, Extension);
                        String_Empty(&IconResFilePath);
                    }
                }
                else
                {
                    LOG_WARNING("Failed to build icon \"%S\" for %S%S. Skipping icon build...", IconFilePath, AssemblyName, Extension);
                }

                Clock_Tick(&IconClock);
            }
            else
            {
                LOG_WARNING("Unable to build icon. You do not have a resource compiler installed on this machine. Skipping icon build...");
            }
        }

        // only build the version resource if we have TitleName, CompanyName, Description, Version, Copyright, or CompanyName
        // and no custom resource file was specified
        if (CountData.NumRcSources == 0 &&
            ((TitleName.Length > 0 || CompanyName.Length > 0 || Description.Length > 0 ||
            (!bFallbackVersion && Version.Length > 0) || Copyright.Length > 0)))
        {
            if (bHasRcProgram)
            {
                Clock_Start(&ResourceCompileClock);

                // TODO: when building both a static/shared lib, we do not generate the correct FILETYPE. fix it boy

                StringLocal(VersionRCPath, MAX_PATH_LENGTH);
                String_Append(&VersionRCPath, IntermediateBaseDirectory);
                String_Append(&VersionRCPath, BuildFileName);
                String_Append(&VersionRCPath, S("_version.rc"));

                // todo: allow custom version rc file?

                // generate version rc file
                if (Export_VersionRC(&p, VersionRCPath))
                {
                    if (!RC_Compile(&p, VersionRCPath, &VersionResFilePath))
                    {
                        LOG_WARNING("Failed to build resource file \"%S\" for %S%S. Skipping...\n", VersionRCPath, AssemblyName, Extension);
                        String_Empty(&VersionResFilePath);
                    }
                }
                else
                {
                    LOG_WARNING("Failed to build resource file \"%S\" for %S%S. Skipping...\n", VersionRCPath, AssemblyName, Extension);
                }

                Clock_Tick(&ResourceCompileClock);
            }
            else
            {
                LOG_WARNING("Unable to build version resource file. You do not have a resource compiler installed on this machine. Skipping icon build...");
            }
        }
        #endif // PLATFORM_WINDOWS
    }

    p.IconResFilePath    = IconResFilePath;
    p.VersionResFilePath = VersionResFilePath;

    if (IconResFilePath.Length > 0 || VersionResFilePath.Length > 0)
    {
        LOG_LINE_BREAK();
    }

    bool bSuccess = false;

    Clock CompileClock;
    Clock_Start(&CompileClock);

    bSuccess = C_Compile(&p, &NumCompiled);

    xx Platform_WaitForMultipleHandles(Processes, (u32)Array_Num(Processes), -1, true);

    Clock_Tick(&CompileClock);

    if (!bSuccess)
    {
        Receipt.ExitCode = 1;
        return Receipt;
    }

    if (NumCompiled == 0)
    {
        if (bRunPostBuildWhenWorkWasDone)
        {
            goto End;
        }

        goto PostBuild;
    }

    LOG_LINE_BREAK();

    // postcompile step
    if (!TryRunBuildCommands(S("PostCompile"), WorkingPath, VariablesDB, &ExternalClock))
    {
        Receipt.ExitCode = 1;
        return Receipt;
    }

    // prelink step
    Clock LinkClock = {0};

    if (bCanLink)
    {
        if (!TryRunBuildCommands(S("PreLink"), WorkingPath, VariablesDB, &ExternalClock))
        {
            Receipt.ExitCode = 1;
            return Receipt;
        }

        #if PLATFORM_WINDOWS
        // try to delete any .pdb files before we try to link
        StringLocal(AssemblyWildcard, MAX_PATH_LENGTH);
        String_Append(&AssemblyWildcard, AssemblyName);
        String_Append(&AssemblyWildcard, S("*.pdb"));
        xx Filesystem_DeleteFiles(BuildBaseDirectory, AssemblyWildcard, true);
        #endif

        Clock_Start(&LinkClock);

        bSuccess = C_Link(&p);

        Clock_Tick(&LinkClock);

        if (!bSuccess)
        {
            Receipt.ExitCode = 1;
            return Receipt;
        }

        // postlink step
        if (!TryRunBuildCommands(S("PostLink"), WorkingPath, VariablesDB, &ExternalClock))
        {
            Receipt.ExitCode = 1;
            return Receipt;
        }
    }

    // TODO
    // PostBundle step?
    // Bundle.Resources key to copy files in macos bundles

    #if PLATFORM_APPLE
    // compile the .app bundle (if desired)
    if (bBundleApp && bIsAssemblyExe)
    {
        Clock_Start(&BundleCompileClock);

        bSuccess = TryBuildMacBundle(*Arena, &p, VariablesDB);
        if (!bSuccess)
        {
            Receipt.ExitCode = 1;
            return Receipt;
        }

        Clock_Tick(&BundleCompileClock);
    }

    // build icon for mach-o executables
    {
        Clock_Start(&IconClock);

        bSuccess = TryBuildOrCleanMacExeIcon(IconFilePath, &p);
        if (!bSuccess)
        {
            Receipt.ExitCode = 1;
            return Receipt;
        }

        Clock_Tick(&IconClock);
    }

    #endif

    // build icon for linux executables
    #if PLATFORM_LINUX || PLATFORM_BSD
    if (Platform_GetDesktopEnvironment() != Desktop_Unknown)
    {
        Clock_Start(&IconClock);
        
        bSuccess = TryBuildOrCleanUnixExeIcon(IconFilePath, &p);
        if (!bSuccess)
        {
            Receipt.ExitCode = 1;
            return Receipt;
        }

        Clock_Tick(&IconClock);
    }
    #endif

    Clock_Tick(&BuildRuntime);

    if (bQuietBuild) { Logging_Enable(); }

    // log all the timings
    if (!bQuietBuild)
    {
        StringLocal(TimingBuffer, 512);

        // calculate the overhead time
        f64 TotalElapsedTime =  CompileClock.ElapsedTime +
                                LinkClock.ElapsedTime +
                                IconClock.ElapsedTime +
                                ResourceCompileClock.ElapsedTime +
                                BundleCompileClock.ElapsedTime +
                                BuildFileParseClock.ElapsedTime +
                                DependencyBuildClock.ElapsedTime +
                                ExternalClock.ElapsedTime;

        Clock OverheadClock = {0};
        OverheadClock.StartTime = 1;
        OverheadClock.ElapsedTime = BuildRuntime.ElapsedTime - TotalElapsedTime;

        PrintClockTimeToBuffer(&TimingBuffer, &CompileClock,         &BuildRuntime, S("\nCompile     time: "), false);
        PrintClockTimeToBuffer(&TimingBuffer, &LinkClock,            &BuildRuntime, S(  "Link        time: "), false);
        PrintClockTimeToBuffer(&TimingBuffer, &IconClock,            &BuildRuntime, S(  "Icon        time: "), false);
        PrintClockTimeToBuffer(&TimingBuffer, &ResourceCompileClock, &BuildRuntime, S(  "Resource    time: "), false);
        PrintClockTimeToBuffer(&TimingBuffer, &BundleCompileClock,   &BuildRuntime, S(  "Bundle      time: "), false);
        PrintClockTimeToBuffer(&TimingBuffer, &BuildFileParseClock,  &BuildRuntime, S(  "Build Parse time: "), false);
        PrintClockTimeToBuffer(&TimingBuffer, &DependencyBuildClock, &BuildRuntime, S(  "Dependency  time: "), true);
        PrintClockTimeToBuffer(&TimingBuffer, &ExternalClock,        &BuildRuntime, S(  "External    time: "), true);
        PrintClockTimeToBuffer(&TimingBuffer, &OverheadClock,        &BuildRuntime, S(  "Overhead    time: "), true);
        PrintClockTimeToBuffer(&TimingBuffer, &BuildRuntime,         NULL,          S(  "Total build time: "), false);

        LOG("%S", TimingBuffer);
    }

    StringLocal(OutputPath, MAX_PATH_LENGTH);
    String_AppendChar(&OutputPath, '"');
    String_Append(&OutputPath, BuildBaseDirectory);
    String_Append(&OutputPath, AssemblyNameWithExt);
    String_AppendChar(&OutputPath, '"');

    if (AssemblyType == AssemblyType_CustomCompilerObject)
    {
        #ifndef HOOD
        LOG_SUCCESS("Build complete", OutputPath);
        #else
        LOG_SUCCESS("lessss fuckinggg goooo", OutputPath);
        #endif
    }
    else
    {
        u32 WhitespaceIndex = 0;
        bool bHasSpace = String_IndexOfFirstWhitespace(Extension_Og, &WhitespaceIndex);

        if (bIsAssemblyExe || !bHasSpace)
        {
            #ifndef HOOD
            LOG_SUCCESS("Build complete: %S", OutputPath);
            #else
            LOG_SUCCESS("lessss fuckinggg goooo: %S", OutputPath);
            #endif
        }
        else
        {
            String NextExt = StrShiftF(Extension_Og, WhitespaceIndex+1);

            StringLocal(OutputPath2, MAX_PATH_LENGTH);
            String_AppendChar(&OutputPath2, '"');
            String_Append    (&OutputPath2, BuildBaseDirectory);
            String_Append    (&OutputPath2, AssemblyName);
            String_Append    (&OutputPath2, NextExt);
            String_AppendChar(&OutputPath2, '"');

            #ifndef HOOD
            LOG_SUCCESS("Build complete: %S\n                          %S", OutputPath, OutputPath2);
            #else
            LOG_SUCCESS("lessss fuckinggg goooo: %S\n                         %S", OutputPath, OutputPath2);
            #endif
        }
    }

    // run post build commands (if specified)
PostBuild:
    if (bQuietBuild) { Logging_Enable(); }

    if (!TryRunBuildCommands(S("PostBuild"), WorkingPath, VariablesDB, NULL))
    {
        Receipt.ExitCode = 1;
        return Receipt;
    }

End:
    // run the assembly (if an executable)
    if (bIsAssemblyExe)
    {
        // todo:if we have .run keys ignore this shit
        if (StringArray_Contains(Parameters, S("Run"), false))
        {
            // todo: args like .run key
            
            Internal_RunAssembly(*Arena, WorkingPath, BuildBaseDirectory, AssemblyNameWithExt, String_Null());
        }

        for each (FileVariable, v, VariablesDB)
        {
            if (String_IsEqual(v.Name, S(".Run"), false))
            {
                bool bIsSpecial = String_IsEqual(v.Params, S("Only_Done_Work"), false);
                if (bIsSpecial)
                {
                    if (NumCompiled == 0)
                    {
                        continue;
                    }
                }

                Internal_RunAssembly(*Arena, WorkingPath, BuildBaseDirectory, AssemblyNameWithExt, v.Value);
            }
        }
    }

    if (bQuietBuild) { Logging_Disable(); }

    Receipt.bWorkWasDone = NumCompiled > 0;

    return Receipt;
}

static void LogDividerLine(void)
{
    if (bQuietBuild) { Logging_Enable(); }

    LOG_LINE_BREAK();

    u32 Rows = 0, Cols = 0;
    if (Platform_GetTerminalDimensions(&Rows, &Cols))
    {
        u8 Separator[256] = {0};
        for (i32 i = 0; i < Min((i32)(Cols-1), 255); i++)
        {
            Separator[i] = '=';
        }

        u32 Len = (i32)(Cols - 1) <= 0 ? 0 : Cols-1;
        LOG("%S\n", StrSlice(Separator, Len));
    }

    if (bQuietBuild) { Logging_Disable(); }
}

static u32 RiftBuild(LinearAllocator* Arena, const StringArray Arguments, const String BaseDirectory)
{
    if (NEVER(Arena == NULL)) { return 1; }

    StringLocal(BuildFileName, 128);
    StringLocal(BuildFilePath, MAX_PATH_LENGTH);

    StringLocal(WorkingDirectory, MAX_PATH_LENGTH);
    String_Copy(&WorkingDirectory, BaseDirectory);

    bool bBuildPathGivenInCmdLine = false;
    bool bNoBuildFileSpecifiedInCmd = false;

    i8 BuildFileIndex = -1;
    i8 RootPathIndex = -1;

    if (Arguments.Num == 0)
    {
        bNoBuildFileSpecifiedInCmd = true;
    }
    else
    {
        for (u8 i = 0; i < Arguments.Num; i++)
        {
            if (IsBuildFile(Arguments.List[i]) ||
                IsBuildBatchFile(Arguments.List[i]))
            {
                BuildFileIndex = (i8)i;
                break;
            }
        }

        // find the root path (if specified)
        for (u8 i = 0; i < Arguments.Num; i++)
        {
            if (i == BuildFileIndex)
            {
                continue;
            }

            if (IsBuildFile(Arguments.List[i]) ||
                IsBuildBatchFile(Arguments.List[i]))
            {
                continue;
            }

            if (String_StartsWith(Arguments.List[i], S("override:"), false) ||
                String_StartsWith(Arguments.List[i], S("export:"), false) ||
                String_StartsWith(Arguments.List[i], S("preset:"), false) ||
                String_StartsWith(Arguments.List[i], S("list:"), false))
            {
                continue;
            }
        
            if (String_IndexOfChar(Arguments.List[i], '\\', NULL) ||
                String_IndexOfChar(Arguments.List[i], '/', NULL))
            {
                RootPathIndex = (i8)i;
                break;
            }
        }

        if (BuildFileIndex == -1)
        {
            bNoBuildFileSpecifiedInCmd = true;
        }

        if (RootPathIndex >= 0)
        {
            String UserPath = Arguments.List[RootPathIndex];

            if (Filesystem_IsPathRelative(UserPath))
            {
                String_BuildPath(&WorkingDirectory, UserPath);
            }
            else
            {
                String_Copy(&WorkingDirectory, Arguments.List[RootPathIndex]);
            }
        }

        if (BuildFileIndex >= 0)
        {
            u32 LastSlash = 0;
            if (String_IndexOfLastPathSlash(Arguments.List[BuildFileIndex], &LastSlash))
            {
                String Name = StrShiftF(Arguments.List[BuildFileIndex], LastSlash+1);
                String_Copy(&BuildFileName, Name);

                if (Filesystem_IsPathRelative(Arguments.List[BuildFileIndex]))
                {
                    String_BuildPath(&BuildFilePath, WorkingDirectory, Arguments.List[BuildFileIndex]);
                }
                else
                {
                    String_Copy(&BuildFilePath, Arguments.List[BuildFileIndex]);
                }
                
                if (!IsBuildFile(Name) && !IsBuildBatchFile(Name))
                {
                    if (!String_EndsWith(BuildFilePath, S(".build"), false))
                    {
                        String_Append(&BuildFilePath, S(".build"));
                    }

                    if (!String_EndsWith(BuildFileName, S(".build"), false))
                    {
                        String_Append(&BuildFileName, S(".build"));
                    }
                }

                // change the working directory to match where this build file lives (if not already specified)
                if (RootPathIndex == -1)
                {
                    u32 Slash = 0;
                    xx String_IndexOfLastPathSlash(BuildFilePath, &Slash);
                    String_Copy(&WorkingDirectory, StrSlice(BuildFilePath.Data, Slash));
                }

                bBuildPathGivenInCmdLine = true;
            }
            else
            {
                String_Copy(&BuildFileName, Arguments.List[BuildFileIndex]);
            }
        }
    }

    xx String_EatPathSeparatorsInlineFromEnd(&WorkingDirectory);
    xx Filesystem_ConvertRelativeToAbsolutePath(&WorkingDirectory);
    String_ConvertSlashToPlatformSlash(&WorkingDirectory);

    if (WorkingDirectory.Length < 1 || WorkingDirectory.Length > MAX_PATH_LENGTH)
    {
        #ifndef HOOD
        LOG_ERROR("Invalid root path: %S", WorkingDirectory);
        #else
        LOG_ERROR("wtf is this my nigga, dis path make no sense, cant work wit it: %S", WorkingDirectory);
        #endif

        return 1;
    }

    if (!Filesystem_DoesDirectoryExist(WorkingDirectory))
    {
        #ifndef HOOD
        LOG_ERROR("Given root directory \"%S\" does not exist", WorkingDirectory);
        #else
        LOG_ERROR("nah cuh, dis path aint nowhere to be seen: %S", WorkingDirectory);
        #endif

        return 1;
    }

    // prevent riftbuild from running in a root drive directory like C:/ (or / on unix).
    // it's non-sensical anyway, it has no business running in those places
    {
        StringLocal(RootCopy, MAX_PATH_LENGTH);
        String_Copy(&RootCopy, WorkingDirectory);
        xx String_EatSpacesInlineFromEnd(&RootCopy);
        xx String_EatPathSeparatorsInlineFromEnd(&RootCopy);
        String_AppendPathSeparator(&RootCopy);

        u32 NumPathSeparators = String_CountPathSeparators(RootCopy);
        if (NumPathSeparators <= 1)
        {
            LOG_ERROR("%S is too shallow of a directory.\n", RootCopy);
            LOG("    Create a new directory from here and then run riftbuild again from the new directory");

            return 1;
        }
    }

    xx String_EatPathSeparatorsInlineFromEnd(&WorkingDirectory);

    if (!Platform_SetWorkingDirectory(WorkingDirectory))
    {
        #ifndef HOOD
        LOG_ERROR("Failed to set working directory to \"%S\"", WorkingDirectory);
        #else
        LOG_ERROR("nah cuh, couldnt set the workin directory to \"%S\"", WorkingDirectory);
        #endif

        return 1;
    }

    if (StringArray_Contains(Arguments, S("-a"), false) ||
        StringArray_Contains(Arguments, S("--about"), false))
    {
        PrintAbout();

        #if !PLATFORM_WINDOWS
        LOG_LINE_BREAK();
        #endif

        return 0;
    }

    if (StringArray_Contains(Arguments, S("-b"), false) ||
        StringArray_Contains(Arguments, S("--buildfiles"), false))
    {
        PrintBuildFiles(WorkingDirectory);

        #if !PLATFORM_WINDOWS
        LOG_LINE_BREAK();
        #endif

        return 0;
    }

    if (StringArray_Contains(Arguments, S("-h"), false) ||
        StringArray_Contains(Arguments, S("--help"), false) ||
        StringArray_Contains(Arguments, S("?"), false) ||
        StringArray_Contains(Arguments, S("-?"), false) ||
        StringArray_Contains(Arguments, S("/?"), false))
    {
        PrintUsage(WorkingDirectory);

        #if !PLATFORM_WINDOWS
        LOG_LINE_BREAK();
        #endif

        return 0;
    }

    if (StringArray_Contains(Arguments, S("-i"), false) ||
        StringArray_Contains(Arguments, S("--internals"), false))
    {
        PrintInternals();

        #if !PLATFORM_WINDOWS
        LOG_LINE_BREAK();
        #endif
        
        return 0;
    }

    if (StringArray_Contains(Arguments, S("-t"), false) ||
        StringArray_Contains(Arguments, S("--tutorial"), false))
    {
        LogPathEnvVarTutorialSteps();
        LOG_LINE_BREAK();
        LogRegularEnvVarTutorialSteps();

        #if !PLATFORM_WINDOWS
        LOG_LINE_BREAK();
        #endif

        return 0;
    }

    bHelp         = StringArray_Contains(Arguments, S("help"), false);
    bOptions      = StringArray_Contains(Arguments, S("options"), false);
    bIsClean      = StringArray_Contains(Arguments, S("clean"), false);
    bVerboseLog   = StringArray_Contains(Arguments, S("-v"), false);
    bSingleThread = StringArray_Contains(Arguments, S("--singlethread"), false) ||
                    StringArray_Contains(Arguments, S("-s"), false);

    BuildFileDirectoryIteratorData Data = {0};
    Data.bNoBuildFileSpecifiedInCmd     = bNoBuildFileSpecifiedInCmd;
    Data.BuildFileIndex                 = BuildFileIndex;
    Data.RootPathIndex                  = RootPathIndex;
    Data.Name                           = &BuildFileName;
    Data.Path                           = &BuildFilePath;
    Data.Arguments                      = Arguments;
    Data.bSearchOnlyBuildBatch          = true;

    // first, find .buildbatch files
    {
        if (IsBuildBatchFile(BuildFileName) && bBuildPathGivenInCmdLine)
        {
            Data.bFoundBuildFile = true;

            if (!Filesystem_DoesFileExist(BuildFilePath))
            {
                LOG_ERROR("Failed to find %S in %S", BuildFileName, WorkingDirectory);
                return 1;
            }
        }

        if (!Data.bFoundBuildFile)
        {
            // TODO: simplify this, this is shit
            for (u8 i = 0; i < Arguments.Num; i++)
            {
                if (IsBuildBatchFile(Arguments.List[i]))
                {
                    String_Copy(&BuildFileName, Arguments.List[i]);
                    Filesystem_IterateDirectory_Ex(WorkingDirectory, &BuildFileDirectoryIterator, false, &Data);
                    break;
                }
            }

            if ((!Data.bFoundBuildFile || Data.NumBuildFilesFound > 1) && Arguments.Num > 0)
            {
                Filesystem_IterateDirectory_Ex(WorkingDirectory, &BuildFileDirectoryIterator_Args, false, &Data);
            }

            if (!Data.bFoundBuildFile) // final search for .buildbatch
            {
                Filesystem_IterateDirectory_Ex(WorkingDirectory, &BuildFileDirectoryIterator, false, &Data);
            }
        }

        if (Data.bFoundBuildFile)
        {
            usize Allocated = Arena->Allocated;

            FileHandle f = FileHandle_Null();
            if (Filesystem_Open(BuildFilePath, FileMode_Read, &f))
            {
                StringLocal(Line, 512);
                bool bInMultiLineComment = false;

                bool bWantsRebuild = StringArray_Contains(Arguments, S("rebuild"), false);
                bool bWantsClean = StringArray_Contains(Arguments, S("clean"), false);

                LOG("Batch build begin");
                LogDividerLine();

                while (Filesystem_ReadLine(f, &Line))
                {
                    String Trimmed = String_EatSpaces(Line);

                    if (Trimmed.Length == 0)
                    {
                        continue;
                    }

                    // multiline comment
                    if (Trimmed.Data[0] == '#' && Trimmed.Data[1] == '#')
                    {
                        bInMultiLineComment = !bInMultiLineComment;
                        continue;
                    }

                    if (bInMultiLineComment)
                    {
                        continue;
                    }

                    // single line comment
                    if (Trimmed.Data[0] == '#')
                    {
                        continue;
                    }

                    StringList List = String_SplitIntoList(Arena, Trimmed, ' ', true);
                    u16 Num = 0;
                    for each_str_list (List) { Num += 1; }

                    // TODO: rework this, so baaaaaddd...
                    if (bWantsRebuild) { Num += 1; }
                    if (bWantsClean)   { Num += 1; }
                    if (bVerboseLog)   { Num += 1; }

                    StringArray NewArguments = {0};
                    if (Num > 0)
                    {
                        NewArguments.List = LinearAllocator_Allocate(Arena, sizeof(String) * Num);
                        NewArguments.Num = Num;

                        u16 i = 0;
                        for each_str_list (List)
                        {
                            NewArguments.List[i] = String_Create(Arena, It.String);
                            i++;
                        }

                        // TODO: other args too, get rid of this code, make it more dynamic
                        if (bWantsRebuild)
                        {
                            NewArguments.List[i] = S("rebuild");
                            i++;
                        }

                        if (bWantsClean)
                        {
                            NewArguments.List[i] = S("clean");
                            i++;
                        }

                        if (bVerboseLog)
                        {
                            NewArguments.List[i] = S("-v");
                        }
                    }

                    u32 ReturnValue = RiftBuild(Arena, NewArguments, WorkingDirectory);
                    if (ReturnValue != 0)
                    {
                        return ReturnValue;
                    }

                    LogDividerLine();

                    // "free" the memory back to the original spot
                    Arena->Allocated = Allocated;
                }

                Filesystem_Close(&f);
                LOG("Batch build complete");
                return 0;
            }

            return 1;
        }
    }

    Data.bSearchOnlyBuildBatch = false;

    if (BuildFilePath.Length == 0) // only search if we did not get an explicit build file path from the user
    {
        if (bNoBuildFileSpecifiedInCmd)
        {
            Filesystem_IterateDirectory_Ex(WorkingDirectory, &BuildFileDirectoryIterator, false, &Data);
            if (Data.NumBuildFilesFound == 1)
            {
                Data.bFoundBuildFile = true;
            }

            if ((!Data.bFoundBuildFile || Data.NumBuildFilesFound > 1) && Arguments.Num > 0)
            {
                Filesystem_IterateDirectory_Ex(WorkingDirectory, &BuildFileDirectoryIterator_Args, true, &Data);
            }
        }
        else
        {
            Filesystem_IterateDirectory_Ex(WorkingDirectory, &BuildFileDirectoryIterator, true, &Data);
        }
    }
    else
    {
        Data.bFoundBuildFile = Filesystem_DoesFileExist(BuildFilePath);
        if (!Data.bFoundBuildFile)
        {
            LOG_ERROR("Failed to find %S in %S", BuildFilePath, WorkingDirectory);
            return 1;
        }
    }

    if (!Data.bFoundBuildFile)
    {
        if (bNoBuildFileSpecifiedInCmd)
        {
            // continue on, we dont need a build file to build a program!
        }
        else
        {
            LOG_ERROR("Failed to find %S in %S", BuildFileName, WorkingDirectory);
            return 1;
        }
    }

    FileHandle BuildFileHandle = FileHandle_Null();

    StringArray BuildArguments = Arguments;

    if (Data.bFoundBuildFile)
    {
        if (Data.NumBuildFilesFound > 1 && BuildFilePath.Length == 0)
        {
            #ifndef HOOD
            LOG_ERROR("Multiple build files found. Please specify a build file\n");
            LOG("Here is the list of all the build files found within %S", WorkingDirectory);
            #else
            LOG_ERROR("yooo thes too many buil files here dawg. gotta be more specific for me\n");
            LOG("got a list for ya here, found em from %S", WorkingDirectory);
            #endif

            Filesystem_IterateDirectory(WorkingDirectory, &MultipleBuildFileDirectoryIterator, true);

            return 1;
        }

        // set the build file index, so it can be ignored and not be stored in the cmd options db array
        if (bNoBuildFileSpecifiedInCmd)
        {
            for (u32 i = 0; i < Arguments.Num; i++)
            {
                if (String_IsEqual(Filesystem_ExtractFileName(BuildFileName, false), Arguments.List[i], false))
                {
                    BuildFileIndex = (i8)i;
                    break;
                }
            }
        }

        StringLocal(BuildFilePathFull, MAX_PATH_LENGTH);
        if (bBuildPathGivenInCmdLine)
        {
            if (Filesystem_IsPathRelative(BuildFilePath))
            {
                String_BuildPath(&BuildFilePathFull, WorkingDirectory, BuildFilePath);
            }
            else
            {
                String_Copy(&BuildFilePathFull, BuildFilePath);
            }
        }
        else
        {
            String_Copy(&BuildFilePathFull, BuildFilePath);
        }

        if (!Filesystem_Open(BuildFilePathFull, FileMode_Read, &BuildFileHandle))
        {
            #ifndef HOOD
            LOG_ERROR("Failed to open build file \"%S\" for reading", BuildFilePathFull);
            #else
            LOG_ERROR("wtf, cant read this shit man, think the path to the build file is wrong or smthg homie. this is what i got: %S", BuildFilePath);
            #endif
            return 1;
        }

        // parse build file for "presets:"
        // swap out the current arguments array with what's specified in the chosen preset
        String PresetValue = String_Null();
        for (u8 i = 0; i < Arguments.Num; i++)
        {
            if (String_StartsWith(Arguments.List[i], S("preset:"), false))
            {
                PresetValue = StrShiftF(Arguments.List[i], 7);
                if (PresetValue.Length == 0)
                {
                    LOG_ERROR("Failed to find preset. No name or number was given after ':'");
                    LOG("\nUsage:");
                    LOG("     preset:0");
                    LOG("     preset:some_name");

                    // todo: better error message, log all avaiable presets in build file

                    return 1;
                }

                break;
            }
        }

        if (PresetValue.Length > 0)
        {
            StringLocal(Line, 512);
            StringLocal(PresetArgumentLine, 512);
            bool bInMultiLineComment = false;
            bool bFoundPreset = false;

            u8 PresetIndex = 0;
            i8 GivenPresetIndex = 0;
            if (!String_ToI8(PresetValue, &GivenPresetIndex))
            {
                GivenPresetIndex = -1;
            }

            while (Filesystem_ReadLine(BuildFileHandle, &Line))
            {
                String Trimmed = String_EatSpaces(Line);

                if (Trimmed.Length == 0)
                {
                    continue;
                }

                // multiline comment
                if (Trimmed.Data[0] == '#' && Trimmed.Data[1] == '#')
                {
                    bInMultiLineComment = !bInMultiLineComment;
                    continue;
                }

                if (bInMultiLineComment)
                {
                    continue;
                }

                // single line comment
                if (Trimmed.Data[0] == '#')
                {
                    continue;
                }

                // @parse name/value
                u32 SpaceIndex = 0;
                bool bFoundSpace = String_IndexOfFirstWhitespace(Trimmed, &SpaceIndex);

                String VarName, VarValue;

                if (bFoundSpace)
                {
                    const String Name = String_EatSpacesFromEnd(StrSlice(Trimmed.Data, SpaceIndex));
                    const String Value = String_EatSpacesFromEnd(String_EatSpaces(StrShiftF(Trimmed, SpaceIndex+1)));

                    VarName = Name;
                    VarValue = Value;
                }
                else
                {
                    VarName = Trimmed;
                    VarValue = String_Null();
                }

                if (String_StartsWith(VarName, S("preset:"), false))
                {
                    if (GivenPresetIndex == PresetIndex ||
                        String_IsEqual(StrShiftF(VarName, 7), PresetValue, false))
                    {
                        bFoundPreset = true;
                        String_Copy(&PresetArgumentLine, VarValue);
                        break;
                    }

                    PresetIndex++;
                }
            }

            xx Filesystem_SeekToBeginning(BuildFileHandle);
        
            if (bFoundPreset)
            {
                StringList List = String_SplitIntoList(Arena, PresetArgumentLine, ' ', true);
                u16 Num = 0;
                for each_str_list (List) { Num += 1; }

                StringArray NewArguments = {0};
                if (Num > 0)
                {
                    NewArguments.List = LinearAllocator_Allocate(Arena, sizeof(String) * Num);
                    NewArguments.Num = Num;

                    u16 i = 0;
                    for each_str_list (List)
                    {
                        NewArguments.List[i] = String_Create(Arena, It.String);
                        i++;
                    }

                    // TODO: other args too, make it more dynamic
                    BuildArguments = NewArguments;
                }
            }
            else
            {
                LOG_ERROR("Failed to find preset \"%S\" in build file \"%S\"", PresetValue, BuildFilePathFull);
                return 1;
            }
        }
    }

    PlatformMutex BuildMutex = {0};
    BuildReceipt Receipt = BuildTarget(Arena, BuildFileHandle, &BuildMutex, WorkingDirectory, BuildArguments, String_Null(), BuildFileIndex, RootPathIndex);
    if (BuildMutex.Handle) { xx Platform_ReleaseMutex(&BuildMutex); }

    Filesystem_Close(&BuildFileHandle);

    return Receipt.ExitCode;
}

static void InitInternalVars(LinearAllocator* Arena)
{
    ENSURE_NO_REENTRY();

    const u32 MaxInternalVars = 256;
    const u32 MaxSize = MaxInternalVars * sizeof(InternalVariable); // 8192 bytes
    InternalVariablesDB = Internal_ArrayCreateStatic(LinearAllocator_Allocate(Arena, MaxSize), MaxInternalVars, sizeof(InternalVariable));

    // store static options. like platform, native os .lib's, etc..
    AddInternalVariable(S(PLATFORM_STRING), String_Null());

    // store riftbuild version
    AddInternalVariable(S("_Version"),       S(RIFTBUILD_VERSION_STRING));
    AddInternalVariable(S("_Version.Major"), S(STRINGIFY(RIFTBUILD_MAJOR_VERSION)));
    AddInternalVariable(S("_Version.Minor"), S(STRINGIFY(RIFTBUILD_MINOR_VERSION)));
    AddInternalVariable(S("_Version.Patch"), S(STRINGIFY(RIFTBUILD_PATCH_VERSION)));

    const PlatformVersion OSVersion = Platform_GetVersion();
    String OSVersionString = String_Reserve(Arena, 24);
    String OSVersionStringMajor = String_Reserve(Arena, 8);
    String OSVersionStringMinor = String_Reserve(Arena, 8);
    String OSVersionStringPatch = String_Reserve(Arena, 8);

    String_Format(&OSVersionString, S("%u.%u.%u"),    OSVersion.Major, OSVersion.Minor, OSVersion.Patch);
    AddInternalVariable(S("_Platform.Version"),       OSVersionString);
    String_Format(&OSVersionStringMajor, S("%u"),     OSVersion.Major);
    AddInternalVariable(S("_Platform.Version.Major"), OSVersionStringMajor);
    String_Format(&OSVersionStringMinor, S("%u"),     OSVersion.Minor);
    AddInternalVariable(S("_Platform.Version.Minor"), OSVersionStringMinor);
    String_Format(&OSVersionStringPatch, S("%u"),     OSVersion.Patch);
    AddInternalVariable(S("_Platform.Version.Patch"), OSVersionStringPatch);

    u32 PosixVersion = Platform_GetPosixVersion();
    if (PosixVersion > 0)
    {
        String Temp = String_Reserve(Arena, 8);
        String_Format(&Temp, S("%u"), PosixVersion);
        AddInternalVariable(S("Posix.Version"), Temp);
        AddInternalVariable(S("Posix"), String_Null());
    }
    // TODO: lib c detection, glibc musl bsd macos
    // TODO: exe type. "elf" "pe"
    // TODO: exe extension. ".elf" ".exe"

    AddInternalVariable(Platform_IsBigEndian() ? S("big_endian") : S("little_endian"), String_Null());

    // detect default int and long sizes
    {
        StringLocal(Temp, 16);
        xx String_FromI32(&Temp, sizeof(int));
        AddInternalVariable(S("sizeof.int"), String_Create(Arena, Temp));

        String_Empty(&Temp);
        xx String_FromI32(&Temp, sizeof(long));
        AddInternalVariable(S("sizeof.long"), String_Create(Arena, Temp));
    }

    // detect default char signed-ness
    {
        char c = -1;
        AddInternalVariable(S("char.signed"), c < 0 ? S("1") : S("0"));
        AddInternalVariable(S("char.unsigned"), c > 0 ? S("1") : S("0"));
    }

    ECpuClipBehaviour CpuClipMode = Platform_GetCpuClippingBehaviour();
    if (CpuClipMode == CpuClip_Both)
    {
        AddInternalVariable(S("cpu.clip_positive"), String_Null());
        AddInternalVariable(S("cpu.clip_negative"), String_Null());
    }
    else
    {
        if (CpuClipMode == CpuClip_Positive)
        {
            AddInternalVariable(S("cpu.clip_positive"), String_Null());
        }

        if (CpuClipMode == CpuClip_Negative)
        {
            AddInternalVariable(S("cpu.clip_negative"), String_Null());
        }
    }

    // TODO _Ram
    //AddInternalVariable(S("_Platform.KernelVersion"), OSVersionString);
    //AddInternalVariable(S("_Platform.BuildVersion"), OSVersionString);
    
    #if PLATFORM_WINDOWS
    AddInternalVariable(S("_Platform"), S("Windows"));
    AddInternalVariable(S("Win32"),     String_Null());
    #if PLATFORM_64_BIT
    AddInternalVariable(S("Win64"),     String_Null());
    #endif
    #elif PLATFORM_MAC
    AddInternalVariable(S("_Platform"), S("macOS"));
    AddInternalVariable(S("Apple"),     String_Null());
    AddInternalVariable(S("Macintosh"), String_Null());
    AddInternalVariable(S("Mac"),       String_Null());
    AddInternalVariable(S("macOS"),     String_Null());
    AddInternalVariable(S("OSX"),       String_Null());
    AddInternalVariable(S("Unix"),      String_Null());
    #elif PLATFORM_LINUX
    AddInternalVariable(S("_Platform"), S("Linux"));
    AddInternalVariable(S("Linux"),     String_Null());
    AddInternalVariable(S("Unix"),      String_Null());

    StringLocal(DistroName, 128);
    StringLocal(DistroPrettyName, 128);
    StringLocal(DistroID, 128);
    Platform_DetectDistro(&DistroName, &DistroPrettyName, &DistroID);
    {
        StringLocal(DistroNameNoSpaces, 128);
        String_StripWhitespace(DistroName, &DistroNameNoSpaces);

        String Name = String_Create(Arena, DistroNameNoSpaces);
        AddInternalVariable(Name, String_Null());
        AddInternalVariable(S("_Distro"), Name);

        Name = String_Create(Arena, DistroPrettyName);
        AddInternalVariable(S("_DistroPrettyName"), Name);

        Name = String_Create(Arena, DistroID);
        AddInternalVariable(Name, String_Null());
        AddInternalVariable(S("_DistroID"), Name);
    }
    #elif PLATFORM_BSD
    AddInternalVariable(S("_Platform"), S("BSD " PLATFORM_STRING));
    AddInternalVariable(S("BSD"),       String_Null());
    AddInternalVariable(S("Unix"),      String_Null());
    #else
    AddInternalVariable(S("_Platform"), S("Unix"));
    AddInternalVariable(S("Unix"),      String_Null());
    #endif

    // TODO: macos
    #if PLATFORM_WINDOWS || PLATFORM_MAC
    {
        StringLocal(DesktopEnv, 32);
        Platform_DetectDesktopEnvironment(&DesktopEnv);

        String Env = String_Create(Arena, DesktopEnv);
        AddInternalVariable(Env,                      String_Null());
        AddInternalVariable(S("_DesktopEnvironment"), Env);
        AddInternalVariable(S("_DesktopEnv"),         Env);
        AddInternalVariable(S("_DE"),                 Env);
    }
    #elif PLATFORM_LINUX || PLATFORM_BSD
    StringLocal(DesktopEnv, 128);
    StringLocal(DesktopSession, 128);
    StringLocal(DesktopSessionType, 128);
    Platform_DetectDesktopEnvironment(&DesktopEnv, &DesktopSession, &DesktopSessionType);
    {
        String Env = String_Create(Arena, DesktopEnv);
        AddInternalVariable(Env,                      String_Null());
        AddInternalVariable(S("_DesktopEnvironment"), Env);
        AddInternalVariable(S("_DesktopEnv"),         Env);
        AddInternalVariable(S("_DE"),                 Env);

        Env = String_Create(Arena, DesktopSession);
        AddInternalVariable(Env,                      String_Null());
        AddInternalVariable(S("_DesktopSession"),     Env);
        AddInternalVariable(S("_DS"),                 Env);

        Env = String_Create(Arena, DesktopSessionType);
        AddInternalVariable(Env,                      String_Null());
        AddInternalVariable(S("_DesktopSessionType"), Env);
        AddInternalVariable(S("_DST"),                Env);
    }
    #endif

    // I dont know what to do, ignore these comments below
    // TODO: move to msvc backend...
    // TODO: update this by looking at the libs directory for winsdk and visual studio
    // scratch that, just iterate the directory and get all the file names in there... duh
    const String Win32Libs = S("kernel32 user32 opengl32 shell32 gdi32 comdlg32 comctl32 ws2_32 ntdll winmm netapi32 ole32 advapi32 "
                               "wldap32 crypt32 rpcrt4 shlwapi dbghelp bcrypt version imm32 cfgmgr32 setupapi oleaut32 shcore "
                               "uuid odbc32 odbccp32 delayimp userenv pathcch");

    const String LinuxLibs = S("m");

    AddInternalVariable(S("_Win32Libs"), Win32Libs);
    AddInternalVariable(S("_LinuxLibs"), LinuxLibs);

    #if PLATFORM_WINDOWS
    AddInternalVariable(S("_NativeLibs"), Win32Libs);
    #elif PLATFORM_LINUX || PLATFORM_UNIX
    AddInternalVariable(S("_NativeLibs"), LinuxLibs);
    #endif

    // TODO: support --arch:value
    AddInternalVariable(S("_Arch"), S(CPU_ARCHITECTURE_STRING));
    
    #if PLATFORM_64_BIT
    AddInternalVariable(S("_Bit"), S("64"));
    AddInternalVariable(S("64_bit"), String_Null());
    #else
    AddInternalVariable(S("_Bit"), S("32"));
    AddInternalVariable(S("32_bit"), String_Null());
    #endif

    const String One  = S("1");
    const String Zero = S("0");

    Uuid ID = UUID_Generate();
    StringLocal(UuidString, 64);
    UUID_ToStringFast(ID, &UuidString);
    AddInternalVariable(S("_UUID"), String_Create(Arena, UuidString));

    const CpuInfo CPUInfo = Platform_QueryCPUInfo();

    String CpuBrandName = S("Unknown");
    StringLocal(CPU, 64);
    if (Platform_GetCpuBrandName(&CPU))
    {
        CpuBrandName = String_Create(Arena, CPU);
        AddInternalVariable(S("_CPUBrand"), CpuBrandName);

        xx String_ReplaceCharInline(&CPU, ' ', '_');
        CpuBrandName = String_Create(Arena, CPU);
        AddInternalVariable(CpuBrandName, One);
    }
    else
    {
        AddInternalVariable(S("_CPUBrand"), CpuBrandName);
    }

    String CpuFullName = S("Unknown");
    if (Platform_GetFullCpuName(&CPU))
    {
        xx String_ReplaceCharInline(&CPU, '@', '|');

        CpuFullName = String_Create(Arena, CPU);
    }

    if (CPUInfo.Intel)
    {
        AddInternalVariable(S("_CPUVendor"), S("Intel"));
        AddInternalVariable(S("_CPU"), CpuFullName);

        AddInternalVariable(S("Intel"), One);
    }

    if (CPUInfo.AMD)
    {
        AddInternalVariable(S("_CPUVendor"), S("AMD"));
        AddInternalVariable(S("_CPU"), CpuFullName);

        AddInternalVariable(S("AMD"), One);
    }

    if (CPUInfo.Apple)
    {
        AddInternalVariable(S("_CPUVendor"), S("Apple"));
        AddInternalVariable(S("_CPU"), CpuFullName);
    }

    #if CPU_X64
    AddInternalVariable(S("x86"),       One);
    AddInternalVariable(S("x86_64"),    One);
    AddInternalVariable(S("x64"),       One);
    #elif CPU_X86
    AddInternalVariable(S("x86"),       One);
    AddInternalVariable(S("x86_32"),    One);
    #elif CPU_ARM64
    AddInternalVariable(S("ARM"),       One);
    AddInternalVariable(S("ARM32"),     One);
    AddInternalVariable(S("ARM64"),     One);
    #elif CPU_ARM
    AddInternalVariable(S("ARM"),       One);
    AddInternalVariable(S("ARM32"),     One);
    #elif CPU_PPC64
    AddInternalVariable(S("PowerPC"),   One);
    AddInternalVariable(S("PPC"),       One);
    AddInternalVariable(S("PowerPC64"), One);
    AddInternalVariable(S("PPC64"),     One);
    #elif CPU_PPC
    AddInternalVariable(S("PowerPC"),   One);
    AddInternalVariable(S("PowerPC32"), One);
    AddInternalVariable(S("PPC"),       One);
    AddInternalVariable(S("PPC32"),     One);
    #endif

    #if defined(_M_IX86)
    AddInternalVariable(S("iX86"), One);
    #endif
    #if defined(__i386__)
    AddInternalVariable(S("i386"), One);
    #endif
    #if defined(__i486__)
    AddInternalVariable(S("i486"), One);
    #endif
    #if defined(__i586__)
    AddInternalVariable(S("i586"), One);
    #endif
    #if defined(__i686__)
    AddInternalVariable(S("i686"), One);
    #endif

    //#define AddInstruction(Instruction) AddInternalVariable(S("_" #Instruction), CPUInfo.Instruction ? One : Zero)

    // TODO: Speed: change to this method
    /*
    const InternalVariable Pairs[] =
    {
        { .Name = S("_MMX"), .Value = CPUInfo.MMX ? One : Zero },
        { .Name = S("_MMX"), .Value = CPUInfo.MMX ? One : Zero },
        { .Name = S("_MMX"), .Value = CPUInfo.MMX ? One : Zero },
        { .Name = S("_MMX"), .Value = CPUInfo.MMX ? One : Zero },
        { .Name = S("_MMX"), .Value = CPUInfo.MMX ? One : Zero },
    };

    for (u16 i = 0; i < SArray_Capacity(Pairs); i++)
    {
        Array_Add(InternalVariablesDB, Pairs[i]);
    }
    */

    // x86
    if (CPUInfo.x86 || CPUInfo.x64)
    {
        AddInternalVariable(S("_MMX"),             CPUInfo.MMX                ? One : Zero);
        AddInternalVariable(S("_SSE"),             CPUInfo.SSE                ? One : Zero);
        AddInternalVariable(S("_SSE2"),            CPUInfo.SSE2               ? One : Zero);
        AddInternalVariable(S("_SSE3"),            CPUInfo.SSE3               ? One : Zero);
        AddInternalVariable(S("_SSSE3"),           CPUInfo.SSSE3              ? One : Zero);
        AddInternalVariable(S("_SSE4"),            CPUInfo.SSE4               ? One : Zero);
        AddInternalVariable(S("_SSE4.1"),          CPUInfo.SSE41              ? One : Zero);
        AddInternalVariable(S("_SSE4.2"),          CPUInfo.SSE42              ? One : Zero);
        AddInternalVariable(S("_AES"),             CPUInfo.AES                ? One : Zero);
        AddInternalVariable(S("_FMA3"),            CPUInfo.FMA3               ? One : Zero);
        AddInternalVariable(S("_AVX"),             CPUInfo.AVX                ? One : Zero);
        AddInternalVariable(S("_AVX2"),            CPUInfo.AVX2               ? One : Zero);
        AddInternalVariable(S("_F16C"),            CPUInfo.F16C               ? One : Zero);
        AddInternalVariable(S("_BMI1"),            CPUInfo.BMI1               ? One : Zero);
        AddInternalVariable(S("_BMI2"),            CPUInfo.BMI2               ? One : Zero);
        AddInternalVariable(S("_LZCNT"),           CPUInfo.LZCNT              ? One : Zero);
        AddInternalVariable(S("_TZCNT"),           CPUInfo.TZCNT              ? One : Zero);
        AddInternalVariable(S("_ADX"),             CPUInfo.ADX                ? One : Zero);
        AddInternalVariable(S("_MPX"),             CPUInfo.MPX                ? One : Zero);
        AddInternalVariable(S("_SHA"),             CPUInfo.SHA                ? One : Zero);
        AddInternalVariable(S("_RDRAND"),          CPUInfo.RDRAND             ? One : Zero);
        AddInternalVariable(S("_PCLMULQDQ"),       CPUInfo.PCLMULQDQ          ? One : Zero);
        AddInternalVariable(S("_DTES64"),          CPUInfo.DTES64             ? One : Zero);
        AddInternalVariable(S("_MONITOR"),         CPUInfo.MONITOR            ? One : Zero);
        AddInternalVariable(S("_DSCPL"),           CPUInfo.DSCPL              ? One : Zero);
        AddInternalVariable(S("_VMX"),             CPUInfo.VMX                ? One : Zero);
        AddInternalVariable(S("_SMX"),             CPUInfo.SMX                ? One : Zero);
        AddInternalVariable(S("_EIST"),            CPUInfo.EIST               ? One : Zero);
        AddInternalVariable(S("_TM2"),             CPUInfo.TM2                ? One : Zero);
        AddInternalVariable(S("_CNXTID"),          CPUInfo.CNXTID             ? One : Zero);
        AddInternalVariable(S("_SDBG"),            CPUInfo.SDBG               ? One : Zero);
        AddInternalVariable(S("_CX16"),            CPUInfo.CX16               ? One : Zero);
        AddInternalVariable(S("_XTPR"),            CPUInfo.XTPR               ? One : Zero);
        AddInternalVariable(S("_PDCM"),            CPUInfo.PDCM               ? One : Zero);
        AddInternalVariable(S("_PCID"),            CPUInfo.PCID               ? One : Zero);
        AddInternalVariable(S("_DCA"),             CPUInfo.DCA                ? One : Zero);
        AddInternalVariable(S("_X2APIC"),          CPUInfo.X2APIC             ? One : Zero);
        AddInternalVariable(S("_MOVBE"),           CPUInfo.MOVBE              ? One : Zero);
        AddInternalVariable(S("_POPCNT"),          CPUInfo.POPCNT             ? One : Zero);
        AddInternalVariable(S("_TSCDEADLINE"),     CPUInfo.TSCDEADLINE        ? One : Zero);
        AddInternalVariable(S("_XSAVE"),           CPUInfo.XSAVE              ? One : Zero);
        AddInternalVariable(S("_OSXSAVE"),         CPUInfo.OSXSAVE            ? One : Zero);
        AddInternalVariable(S("_HYPERVISOR"),      CPUInfo.HYPERVISOR         ? One : Zero);

        AddInternalVariable(S("_FPU"),             CPUInfo.FPU                ? One : Zero);
        AddInternalVariable(S("_VME"),             CPUInfo.VME                ? One : Zero);
        AddInternalVariable(S("_DE"),              CPUInfo.DE                 ? One : Zero);
        AddInternalVariable(S("_PSE"),             CPUInfo.PSE                ? One : Zero);
        AddInternalVariable(S("_TSC"),             CPUInfo.TSC                ? One : Zero);
        AddInternalVariable(S("_MSR"),             CPUInfo.MSR                ? One : Zero);
        AddInternalVariable(S("_PAE"),             CPUInfo.PAE                ? One : Zero);
        AddInternalVariable(S("_MCE"),             CPUInfo.MCE                ? One : Zero);
        AddInternalVariable(S("_CX8"),             CPUInfo.CX8                ? One : Zero);
        AddInternalVariable(S("_APIC"),            CPUInfo.APIC               ? One : Zero);
        AddInternalVariable(S("_SEP"),             CPUInfo.SEP                ? One : Zero);
        AddInternalVariable(S("_MTRR"),            CPUInfo.MTRR               ? One : Zero);
        AddInternalVariable(S("_PGE"),             CPUInfo.PGE                ? One : Zero);
        AddInternalVariable(S("_MCA"),             CPUInfo.MCA                ? One : Zero);
        AddInternalVariable(S("_CMOV"),            CPUInfo.CMOV               ? One : Zero);
        AddInternalVariable(S("_PAT"),             CPUInfo.PAT                ? One : Zero);
        AddInternalVariable(S("_PSE36"),           CPUInfo.PSE36              ? One : Zero);
        AddInternalVariable(S("_PSN"),             CPUInfo.PSN                ? One : Zero);
        AddInternalVariable(S("_CLFLUSH"),         CPUInfo.CLFLUSH            ? One : Zero);
        AddInternalVariable(S("_DS"),              CPUInfo.DS                 ? One : Zero);
        AddInternalVariable(S("_ACPI"),            CPUInfo.ACPI               ? One : Zero);
        AddInternalVariable(S("_FXSR"),            CPUInfo.FXSR               ? One : Zero);
        AddInternalVariable(S("_SS"),              CPUInfo.SS                 ? One : Zero);
        AddInternalVariable(S("_HTT"),             CPUInfo.HTT                ? One : Zero);
        AddInternalVariable(S("_TM"),              CPUInfo.TM                 ? One : Zero);
        AddInternalVariable(S("_PBE"),             CPUInfo.PBE                ? One : Zero);

        AddInternalVariable(S("_RDSEED"),           CPUInfo.RDSEED            ? One : Zero);
        AddInternalVariable(S("_PREFETCHWT1"),      CPUInfo.PREFETCHWT1       ? One : Zero);
        AddInternalVariable(S("_RDPID"),            CPUInfo.RDPID             ? One : Zero);
        AddInternalVariable(S("_AVX512F"),          CPUInfo.AVX512F           ? One : Zero);
        AddInternalVariable(S("_AVX512DQ"),         CPUInfo.AVX512DQ          ? One : Zero);
        AddInternalVariable(S("_AVX512IFMA"),       CPUInfo.AVX512IFMA        ? One : Zero);
        AddInternalVariable(S("_AVX512PF"),         CPUInfo.AVX512PF          ? One : Zero);
        AddInternalVariable(S("_AVX512ER"),         CPUInfo.AVX512ER          ? One : Zero);
        AddInternalVariable(S("_AVX512CD"),         CPUInfo.AVX512CD          ? One : Zero);
        AddInternalVariable(S("_AVX512BW"),         CPUInfo.AVX512BW          ? One : Zero);
        AddInternalVariable(S("_AVX512VL"),         CPUInfo.AVX512VL          ? One : Zero);
        AddInternalVariable(S("_AVX512"),           CPUInfo.AVX512            ? One : Zero);
        AddInternalVariable(S("_AVX512VBMI"),       CPUInfo.AVX512VBMI        ? One : Zero);
        AddInternalVariable(S("_AVX512VBMI2"),      CPUInfo.AVX512VBMI2       ? One : Zero);
        AddInternalVariable(S("_AVX512VPCLMUL"),    CPUInfo.AVX512VPCLMUL     ? One : Zero);
        AddInternalVariable(S("_AVX512VNNI"),       CPUInfo.AVX512VNNI        ? One : Zero);
        AddInternalVariable(S("_AVX512BITALG"),     CPUInfo.AVX512BITALG      ? One : Zero);
        AddInternalVariable(S("_AVX512VPOPCNTDQ"),  CPUInfo.AVX512VPOPCNTDQ   ? One : Zero);
        AddInternalVariable(S("_AVX5124VNNIW"),     CPUInfo.AVX512VNNI        ? One : Zero);
        AddInternalVariable(S("_AVX5124FMAPS"),     CPUInfo.AVX5124FMAPS      ? One : Zero);
        AddInternalVariable(S("_AVX512BF16"),       CPUInfo.AVX512BF16        ? One : Zero);
        AddInternalVariable(S("_AVX512FP16"),       CPUInfo.AVX512FP16        ? One : Zero);
        AddInternalVariable(S("_GFNI"),             CPUInfo.GFNI              ? One : Zero);
        AddInternalVariable(S("_VAES"),             CPUInfo.VAES              ? One : Zero);
        AddInternalVariable(S("_FSGSBASE"),         CPUInfo.FSGSBASE          ? One : Zero);
        AddInternalVariable(S("_TSCADJUST"),        CPUInfo.TSCADJUST         ? One : Zero);
        AddInternalVariable(S("_SGX"),              CPUInfo.SGX               ? One : Zero);
        AddInternalVariable(S("_HLE"),              CPUInfo.HLE               ? One : Zero);
        AddInternalVariable(S("_FDP_EXCEPTN_ONLY"), CPUInfo.FDP_EXCEPTN_ONLY  ? One : Zero);
        AddInternalVariable(S("_SMEP"),             CPUInfo.SMEP              ? One : Zero);
        AddInternalVariable(S("_ERMS"),             CPUInfo.ERMS              ? One : Zero);
        AddInternalVariable(S("_INVPCID"),          CPUInfo.INVPCID           ? One : Zero);
        AddInternalVariable(S("_RTM"),              CPUInfo.RTM               ? One : Zero);
        AddInternalVariable(S("_PQM"),              CPUInfo.PQM               ? One : Zero);
        AddInternalVariable(S("_FPU_DEPR"),         CPUInfo.FPU_DEPR          ? One : Zero);
        AddInternalVariable(S("_PQE"),              CPUInfo.PQE               ? One : Zero);
        AddInternalVariable(S("_SMAP"),             CPUInfo.SMAP              ? One : Zero);
        AddInternalVariable(S("_PCOMMIT"),          CPUInfo.PCOMMIT           ? One : Zero);
        AddInternalVariable(S("_CLFLUSHOPT"),       CPUInfo.CLFLUSHOPT        ? One : Zero);
        AddInternalVariable(S("_CLWB"),             CPUInfo.CLWB              ? One : Zero);
        AddInternalVariable(S("_INTELPT"),          CPUInfo.INTELPT           ? One : Zero);
        AddInternalVariable(S("_UMIP"),             CPUInfo.UMIP              ? One : Zero);
        AddInternalVariable(S("_PKU"),              CPUInfo.PKU               ? One : Zero);
        AddInternalVariable(S("_OSPKE"),            CPUInfo.OSPKE             ? One : Zero);
        AddInternalVariable(S("_WAITPKG"),          CPUInfo.WAITPKG           ? One : Zero);
        AddInternalVariable(S("_CET_SS"),           CPUInfo.CET_SS            ? One : Zero);
        AddInternalVariable(S("_VPCLMULQDQ"),       CPUInfo.VPCLMULQDQ        ? One : Zero);
        AddInternalVariable(S("_TME"),              CPUInfo.TME               ? One : Zero);
        AddInternalVariable(S("_LA57"),             CPUInfo.LA57              ? One : Zero);
        AddInternalVariable(S("_KL"),               CPUInfo.KL                ? One : Zero);
        AddInternalVariable(S("_CLDEMOTE"),         CPUInfo.CLDEMOTE          ? One : Zero);
        AddInternalVariable(S("_MOVDIRI"),          CPUInfo.MOVDIRI           ? One : Zero);
        AddInternalVariable(S("_MOVDIR64B"),        CPUInfo.MOVDIR64B         ? One : Zero);
        AddInternalVariable(S("_ENQCMD"),           CPUInfo.ENQCMD            ? One : Zero);
        AddInternalVariable(S("_SGXLC"),            CPUInfo.SGXLC             ? One : Zero);
        AddInternalVariable(S("_BUSLOCKDETECT"),    CPUInfo.BUSLOCKDETECT     ? One : Zero);
    }

    // Arm
    if (CPUInfo.ARM || CPUInfo.ARM64)
    {
        AddInternalVariable(S("_NEON"),             CPUInfo.NEON              ? One : Zero);
        AddInternalVariable(S("_NEON_HPFP"),        CPUInfo.NEON_HPFP         ? One : Zero);
        AddInternalVariable(S("_NEON_FP16"),        CPUInfo.NEON_FP16         ? One : Zero);
        AddInternalVariable(S("_ARMV8_1_ATOMICS"),  CPUInfo.ARMV8_1_ATOMICS   ? One : Zero);
        AddInternalVariable(S("_ARMV8_2_FHM"),      CPUInfo.ARMV8_2_FHM       ? One : Zero);
        AddInternalVariable(S("_ARMV8_2_SHA512"),   CPUInfo.ARMV8_2_SHA512    ? One : Zero);
        AddInternalVariable(S("_ARMV8_2_SHA3"),     CPUInfo.ARMV8_2_SHA3      ? One : Zero);
        AddInternalVariable(S("_ARMV8_3_COMPNUM"),  CPUInfo.ARMV8_3_COMPNUM   ? One : Zero);
        AddInternalVariable(S("_ARMV8_CRC32"),      CPUInfo.ARMV8_CRC32       ? One : Zero);
        AddInternalVariable(S("_ARMV8_GPI"),        CPUInfo.ARMV8_GPI         ? One : Zero);
        AddInternalVariable(S("_AdvSIMD"),          CPUInfo.AdvSIMD           ? One : Zero);
        AddInternalVariable(S("_AdvSIMD_HPFPCvt"),  CPUInfo.AdvSIMD_HPFPCVT   ? One : Zero);
        AddInternalVariable(S("_UCNORMAL_MEM"),     CPUInfo.UCNORMAL_MEM      ? One : Zero);
        AddInternalVariable(S("_M1"),               CPUInfo.FLAGM             ? One : Zero);
        AddInternalVariable(S("_M2"),               CPUInfo.FLAGM2            ? One : Zero);
        AddInternalVariable(S("_M3"),               CPUInfo.FLAGM3            ? One : Zero);
        AddInternalVariable(S("_M4"),               CPUInfo.FLAGM4            ? One : Zero);
        AddInternalVariable(S("_FHM"),              CPUInfo.FHM               ? One : Zero);
        AddInternalVariable(S("_DOTPROD"),          CPUInfo.DOTPROD           ? One : Zero);
        AddInternalVariable(S("_SHA3"),             CPUInfo.SHA3              ? One : Zero);
        AddInternalVariable(S("_RDM"),              CPUInfo.RDM               ? One : Zero);
        AddInternalVariable(S("_LSE"),              CPUInfo.LSE               ? One : Zero);
        AddInternalVariable(S("_SHA256"),           CPUInfo.SHA256            ? One : Zero);
        AddInternalVariable(S("_SHA512"),           CPUInfo.SHA512            ? One : Zero);
        AddInternalVariable(S("_SHA1"),             CPUInfo.SHA1              ? One : Zero);
        AddInternalVariable(S("_AES"),              CPUInfo.AES               ? One : Zero);
        AddInternalVariable(S("_PMULL"),            CPUInfo.PMULL             ? One : Zero);
        AddInternalVariable(S("_SPECRES"),          CPUInfo.SPECRES           ? One : Zero);
        AddInternalVariable(S("_SB"),               CPUInfo.SB                ? One : Zero);
        AddInternalVariable(S("_FRINTTS"),          CPUInfo.FRINTTS           ? One : Zero);
        AddInternalVariable(S("_LRCPC"),            CPUInfo.LRCPC             ? One : Zero);
        AddInternalVariable(S("_LRCPC2"),           CPUInfo.LRCPC2            ? One : Zero);
        AddInternalVariable(S("_FCMA"),             CPUInfo.FCMA              ? One : Zero);
        AddInternalVariable(S("_JSCVT"),            CPUInfo.JSCVT             ? One : Zero);
        AddInternalVariable(S("_PAUTH"),            CPUInfo.PAUTH             ? One : Zero);
        AddInternalVariable(S("_PAUTH2"),           CPUInfo.PAUTH2            ? One : Zero);
        AddInternalVariable(S("_FPAC"),             CPUInfo.FPAC              ? One : Zero);
        AddInternalVariable(S("_DPB"),              CPUInfo.DPB               ? One : Zero);
        AddInternalVariable(S("_DPB2"),             CPUInfo.DPB2              ? One : Zero);
        AddInternalVariable(S("_BF16"),             CPUInfo.BF16              ? One : Zero);
        AddInternalVariable(S("_I8MM"),             CPUInfo.I8MM              ? One : Zero);
        AddInternalVariable(S("_ECV"),              CPUInfo.ECV               ? One : Zero);
        AddInternalVariable(S("_LSE2"),             CPUInfo.LSE2              ? One : Zero);
        AddInternalVariable(S("_CSV2"),             CPUInfo.CSV2              ? One : Zero);
        AddInternalVariable(S("_CSV3"),             CPUInfo.CSV3              ? One : Zero);
        AddInternalVariable(S("_DIT"),              CPUInfo.DIT               ? One : Zero);
        AddInternalVariable(S("_FP16"),             CPUInfo.FP16              ? One : Zero);
        AddInternalVariable(S("_SSBS"),             CPUInfo.SSBS              ? One : Zero);
        AddInternalVariable(S("_BTI"),              CPUInfo.BTI               ? One : Zero);
    }

    // todo: store all supported extensions
    //AddInternalVariable(S("_CPUExtensions"), String_Null());

    StringLocal(CacheLineSize, 8);
    String_Format(&CacheLineSize, S("%u"), Platform_GetCpuCacheLineSize());
    AddInternalVariable(S("_CacheLineSize"), String_Create(Arena, CacheLineSize));

    // todo: check if these are on bsd as well. maybe linux?
    #if PLATFORM_APPLE
    /*
    hw.ncpu: 8
    hw.byteorder: 1234
    hw.memsize: 8589934592
    hw.activecpu: 8
    hw.perflevel0.physicalcpu: 4
    hw.perflevel0.physicalcpu_max: 4
    hw.perflevel0.logicalcpu: 4
    hw.perflevel0.logicalcpu_max: 4
    hw.perflevel0.l1icachesize: 196608
    hw.perflevel0.l1dcachesize: 131072
    hw.perflevel0.l2cachesize: 16777216
    hw.perflevel0.cpusperl2: 4
    hw.perflevel0.name: Performance
    hw.perflevel1.physicalcpu: 4
    hw.perflevel1.physicalcpu_max: 4
    hw.perflevel1.logicalcpu: 4
    hw.perflevel1.logicalcpu_max: 4
    hw.perflevel1.l1icachesize: 131072
    hw.perflevel1.l1dcachesize: 65536
    hw.perflevel1.l2cachesize: 4194304
    hw.perflevel1.cpusperl2: 4
    hw.perflevel1.name: Efficiency
    hw.optional.arm.FEAT_FlagM: 1
    hw.optional.arm.FEAT_FlagM2: 1
    hw.optional.arm.FEAT_FHM: 1
    hw.optional.arm.FEAT_DotProd: 1
    hw.optional.arm.FEAT_SHA3: 1
    hw.optional.arm.FEAT_RDM: 1
    hw.optional.arm.FEAT_LSE: 1
    hw.optional.arm.FEAT_SHA256: 1
    hw.optional.arm.FEAT_SHA512: 1
    hw.optional.arm.FEAT_SHA1: 1
    hw.optional.arm.FEAT_AES: 1
    hw.optional.arm.FEAT_PMULL: 1
    hw.optional.arm.FEAT_SPECRES: 0
    hw.optional.arm.FEAT_SB: 1
    hw.optional.arm.FEAT_FRINTTS: 1
    hw.optional.arm.FEAT_LRCPC: 1
    hw.optional.arm.FEAT_LRCPC2: 1
    hw.optional.arm.FEAT_FCMA: 1
    hw.optional.arm.FEAT_JSCVT: 1
    hw.optional.arm.FEAT_PAuth: 1
    hw.optional.arm.FEAT_PAuth2: 1
    hw.optional.arm.FEAT_FPAC: 1
    hw.optional.arm.FEAT_DPB: 1
    hw.optional.arm.FEAT_DPB2: 1
    hw.optional.arm.FEAT_BF16: 1
    hw.optional.arm.FEAT_I8MM: 1
    hw.optional.arm.FEAT_ECV: 1
    hw.optional.arm.FEAT_LSE2: 1
    hw.optional.arm.FEAT_CSV2: 1
    hw.optional.arm.FEAT_CSV3: 1
    hw.optional.arm.FEAT_DIT: 1
    hw.optional.arm.FEAT_FP16: 1
    hw.optional.arm.FEAT_SSBS: 1
    hw.optional.arm.FEAT_BTI: 1
    hw.optional.arm.FP_SyncExceptions: 1
    hw.optional.floatingpoint: 1
    hw.optional.neon: 1
    hw.optional.neon_hpfp: 1
    hw.optional.neon_fp16: 1
    hw.optional.armv8_1_atomics: 1
    hw.optional.armv8_2_fhm: 1
    hw.optional.armv8_2_sha512: 1
    hw.optional.armv8_2_sha3: 1
    hw.optional.armv8_3_compnum: 1
    hw.optional.watchpoint: 4
    hw.optional.breakpoint: 6
    hw.optional.armv8_crc32: 1
    hw.optional.armv8_gpi: 1
    hw.optional.AdvSIMD: 1
    hw.optional.AdvSIMD_HPFPCvt: 1
    hw.optional.ucnormal_mem: 1
    hw.optional.arm64: 1
    hw.features.allows_security_research: 0
    hw.physicalcpu: 8
    hw.physicalcpu_max: 8
    hw.logicalcpu: 8
    hw.logicalcpu_max: 8
    hw.cputype: 16777228
    hw.cpusubtype: 2
    hw.cpu64bit_capable: 1
    hw.cpufamily: -634136515
    hw.cpusubfamily: 2
    hw.cacheconfig: 8 1 4 0 0 0 0 0 0 0
    hw.cachesize: 3697426432 65536 4194304 0 0 0 0 0 0 0
    hw.pagesize: 16384
    hw.pagesize32: 16384
    hw.cachelinesize: 128
    hw.l1icachesize: 131072
    hw.l1dcachesize: 65536
    hw.l2cachesize: 4194304
    hw.tbfrequency: 24000000
    hw.memsize_usable: 7992393728
    hw.packages: 1
    hw.osenvironment: 
    hw.ephemeral_storage: 0
    hw.use_recovery_securityd: 0
    hw.use_kernelmanagerd: 1
    hw.serialdebugmode: 0
    hw.nperflevels: 2
    hw.targettype: J413
    machdep.cpu.cores_per_package: 8
    machdep.cpu.core_count: 8
    machdep.cpu.logical_per_package: 8
    machdep.cpu.thread_count: 8
    machdep.cpu.brand_string: Apple M2
    machdep.user_idle_level: 0
    machdep.wake_abstime: 11797809078509
    machdep.time_since_reset: 44172175964
    machdep.wake_conttime: 201324628
    machdep.deferred_ipi_timeout: 64000
    machdep.virtual_address_size: 47
    machdep.report_phy_read_delay: 0
    machdep.report_phy_write_delay: 0
    machdep.trace_phy_read_delay: 0
    machdep.trace_phy_write_delay: 0
    machdep.phy_read_delay_panic: 0
    machdep.phy_write_delay_panic: 0
    */
    #endif

    StringLocal(AccountName, 256);
    String Allocated;
    if (Platform_GetAccountName(&AccountName))
    {
        Allocated = String_Create(Arena, AccountName);
        AddInternalVariable(S("_Account"), Allocated);
        AddInternalVariable(S("_AccountName"), Allocated);
    }

    StringLocal(UserName, 256);
    if (Platform_GetUserName(&UserName))
    {
        Allocated = String_Create(Arena, UserName);
        AddInternalVariable(S("_User"), Allocated);
        AddInternalVariable(S("_UserName"), Allocated);
    }

    StringLocal(HomeDirectory, MAX_PATH_LENGTH);
    Platform_GetHomeDirectory(&HomeDirectory);
    xx String_EatPathSeparatorsInlineFromEnd(&HomeDirectory);
    String AllocatedHome = String_Create(Arena, HomeDirectory);
    AddInternalVariable(S("_HomeDirectory"), AllocatedHome);
    AddInternalVariable(S("_Home"),          AllocatedHome);

    StringLocal(UserDirectory, MAX_PATH_LENGTH);
    if (Platform_GetUserDirectory(&UserDirectory))
    {
        xx String_EatPathSeparatorsInlineFromEnd(&UserDirectory);
        Allocated = String_Create(Arena, UserDirectory);
        AddInternalVariable(S("_UserDirectory"), Allocated);
    }
    else
    {
        AddInternalVariable(S("_UserDirectory"), AllocatedHome);
    }

    StringLocal(HostName, 256);
    Platform_GetComputerName(&HostName);
    Allocated = String_Create(Arena, HostName);
    AddInternalVariable(S("_Host"), Allocated);
    AddInternalVariable(S("_HostName"), Allocated);
    
    StringLocal(ComputerName, 256);
    Platform_GetFriendlyComputerName(&ComputerName);
    Allocated = String_Create(Arena, ComputerName);
    AddInternalVariable(S("_ComputerName"), Allocated);

    FileVariable_Empty.Name = String_Null();
    FileVariable_Empty.Value = String_Null();
    FileVariable_Empty.Params = String_Null();
    // FileVariable_Empty.bHasSpecial = false;
}

u32 RunApplication(const StringArray Arguments)
{
    ENSURE_NO_REENTRY();

    Logging_ToggleLogFile(false);
    Logging_ToggleLogTimeStamp(false);
    Logging_ToggleLogCategory(false);

    // todo: clean, rebuild, verbose
    bNoWordWrapLogging = StringArray_Contains(Arguments, S("-nowordwrap"), false);
    bQuietBuild        = StringArray_Contains(Arguments, S("-q"), false);

    if (bQuietBuild)
    {
        Logging_Disable();
        Logging_ToggleEnableOnError(true);
    }

    const bool bOutputToLog = StringArray_Contains(Arguments, S("-l"), false);
    Logging_ToggleLogFile(bOutputToLog);

    StringLocal(ExtraFlags, 128);
    #ifdef RIFT_DEBUG
    String_BuildSeparator(&ExtraFlags, ' ', S("[DEBUG]"));
    #endif
    #ifdef RIFT_ASAN
    String_BuildSeparator(&ExtraFlags, ' ', S("[ASAN]"));
    #endif
    #ifdef HOOD
    String_BuildSeparator(&ExtraFlags, ' ', S("- (HOOD EDITION)"));
    #endif

    const PlatformVersion OSVersion = Platform_GetVersion();

    LOG("\nRift Build System v%S (%S %u.%u.%u %S) %S\n", S(RIFTBUILD_VERSION_STRING), S(PLATFORM_STRING), OSVersion.Major, OSVersion.Minor, OSVersion.Patch, S(CPU_ARCHITECTURE_STRING), ExtraFlags);

    #ifdef HOOD
    LOG("\nwasssup ma nigga. les get build'n cuh...\n");
    #endif

    StringLocal(WorkingDirectory, MAX_PATH_LENGTH);
    Platform_GetWorkingDirectory(&WorkingDirectory);

    LinearAllocator ProgramArena = {0};
    i8 ProgramMemory[Mebibytes(1)] = {0};
    LinearAllocator_Create(Mebibytes(1), ProgramMemory, &ProgramArena);

    InitInternalVars(&ProgramArena);

    bWasVCVarsBatchExecuted = Platform_DoesEnvironmentVariableExist(S("VSCMD_ARG_TGT_ARCH"));

    u32 ExitCode = RiftBuild(&ProgramArena, Arguments, WorkingDirectory);

    #if !PLATFORM_MAC
    const bool bLaunchedFromDesktop = StringArray_Contains(Arguments, S("--from-desktop"), false);
    if (Platform_GetConsoleProcessCount() == 1 || bLaunchedFromDesktop)
    {
        #ifndef HOOD
        // LOG_INLINE_WARNING("\nLaunched outside an existing terminal, suspending until user exit.\nPress any key to exit ... ");
        // LOG_INLINE_WARNING("\nLaunched outside an existing terminal, waiting until you press any key to exit ... ");
        // LOG_INLINE_WARNING("\nLaunched outside an existing terminal. Press any key to exit ... ");
        LOG_INLINE_WARNING("\nLaunched outside an existing terminal. Waiting for any key press to exit ... ");
        #else
        LOG_INLINE_WARNING("\nyo i noticed we aren't in a terminal. press any key to shoot me nigga ... ");
        #endif

        Platform_BeginNonBlockingMode();
        while (true)
        {
            Platform_Wait(10 milliseconds);
            if (Platform_IsWindowFocused() && Platform_AnyKeyPressed())
            {
                break;
            }
        }
        Platform_EndNonBlockingMode();
    }
    #endif

    #if !PLATFORM_WINDOWS
    LOG_LINE_BREAK();
    #endif

    return ExitCode;
}
