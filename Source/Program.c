// Copyright (c) 2024 Ali El Saleh

#include "EntryPoint.h"

usize GEngineMemoryAmount  = Kibibytes(128);
usize GEngineScratchAmount = 0;

#ifndef UNITY_BUILD
#include "Backend.h"
#include "Allocators.h"
#include "Clock.h"
#include "StringUtils.h"
#include "Array.h"
#include "Globals.h"
#include "Uuid.h"
#endif

#if PLATFORM_WINDOWS
#include "microsoft_craziness.h"
#endif

TArray(InternalVariable) InternalVariablesDB = NULL;
bool bQuietBuild = false;
bool bNoWordWrapLogging = false;
bool bSingleThread = false;
bool bIsRebuild = false;
bool bIsClean = false;
bool bVerboseLog = false;
bool bHelp = false;

STRUCT(BuildFileDirectoryIteratorData)
{
    bool bFoundBuildFile;
    bool bNoBuildFileSpecifiedInCmd;
    bool bSearchOnlyBuildBatch;
    i8 BuildFileIndex;
    i8 RootPathIndex;
    u8 NumBuildFilesFound;
    String* Name;
    String* Path;
    StringArray Arguments;
};

internal bool IsBuildFile(const String FilePath)
{
    return String_EndsWith(FilePath, S(".build"), false);
}

internal bool IsBuildBatchFile(const String FilePath)
{
    return String_EndsWith(FilePath, S(".buildbatch"), false);
}

String GetCmdOptionValue(TArray(CmdOption) CmdOptionsDB, const String Name)
{
    for each (CmdOption, o, CmdOptionsDB)
    {
        if (String_IsEqual(o.Name, Name, false))
        {
            return o.Value;
        }
    }

    return String_Null();
}

bool DoesBuildVarExist(TArray(FileVariable) VariablesDB, const String Name)
{
    for each (FileVariable, Var, VariablesDB)
    {
        if (String_IsEqual(Var.Name, Name, false))
        {
            return true;
        }
    }

    return false;
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

String GetVariableValue(TArray(FileVariable) Variables, const String Name)
{
    for each (FileVariable, Var, Variables)
    {
        if (String_IsEqual(Var.Name, Name, false))
        {
            return Var.Value;
        }
    }

    return String_Null();
}

String* GetVariableValue_Ref(TArray(FileVariable) Variables, const String Name)
{
    for each (FileVariable, Var, Variables)
    {
        if (String_IsEqual(Var.Name, Name, false))
        {
            return &Var_->Value;
        }
    }

    return NULL;
}

internal void PrefixVariables(String* Dest, String VariableValue, const String Prefix, bool bWrapWithQuotes)
{
    bool bInsideQuote = false;
    bool bSawSpace = false;

    if (VariableValue.Length > 0)
    {
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

        if (bWrapWithQuotes && VariableValue.Data[0] != '"')
        {
            String_AppendChar(Dest, '"');
        }
    }

    for (u32 i = 0; i < VariableValue.Length; i++)
    {
        char C = VariableValue.Data[i];

        // ignore trailing space
        if (C ==' ' && i == VariableValue.Length-1)
            continue;

        if (C == ' ')
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

                    if (bWrapWithQuotes && C != '"')
                    {
                        String_AppendChar(Dest, '"');
                    }
                }
            }
        }

        if (C == '"')
        {
            bInsideQuote = !bInsideQuote;
        }

        if (C == ' ')
        {
            if (bWrapWithQuotes)
                String_AppendChar(Dest, '"');
        }
        
        String_AppendChar(Dest, C);
    }

    if (Dest->Length > 0)
    {
        String_EatSpacesInlineFromEnd(Dest);

        if (bWrapWithQuotes)// && !String_IsLast(*Dest, '"'))
            String_AppendChar(Dest, '"');
    }
}

internal void SuffixVariables(String* Dest, String VariableValue, const String Suffix)
{
    for (u32 i = 0; i < VariableValue.Length; i++)
    {
        char C = VariableValue.Data[i];

        bool bSawSpace;
        if (C == ' ')
        {
            bSawSpace = true;
        }
        else
        {
            bSawSpace = false;
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
bool ExtensionHas(LinearAllocator Scratch, const String ExtensionString, const String Ext)
{
    StringArray Options = String_ParseIntoArray(&Scratch, ExtensionString, ' ', 0, 8);

    for each_str (s, Options)
    {
        String e = String_EatChar(*s, '.');

        if (String_IsEqual(e, Ext, false))
        {
            return true;
        }
    }

    return false;
}
*/

internal bool VariableHasSpecial(TArray(FileVariable) VariablesDB, const String Name)
{
    for each (FileVariable, Var, VariablesDB)
    {
        if (String_IsEqual(Var.Name, Name, false))
        {
            return Var.bHasSpecial;
        }
    }

    return false;
}

bool LogCustomErrorMessage(TArray(FileVariable) VariablesDB, const String Context, const String Key, const bool bLineBreak)
{
    if (bQuietBuild) Logging_Enable();

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
            
            char ScratchMemory[64] = {0};
            LinearAllocator Scratch = {0};
            LinearAllocator_Create(64, ScratchMemory, &Scratch);
            StringArray Keys = String_ParseIntoArray(&Scratch, Slice, '|', 0, 8);
            for each_str (k, Keys)
            {
                if (String_IsEqual(*k, Key, false) &&
                   (Context.Length == 0 || String_StartsWith(Var.Name, Context, false)))
                {
                    if (bLineBreak) LOG_LINE_BREAK();

                    LOG("%S", Var.Value);
                    bLogged = true;
                    break;
                }
            }

            if (bLogged)
                break;
        }
    }

    if (bQuietBuild) Logging_Disable();

    return bLogged;
}

internal bool IconFileDirectoryIterator(const String FullPath, const String RelativePath, const String FileName, u64 FileSize, bool bIsDirectory, void* UserData)
{
    if (FileSize > 0)
    {
        struct Data
        {
            TArray(FileVariable) ExpandedVarsArray;
            String* IconFilePath;
            bool bSuccess;
        };

        struct Data* D = UserData;

        const String IconName = GetVariableValue(D->ExpandedVarsArray, S("Icon"));

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
            const String IconExtensions[] = 
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

internal bool SourceFileCounterDirectoryIterator(const String FullPath, const String RelativePath, const String FileName, u64 FileSize, bool bIsDirectory, void* UserData)
{
    if (FileSize > 0)
    {
        if (String_StartsWith(FileName, S("__"), false))
        {
            return true;
        }

        struct SourceCountData
        {
            u32 NumSources;
            u32 NumAsmSources;
            u32 NumHeaders;
            u32 NumRcSources;
            String* FirstSourceFileName;
            String WorkingDirectory;
            String SourceDirectory;
            StringList WhitelistArray;
            StringList BlacklistArray;
            StringList WhitelistDirArray;
            StringList BlacklistDirArray;
            bool bHasCppFiles;
            bool bIsPCHBuild;
        };

        // TODO: ignore intermediate and build directories

        struct SourceCountData* Data = UserData;

        u32 DotIndex = 0;
        String_IndexOfLastChar(FileName, '.', &DotIndex);

        const String Extension = StrShiftF(FileName, DotIndex);

        if (IsSource(Extension))
        {
            // we will build this later
            if (String_IsEqual(Extension, S(".rc"), false))
            {
                bool bIgnore = String_IsEqual(FileName, S("icon.rc"), false);

                if (!bIgnore)
                    Data->NumRcSources++;

                return true;
            }

            if (FilterSourceFile(Data->WorkingDirectory, Data->SourceDirectory, FullPath, RelativePath, Data->WhitelistArray, Data->BlacklistArray, Data->WhitelistDirArray, Data->BlacklistDirArray))
            {
                Data->NumSources++;

                if (Data->FirstSourceFileName->Length == 0)
                {
                    String_Copy(Data->FirstSourceFileName, FileName);
                }

                if (String_IsEqual(Extension, S(".asm"), false))
                {
                    Data->NumAsmSources++;
                }

                if (!Data->bHasCppFiles)
                {
                    if (String_EndsWith(RelativePath, S(".cpp"), false) ||
                        String_EndsWith(RelativePath, S(".c++"), false) ||
                        String_EndsWith(RelativePath, S(".cc"), false) ||
                        String_EndsWith(RelativePath, S(".cxx"), false) ||
                        String_EndsWith(RelativePath, S(".hpp"), false) ||
                        String_EndsWith(RelativePath, S(".h++"), false) ||
                        String_EndsWith(RelativePath, S(".hh"), false) ||
                        String_EndsWith(RelativePath, S(".hxx"), false))
                    {
                        Data->bHasCppFiles = true;
                    }
                }
            }
        }
        else if (IsHeader(Extension))
        {
            if (FilterSourceFile(Data->WorkingDirectory, Data->SourceDirectory, FullPath, RelativePath, Data->WhitelistArray, Data->BlacklistArray, Data->WhitelistDirArray, Data->BlacklistDirArray))
            {
                Data->NumHeaders++;

                if (!Data->bHasCppFiles)
                {
                    // is cpp header?
                    if (String_EndsWith(RelativePath, S(".hh"), false) ||
                        String_EndsWith(RelativePath, S(".hpp"), false) ||
                        String_EndsWith(RelativePath, S(".hxx"), false) ||
                        String_EndsWith(RelativePath, S(".h++"), false))
                    {
                        Data->bHasCppFiles = true;
                    }
                }
                
                if (Data->bIsPCHBuild)
                {
                    Data->NumSources++;
                    String_Copy(Data->FirstSourceFileName, FileName);
                    return false;
                }
            }
        }
    }

    return true;
}

internal bool HeaderFileRebuildCheckDirectoryIterator(const String FullPath, const String RelativePath, const String FileName, u64 FileSize, bool bIsDirectory, void* UserData)
{
    if (FileSize > 0)
    {
        u32 DotIndex = 0;
        String_IndexOfLastChar(FileName, '.', &DotIndex);

        const String Extension = StrShiftF(FileName, DotIndex);

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

internal bool BuildFileDirectoryIterator_Args(const String FullPath, const String RelativePath, const String FileName, u64 FileSize, bool bIsDirectory, void* UserData)
{
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
                String_Copy(Data->Path, RelativePath);

                return false;
            }
        }
    }

    return true;
}

/*
internal bool LibraryDirectoryIterator(const String FullPath, const String RelativePath, const String FileName, u64 FileSize, bool bIsDirectory, void* UserData)
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

internal bool MultipleBuildFileDirectoryIterator(const String FullPath, const String RelativePath, const String FileName, u64 FileSize, bool bIsDirectory, void* UserData)
{
    if (IsBuildFile(FileName))
    {
        static u8 i = 0;
        LOG("    [%hhu] %S", i, RelativePath);
        i++;
    }

    return true;
}

internal bool BuildFileDirectoryIterator(const String FullPath, const String RelativePath, const String FileName, u64 FileSize, bool bIsDirectory, void* UserData)
{
    if (FileSize > 0)
    {
        BuildFileDirectoryIteratorData* Data = UserData;

        if ((IsBuildFile(FileName) && !Data->bSearchOnlyBuildBatch) || (Data->bSearchOnlyBuildBatch && IsBuildBatchFile(FileName)))
        {
            if (NEVER(Data->Name == NULL       || Data->Path == NULL)) return false;
            if (NEVER(Data->Name->Data == NULL || Data->Path->Data == NULL)) return false;

            if (String_StartsWith(FileName, S("__"), false))
            {
                if (Data->bNoBuildFileSpecifiedInCmd) // maybe people wanna explicity specify the build file if they type it in the command line, so dont ignore it
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
                    String_Copy(Data->Path, RelativePath);
                }

                Data->NumBuildFilesFound++;

                if (!Data->bNoBuildFileSpecifiedInCmd)
                    return false;
            }
        }
    }

    return true;
}

internal bool PathFlagDirectoryIterator(const String FullPath, const String RelativePath, const String FileName, u64 FileSize, bool bIsDirectory, void* UserData)
{
    if (NEVER(UserData == NULL)) return false;

    if (bIsDirectory)
    {
        STRUCT(PathIterData)
        {
            String BaseDirectory;
            String* Flags;
        };

        PathIterData* Data = UserData;

        String_Append(Data->Flags, Data->BaseDirectory);
        String_AppendPathSeparator(Data->Flags);
        String_Append(Data->Flags, RelativePath);
        String_AppendSpace(Data->Flags);
    }

    return true;
}

internal bool EnforceCopyright(CompileData* Data, const String FullPath, const String RelativePath)
{
    struct { bool bSuccess; String Content; } * AuxData = Data->AdditionalData;

    FileHandle f = FileHandle_Null();
    Filesystem_Open(FullPath, FileMode_Read, &f);
    StringLocal(Line, 4096);
    Filesystem_ReadLine(f, &Line);
    if (!String_Contains(Line, AuxData->Content, false))
    {
        AuxData->bSuccess = false;
        Filesystem_Close(&f);
        LOG_ERROR("Source file \"%S\" does not contain the required copyright notice on the first line", RelativePath);
        LOG("\n    This is the missing notice string -> %S", AuxData->Content);
        return false;
    }
    Filesystem_Close(&f);

    return true;
}

bool LogStringList_WordWrapped(LinearAllocator Scratch, const String Name, const StringList List)
{
    StringList History = {0};
    u32 ParentCount = 0;
    bool bLogged = false;

    for each_str_list (List)
    {
        StringList ValueList = String_SplitIntoList(&Scratch, It.String, ' ', true);
        u32 Count = 0;
        u32 Spaces = 0;
        u32 Index = 0;

        u32 Num = 0;
        for each_str_list_it (_, ValueList) Num++;

        for each_str_list_it (v, ValueList)
        {
            u32 Rows = 0, Cols = 0;
            Platform_GetTerminalDimensions(&Rows, &Cols);
            Cols = Clamp(Cols, 30, 1000);
            if (v.String.Length + ParentCount + Spaces > (u32)((f32)Cols/1.5f))
            {
                StringList** Next = &History.Next;
                while (*Next)
                {
                    const String Slice = String_EatSpacesFromEnd((*Next)->String);
                    LOG_INLINE("%S ", Slice);

                    Next = &(*Next)->Next;
                }

                History = (StringList){0};

                LOG_LINE_BREAK();

                String NameCopy = String_Reserve(&Scratch, Name.Length);
                String_Copy(&NameCopy, Name);
                NameCopy.Length = Name.Length;
                for (u32 i = 0; i < Name.Length; i++)
                {
                    NameCopy.Data[i] = ' ';
                }

                // i could put this in that loop, but logging is relatively slowww, so just do it once
                LOG_INLINE("%S", NameCopy);

                Count = 0;
                ParentCount = 0;
                Spaces = 0;
            }

            Count += v.String.Length;
            ParentCount += v.String.Length;
            if (Index != Num-1)
                Spaces++; // for the spaces in between
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
                LOG_INLINE("%S ", Slice);

                Next = &(*Next)->Next;
            }

            History = (StringList){0};
        }

        bLogged = true;
    }

    return bLogged;
}

internal void LogNameValuePair(LinearAllocator Scratch, const String Name, const String Value, const bool bWordWrap)
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

internal void LogBuildVariable(LinearAllocator Scratch, TArray(FileVariable) VariablesDB, const String Name, const String DisplayName, const bool bWordWrap)
{
    LOG_INLINE("%S", DisplayName);

    StringList List = GetVariableValueList(&Scratch, VariablesDB, Name);

    if (bWordWrap)
    {
        LogStringList_WordWrapped(Scratch, DisplayName, List);
    }
    else
    {
        for each_str_list (List)
        {
            if (String_IsValid(It.String))
            {
                LOG_INLINE("%S", It.String);
            }
        }
    }

    LOG_LINE_BREAK();
}

bool LogString_WordWrapped(LinearAllocator Scratch, const String Name, const String Value, const bool bAddNewLine)
{
    if (Value.Length > 0)
    {
        LOG_INLINE("%S", Name);

        const StringList l = {Value, NULL};
        if (LogStringList_WordWrapped(Scratch, Name, l))
        {
            LOG_LINE_BREAK();
            if (bAddNewLine) LOG_LINE_BREAK();
            return true;
        }
    }

    return false;
}

internal void ListVariables(LinearAllocator Arena, const String Name, TArray(FileVariable) ExpandedVariablesDB) 
{
    const String Exclusions[] =
    {
        S("AssertProgramExists"),
        S("AssertLibExists"),
        S("AssertBuildVarExists"),
        S("AssertWorkingDirectory"),
        S("AssertArgExists"),
        S("AssertEnvVarExists"),
        S("AssertPlatform"),
        S("PreBuild"),
        S("PostBuild"),
        S("RunAssembly"),
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
        S("Icon")
    };

    for each (FileVariable, v, ExpandedVariablesDB)
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

internal bool Internal_ExecuteBuildCmd(const String WorkingDirectory, const String Name, const String Value, bool bHasSpecial, u32* ExitCode)
{
    if (!String_IsValid(Value))
        return true;

    if (String_EndsWith(Name, S("Cmd"), false))
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
        LOG("CMD: %S", Cmd);
        #else
        LOG("da cmd: %S", Cmd);
        #endif

        bool bIgnoreErrors = bHasSpecial;

        PlatformHandle Handle = Platform_RunCommand(CmdLine, WorkingDirectory, String_Null());
        if (!Platform_IsValidHandle(Handle) && !bIgnoreErrors)
        {
            *ExitCode = 1;
            return false;
        }

        *ExitCode = Platform_WaitForProcessAndGetExitCode(Handle);
        if (*ExitCode != 0 && !bIgnoreErrors)
        {
            LOG("    You can ignore this error by using ._Cmd instead");

            return false;
        }
    }
    else if (String_EndsWith(Name, S("Copy"), false))
    {
        const String Cmd = Value;

        u32 FirstSpace = 0;
        String_IndexOfFirstWhitespace(Cmd, &FirstSpace);

        const String SourceFile           = String_EatPathSeparatorsFromEnd(StrSlice(Cmd.Data, FirstSpace));
        const String DestinationDirectory = String_EatPathSeparatorsFromEnd(String_EatSpaces(StrShiftF(Cmd, FirstSpace+1)));

        LOG("Copy: %S", Cmd);

        // only copy if dest does not exist
        if (bHasSpecial)
        {
            u32 LastDot = 0;
            String_IndexOfLastChar(DestinationDirectory, '.', &LastDot);

            bool bHasExtension = false;
            bool bHasPathSeparator = String_IndexOfFirstPathSlash(StrShiftF(DestinationDirectory, LastDot), NULL);
            if (!bHasPathSeparator)
            {
                bHasExtension = true;
            }

            if (bHasExtension)
            {
                if (Filesystem_DoesFileExist(DestinationDirectory))
                {
                    return true;
                }
            }
            else
            {
                if (Filesystem_DoesDirectoryExist(DestinationDirectory))
                {
                    return true;
                }
            }
        }

        bool bIgnoreErrors = String_EndsWith(Name, S("_Copy"), false);

        StringLocal(FullSourcePath, MAX_PATH_LENGTH);
        String_BuildPath(&FullSourcePath, WorkingDirectory, SourceFile);

        StringLocal(FullDestPath, MAX_PATH_LENGTH);
        String_BuildPath(&FullDestPath, WorkingDirectory, DestinationDirectory);

        if (!Filesystem_Copy(FullSourcePath, FullDestPath) && !bIgnoreErrors)
        {
            LOG("    You can ignore this error by using ._Copy instead\n"
            "    or you can use .Copy! to check whether the source files exist\n"
            "    and gracefully skip the copy operation if they don't.\n");

            *ExitCode = 1;
            return false;
        }
    }
    else if (String_EndsWith(Name, S("Move"), false) ||
             String_EndsWith(Name, S("Rename"), false))
    {
        // TODO: only deal with relative paths?

        const String Cmd = Value;

        u32 FirstSpace = 0;
        String_IndexOfFirstWhitespace(Cmd, &FirstSpace);

        const String SourceFile           = String_EatPathSeparatorsFromEnd(StrSlice(Cmd.Data, FirstSpace));
        const String DestinationDirectory = String_EatPathSeparatorsFromEnd(String_EatSpaces(StrShiftF(Cmd, FirstSpace+1)));

        bool bIsRename = String_EndsWith(Name, S("Rename"), false);
        if (bIsRename)
            LOG("Rename: %S", Cmd);
        else
            LOG("Move: %S", Cmd);

        // only move if dest does not exist
        if (bHasSpecial)
        {
            u32 LastDot = 0, LastSlash = 0;
            bool bHasDot = String_IndexOfLastChar(SourceFile, '.', &LastDot);
            String_IndexOfLastPathSlash(SourceFile, &LastSlash);

            bool bHasExtension = false;
            bool bHasPathSeparator = String_IndexOfFirstPathSlash(StrShiftF(SourceFile, LastDot), NULL);
            if (bHasDot && !bHasPathSeparator)
            {
                bHasExtension = true;
            }

            StringLocal(FullPath, MAX_PATH_LENGTH);
            String_BuildPath(&FullPath, WorkingDirectory, StrSlice(SourceFile.Data, LastSlash));
            if (!Filesystem_DoesDirectoryExist(FullPath))
            {
                return true;
            }

            String_Empty(&FullPath);
            String_BuildPath(&FullPath, WorkingDirectory, SourceFile);
            if (!Filesystem_DoesFileExist(FullPath))
            {
                return true;
            }

            String FileName = StrShiftF(SourceFile, LastSlash == 0 ? 0 : LastSlash+1);
            String_BuildPath(&FullPath, WorkingDirectory, DestinationDirectory, FileName);

            if (bHasExtension)
            {
                if (Filesystem_DoesFileExist(FullPath))
                {
                    return true;
                }
            }
            else
            {
                if (Filesystem_DoesDirectoryExist(FullPath))
                {
                    return true;
                }
            }
        }

        bool bIgnoreErrors = String_EndsWith(Name, S("_Move"), false) ||
                             String_EndsWith(Name, S("_Rename"), false);

        StringLocal(FullSourcePath, MAX_PATH_LENGTH);
        String_BuildPath(&FullSourcePath, WorkingDirectory, SourceFile);

        StringLocal(FullDestPath, MAX_PATH_LENGTH);
        String_BuildPath(&FullDestPath, WorkingDirectory, DestinationDirectory);

        if (bIgnoreErrors) Logging_Disable();
        bool bResult = Filesystem_Move(FullSourcePath, FullDestPath, bIsRename);
        if (bIgnoreErrors) Logging_Enable();

        if (!bResult && !bIgnoreErrors)
        {
            LOG("    You can ignore this error by using %S instead\n"
            "    or you can use %S to check whether the source files exist\n"
            "    and gracefully skip the move operation if they don't.\n",
            bIsRename ? S("._Rename") : S("._Move"),
            bIsRename ? S(".Rename!") : S(".Move!"));

            *ExitCode = 1;
            return false;
        }
    }
    else if (String_EndsWith(Name, S("Delete"), false))
    {
        const String Cmd = Value;

        LOG("Delete: %S", Cmd);

        // todo: make sure we only delete stuff relative to the working directory

        if (String_IsEqual(Cmd, S("*"), false) ||
            String_IsEqual(Cmd, S("."), false) ||
            String_IsEqual(Cmd, S(".."), false) ||
            String_IsEqual(Cmd, S("/"), false))
        {
            return false;
        }

        // todo: handle wildcards?

        u32 LastDot = 0, LastSlash = 0;
        bool bHasDot = String_IndexOfLastChar(Cmd, '.', &LastDot);
        String_IndexOfLastPathSlash(Cmd, &LastSlash);

        bool bHasExtension = false;
        bool bHasPathSeparator = String_IndexOfFirstPathSlash(StrShiftF(Cmd, LastDot), NULL);
        if (bHasDot && !bHasPathSeparator)
        {
            bHasExtension = true;
        }

        bool bIgnoreErrors = String_EndsWith(Name, S("_Delete"), false);

        StringLocal(FullFilePath, MAX_PATH_LENGTH);
        String_BuildPath(&FullFilePath, WorkingDirectory, Cmd);

        if (bIgnoreErrors) Logging_Disable();
        bool bResult = false;
        if (bHasExtension)
        {
            if (!Filesystem_DoesFileExist(FullFilePath))
                return true;

            bResult = Filesystem_DeleteFile(FullFilePath);
        }
        else
        {
            if (!Filesystem_DoesDirectoryExist(FullFilePath))
                return true;

            bResult = Filesystem_DeleteDirectory(FullFilePath);
        }
        if (bIgnoreErrors) Logging_Enable();

        if (!bResult && !bIgnoreErrors)
        {
            *ExitCode = 1;
            return false;
        }
    }
    else if (String_EndsWith(Name, S("NewFile"), false))
    {
        const String Cmd = Value;

        LOG("New File: %S", Cmd);

        bool bIgnoreErrors = String_EndsWith(Name, S("_NewFile"), false);

        StringLocal(FullFilePath, MAX_PATH_LENGTH);
        String_BuildPath(&FullFilePath, WorkingDirectory, Cmd);

        if (bIgnoreErrors) Logging_Disable();
        bool bResult = Filesystem_NewFile(FullFilePath);
        if (bIgnoreErrors) Logging_Enable();

        if (!bResult && !bIgnoreErrors)
        {
            *ExitCode = 1;
            return false;
        }
    }
    else if (String_EndsWith(Name, S("NewDirectory"), false) ||
             String_EndsWith(Name, S("NewDir"), false))
    {
        const String Cmd = Value;

        LOG("New Directory: %S", Cmd);

        bool bIgnoreErrors = String_EndsWith(Name, S("_NewDirectory"), false) ||
                             String_EndsWith(Name, S("_NewDir"), false);

        StringLocal(FullDirPath, MAX_PATH_LENGTH);
        String_BuildPath(&FullDirPath, WorkingDirectory, Cmd);

        if (bIgnoreErrors) Logging_Disable();
        bool bResult = Filesystem_OpenDirectory(FullDirPath);
        if (bIgnoreErrors) Logging_Enable();

        if (!bResult && !bIgnoreErrors)
        {
            *ExitCode = 1;
            return false;
        }
    }

    return true;
}

internal void Internal_RemoveBuildVariable(TArray(FileVariable) VariablesDB, const String Name)
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

internal void Internal_AddOrUpdateBuildVariable(TArray(FileVariable) VariablesDB, FileVariable Expanded)
{
    Internal_RemoveBuildVariable(VariablesDB, Expanded.Name);
    Array_Add(VariablesDB, Expanded);
}

internal void Internal_SetDefaultBuildVariables(LinearAllocator* Arena, const FileHandle BuildFileHandle, TArray(FileVariable) VariablesDB, TArray(FileVariable) ExpandedVariablesDB)
{
    if (!DoesBuildVarExist(VariablesDB, S("Assembly")))
    {
        StringLocal(Path, MAX_PATH_LENGTH);

        String Name = S("Untitled");

        if (IsValidFileHandle(BuildFileHandle))
        {
            Filesystem_GetFilePath(BuildFileHandle, &Path);

            u32 LastSlash = 0;
            String_IndexOfLastPathSlash(Path, &LastSlash);

            u32 LastDot = 0;
            String FileName = StrShiftF(Path, LastSlash+1);
            String_IndexOfLastChar(FileName, '.', &LastDot);
            FileName = StrSlice(FileName.Data, LastDot);
            if (FileName.Length > 0)
            {
                Name = FileName;
            }
        }

        FileVariable Expanded;
        Expanded.Name = S("Assembly");
        Expanded.Value = String_Create(Arena, Name);
        Expanded.bHasSpecial = false;

        Array_Add(VariablesDB, Expanded);
        Array_Add(ExpandedVariablesDB, Expanded);
    }

    const String Type = GetVariableValue(ExpandedVariablesDB, S("Type"));
    if (String_IsValid(Type))
    {
        String Extension = S("");
        
        FileVariable Expanded;
        Expanded.Name = S("Extension");
        Expanded.bHasSpecial = false;

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
                Extension = S("");
            #else
                Extension = S("");
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

        Expanded.Value = Extension;

        Array_Add(VariablesDB, Expanded);
        Array_Add(ExpandedVariablesDB, Expanded);
    }

    if (!DoesBuildVarExist(VariablesDB, S("Extension")))
    {
        FileVariable Expanded;
        Expanded.Name = S("Extension");
        Expanded.bHasSpecial = false;

        #if PLATFORM_WINDOWS
            Expanded.Value = S(".exe");
        #elif PLATFORM_APPLE
            Expanded.Value = S("");
        #else
            Expanded.Value = S("");
        #endif

        Array_Add(VariablesDB, Expanded);
        Array_Add(ExpandedVariablesDB, Expanded);
    }

    if (!DoesBuildVarExist(VariablesDB, S("Compiler")))
    {
        FileVariable Expanded;
        Expanded.Name = S("Compiler");
        Expanded.Value = S("");
        Expanded.bHasSpecial = false;

        Array_Add(VariablesDB, Expanded);
        Array_Add(ExpandedVariablesDB, Expanded);
    }

    if (!DoesBuildVarExist(VariablesDB, S("Version")))
    {
        FileVariable Expanded;
        Expanded.Name = S("Version");
        Expanded.Value = S("1.0.0");
        Expanded.bHasSpecial = false;

        Array_Add(VariablesDB, Expanded);
        Array_Add(ExpandedVariablesDB, Expanded);
    }

    if (!DoesBuildVarExist(VariablesDB, S("BuildDirectory")))
    {
        FileVariable Expanded;
        Expanded.Name = S("BuildDirectory");
        Expanded.Value = S("Build");
        Expanded.bHasSpecial = false;

        Array_Add(VariablesDB, Expanded);
        Array_Add(ExpandedVariablesDB, Expanded);
    }

    if (!DoesBuildVarExist(VariablesDB, S("IntermediateDirectory")))
    {
        FileVariable Expanded;
        Expanded.Name = S("IntermediateDirectory");
        Expanded.Value = S("Intermediate");
        Expanded.bHasSpecial = false;

        Array_Add(VariablesDB, Expanded);
        Array_Add(ExpandedVariablesDB, Expanded);
    }

    if (!DoesBuildVarExist(VariablesDB, S("SourceDirectory")))
    {
        FileVariable Expanded;
        Expanded.Name = S("SourceDirectory");
        Expanded.Value = S("");
        Expanded.bHasSpecial = false;

        Array_Add(VariablesDB, Expanded);
        Array_Add(ExpandedVariablesDB, Expanded);
    }
}

internal void AddCmdOption(TArray(CmdOption)* CmdOptionsDB, const String Name, const String Value)
{
    CmdOption c;
    c.Name = Name;
    c.Value = Value;
    c.bEqualsToSomething = Value.Length > 0;

    Array_Add(*CmdOptionsDB, c);
}

internal void AddInternalVariable(const String Name, const String Value)
{
    InternalVariable c;
    c.Name = Name;
    c.Value = Value;

    Array_Add(InternalVariablesDB, c);
}

internal bool CheckForBuildVariableOverrides(TArray(FileVariable) VariablesDB, TArray(FileVariable) ExpandedVariablesDB, TArray(CmdOption) CmdOptionsDB)
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

            // also update the expanded version
            for each (FileVariable, Var, ExpandedVariablesDB)
            {
                if (String_IsEqual(Var.Name, VarToOverride, false))
                {
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
                NewOverride.bHasSpecial = false;

                Internal_AddOrUpdateBuildVariable(VariablesDB, NewOverride);
                Internal_AddOrUpdateBuildVariable(ExpandedVariablesDB, NewOverride);

                //bOverriden = true;
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

internal bool BuildFilesIterator(const String FullPath, const String RelativePath, const String FileName, u64 FileSize, bool bIsDirectory, void* UserData)
{
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

internal void PrintUsage(const String WorkingDirectory)
{
    LOG_INLINE_WARNING("Usage\n");
    LOG("   riftbuild");
    LOG("   riftbuild [options]");
    LOG("   riftbuild [.build file] [options]");

    LOG_LINE_BREAK();

    LOG_INLINE_WARNING("Build Files\n");
    Filesystem_IterateDirectory(WorkingDirectory, BuildFilesIterator, true);

    LOG_LINE_BREAK();

    // TODO: custom usage message from each build file
    // TODO: custom descrption message for each build file
    // TODO: log the preset options as well
    // TODO: log command options

    LOG_INLINE_WARNING("Options\n");
    LOG("   -h, --help, /?, -?, ? : Display this help message");
    LOG("   -v                    : Enable verbose logging");
    LOG("   -q                    : Quiet mode. Disables logging but outputs necessary information, like errors");
    LOG("   -t                    : Display a tutorial on how to set environment variables");
    LOG("   help                  : Print out custom help message from the build file");
    LOG("   clean                 : Delete all intermediate and binary files");
    LOG("   rebuild               : Clean all and build");
    LOG("   list                  : List all the build files in the current directory");
    LOG("   override              : Override a build variable");
    LOG("   export                : Generate a compile_commands.json, visual studio or xcode project");
    LOG("   preset                : Build with a preset of command line arguments");
}

// TODO: have a relook at this filter code
bool FilterSourceFile(const String WorkingDirectory, const String SourceDirectory,
                      const String FullPath, const String RelativePath,
                      StringList WhitelistFiles, StringList BlacklistFiles,
                      StringList WhitelistDirectories, StringList BlacklistDirectories)
{
    String TrimmedFileName;
    String TrimmedDirName = String_Null();
    u32 SlashIndex = 0;
    if (String_IndexOfLastPathSlash(RelativePath, &SlashIndex))
    {
        TrimmedFileName = StrShiftF(RelativePath, SlashIndex + 1);
        TrimmedDirName = StrSlice(RelativePath.Data, SlashIndex);
    }
    else
    {
        TrimmedFileName = RelativePath;
    }

    // todo: immediate return
    bool bIsBlacklisted = false;
    for each_str_list (BlacklistFiles)
    {
        u32 Index = 0;
        if (String_IndexOfLastChar(It.String, '*', &Index))
        {
            String Left = StrSlice(It.String.Data, Index);
            String Right = StrShiftF(It.String, Index+1);
            if (Index < TrimmedFileName.Length)
            {
                if ((Index == 0 || String_IsEqual(Left, StrSlice(TrimmedFileName.Data, Index), true)) &&
                    String_IsEqual(Right, StrSlice(TrimmedFileName.Data+TrimmedFileName.Length-Right.Length, Right.Length), true))
                {
                    bIsBlacklisted = true;
                    break;
                }
            }
        }

        if (String_IsEqual(It.String, TrimmedFileName, true))
        {
            bIsBlacklisted = true;
            break;
        }

        if (String_IsEqual(It.String, RelativePath, true))
        {
            bIsBlacklisted = true;
            break;
        }
    }

    StringLocal(DirPath, MAX_PATH_LENGTH);
    String_BuildPath(&DirPath, WorkingDirectory, SourceDirectory, TrimmedDirName);

    for each_str_list (BlacklistDirectories)
    {
        if (String_IsEqual(It.String, S("*"), false) && TrimmedDirName.Length > 0)
        {
            bIsBlacklisted = true;
            break;
        }

        StringLocal(TestPath, MAX_PATH_LENGTH);
        String_BuildPath(&TestPath, WorkingDirectory, SourceDirectory, It.String);
        String_ConvertSlashToPlatformSlash(&TestPath);

        u32 Index = 0;
        if (String_IndexOfLastChar(TestPath, '*', &Index))
        {
            String Left = StrSlice(TestPath.Data, Index);
            String Right = StrShiftF(TestPath, Index+1);
            if (Index < DirPath.Length)
            {
                String Left2 = StrSlice(DirPath.Data, Index);
                String Right2 = StrShiftF(DirPath, Index);

                if ((String_IsEqual(Left, Left2, true) || Left.Length == 0) &&
                    (String_Contains(Right2, Right, true) || Right.Length == 0))
                {
                    if (Filesystem_ArePathsCommon(DirPath, FullPath))
                    {
                        bIsBlacklisted = true;
                        break;
                    }
                }
            }
        }

        if (Filesystem_ArePathsCommon(TestPath, FullPath))
        {
            bIsBlacklisted = true;
            break;
        }

        // also look in the outside directories
        /*
        if (String_IsValid(OutsideSourceDirectories))
        {
            StringArray Dirs = String_ParseIntoArray(Scratch_Filter.Allocator, OutsideSourceDirectories, ' ', 0, 128);
            for each_str (DirO, Dirs)
            {
                StringLocal(DirCopy, MAX_PATH_LENGTH);
                String_Copy(&DirCopy, *DirO);
                String_EatPathSeparatorsInlineFromEnd(&DirCopy);
                String_ConvertSlashToPlatformSlash(&DirCopy);

                if (Filesystem_ArePathsCommon(DirCopy, FullPath))
                {
                    bIsBlacklisted = true;
                    break;
                }
            }
        }
        */
    }

    if (bIsBlacklisted)
        return false;

    // TODO: why am i doing this
    u32 NumWhitelist = 0, NumWhitelistDir = 0;
    for each_str_list (WhitelistFiles) NumWhitelist++;
    for each_str_list (WhitelistDirectories) NumWhitelistDir++;
    bool bIsAllowed = NumWhitelist == 0 && NumWhitelistDir == 0;

    if (NumWhitelist > 0)
    {
        for each_str_list (WhitelistFiles)
        {
            u32 Index = 0;
            if (String_IndexOfLastChar(It.String, '*', &Index))
            {
                String Left = StrSlice(It.String.Data, Index);
                String Right = StrShiftF(It.String, Index+1);

                u32 LeftLastSlash = 0;
                String_IndexOfLastPathSlash(Left, &LeftLastSlash);
                String TrimmedLeft = StrShiftF(Left, LeftLastSlash+1);

                if (String_StartsWith(RelativePath, Left, true) &&
                    String_EndsWith(RelativePath, Right, true))
                {
                    bIsAllowed = true;
                    break;
                }

                {
                    if (String_IsEqual(TrimmedLeft, StrSlice(TrimmedFileName.Data, Index), true) &&
                        String_IsEqual(Right, StrSlice(TrimmedFileName.Data+TrimmedFileName.Length-Right.Length, Right.Length), true))
                    {
                        bIsAllowed = true;
                        break;
                    }
                }
            }

            if (String_IsEqual(It.String, TrimmedFileName, true))
            {
                bIsAllowed = true;
                break;
            }

            if (String_IsEqual(It.String, RelativePath, true))
            {
                bIsAllowed = true;
                break;
            }
        }
    }

    if (NumWhitelistDir > 0)
    {
        for each_str_list (WhitelistDirectories)
        {
            StringLocal(TestPath, MAX_PATH_LENGTH);
            String_BuildPath(&TestPath, WorkingDirectory, SourceDirectory, It.String);
            String_ConvertSlashToPlatformSlash(&TestPath);

            u32 Index = 0;
            if (String_IndexOfLastChar(TestPath, '*', &Index))
            {
                u32 LastSlash = 0;
                String_IndexOfLastPathSlash(TestPath, &LastSlash);
                i32 Diff = (i32)(Index-LastSlash+(TestPath.Length-1-Index));
                if (Diff <= 1 && TrimmedDirName.Length > 0)
                {
                    //bIsBlacklisted = true;
                    break;
                }

                String Left = StrSlice(TestPath.Data, Index);
                String Right = StrShiftF(TestPath, Index+1);
                if (Index < DirPath.Length)
                {
                    String Left2 = StrSlice(DirPath.Data, Index);
                    String Right2 = StrShiftF(DirPath, Index);

                    if ((String_IsEqual(Left, Left2, true) || Left.Length == 0) &&
                        (String_Contains(Right2, Right, true) || Right.Length == 0))
                    {
                        if (Filesystem_ArePathsCommon(DirPath, FullPath))
                        {
                            bIsAllowed = true;
                            break;
                        }
                    }
                }
            }

            if (Filesystem_ArePathsCommon(TestPath, FullPath))
            {
                bIsAllowed = true;
                break;
            }

            // also look in the outside directories
            /*
            if (String_IsValid(OutsideSourceDirectories))
            {
                StringArray Dirs = String_ParseIntoArray(Scratch_Filter.Allocator, OutsideSourceDirectories, ' ', 0, 128);
                for each_str (DirO, Dirs)
                {
                    StringLocal(DirCopy, MAX_PATH_LENGTH);
                    String_Copy(&DirCopy, *DirO);
                    String_EatPathSeparatorsInlineFromEnd(&DirCopy);
                    String_ConvertSlashToPlatformSlash(&DirCopy);

                    if (Filesystem_ArePathsCommon(DirCopy, FullPath))
                    {
                        bIsAllowed = true;
                        break;
                    }
                }
            }
            */
        }
    }

    if (bIsAllowed)
    {
        return true;
    }
    
    return false;
}

internal void ExpandPathFlags(LinearAllocator Scratch, String* Dest, const String Flags, const String FlagPrefix, bool bWrapWithQuotes)
{
    // expand include flags with * and ** wildcards
    StringLocal(WildcardFlags, 4096);
    StringLocal(NonWildcardFlags, 4096);

    StringList List = String_SplitIntoList(&Scratch, Flags, ' ', true);
    for each_str_list (List)
    {
        StringLocal(SearchDir, MAX_PATH_LENGTH);

        bool bWildcard = false;
        bool bRecursive = false;
        if (String_EndsWith(It.String, S("**"), false))
        {
            String_Copy(&SearchDir, StrSlice(It.String.Data, It.String.Length-2));
            String_EatPathSeparatorsInlineFromEnd(&SearchDir);
            bRecursive = true;
            bWildcard = true;
        }
        else if (String_EndsWith(It.String, S("*"), false))
        {
            String_Copy(&SearchDir, StrSlice(It.String.Data, It.String.Length-1));
            String_EatPathSeparatorsInlineFromEnd(&SearchDir);
            bWildcard = true;
        }

        if (bWildcard)
        {
            STRUCT(PathIterData)
            {
                String BaseDirectory;
                String* Flags;
            };
            
            PathIterData Data = { SearchDir, &WildcardFlags };

            Filesystem_IterateDirectory_Ex(SearchDir, PathFlagDirectoryIterator, bRecursive, &Data);
        }
        else
        {
            StringLocal(ItCopy, MAX_PATH_LENGTH);
            String_SanitizeQuotes(&ItCopy, It.String);

            String_Append(&NonWildcardFlags, ItCopy);
            String_AppendSpace(&NonWildcardFlags);
        }
    }

    String_EatSpacesInlineFromEnd(&WildcardFlags);
    String_EatSpacesInlineFromEnd(&NonWildcardFlags);

    PrefixVariables(Dest, WildcardFlags, FlagPrefix, bWrapWithQuotes);
    if (WildcardFlags.Length > 0)
        String_AppendSpace(Dest);
    PrefixVariables(Dest, NonWildcardFlags, FlagPrefix, bWrapWithQuotes);
}

internal void Internal_RunAssembly(LinearAllocator Scratch, const String WorkingPath, const String BuildDirectory, const String AssemblyNameWithExt, const String ArgString)
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
            String_EatSpacesInlineFromEnd(&EnvArgs);
            String_AppendChar(&EnvArgs, '\0');
        }
        else
        {
            String_Append(&ProgramArgs, It.String);
            String_EatSpacesInlineFromEnd(&ProgramArgs);
            String_AppendChar(&ProgramArgs, ' ');
        }
    }

    String_EatSpacesInlineFromEnd(&ProgramArgs);

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
            String_EatPathSeparatorsInlineFromEnd(&ExecutableWorkingPath);
        }
    }
    else
    {
        String_Copy(&ExecutableWorkingPath, BuildDirectory);
    }

    Filesystem_ConvertRelativeToAbsolutePath(&ExecutableWorkingPath);

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

    String_EatSpacesInlineFromEnd(&CmdLine);

    #if PLATFORM_WINDOWS
    String_AppendChar(&CmdLine, '"');

    LOG_LINE_BREAK();
    #endif

    if (AssemblyNameWithExt.Length > 0 && Filesystem_DoesFileExist(ExePath))
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

        Platform_WaitForHandle(Platform_RunCommand(CmdLine, ExecutableWorkingPath, EnvArgs), -1);
    }
}

internal u32 BuildTarget(LinearAllocator* Arena,
                        const FileHandle BuildFileHandle, PlatformMutex* BuildMutex,
                        const String WorkingPath, const StringArray Parameters, const String CameFromBuildFile,
                        i8 BuildFileIndex, i8 RootPathIndex)
{
    // make sure no one else is building this target
    const bool bNoMutex = StringArray_Contains(Parameters, S("-no-mutex"), false);
    if (!bNoMutex)
    {
        String MutexString = String_Reserve(Arena, MAX_PATH_LENGTH);
        if (IsValidFileHandle(BuildFileHandle))
        {
            Filesystem_GetFilePath(BuildFileHandle, &MutexString);
        }
        else
        {
            String_Copy(&MutexString, WorkingPath);
        }

        String_ReplaceNonAlphaNumericCharInline(&MutexString, '_'); // mainly for windows, but keep it for other platforms just in case

        if (!Platform_CreateNamedMutex(MutexString, BuildMutex))
        {
            StringLocal(BuildPath, MAX_PATH_LENGTH);
            if (IsValidFileHandle(BuildFileHandle))
            {
                Filesystem_GetFilePath(BuildFileHandle, &BuildPath);
            }
            else
            {
                String_Append(&BuildPath, WorkingPath);
            }

            u32 LastSlash = 0;
            String_IndexOfLastPathSlash(BuildPath, &LastSlash);
            String BuildFileName = StrShiftF(BuildPath, LastSlash+1);

            LOG_ERROR("Failed to acquire a build mutex. Aborting build...");
            
            if (BuildMutex->ID > 0)
            {
                LOG("\n    An existing riftbuild process is running for \"%S\" [Process ID: %i]", BuildFileName.Length > 0 ? BuildFileName : WorkingPath, BuildMutex->ID);
            } 
            else
            {
                LOG("\n    An existing riftbuild process is running for \"%S\"", BuildFileName.Length > 0 ? BuildFileName : WorkingPath);
            }

            LOG("    To prevent conflicts, please wait for the existing build to finish before trying again.\n");
            LOG("    This feature can be disabled with -no-mutex");

            #if !PLATFORM_WINDOWS
            //LOG_LINE_BREAK();
            #endif
            
            return 1;
        }
    }

    Clock BuildRuntime;
    Clock_Start(&BuildRuntime);

    StringLocal(RiftCmdLine, 2048);
    for (u8 i = 0; i < Parameters.Num; i++)
    {
        // todo: something better, ignore certain parameters
        if (String_IsEqual(Parameters.List[i], S("rebuild"), false) ||
            String_IsEqual(Parameters.List[i], S("clean"), false) ||
            String_IsEqual(Parameters.List[i], S("-v"), false) ||
            String_IsEqual(Parameters.List[i], S("-q"), false) ||
            String_IsEqual(Parameters.List[i], S("-s"), false) ||
            String_IsEqual(Parameters.List[i], S("--from-desktop"), false) ||
            String_StartsWith(Parameters.List[i], S("export:"), false))
            continue;

        String_Append     (&RiftCmdLine, Parameters.List[i]);
        String_AppendSpace(&RiftCmdLine);
    }

    String_EatSpacesInlineFromEnd(&RiftCmdLine);

    bool bFoundBuildFile = IsValidFileHandle(BuildFileHandle);

    StringLocal(IconFilePath, MAX_PATH_LENGTH);

    StringLocal(IconResFilePath, MAX_PATH_LENGTH);
    StringLocal(VersionResFilePath, MAX_PATH_LENGTH);

    ArrayLocal_Arena(FileVariable,   VariablesDB,         256, Arena); // 8192 bytes
    ArrayLocal_Arena(FileVariable,   ExpandedVariablesDB, 256, Arena); // 8192 bytes
    ArrayLocal_Arena(FileHandle,     IncludeFiles,        64,  Arena); // 1024 bytes
    ArrayLocal_Arena(CmdOption,      CmdOptionsDB,        128, Arena); // 4608 bytes
    ArrayLocal_Arena(String,         Messages,            128, Arena); // 2048 bytes

    // 256 is a reasonable max number of compilers to run in parrallel. if you have more than 256 cores then what the fuck lol
    ArrayLocal_Arena(PlatformHandle, Processes,           256, Arena); // 2048 bytes
    //ArrayLocal_Arena(PlatformPipe,   Pipes,               256, Arena); // 4096 bytes

    // store custom command line options to be referenced inside of a .build file
    for (u8 i = 0; i < Parameters.Num; i++)
    {
        const String Param = Parameters.List[i];

        if (i != BuildFileIndex &&
            i != RootPathIndex)
        {
            //const String P = String_EatChar(Param, '-');

            u32 EqualIndex = 0;
            bool bFoundEqual = String_IndexOfChar(Param, '=', &EqualIndex);

            CmdOption c;
            if (bFoundEqual)
            {
                c.Name = StrSlice(Param.Data, EqualIndex);
                String_EatSpacesInlineFromEnd(&c.Name);
                c.Value = StrSlice(Param.Data+EqualIndex+1, Param.Length - (EqualIndex + 1));
                String_EatSpacesInline(&c.Value);
                c.bEqualsToSomething = true;
            }
            else
            {
                c.Name = Param;
                String_EatSpacesInlineFromEnd(&c.Name);
                c.Value = String_Null();
                c.bEqualsToSomething = false;
            }

            Array_Add(CmdOptionsDB, c);
        }
    }

    StringLocal(BuildFilePathFull, MAX_PATH_LENGTH);
    Filesystem_GetFilePath(BuildFileHandle, &BuildFilePathFull);

    if (IsValidFileHandle(BuildFileHandle) && BuildFilePathFull.Length == 0)
    {
        LOG_FATAL("Operating system error: failed to retrieve file path from handle. Aborting...");
        return 1;
    }

    String BuildFileName;
    StringLocal(BuildFilePath, MAX_PATH_LENGTH);
    {
        u32 LastSlash = 0;
        String_IndexOfLastPathSlash(BuildFilePathFull, &LastSlash);

        u32 LastDot = 0;
        BuildFileName = StrShiftF(BuildFilePathFull, LastSlash+1);
        
        const String NameCopy = String_Create(Arena, BuildFileName);
        String_IndexOfLastChar(NameCopy, '.', &LastDot);

        AddCmdOption(&CmdOptionsDB, S("_FileName"), StrSlice(NameCopy.Data, LastDot));
        AddCmdOption(&CmdOptionsDB, S("_FileNameExt"), NameCopy);

        const String PathNoExt = StrSlice(BuildFilePathFull.Data, LastSlash);
        const String PathFull = String_Create(Arena, PathNoExt);
        AddCmdOption(&CmdOptionsDB, S("_FileDirectoryFull"), PathFull);

        const String PathRelative = StrShiftF(PathNoExt, WorkingPath.Length+1);

        String_BuildPath(&BuildFilePath, PathRelative, BuildFileName);

        AddCmdOption(&CmdOptionsDB, S("_FileDirectory"), String_Create(Arena, PathRelative));
        
        u32 SecondLastSlash = 0;
        String_IndexOfLastPathSlash(PathNoExt, &SecondLastSlash);

        AddCmdOption(&CmdOptionsDB, S("_DirectoryName"), String_Create(Arena, StrShiftF(PathNoExt, SecondLastSlash+1)));

        AddCmdOption(&CmdOptionsDB, S("_WorkingDirectory"), WorkingPath);
    }

    SystemTime TimeNow = Platform_GetSystemLocalTime();
    
    StringLocal(TimeZone, 64);
    Platform_GetTimeZone(&TimeZone);

    StringLocal(TimeStamp, 64);
    String_Format(&TimeStamp, S("%hu-%.2hu-%.2hu %.2hu:%.2hu:%.2hu [%S]"), TimeStamp.Capacity, TimeNow.Year, TimeNow.Month, TimeNow.Day, TimeNow.Hour, TimeNow.Minute, TimeNow.Second, TimeZone);

    {
        StringLocal(TimeStampVar, 64);
        String_Format(&TimeStampVar, S("%hu.%.2hu.%.2hu.%.2hu.%.2hu.%.2hu"), TimeStamp.Capacity, TimeNow.Year, TimeNow.Month, TimeNow.Day, TimeNow.Hour, TimeNow.Minute, TimeNow.Second);
        String a = String_Create(Arena, TimeStampVar);
        AddCmdOption(&CmdOptionsDB, S("_Timestamp"), a);

        // add another for time zone information
        String_Format(&TimeStampVar, S("%hu.%.2hu.%.2hu.%.2hu.%.2hu.%.2hu.%S"), TimeStamp.Capacity, TimeNow.Year, TimeNow.Month, TimeNow.Day, TimeNow.Hour, TimeNow.Minute, TimeNow.Second, TimeZone);
        a = String_Create(Arena, TimeStampVar);

        AddCmdOption(&CmdOptionsDB, S("_Timestamp_z"), a);
        String_Format(&TimeStampVar, S("%hu%.2hu%.2hu%.2hu%.2hu%.2hu"), TimeStamp.Capacity, TimeNow.Year, TimeNow.Month, TimeNow.Day, TimeNow.Hour, TimeNow.Minute, TimeNow.Second);
        a = String_Create(Arena, TimeStampVar);
        AddCmdOption(&CmdOptionsDB, S("_TimestampNoSep"), a);

        String_Format(&TimeStampVar, S("%hu.%.2hu.%.2hu"), TimeStamp.Capacity, TimeNow.Year, TimeNow.Month, TimeNow.Day);
        a = String_Create(Arena, TimeStampVar);
        AddCmdOption(&CmdOptionsDB, S("_Date"), a);

        String_Format(&TimeStampVar, S("%hu%.2hu%.2hu"), TimeStamp.Capacity, TimeNow.Year, TimeNow.Month, TimeNow.Day);
        a = String_Create(Arena, TimeStampVar);
        AddCmdOption(&CmdOptionsDB, S("_DateNoSep"), a);

        String_Format(&TimeStampVar, S("%.2hu.%.2hu.%.2hu"), TimeStamp.Capacity, TimeNow.Hour, TimeNow.Minute, TimeNow.Second);
        a = String_Create(Arena, TimeStampVar);
        AddCmdOption(&CmdOptionsDB, S("_Time"), a);

        String_Format(&TimeStampVar, S("%.2hu%.2hu%.2hu"), TimeStamp.Capacity, TimeNow.Hour, TimeNow.Minute, TimeNow.Second);
        a = String_Create(Arena, TimeStampVar);
        AddCmdOption(&CmdOptionsDB, S("_TimeNoSep"), a);
    }

    StringLocal(RiftBuildArgs, 4096);

    // add all arguments, except working directory and build file path
    {
        for (u8 i = 0; i < Parameters.Num; i++)
        {
            if (i == BuildFileIndex ||
                i == RootPathIndex)
                continue;

            String_Append(&RiftBuildArgs, Parameters.List[i]);
            String_AppendSpace(&RiftBuildArgs);
        }

        AddCmdOption(&CmdOptionsDB, S("%"), RiftBuildArgs);
        AddCmdOption(&CmdOptionsDB, S("_Args"), RiftBuildArgs);
    }

    if (bFoundBuildFile)
    {
        #ifndef HOOD
        LOG("Using build file:  %S", BuildFilePath);
        #else
        LOG("alright sweet, using this build file btw: %S", BuildFilePath);
        #endif
    }

    if (RiftCmdLine.Length > 0)
        LOG("Arguments:         %S", RiftCmdLine);
    
    #ifndef HOOD
    LOG("Working Directory: %S", WorkingPath);
    LOG("Timestamp:         %S\n", TimeStamp);
    #else
    LOG("dis da work'n directory bro: %S", WorkingPath);
    LOG("Timestamp: %S\n", TimeStamp);
    #endif

    EAssemblyType AssemblyType = AssemblyType_None;
    //bool bIsAssemblyExe = false;
    bool bShouldWaitPerCompileProcess = false;

    #if PLATFORM_WINDOWS
    bool bFallbackVersion = false;
    #endif

    Clock BuildFileParseClock = {0};
    Clock MSVCInitClock = {0};

    bool bAnyVarsOverriden = false;

    if (bFoundBuildFile)
    {
        Clock_Start(&BuildFileParseClock);

        if (!ParseBuildFile(Arena, BuildFileHandle, BuildFilePath, WorkingPath, VariablesDB, ExpandedVariablesDB,
                            CmdOptionsDB, Messages, IncludeFiles, NULL, false, NULL, false))
        {
            return 1;
        }

        bAnyVarsOverriden = CheckForBuildVariableOverrides(VariablesDB, ExpandedVariablesDB, CmdOptionsDB);

        // first expand Type and Extension. so on linux we can tell if its an assembly exe and not a library
        for each (FileVariable, v, VariablesDB)
        {
            if (String_IsEqual(v.Name, S("Extension"), false) ||
                String_IsEqual(v.Name, S("Type"), false))
            {
                StringLocal(ExpandedVar, 64);

                {
                    LinearAllocator Scratch = *Arena;
                    StringList List = GetVariableValueList(&Scratch, VariablesDB, v.Name);
                    for each_str_list (List)
                    {
                        if (!ExpandBuildVariable(Scratch, VariablesDB, CmdOptionsDB, &ExpandedVar, v.Name, It.String, v.Name, WorkingPath, false, false))
                        {
                            return 1;
                        }

                        String_EatSpacesInlineFromEnd(&ExpandedVar);

                        if (ExpandedVar.Length > 0)
                            String_AppendSpace(&ExpandedVar);
                    }
                }

                String_EatSpacesInlineFromEnd(&ExpandedVar);
                
                StringLocal(SanitizedVar, 64);
                bool bSawSpace = false;
                for (u32 i = 0; i < ExpandedVar.Length; i++)
                {
                    if (ExpandedVar.Data[i] == '.')
                        continue;

                    if (IsWhitespace(ExpandedVar.Data[i]))
                    {
                        bSawSpace = true;
                        continue;
                    }

                    if (bSawSpace)
                    {
                        bSawSpace = false;
                        String_AppendSpace(&SanitizedVar);
                    }

                    String_AppendChar(&SanitizedVar, ExpandedVar.Data[i]);
                }

                String Value;
                StringLocal(Extension, 64);
                if (String_IsEqual(v.Name, S("Extension"), false) && SanitizedVar.Length > 0)
                {
                    PrefixVariables(&Extension, SanitizedVar, S("."), false);
                    Value = Extension;
                }
                else
                {
                    Value = SanitizedVar;
                }

                FileVariable Expanded;
                Expanded.Name = v.Name;
                Expanded.Value = String_Create(Arena, Value);
                Array_Add(ExpandedVariablesDB, Expanded);
            }
        }

        const String Ext  = GetVariableValue(ExpandedVariablesDB, S("Extension"));
        const String Type = GetVariableValue(ExpandedVariablesDB, S("Type"));

        bool bIsAssemblyExe = Type.Length == 0 && Ext.Length == 0;
        if (bIsAssemblyExe)
        {
            FileVariable Var;
            Var.Name = S("Type");
            Var.Value = S("app");
            Array_Add(ExpandedVariablesDB, Var);
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
                             String_IsEqual(Ext, S(".elf"), false) ||
                             String_IsEqual(Ext, S(".out"), false) ||
                             String_IsEqual(Ext, S(".exe"), false) ||
                             String_IsEqual(Ext, S(".com"), false);
        }

        if (bIsAssemblyExe)
        {
            AssemblyType = AssemblyType_Executable;
        }

        String AssemblyKey = S("Assembly");
        if (DoesBuildVarExist(VariablesDB, AssemblyKey))
        {
            StringLocal(ExpandedVar, 256);
            if (!ExpandBuildVariable(*Arena, VariablesDB, CmdOptionsDB, &ExpandedVar,
                                    AssemblyKey, GetVariableValue(VariablesDB, AssemblyKey), AssemblyKey, WorkingPath, false, bIsAssemblyExe))
            {
                return 1;
            }

            String_EatSpacesInlineFromEnd(&ExpandedVar);

            FileVariable Expanded;
            Expanded.Name = AssemblyKey;
            Expanded.Value = String_Create(Arena, ExpandedVar);
            Array_Add(ExpandedVariablesDB, Expanded);
        }

        String VersionKey = S("Version");
        bool bDoesVersionVarExist = DoesBuildVarExist(VariablesDB, VersionKey);

        #if PLATFORM_WINDOWS
        if (!bDoesVersionVarExist)
        {
            bFallbackVersion = true;
        }
        #endif

        // set defaults for a few key build variables
        bool bAnyOverriden = CheckForBuildVariableOverrides(VariablesDB, ExpandedVariablesDB, CmdOptionsDB);
        Internal_SetDefaultBuildVariables(Arena, BuildFileHandle, VariablesDB, ExpandedVariablesDB);

        if (!bAnyVarsOverriden) bAnyVarsOverriden = bAnyOverriden;

        // try expand Version (if it exists)
        if (bDoesVersionVarExist)
        {
            StringLocal(ExpandedVar, 256);
            if (!ExpandBuildVariable(*Arena, VariablesDB, CmdOptionsDB, &ExpandedVar,
                                    VersionKey, GetVariableValue(VariablesDB, VersionKey),
                                    VersionKey, WorkingPath, false, bIsAssemblyExe))
            {
                return 1;
            }

            String_EatSpacesInlineFromEnd(&ExpandedVar);

            if (ExpandedVar.Length > 0)
            {
                FileVariable Expanded;
                Expanded.Name = VersionKey;
                Expanded.Value = String_Create(Arena, ExpandedVar);
                Array_Add(ExpandedVariablesDB, Expanded);

                // add the defines (if desired)
                if (VariableHasSpecial(VariablesDB, S("Version")))
                {
                    const String VersionLevels[3] = 
                    {
                        S("MAJOR"),
                        S("MINOR"),
                        S("PATCH")
                    };

                    StringLocal(AssemblyNameUpper, 128);
                    String_Copy(&AssemblyNameUpper, GetVariableValue(ExpandedVariablesDB, S("Assembly")));
                    String_ReplaceCharInline(&AssemblyNameUpper, '-', '_');
                    String_ToUpper(&AssemblyNameUpper);

                    {
                        StringLocal(VersionDefineString, 256);
                        String_Format(&VersionDefineString, S("%S_VERSION_STRING=\\\"%S\\\""), VersionDefineString.Capacity, AssemblyNameUpper, ExpandedVar);

                        FileVariable Var;
                        Var.Name = S("Defines");
                        Var.Value = String_Create(Arena, VersionDefineString);
                        Var.bHasSpecial = false;
                        Array_Add(VariablesDB, Var);
                    }

                    String_ReplaceNonAlphaNumericCharInline(&ExpandedVar, '.');

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
                                        String_Format(&VersionDefine, S("%S_%S_VERSION=\\\"%S\\\""), VersionDefine.Capacity, AssemblyNameUpper, VersionLevels[i], *v);
                                    else
                                        String_Format(&VersionDefine, S("%S_%S_VERSION=%S"), VersionDefine.Capacity, AssemblyNameUpper, VersionLevels[i], *v);
                                }
                                else
                                {
                                    if (bContainsNonDigit)
                                        String_Format(&VersionDefine, S("%S_EXTRA_VERSION_%hhu=\\\"%S\\\""), VersionDefine.Capacity, AssemblyNameUpper, i-3, *v);
                                    else
                                        String_Format(&VersionDefine, S("%S_EXTRA_VERSION_%hhu=%S"), VersionDefine.Capacity, AssemblyNameUpper, i-3, *v);
                                }

                                // TODO: wrap into function
                                FileVariable Var;
                                Var.Name = S("Defines");
                                Var.Value = String_Create(Arena, VersionDefine);
                                Var.bHasSpecial = false;
                                Array_Add(VariablesDB, Var);

                                i++;
                            }
                        }
                    }
                    else
                    {
                        const bool bContainsNonDigit = String_ContainsNonDigits(ExpandedVar);

                        StringLocal(VersionDefine, 256);

                        if (bContainsNonDigit)
                            String_Format(&VersionDefine, S("%S_VERSION=\\\"%S\\\""), VersionDefine.Capacity, AssemblyNameUpper, ExpandedVar);
                        else
                            String_Format(&VersionDefine, S("%S_VERSION=%S"), VersionDefine.Capacity, AssemblyNameUpper, ExpandedVar);

                        FileVariable Var;
                        Var.Name = S("Defines");
                        Var.Value = String_Create(Arena, VersionDefine);
                        Var.bHasSpecial = false;
                        Array_Add(VariablesDB, Var);
                    }
                }
            }
        }

        // expand all build variables
        for each (FileVariable, v, VariablesDB)
        {
            // already expanded
            if (String_IsEqual(v.Name, S("Extension"), false) ||
                String_IsEqual(v.Name, S("Assembly"), false) ||
                String_IsEqual(v.Name, S("Version"), false) ||
                String_IsEqual(v.Name, S("Type"), false))
            {
                continue;
            }

            StringLocal(ExpandedVar, 4096);

            const String Exclusions[] =
            {
                S("AssertProgramExists"),
                S("AssertBuildVarExists"),
                S("AssertLibExists"),
                S("AssertWorkingDirectory"),
                S("AssertArgExists"),
                S("AssertEnvVarExists"),
                S("AssertPlatform"),
                S("PreBuild"),
                S("PostBuild"),
                S("Depend"),
                S("Depends"),
                S("RunAssembly"),
                S("_"),
            };

            // do not join the above variables into one long string basically, is what this is for
            bool bIsExcludedFromMultiVarDeclarations = false;
            for (u8 i = 0; i < SArray_Capacity(Exclusions); i++)
            {
                if (String_StartsWith(v.Name, Exclusions[i], false))
                {
                    bIsExcludedFromMultiVarDeclarations = true;
                    break;
                }
            }

            if (!bIsExcludedFromMultiVarDeclarations)
            {
                bool bAlreadyExpanded = GetVariableValue(ExpandedVariablesDB, v.Name).Length > 0;
                if (bAlreadyExpanded)
                    continue;

                LinearAllocator Scratch = *Arena;
                StringList List = GetVariableValueList(&Scratch, VariablesDB, v.Name);
                for each_str_list (List)
                {
                    if (!ExpandBuildVariable(Scratch, VariablesDB, CmdOptionsDB, &ExpandedVar, v.Name, It.String, v.Name, WorkingPath, false, bIsAssemblyExe))
                    {
                        return 1;
                    }

                    if (ExpandedVar.Length > 0)
                        String_AppendSpace(&ExpandedVar);
                }
            }
            else
            {
                if (!ExpandBuildVariable(*Arena, VariablesDB, CmdOptionsDB, &ExpandedVar, v.Name, v.Value, v.Name, WorkingPath, false, bIsAssemblyExe))
                {
                    return 1;
                }
            }

            String_EatSpacesInlineFromEnd(&ExpandedVar);

            FileVariable Expanded;
            Expanded.Name = v.Name;
            Expanded.Value = String_Create(Arena, ExpandedVar);
            Expanded.bHasSpecial = v.bHasSpecial;

            Array_Add(ExpandedVariablesDB, Expanded);
        }

        Clock_Tick(&BuildFileParseClock);
    }
    else
    {
        //bNoCompilerProgramExplicityGiven = true;

        #if PLATFORM_WINDOWS
        bFallbackVersion = true;
        #endif

        // set defaults for a few key build variables
        FileHandle f = {0};
        bool bAnyOverriden = CheckForBuildVariableOverrides(VariablesDB, ExpandedVariablesDB, CmdOptionsDB);
        Internal_SetDefaultBuildVariables(Arena, f, VariablesDB, ExpandedVariablesDB);

        bAnyVarsOverriden = bAnyOverriden;
    }

    if (bAnyVarsOverriden)
    {
        LOG_LINE_BREAK();
    }

    // build file variable listing feature. list:all or list:varname
    for (u8 i = 0; i < Parameters.Num; i++)
    {
        const String Arg = Parameters.List[i];

        if (String_StartsWith(Arg, S("list:"), false))
        {
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
                    return 1;
                }

                LinearAllocator Scratch = *Arena;
                StringArray Vars = String_ParseIntoArray(&Scratch, VarToList, ',', 0, 128);
            
                for each_str (var, Vars)
                {
                    if (String_IsEqual(*var, S("all"), false))
                    {
                        LOG("Listing all build variables...\n");

                        ListVariables(Scratch, String_Null(), ExpandedVariablesDB);
                    }
                    else
                    {
                        if (!DoesBuildVarExist(VariablesDB, *var))
                        {
                            LOG_ERROR("Failed to list \"%S\". It does not exist in \"%S\" (within the context of the given build parameters)", *var, BuildFilePath);
                            return 1;
                        }

                        ListVariables(Scratch, *var, ExpandedVariablesDB);
                    }
                }

                // todo: put in function? clean up code routine?
                for each (FileHandle, File, IncludeFiles)
                    Filesystem_Close(&File);

                return 0;
            }
        }
    }

    String AssemblyName                     = GetVariableValue(ExpandedVariablesDB, S("Assembly"));
    String Extension                        = GetVariableValue(ExpandedVariablesDB, S("Extension"));
    String Type                             = GetVariableValue(ExpandedVariablesDB, S("Type"));
    String CompilerProgram                  = GetVariableValue(ExpandedVariablesDB, S("Compiler"));
    // TODO: specify a linker program
    //String LinkerProgram                  = GetVariableValue(ExpandedVariablesDB, S("Linker"));
    String AsmProgram                       = GetVariableValue(ExpandedVariablesDB, S("Assembler"));
    String CompilerFlagPrefixSymbol         = S("-");
    const String CompilerFlags              = GetVariableValue(ExpandedVariablesDB, S("CompilerFlags"));
    const String AssemblerFlags             = GetVariableValue(ExpandedVariablesDB, S("AssemblerFlags"));
    String IncludeFlags                     = GetVariableValue(ExpandedVariablesDB, S("Includes"));
    const String Libraries                  = GetVariableValue(ExpandedVariablesDB, S("Libraries"));
    String LibraryDirectories               = GetVariableValue(ExpandedVariablesDB, S("LibraryDirectories"));
    String LinkerFlags                      = GetVariableValue(ExpandedVariablesDB, S("LinkerFlags"));
    const String Defines                    = GetVariableValue(ExpandedVariablesDB, S("Defines"));
    const String UnDefines                  = GetVariableValue(ExpandedVariablesDB, S("UnDefines"));
    const String LinkerDefines              = GetVariableValue(ExpandedVariablesDB, S("LinkerDefines"));
    const String AssertCompilers            = GetVariableValue(ExpandedVariablesDB, S("AssertCompiler"));
    const String AssertAssemblers           = GetVariableValue(ExpandedVariablesDB, S("AssertAssembler"));
    const String AssertPlatforms            = GetVariableValue(ExpandedVariablesDB, S("AssertPlatform"));
    const String AssertArchitecture         = GetVariableValue(ExpandedVariablesDB, S("AssertArchitecture"));
    const String AssertPrograms             = GetVariableValue(ExpandedVariablesDB, S("AssertProgramExists"));
    const String AssertEnvVars              = GetVariableValue(ExpandedVariablesDB, S("AssertEnvVarExists"));
    const String AssertBuildVars            = GetVariableValue(ExpandedVariablesDB, S("AssertBuildVarExists"));
    //const String AssertLibs                 = GetVariableValue(ExpandedVariablesDB, S("AssertLibExists"));
    String AssertWorkingDirectory           = GetVariableValue(ExpandedVariablesDB, S("AssertWorkingDirectory"));
    String IncludedSourceFiles              = GetVariableValue(ExpandedVariablesDB, S("IncludedSourceFiles"));
    String ExcludedSourceFiles              = GetVariableValue(ExpandedVariablesDB, S("ExcludedSourceFiles"));
    const String IncludedSourceDir          = GetVariableValue(ExpandedVariablesDB, S("IncludedSourceDirectories"));
    const String ExcludedSourceDir          = GetVariableValue(ExpandedVariablesDB, S("ExcludedSourceDirectories"));
    const String MaxConcurrentCompilations  = GetVariableValue(ExpandedVariablesDB, S("MaxConcurrentCompilations"));
    //const String OutsideSourceDirectories   = GetExpandedVariableValue(ExpandedVariablesDB, S("ExternalSourceDirectories"));
    String Icon                             = GetVariableValue(ExpandedVariablesDB, S("Icon"));
    const String MaxCompilerErrors          = GetVariableValue(ExpandedVariablesDB, S("MaxCompilerErrors"));
    const String PCHPath                    = GetVariableValue(ExpandedVariablesDB, S("PCH"));
    const String PCHHeaderPath              = GetVariableValue(ExpandedVariablesDB, S("PCH.h"));
    const String HelpMessage                = GetVariableValue(ExpandedVariablesDB, S(".Help"));

    #if PLATFORM_APPLE
    const String CustomInfoPlist            = GetVariableValue(ExpandedVariablesDB, S("Bundle.InfoPlist"));
    const String CustomVersionPlist         = GetVariableValue(ExpandedVariablesDB, S("Bundle.VersionPlist"));
    const String CustomPkgInfo              = GetVariableValue(ExpandedVariablesDB, S("Bundle.PkgInfo"));
    #endif

    const String TitleName                  = GetVariableValue(ExpandedVariablesDB, S("TitleName"));
    const String InternalName               = GetVariableValue(ExpandedVariablesDB, S("InternalName"));
    const String Description                = GetVariableValue(ExpandedVariablesDB, S("Description"));
    const String CompanyName                = GetVariableValue(ExpandedVariablesDB, S("CompanyName"));
    const String Copyright                  = GetVariableValue(ExpandedVariablesDB, S("Copyright"));

    String Version                          = GetVariableValue(ExpandedVariablesDB, S("Version"));

    const bool bNoRebuildOnDependencyChange = String_ToBool(GetVariableValue(ExpandedVariablesDB, S("NoRebuildOnDependencyChange")));
    // todo: run pre build?
    const bool bRunPostBuildWhenWorkWasDone = String_ToBool(GetVariableValue(ExpandedVariablesDB, S(".OnlyRunPostBuildOnChange")));

    #if PLATFORM_APPLE
    const bool bBundleApp                   = DoesBuildVarExist(ExpandedVariablesDB, S("Bundle"));
    const bool bBundleAppIsTerminal         = DoesBuildVarExist(ExpandedVariablesDB, S("Bundle.IsTerminal"));
    #endif

    if (bHelp && bFoundBuildFile)
    {
        LOG_INLINE_WARNING("Help\n");
        if (HelpMessage.Length > 0)
        {
            LOG("%S", HelpMessage);
        }
        else
        {
            LOG("    No help message provided. Use -h to view this program's usage help instead.");
        }

        return 0;
    }

    bShouldWaitPerCompileProcess = bSingleThread;

    u8 MaxErrorsAllowed = 1; // default to 1 error (for the people's sanity)
    if (String_IsValid(MaxCompilerErrors))
    {
        String_ToU8(MaxCompilerErrors, &MaxErrorsAllowed);
    }

    String RequireCompilerVersion = String_Null();
    EComparisonType CompilerVersionComparisonType = Cmp_Equal;
    if (CompilerProgram.Length > 0)
    {
        u32 Index = 0;
        if (String_IndexOfChar(CompilerProgram, '|', &Index))
        {
            const String V  = String_EatSpacesFromEnd(String_EatSpaces(StrShiftF(CompilerProgram, Index+1)));
            CompilerProgram = String_EatSpacesFromEnd(String_EatSpaces(StrSlice(CompilerProgram.Data, Index)));

            if (V.Length > 0)
            {
                u8 SymbolLength = 0;

                if (String_StartsWith(V, S("=="), false))
                {
                    CompilerVersionComparisonType = Cmp_Equal;
                    SymbolLength = 2;
                }
                else if (String_StartsWith(V, S(">="), false))
                {
                    CompilerVersionComparisonType = Cmp_GreaterThanOrEqual;
                    SymbolLength = 2;
                }
                else if (String_StartsWith(V, S("<="), false))
                {
                    CompilerVersionComparisonType = Cmp_LessThanOrEqual;
                    SymbolLength = 2;
                }
                else if (String_StartsWith(V, S(">"), false))
                {
                    CompilerVersionComparisonType = Cmp_GreaterThan;
                    SymbolLength = 1;
                }
                else if (String_StartsWith(V, S("<"), false))
                {
                    CompilerVersionComparisonType = Cmp_LessThan;
                    SymbolLength = 1;
                }
                else if (String_StartsWith(V, S("="), false))
                {
                    CompilerVersionComparisonType = Cmp_Equal;
                    SymbolLength = 1;
                }

                RequireCompilerVersion = String_EatSpacesFromEnd(String_EatSpaces(StrShiftF(V, SymbolLength)));
            }
        }
    }

    bool bNoCompilerProgramExplicityGiven = false;
    if (CompilerProgram.Length == 0)
    {
        bNoCompilerProgramExplicityGiven = true;
    }

    bool bNoAsmCompilerProgramExplicityGiven = false;
    if (AsmProgram.Length == 0)
    {
        bNoAsmCompilerProgramExplicityGiven = true;
    }

    String_ConvertSlashToPlatformSlash(&CompilerProgram);
    String_ConvertSlashToPlatformSlash(&LibraryDirectories);
    String_ConvertSlashToPlatformSlash(&IncludeFlags);
    String_ConvertSlashToPlatformSlash(&Icon);
    String_ConvertSlashToPlatformSlash(&IncludedSourceFiles);
    String_ConvertSlashToPlatformSlash(&ExcludedSourceFiles);
    String_ConvertSlashToPlatformSlash(&AssertWorkingDirectory);

    // Extension could have multiple options listed
    // for example: to allow for a .dll and a static lib to be generated. So the first one is always the real extension
    String Extension_Og = Extension;
    {
        u32 Index = 0;
        String_IndexOfFirstWhitespace(Extension, &Index);
        if (Index > 0)
            Extension.Length = Index;
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
                    bHasDynamicLib = String_EndsWith(*e, S("dll"), false) ||
                                     String_EndsWith(*e, S("so"), false) ||
                                     String_EndsWith(*e, S("dylib"), false);
                }

                if (!bHasStaticLib)
                {
                    bHasStaticLib = String_EndsWith(*e, S("lib"), false) ||
                                    String_EndsWith(*e, S("a"), false);
                }

                if (!bHasPCH)
                {
                    bHasPCH = String_EndsWith(*e, S("pch"), false) ||
                              String_EndsWith(*e, S("gch"), false);
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
                AssemblyType = AssemblyType_Executable;
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

    // to make life easier on linux because of case fuckjing sensitive commands
    #if !PLATFORM_WINDOWS
    StringLocal(AssemblyNameCopy, 256);
    if (bIsAssemblyExe || !bFoundBuildFile) // only do this for executables. people may want case sensitivity when building libraries
    {
        String_Copy(&AssemblyNameCopy, AssemblyName);
        String_ToLower(&AssemblyNameCopy);
        AssemblyName = AssemblyNameCopy;
    }
    else
    {
        String_Append(&AssemblyNameCopy, S("lib"));
        String_Append(&AssemblyNameCopy, AssemblyName);
        AssemblyName = AssemblyNameCopy;
    }
    #endif

    StringLocal(AssemblyNameWithExt, 128);
    String_Copy(&AssemblyNameWithExt, AssemblyName);
    if (Extension.Length > 0)
    {
        if (Extension.Data[0] != '.')
            String_AppendChar(&AssemblyNameWithExt, '.');

        String_Append(&AssemblyNameWithExt, Extension);
    }

    #if !PLATFORM_WINDOWS
    if (bIsAssemblyExe)
    {
        String_ToLower(&AssemblyNameWithExt);
    }
    #endif

    StringLocal(CompilerPath, MAX_PATH_LENGTH);

    bool bExplicitCompilerPath = false;
    if (String_IndexOfFirstPathSlash(CompilerProgram, NULL))
    {
        StringLocal(CompilerPathCopy, MAX_PATH_LENGTH);
        String_Copy(&CompilerPathCopy, CompilerProgram);

        #if PLATFORM_WINDOWS
        if (!String_EndsWith(CompilerProgram, S(".exe"), false))
        {
            String_Copy(&CompilerPathCopy, CompilerProgram);
            String_Append(&CompilerPathCopy, S(".exe"));
        }
        #endif

        if (Filesystem_DoesFileExist(CompilerPathCopy))
        {
            bExplicitCompilerPath = true;
            String_Copy(&CompilerPath, CompilerPathCopy);
        }
        else
        {
            LOG_ERROR("Compiler program \"%S\" does not exist", CompilerPathCopy);
            return 1;
        }
    }

    // does the compiler program exist on the user's machine
    if (!bExplicitCompilerPath)
    {
        bool bCompilerProgramFound = Platform_FindProgram_Ex(CompilerProgram, &CompilerPath);

        if (!bCompilerProgramFound && bNoCompilerProgramExplicityGiven)
        {
            const String CompilerPrograms[] =
            {
                S("clang"),
                S("gcc"),
                S("egcc"),
                S("cc"),
                S("x86_64-w64-mingw32-gcc"),
                S("g++"),
                S("clang++"),
                S("cl"),
                S("clang-cl"),
            };

            for (u8 i = 0; i < SArray_Capacity(CompilerPrograms); i++)
            {
                const bool bFound = Platform_FindProgram_Ex(CompilerPrograms[i], &CompilerPath);
                if (bFound)
                {
                    CompilerProgram = CompilerPrograms[i];
                    bCompilerProgramFound = true;
                    break;
                }
            }
        }

        if (!bCompilerProgramFound)
        {
            if (bNoCompilerProgramExplicityGiven)
            {
                // todo: prettier log messaging
                #if PLATFORM_WINDOWS
                LOG_ERROR(
                    "You don't seem to have a C compiler installed on your machine."
                    " Install either \"clang\", \"gcc\" or \"cl (msvc)\" and add to the path environment"
                    " before using RiftBuild, as we require a working compiler program to function properly. Aborting build...\n");
                #else
                LOG_ERROR(
                    "You don't seem to have a C compiler installed on your machine."
                    " Install either \"clang\" or \"gcc\" and add to the PATH environment"
                    " before using RiftBuild, as we require a working compiler program to function properly. Aborting build...\n");

                #endif

                LogPathEnvVarTutorialSteps();
                    
                return 1;
            }

            #ifndef HOOD
            if (String_IsEqual(CompilerProgram, S("cl"), false))
            {
                #if PLATFORM_WINDOWS
                const bool bShowExtraInfo = StringArray_Contains(Parameters, S("--help-msvc-init"), false);

                LOG("Initializing MSVC environment... %S\n", !bShowExtraInfo ? S("[--help-msvc-init for more info]") : String_Null());

                Clock_Start(&MSVCInitClock);

                GMSVCFindAllocator = *Arena;

                Find_Result MSVC_SDK_Result = {0};
                MSVC_SDK_Result.allocate = MSVC_Find_Allocate;
                MSVC_SDK_Result.release  = MSVC_Find_Release;
                find_visual_studio_and_windows_sdk(&MSVC_SDK_Result);

                if (bShowExtraInfo)
                {
                    LOG(" Windows SDK Version: %d", MSVC_SDK_Result.windows_sdk_version);

                    if (MSVC_SDK_Result.windows_sdk_root)
                    {
                        String16 PathWide = CStr16(MSVC_SDK_Result.windows_sdk_root);
                        StringLocal(Path, MAX_PATH_LENGTH);
                        String_ToNarrow(PathWide, &Path);
                        LOG("   Windows SDK path:      %S", Path);
                    }

                    if (MSVC_SDK_Result.windows_sdk_um_library_path)
                    {
                        String16 PathWide = CStr16(MSVC_SDK_Result.windows_sdk_um_library_path);
                        StringLocal(Path, MAX_PATH_LENGTH);
                        String_ToNarrow(PathWide, &Path);
                        LOG("   Windows SDK um path:   %S", Path);
                    }

                    if (MSVC_SDK_Result.windows_sdk_ucrt_library_path)
                    {
                        String16 PathWide = CStr16(MSVC_SDK_Result.windows_sdk_ucrt_library_path);
                        StringLocal(Path, MAX_PATH_LENGTH);
                        String_ToNarrow(PathWide, &Path);
                        LOG("   Windows SDK ucrt path: %S", Path);
                    }

                    if (MSVC_SDK_Result.vs_exe_path)
                    {
                        String16 PathWide = CStr16(MSVC_SDK_Result.vs_exe_path);
                        StringLocal(Path, MAX_PATH_LENGTH);
                        String_ToNarrow(PathWide, &Path);
                        LOG("   VS exe path:           %S", Path);
                    }

                    if (MSVC_SDK_Result.vs_library_path)
                    {
                        String16 PathWide = CStr16(MSVC_SDK_Result.vs_library_path);
                        StringLocal(Path, MAX_PATH_LENGTH);
                        String_ToNarrow(PathWide, &Path);
                        LOG("   VS library path:       %S", Path);
                    }

                    if (MSVC_SDK_Result.vs_base_path)
                    {
                        String16 PathWide = CStr16(MSVC_SDK_Result.vs_base_path);
                        StringLocal(Path, MAX_PATH_LENGTH);
                        String_ToNarrow(PathWide, &Path);
                        LOG("   VS base path:          %S", Path);
                    }
                }

                StringLocal(BasePath, MAX_PATH_LENGTH);
                if (MSVC_SDK_Result.vs_base_path) String_ToNarrow(CStr16(MSVC_SDK_Result.vs_base_path), &BasePath);

                StringLocal(ExePath, MAX_PATH_LENGTH);
                if (MSVC_SDK_Result.vs_exe_path) String_ToNarrow(CStr16(MSVC_SDK_Result.vs_exe_path), &ExePath);

                // TODO: .InitMSVCEnvironment in build file to trigger this
                
                // Initialize vcvars environment
                // find the vcvars bat file so we can run cl from a regular cmd line.
                // luckily the bat file is always in the same place relative to the base path
                if (Filesystem_DoesDirectoryExist(BasePath))
                {
                    StringLocal(CmdLine, 8192);
                    String_Append(&CmdLine, S("\""));

                    String_Append(&CmdLine, BasePath);
                    String_Append(&CmdLine, S("\\VC\\Auxiliary\\Build\\"));

                    // todo: think about target instead of host??
                    #if PLATFORM_64_BIT
                    String_Append(&CmdLine, S("vcvars64.bat"));
                    #else
                    String_Append(&CmdLine, S("vcvars32.bat"));
                    #endif

                    String_AppendChar(&CmdLine, '"');
                    String_Append(&CmdLine, S(" >NUL 2>&1 && set")); // suppress output logs from the bat script

                    if (bShowExtraInfo)
                    {
                        LOG_WARNING("\nBuild speed is affected, this may take a few seconds...");
                        LOG(
                        "\n    To fix this issue for your next build, exit this terminal"
                        "\n    and run riftbuild from a different terminal application named"
                        "\n    \"x64 (or x86) Native Tools Command Prompt for VS\"."
                        "\n\n    This can be found through Windows Search.\n");
                    }

                    PlatformPipe StdOutHandle = {0};
                    PlatformHandle H = Platform_RunCommand_Ex(CmdLine, WorkingPath, &StdOutHandle);
                    if (!Platform_IsValidHandle(H))
                    {
                        LOG_ERROR("Failed to initialize MSVC environment. Aborting build...");
                        return 1;
                    }

                    Platform_CloseHandle(StdOutHandle[1]);
                    
                    // dummy first read, nothing useful here
                    StringLocal(StdOutData, UINT16_MAX);
                    Filesystem_ReadPipe(StdOutHandle, StdOutData.Capacity, StdOutData.Data, NULL);

                    while (1)
                    {
                        u64 BytesRead = 0;
                        if (!Filesystem_ReadPipe(StdOutHandle, StdOutData.Capacity, StdOutData.Data, &BytesRead))
                            break;
                        
                        if (BytesRead == 0)
                            break;
                        
                        StdOutData.Length = Min((u32)BytesRead, StdOutData.Capacity);

                        LinearAllocator Scratch = *Arena;
                        StringArray EnvVars = String_ParseIntoArray(&Scratch, StdOutData, '\n', 0, 128);
                        for each_str (e, EnvVars)
                        {
                            String Trimmed = String_EatNewLinesFromEnd(*e);

                            u32 Equals = 0;
                            String_IndexOfChar(Trimmed, '=', &Equals);

                            if (Equals)
                            {
                                const String Key = StrSlice(Trimmed.Data, Equals);
                                const String Value = StrShiftF(Trimmed, Equals+1);
                                //LOG("  %S=%S", Key, Value);
                                Platform_SetEnvironmentVariableValue(Key, Value);
                            }
                        }
                    }

                    Platform_CloseHandle(StdOutHandle[0]);

                    String_Copy(&CompilerPath, ExePath);
                    String_Append(&CompilerPath, S("\\cl.exe"));

                    bCompilerProgramFound = true;

                    Clock_Tick(&MSVCInitClock);
                }
                #endif

                if (!bCompilerProgramFound)
                {
                    #if PLATFORM_WINDOWS
                    LOG_ERROR("Compiler program \"%S\" does not exist. Aborting build...", CompilerProgram);
                    
                    LOG("\n    Make sure that the Visual Studio build tools and Windows SDK are installed and "
                        "\n    that you run riftbuild from a different terminal application named"
                        "\n    \"x64 (or x86) Native Tools Command Prompt for VS\".");

                    LOG("\n    This can be found through Windows Search.");
                    #else
                    LOG_ERROR("Compiler program \"cl\" does not exist on non-Windows platforms. Use a different compiler. Aborting build...");
                    #endif
                }
            }
            else
            {
                LOG_ERROR("Compiler program \"%S\" does not exist. Make sure that it is installed and added to the path environment.\n"
                          "        Alternatively, you can specify the full path to the compiler executable instead. Aborting build...\n", CompilerProgram);

                LogPathEnvVarTutorialSteps();
            }
            #else
            LOG_ERROR(
                "yo dat compiler program \"%S\" don exist cuh."
                " need to be installed and set in da path ma nigga", CompilerProgram);
            #endif
        }

        if (!bCompilerProgramFound)
            return 1;
    }

    if (RequireCompilerVersion.Length > 0)
    {
        PlatformPipe StdOutPipe = {0};
        StringLocal(CmdLine, 2048);
        String_Append(&CmdLine, CompilerPath);
        String_AppendSpace(&CmdLine);

        if (!String_IsEqual(CompilerProgram, S("cl"), false))
        {
            String_Append(&CmdLine, S("-v"));
        }

        PlatformHandle H = Platform_RunCommand_Ex(CmdLine, WorkingPath, &StdOutPipe);

        Platform_CloseHandle(StdOutPipe[1]);

        if (Platform_IsValidHandle(H))
        {
            Platform_WaitForHandle(H, -1);
            
            StringLocal(StdOutData, UINT16_MAX);

            u64 BytesRead = 0;
            if (Filesystem_ReadPipe(StdOutPipe, StdOutData.Capacity, StdOutData.Data, &BytesRead))
            {
                StdOutData.Length = Min((u32)BytesRead, StdOutData.Capacity);

                u32 Index = 0;
                if (String_IndexOfSubstring(StdOutData, S("version "), false, &Index))
                {
                    String FoundVersion = StrShiftF(StdOutData, Index+8);

                    String_IndexOfFirstWhitespace(FoundVersion, &Index);
                    FoundVersion = StrSlice(FoundVersion.Data, Index);

                    ECompareResult Result = String_CompareVersion(FoundVersion, RequireCompilerVersion);

                    bool bCompareResultsMatch = false;
                    if (Result == CompareResult_Equal)
                    {
                        bCompareResultsMatch = CompilerVersionComparisonType == Cmp_Equal ||
                                               CompilerVersionComparisonType == Cmp_GreaterThanOrEqual ||
                                               CompilerVersionComparisonType == Cmp_LessThanOrEqual;
                    }
                    else if (Result == CompareResult_Greater)
                    {
                        bCompareResultsMatch = CompilerVersionComparisonType == Cmp_GreaterThan ||
                                               CompilerVersionComparisonType == Cmp_GreaterThanOrEqual;
                    }
                    else if (Result == CompareResult_Less)
                    {
                        bCompareResultsMatch = CompilerVersionComparisonType == Cmp_LessThan ||
                                               CompilerVersionComparisonType == Cmp_LessThanOrEqual;
                    }

                    if (!bCompareResultsMatch)
                    {
                        String Prefix = S("of");

                        String Extra = S(" exactly");

                        if (CompilerVersionComparisonType == Cmp_GreaterThan)
                        {
                            Prefix = S("above");
                            Extra = S("");
                        }
                        else if (CompilerVersionComparisonType == Cmp_GreaterThanOrEqual)
                        {
                            Extra = S(" or above");
                        }
                        else if (CompilerVersionComparisonType == Cmp_LessThan)
                        {
                            Prefix = S("below");
                            Extra = S("");
                        }
                        else if (CompilerVersionComparisonType == Cmp_LessThanOrEqual)
                        {
                            Extra = S(" or below");
                        }

                        LOG_INLINE_ERROR("[ASSERTION FAILURE] %S compiler version \"%S\" does not meet the required version %S \"%S\"%S. Aborting build...\n", CompilerProgram, FoundVersion, Prefix, RequireCompilerVersion, Extra);
                        return 1;
                    }
                }
            }
        }

        Platform_CloseHandle(StdOutPipe[0]);
    }

    //ECompiler Compiler = Compiler_Clang;

    if (String_IsEqual(CompilerProgram, S("cl"), false) ||
        String_IsEqual(CompilerProgram, S("clang-cl"), false) ||
        String_IsEqual(CompilerProgram, S("msvc"), false) || // todo: detect msvc and chang "Compiler" value to "cl"
        String_IsEqual(CompilerProgram, S("clang"), false) ||
        String_IsEqual(CompilerProgram, S("clang++"), false) ||
        String_IsEqual(CompilerProgram, S("gcc"), false) ||
        String_IsEqual(CompilerProgram, S("x86_64-w64-mingw32-gcc"), false) ||
        String_IsEqual(CompilerProgram, S("g++"), false))
    {
        if (String_IsEqual(CompilerProgram, S("cl"), false) ||
            String_IsEqual(CompilerProgram, S("clang-cl"), false) ||
            String_IsEqual(CompilerProgram, S("msvc"), false))
        {
            CompilerFlagPrefixSymbol = S("/");
            //Compiler = Compiler_MSVC;
        }
    }
    else
    {
        //Compiler = Compiler_Clang;
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
    {
        LinearAllocator Scratch = *Arena;
        StringArray ProgramsArray      = String_ParseIntoArray(&Scratch, AssertPrograms, ' ', 0, 128);
        StringArray EnvVarsArray       = String_ParseIntoArray(&Scratch, AssertEnvVars, ' ', 0, 128);
        StringArray BuildVarsArray     = String_ParseIntoArray(&Scratch, AssertBuildVars, ' ', 0, 128);
        StringArray PlatformsArray     = String_ParseIntoArray(&Scratch, AssertPlatforms, ' ', 0, 128);
        StringArray ArchitecturesArray = String_ParseIntoArray(&Scratch, AssertArchitecture, ' ', 0, 128);
        StringArray CompilersArray     = String_ParseIntoArray(&Scratch, AssertCompilers, ' ', 0, 128);

        StringLocal(PlatformsLogString, 128);
        {
            u8 i = 0;
            for each_str_i (i, p, PlatformsArray)
            {
                String_Append(&PlatformsLogString, *p);
                if (PlatformsArray.Num > 1 && i != PlatformsArray.Num-1)
                {
                    if (i == PlatformsArray.Num-2)
                    {
                        String_Append(&PlatformsLogString, S(" and "));
                    }
                    else
                    {
                        String_AppendChar(&PlatformsLogString, ',');
                        String_AppendSpace(&PlatformsLogString);
                    }
                }
            }
        }

        #if PLATFORM_WINDOWS
        const String HostPlatform = S("Windows");
        #elif PLATFORM_MAC
        const String HostPlatform = S("Apple Mac MacOS Unix");
        #elif PLATFORM_LINUX
        const String HostPlatform = S("Linux Unix");
        #elif PLATFORM_BSD
        const String HostPlatform = S("BSD " PLATFORM_STRING);
        #else
        const String HostPlatform = S("Unix");
        #endif

        if (PlatformsArray.Num > 0)
        {
            bool bAnyPlatformMatch = false;
            for each_str (s, PlatformsArray)
            {
                String Trimmed = String_EatSpaces(*s);

                bool bMatch = String_IsEqual(Trimmed, HostPlatform, false);
                if (bMatch)
                {
                    bAnyPlatformMatch = true;
                    break;
                }

                StringArray AdditionalPlatforms = String_ParseIntoArray(&Scratch, HostPlatform, ' ', 0, 128);
                for each_str (p, AdditionalPlatforms)
                {
                    bMatch = String_IsEqual(Trimmed, *p, false);
                    if (bMatch)
                    {
                        bAnyPlatformMatch = true;
                        break;
                    }
                }
            }

            if (!bAnyPlatformMatch)
            {
                #ifndef HOOD
                LOG_INLINE_ERROR("[ASSERTION FAILURE] %S can only be built on %S. You are on %S. Aborting build...\n", BuildFileName, PlatformsLogString, S(PLATFORM_STRING));
                #else
                LOG_ERROR("yo u cant build on dis platform nigga\n");
                #endif

                StringArray AdditionalPlatforms = String_ParseIntoArray(&Scratch, HostPlatform, ' ', 0, 128);
                for each_str (p, AdditionalPlatforms)
                {
                    if (LogCustomErrorMessage(ExpandedVariablesDB, S("Platform"), *p, true))
                    {
                        break;
                    }
                }

                return 1;
            }
        }

        StringLocal(ArchitecturesLogString, 128);
        {
            u8 i = 0;
            for each_str_i (i, a, ArchitecturesArray)
            {
                String_Append(&ArchitecturesLogString, *a);
                if (ArchitecturesArray.Num > 1 && i != ArchitecturesArray.Num-1)
                {
                    if (i == ArchitecturesArray.Num-2)
                    {
                        String_Append(&ArchitecturesLogString, S(" and "));
                    }
                    else
                    {
                        String_AppendChar (&ArchitecturesLogString, ',');
                        String_AppendSpace(&ArchitecturesLogString);
                    }
                }
            }
        }

        if (ArchitecturesArray.Num > 0)
        {
            bool bAnyArchMatch = false;
            for each_str (S, ArchitecturesArray)
            {
                String Trimmed = String_EatSpaces(*S);

                bool bMatch = String_IsEqual(Trimmed, S(CPU_ARCHITECTURE_STRING), false);
                if (bMatch)
                {
                    bAnyArchMatch = true;
                    break;
                }

                StringArray AdditionalArchs = String_ParseIntoArray(&Scratch, S(CPU_ARCHITECTURE_STRING_EX), '|', 0, 128);
                for each_str (p, AdditionalArchs)
                {
                    bMatch = String_IsEqual(Trimmed, *p, false);
                    if (bMatch)
                    {
                        bAnyArchMatch = true;
                        break;
                    }
                }
            }

            if (!bAnyArchMatch)
            {
                #ifndef HOOD
                LOG_INLINE_ERROR("[ASSERTION FAILURE] %S can only be built on %S architectures. You are on %S. Aborting build...\n", BuildFileName, ArchitecturesLogString, S(CPU_ARCHITECTURE_STRING));
                #else
                LOG_ERROR("yo u cant build on dis platform nigga\n");
                #endif

                StringArray AdditionalArchs = String_ParseIntoArray(&Scratch, S(CPU_ARCHITECTURE_STRING_EX), '|', 0, 128);
                for each_str (p, AdditionalArchs)
                {
                    if (LogCustomErrorMessage(ExpandedVariablesDB, S("Arch"), *p, true))
                    {
                        break;
                    }
                }

                return 1;
            }
        }

        if (AssertWorkingDirectory.Length > 0)
        {
            // did we get a relative directory?
            #if PLATFORM_WINDOWS
            bool bDriveSymbol = String_IndexOfChar(AssertWorkingDirectory, ':', NULL);
            #else
            bool bDriveSymbol = AssertWorkingDirectory.Data[0] == '/';
            #endif

            bool bRelative = !bDriveSymbol;

            StringLocal(AssertPath, MAX_PATH_LENGTH);

            if (bRelative)
            {
                u32 LastSlash = 0;
                if (String_IndexOfLastPathSlash(BuildFilePathFull, &LastSlash))
                {
                    String_Append(&AssertPath, StrSlice(BuildFilePathFull.Data, LastSlash+1));
                    String_Append(&AssertPath, AssertWorkingDirectory);
                }
            }
            else
            {
                String_Copy(&AssertPath, AssertWorkingDirectory);
            }

            Filesystem_ConvertRelativeToAbsolutePath(&AssertPath);
            String_EatPathSeparatorsInlineFromEnd(&AssertPath);

            if (AssertPath.Length > 0 && !String_IsEqual(WorkingPath, AssertPath, false))
            {
                #ifndef HOOD
                LOG_INLINE_ERROR("[ASSERTION FAILURE] %S must be ran from this directory -> \"%S\"\n                    but we are in \"%S\". Aborting build...\n", BuildFileName, AssertPath, WorkingPath);
                #else
                LOG_ERROR("yo we cant run from this dir cuh \"%S\" you gotta run from \"%S\"", WorkingPath, AssertPath);
                #endif

                return 1;
            }
        }

        if (CompilersArray.Num > 0)
        {
            bool bAnyCompilerMatch = false;
            for each_str (S, CompilersArray)
            {
                String Trimmed = String_EatSpaces(*S);

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
                            LOG("      or Compiler %S", *a);
                        else
                            LOG("         Compiler %S", *a);
                    }
                }

                #else
                LOG_ERROR("yo dis compiler program \"%S\" cant be used cuh", CompilerProgram);
                #endif

                if (LogCustomErrorMessage(ExpandedVariablesDB, S("Compiler"), CompilerProgram, false))
                {
                    LOG_LINE_BREAK();
                }

                return 1;
            }
        }

        for each_str (S, ProgramsArray)
        {
            String Trimmed = String_EatSpaces(*S);

            bool bFound = Platform_FindProgram(Trimmed);

            if (!bFound)
            {
                #ifndef HOOD
                LOG_INLINE_ERROR("[ASSERTION FAILURE] Program \"%S\" does not exist. Make sure that \"%S\" is installed and that its directory has been set in the path environment variable. Aborting build...\n\n", Trimmed, Trimmed);
                #else
                LOG_ERROR("yo dis program \"%S\" don exist cuh. need to be installed and set in da path ma nigga", Trimmed);
                #endif

                if (LogCustomErrorMessage(ExpandedVariablesDB, S("Program"), Trimmed, false))
                {
                    LOG_LINE_BREAK();
                }
                
                LogPathEnvVarTutorialSteps();

                return 1;
            }
        }

        for each_str (S, EnvVarsArray)
        {
            String Trimmed = String_EatSpaces(*S);
            Trimmed = String_EatSpacesFromEnd(Trimmed);

            bool bFound = Platform_DoesEnvironmentVariableExist(Trimmed);

            if (!bFound)
            {
                #ifndef HOOD
                LOG_INLINE_ERROR("[ASSERTION FAILURE] Environment variable \"%S\" does not exist. Aborting build...\n\n", Trimmed);
                #else
                LOG_ERROR("yo da environment var \"%S\" don exist cuh. need to be setup n' shit ma nigga\n", Trimmed);
                #endif

                if (LogCustomErrorMessage(ExpandedVariablesDB, S("Env"), Trimmed, false))
                {
                    LOG_LINE_BREAK();
                }

                LogRegularEnvVarTutorialSteps();

                return 1;
            }
        }

        for each_str (S, BuildVarsArray)
        {
            String Trimmed = String_EatSpaces(*S);

            bool bFound = DoesBuildVarExist(VariablesDB, Trimmed);

            if (!bFound)
            {
                #ifndef HOOD
                LOG_INLINE_ERROR("[ASSERTION FAILURE] Build variable \"%S\" does not exist. Aborting build...\n", Trimmed);
                #else
                LOG_ERROR("yo da build var \"%S\" don exist cuh. dat shit not there nigga", Trimmed);
                #endif

                return 1;
            }
        }
    }

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

    Clock DependencyBuildClock;
    Clock_Start(&DependencyBuildClock);

    // run build depenencies
    bool bRanAnyDependencies = false;
    for each (FileVariable, Var, ExpandedVariablesDB)
    {
        if (String_IsEqual(Var.Name, S("Depend"), false) ||
            String_IsEqual(Var.Name, S("Depends"), false))
        {
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
            if (String_IndexOfFirstWhitespace(Value, &SpaceIndex))
            {
                BuildFile = StrSlice(Value.Data, SpaceIndex);
            }
            else
            {
                BuildFile = Value;

                // if someone wants to not specify a build file, they can specify the path instead
                if (String_CountPathSeparators(BuildFile) > 0)
                {
                    BuildFile = String_Null();
                    Directory = Value;
                    bDirectoryOnly = true;
                }
            }

            // circular dependency. abort, this is bad...
            if (String_IsValid(CameFromBuildFile))
            {
                u32 Dot = 0;
                String_IndexOfLastChar(CameFromBuildFile, '.', &Dot);
                if (String_IsEqual(BuildFile, StrSlice(CameFromBuildFile.Data, Dot), false))
                {
                    LOG_ERROR("Circular build dependency. We came from \"%S\" but \"%S\" is trying to build \"%S\", which is circular and doesn't make sense", CameFromBuildFile, BuildFileName, CameFromBuildFile);
                    return 1;
                }
            }

            StringLocal(CustomWorkingPath, MAX_PATH_LENGTH);

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

                String_EatPathSeparatorsInlineFromEnd(&CustomPath);
                String_ConvertSlashToPlatformSlash(&CustomPath);

                if (Filesystem_IsPathRelative(CustomPath))
                {
                    String_BuildPath(&CustomWorkingPath, WorkingPath, CustomPath);
                    Filesystem_ConvertRelativeToAbsolutePath(&CustomWorkingPath);
                }
                else
                {
                    String_Copy(&CustomWorkingPath, CustomPath);
                }
            }
            else
            {
                if (bDirectoryOnly)
                {
                    if (Filesystem_IsPathRelative(Directory))
                    {
                        String_BuildPath(&CustomWorkingPath, WorkingPath, Directory);
                        Filesystem_ConvertRelativeToAbsolutePath(&CustomWorkingPath);
                    }
                    else
                    {
                        String_Copy(&CustomWorkingPath, Directory);
                    }
                }
                else
                {
                    String_Copy(&CustomWorkingPath, WorkingPath);
                }
            }

            String_EatPathSeparatorsInlineFromEnd(&CustomWorkingPath);

            StringLocal(BuildFileNameWithExt, 128);
            if (!bDirectoryOnly)
            {
                String_Append(&BuildFileNameWithExt, BuildFile);

                if (!String_EndsWith(BuildFile, S(".build"), false))
                    String_Append(&BuildFileNameWithExt, S(".build"));
            }

            //void* ArenaMemory = Platform_MemAllocZero(Kibibytes(512));
            /*
            if (!ArenaMemory)
            {
                LOG_ERROR("Failed to allocate memory from the operating system for %S", BuildFileNameWithExt);
                return 1;
            }
            */

            LinearAllocator NewArena = {0};
            char ArenaMemory[Kilobytes(512)] = {0};
            LinearAllocator_Create(Kibibytes(512), ArenaMemory, &NewArena);

            StringList List = String_SplitIntoList(&NewArena, SpecifiedParams, ' ', true);
            u8 Num = 0;
            for each_str_list (List)
            {
                Num++;
            }

            StringArray NewParams = StringArray_Null();

            if (Num > 0)
            {
                NewParams.List = LinearAllocator_Allocate(&NewArena, sizeof(String) * Num);
                NewParams.Num = Num;

                u8 i = 0;
                for each_str_list (List)
                {
                    NewParams.List[i] = It.String;
                    i++;
                }
            }
            
            LOG("Depend -> %S\n", bDirectoryOnly ? Directory : BuildFileNameWithExt);
            bRanAnyDependencies = true;

            StringLocal(NewBuildFilePath, MAX_PATH_LENGTH);

            BuildFileDirectoryIteratorData Data = {0};
            Data.bNoBuildFileSpecifiedInCmd = false;
            Data.BuildFileIndex = -1;
            Data.RootPathIndex = -1;
            Data.Name = &BuildFileNameWithExt;
            Data.Path = &NewBuildFilePath;
            Data.Arguments = NewParams;

            Filesystem_IterateDirectory_Ex(CustomWorkingPath, BuildFileDirectoryIterator, true, &Data);

            if (!Data.bFoundBuildFile)
            {
                if (!bDirectoryOnly)
                {
                    LOG_ERROR("Failed to find %S in %S", BuildFileNameWithExt, CustomWorkingPath);
                }

                return 1;
            }

            StringLocal(Path, MAX_PATH_LENGTH);
            String_BuildPath(&Path, CustomWorkingPath, NewBuildFilePath);

            FileHandle f = {0};
            if (!Filesystem_Open(Path, FileMode_Read, &f))
            {
                #ifndef HOOD
                LOG_ERROR("Failed to open build file \"%S\" for reading", Path);
                #else
                LOG_ERROR("wtf, cant read this shit man, think the path to the build file is wrong or smthg homie. this is what i got: %S", Path);
                #endif

                return 1;
            }

            PlatformMutex NewMutex = {0};
            u32 ExitCode = BuildTarget(&NewArena, f, &NewMutex, CustomWorkingPath, NewParams, BuildFileName, -1, -1);
            
            // we're intentionally not releasing build mutexes on dependency builds because
            // we dont want others to try and build a dependency by itself (unrelated to this build)
            // when this process has not finished. This is to make things more robust against compiler/file conflicts and invalid state.

            // once this process dies then the OS will clean these mutexes automatically
            // people can pass in -no-mutex to bypass the build mutex feature

            // if (NewMutex.Handle) Platform_ReleaseMutex(&NewMutex);

            Filesystem_Close(&f);

            LinearAllocator_Destroy(&NewArena);
            //Platform_MemFree(ArenaMemory);

            if (ExitCode == 2)
            {
                if (!bIsRebuild && !bNoRebuildOnDependencyChange)
                {
                    LOG("\nDependency \"%S\" was modified. Forcing rebuild...", BuildFileNameWithExt);
                    bIsRebuild = true;
                }
            }
            else if (ExitCode != 0)
            {
                #ifndef HOOD
                LOG_ERROR("Dependency build \"%S\" failed. Aborting build...", BuildFileNameWithExt);
                #else
                LOG_ERROR("brah wtf, depndncy buil faild nigga");
                #endif

                return 1;
            }

            LOG_LINE_BREAK();
        }
    }

    if (bRanAnyDependencies && !bIsClean)
    {
        Clock_Tick(&DependencyBuildClock);
        LOG("[All build dependencies complete. Continuing with %S]\n", BuildFileName);
    }

    u16 NumPreBuildCmds = 0;
    u16 NumPostBuildCmds = 0;

    // TODO: time this
    for each (FileVariable, Var, ExpandedVariablesDB)
    {
        if (String_StartsWith(Var.Name, S("PreBuild"), false))
        {
            NumPreBuildCmds++;
        }
    }

    if (NumPreBuildCmds > 0 && !bIsClean)
    {
        #ifndef HOOD
        LOG("Running pre build commands...");
        #else
        LOG("cool mang, gonna run some pre build cmds...");
        #endif

        // run pre build commands (if specified)
        for each (FileVariable, Var, ExpandedVariablesDB)
        {
            if (String_StartsWith(Var.Name, S("PreBuild"), false))
            {
                u32 ExitCode = 0;
                bool bResult = Internal_ExecuteBuildCmd(WorkingPath, Var.Name, Var.Value, Var.bHasSpecial, &ExitCode);
                if (!bResult)
                {
                    #ifndef HOOD
                    LOG_ERROR("Pre-build command exited with a failure result: %u", ExitCode);
                    #else
                    LOG_ERROR("brah wtf, gon have to stop you there nigga. da command we jus run fuck'n failed on me nigga");
                    #endif
                    return 1;
                }
            }
        }

        LOG_LINE_BREAK();
    }

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

                if (LogCustomErrorMessage(ExpandedVariablesDB, S("Lib"), TrimmedCopy, false))
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

    String SourceDirectory       = GetVariableValue(ExpandedVariablesDB, S("SourceDirectory"));
    String BuildDirectory        = GetVariableValue(ExpandedVariablesDB, S("BuildDirectory"));
    String IntermediateDirectory = GetVariableValue(ExpandedVariablesDB, S("IntermediateDirectory"));

    String_ConvertSlashToPlatformSlash(&SourceDirectory);
    String_ConvertSlashToPlatformSlash(&IntermediateDirectory);
    String_ConvertSlashToPlatformSlash(&BuildDirectory);

    StringLocal(BuildBaseDirectory, MAX_PATH_LENGTH);
    String_BuildPath(&BuildBaseDirectory, WorkingPath, BuildDirectory);
    String_AppendPathSeparator(&BuildBaseDirectory);
    Filesystem_ConvertRelativeToAbsolutePath(&BuildBaseDirectory);

    StringLocal(IntermediateBaseDirectory, MAX_PATH_LENGTH);
    String_BuildPath(&IntermediateBaseDirectory, WorkingPath, IntermediateDirectory);
    String_AppendPathSeparator(&IntermediateBaseDirectory);
    Filesystem_ConvertRelativeToAbsolutePath(&IntermediateBaseDirectory);

    StringLocal(SourceDir, MAX_PATH_LENGTH);
    String_BuildPath(&SourceDir, WorkingPath, SourceDirectory);
    String_AppendPathSeparator(&SourceDir);
    Filesystem_ConvertRelativeToAbsolutePath(&SourceDir);

    const bool bBuildDirSameAsSource        = String_IsEqual(BuildBaseDirectory, SourceDir, false);
    const bool bIntermediateDirSameAsSource = String_IsEqual(IntermediateBaseDirectory, SourceDir, false);

    const bool bDidIntermediateDirectoryExist = Filesystem_DoesDirectoryExist(IntermediateBaseDirectory);
    const bool bDidBuildDirectoryExist        = Filesystem_DoesDirectoryExist(BuildBaseDirectory);

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

            return 1;
        }

        // check if the given LibraryDirectories exist
        // TODO: verify if this is a good idea...
        StringList DirList = String_SplitIntoList(&Scratch, LibraryDirectories, ' ', true);
        for each_str_list (DirList)
        {
            StringLocal(DirPath, MAX_PATH_LENGTH);

            StringLocal(DirCopy, MAX_PATH_LENGTH);
            String_SanitizeQuotes(&DirCopy, It.String);

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

            Filesystem_ConvertRelativeToAbsolutePath(&DirPath);

            if (!Filesystem_DoesDirectoryExist(DirPath))
            {
                #ifndef HOOD
                LOG_ERROR("%S: Given library directory \"%S\" does not exist. Aborting build...", BuildFileName, DirPath);
                #else
                LOG_ERROR("yo dis library path \"%S\" dont exist cuhh", DirPath);
                #endif

                return 1;
            }
        }
    }

    //LinearAllocator_Create(Kibibytes(512), NULL, &GSourceFilePathAllocator); // todo: don't do this, it's very limiting
    //GSourceFiles = Array_Reserve(SourceFileData, 32);
    //GHeaderFiles = Array_Reserve(SourceFileData, 32);

    //Filesystem_IterateDirectory(SourceDir, SourceFileDirectoryIterator, true);

    // also include outside source directories if specified
    /*
    if (String_IsValid(OutsideSourceDirectories))
    {
        StringArray Dirs = String_ParseIntoArray(Scratch_Search.Allocator, OutsideSourceDirectories, ' ', 0, 128);
        for each_str (Dir, Dirs)
        {
            StringLocal(DirCopy, MAX_PATH_LENGTH);
            String_Copy(&DirCopy, *Dir);
            String_EatPathSeparatorsInlineFromEnd(&DirCopy);
            String_AppendPathSeparator(&DirCopy);
            String_ConvertSlashToPlatformSlash(&DirCopy);

            Filesystem_IterateDirectory(DirCopy, SourceFileDirectoryIterator, true);
        }
    }
    */

    StringList WhitelistArray    = String_SplitIntoList(Arena, IncludedSourceFiles, ' ', true);
    StringList BlacklistArray    = String_SplitIntoList(Arena, ExcludedSourceFiles, ' ', true);
    StringList WhitelistDirArray = String_SplitIntoList(Arena, IncludedSourceDir, ' ', true);
    StringList BlacklistDirArray = String_SplitIntoList(Arena, ExcludedSourceDir, ' ', true);

    // assert that these files exist
    // TODO: black list files and directories too
    /*
    for each_str (File, WhitelistArray)
    {
        if (String_IndexOfChar(*File, '*', NULL))
        {
            continue;
        }

        bool bExists = false;
        for each (SFile, GSourceFiles)
        {
            String TrimmedFileName = String_Null();
            u32 SlashIndex = 0;
            if (String_IndexOfLastPathSlash(SFile.RelativePath, &SlashIndex))
            {
                u32 Len = SFile.RelativePath.Length - SlashIndex;
                TrimmedFileName = StrCompC(SFile.RelativePath.Data + (SlashIndex+1), Len-1, Len);
            }
            else
            {
                TrimmedFileName = SFile.RelativePath;
            }
        
            if (String_IsEqual(*File, TrimmedFileName, true) ||
                String_IsEqual(*File, SFile.RelativePath, true))
            {
                bExists = true;
                break;
            }
        }

        if (!bExists)
        {
            #ifndef HOOD
            LOG_ERROR("Given source file \"%S\" from \"IncludedSourceFiles\" does not exist. Aborting build...", *File);
            #else
            LOG_ERROR("yo dis file \"%S\" dont exist cuhh", *File);
            #endif

            return 1;
        }
    }
    */

    struct SourceCountData
    {
        u32 NumSources;
        u32 NumAsmSources;
        u32 NumHeaders;
        u32 NumRcSources;
        String* FirstSourceFileName;
        String WorkingDirectory;
        String SourceDirectory;
        StringList WhitelistArray;
        StringList BlacklistArray;
        StringList WhitelistDirArray;
        StringList BlacklistDirArray;
        bool bHasCppFiles;
        bool bIsPCHBuild;
    };

    StringLocal(FirstSourceFileName, 256);
    struct SourceCountData CountData = { 0, 0, 0, 0, &FirstSourceFileName, WorkingPath, SourceDirectory, WhitelistArray, BlacklistArray, WhitelistDirArray, BlacklistDirArray, false, AssemblyType == AssemblyType_PCH};

    Filesystem_IterateDirectory_Ex(SourceDir, SourceFileCounterDirectoryIterator, true, &CountData);

    const u32 NumSources = AssemblyType == AssemblyType_PCH ? CountData.NumHeaders : CountData.NumSources;

    u32 NumCompiled = 0;

    if (NumSources == 0)
    {
        if (bQuietBuild) Logging_Enable();

        #ifndef HOOD
        LOG("Nothing to compile");
        #else
        LOG("no work to do homie");
        #endif

        goto End;
    }

    // use the first source file as the assembly name (if none provided or if "untitled" was set)
    //if (Array_Num(GSourceFiles) == 1)
    if (NumSources == 1)
    {
        if (!String_IsValid(AssemblyName) ||
            String_IsEqual(AssemblyName, S("Untitled"), false))
        {
            String TrimmedFileName = FirstSourceFileName;

            // todo: left chop function
            u32 DotIndex = 0;
            if (String_IndexOfLastChar(TrimmedFileName, '.', &DotIndex))
            {
                TrimmedFileName.Length -= TrimmedFileName.Length-DotIndex;
            }

            AssemblyName = TrimmedFileName;
        }
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
                struct SourceCountData CountData_File = {0};
                if (Filesystem_Read(f, sizeof(struct SourceCountData), &CountData_File, NULL))
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

                        if (CountData_File.NumAsmSources > CountData.NumAsmSources)
                        {
                            LOG("    %u assembly file(s) removed", CountData_File.NumAsmSources - CountData.NumAsmSources);
                        }
                        else if (CountData.NumAsmSources > 0)
                        {
                            LOG("    %u assembly file(s) added", CountData.NumAsmSources - CountData_File.NumAsmSources);
                        }

                        if (CountData_File.NumHeaders > CountData.NumHeaders)
                        {
                            LOG("    %u header file(s) removed", CountData_File.NumHeaders - CountData.NumHeaders);
                        }
                        else if (CountData.NumHeaders > 0)
                        {
                            LOG("    %u header file(s) added", CountData.NumHeaders - CountData_File.NumHeaders);
                        }

                        if (CountData_File.NumRcSources > CountData.NumRcSources)
                        {
                            LOG("    %u resource file(s) removed", CountData_File.NumRcSources - CountData.NumRcSources);
                        }
                        else if (CountData.NumRcSources > 0)
                        {
                            LOG("    %u resource file(s) added", CountData.NumRcSources - CountData_File.NumRcSources);
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
            Filesystem_Write(f, sizeof(struct SourceCountData), &CountData, NULL);
            Filesystem_Close(&f);
        }
    }


    StringLocal(AsmCompilerPath, MAX_PATH_LENGTH);

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
            return 1;
        }
    }

    // does the asm program exist on the user's machine
    if (!bExplicitAsmPath && CountData.NumAsmSources > 0)
    {
        bool bCompilerProgramFound = Platform_FindProgram_Ex(AsmProgram, &AsmCompilerPath);

        if (!bCompilerProgramFound && bNoAsmCompilerProgramExplicityGiven)
        {
            const String AsmPrograms_Default[] =
            {
                S("nasm"),
                S("yasm"),
            };

            const String AsmPrograms_MSVC[] =
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

        if (!bCompilerProgramFound)
        {
            if (bNoAsmCompilerProgramExplicityGiven)
            {
                // todo: prettier log messaging
                #if PLATFORM_WINDOWS
                LOG_ERROR(
                    "You don't seem to have an assember installed on your machine."
                    " Install either \"nasm\", \"yasm\" or \"ml (MSVC)\" and add to the path environment"
                    " before using RiftBuild, as we require a working assembler program to function properly. Aborting build...\n");
                #else
                LOG_ERROR(
                    "You don't seem to have an assmebler installed on your machine."
                    " Install either \"nasm\" or \"yasm\" and add to the PATH environment"
                    " before using RiftBuild, as we require a working assembler program to function properly. Aborting build...\n");

                #endif

                LogPathEnvVarTutorialSteps();
                    
                return 1;
            }

            #ifndef HOOD
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
            #else
            LOG_ERROR(
                "yo dat assembler program \"%S\" don exist cuh."
                " need to be installed and set in da path ma nigga", AsmProgram);
            #endif

            return 1;
        }
        
        {
            LinearAllocator Scratch = *Arena;
            StringArray AssemblersArray    = String_ParseIntoArray(&Scratch, AssertAssemblers, ' ', 0, 128);
            if (AssemblersArray.Num > 0)
            {
                bool bAnyAssemblerMatch = false;
                for each_str (S, AssemblersArray)
                {
                    String Trimmed = String_EatSpaces(*S);

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
                                LOG("      or Assembler %S", *a);
                            else
                                LOG("         Assembler %S", *a);
                        }
                    }

                    #else
                    LOG_ERROR("yo dis assembler program \"%S\" cant be used cuh", AsmProgram);
                    #endif

                    if (LogCustomErrorMessage(ExpandedVariablesDB, S("Assembler"), AsmProgram, false))
                    {
                        LOG_LINE_BREAK();
                    }

                    return 1;
                }
            }
        }
    }

    // automatically switch to a c++ compiler if we have c++ source code files
    if (bNoCompilerProgramExplicityGiven)
    {
        const String CppCompilers[] =
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

            //if (!bHasCppFiles)
            {
                // also include outside source directories if specified
                /*
                if (String_IsValid(OutsideSourceDirectories))
                {
                    StringArray Dirs = String_ParseIntoArray(Scratch_Search.Allocator, OutsideSourceDirectories, ' ', 0, 128);
                    for each_str (Dir, Dirs)
                    {
                        StringLocal(DirCopy, MAX_PATH_LENGTH);
                        String_Copy(&DirCopy, *Dir);
                        String_EatPathSeparatorsInlineFromEnd(&DirCopy);
                        String_AppendPathSeparator(&DirCopy);
                        String_ConvertSlashToPlatformSlash(&DirCopy);

                        Filesystem_IterateDirectory_Ex(DirCopy, DetectCppFilesDirectoryIterator, true, &bHasCppFiles);
                        if (bHasCppFiles)
                            break;
                    }
                }
                */
            }

            if (bHasCppFiles)
            {
                CompilerProgram = CompilerToUse;
                String_Copy(&CompilerPath, NewCompilerPath);
            }
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

    if (!bExportingSomething)
    {
        // force a rebuild if the .build file has been modified
        if (!bIsRebuild && !bIsClean && bFoundBuildFile)
        {
            // build the full assembly path
            StringLocal(AssemblyPath, MAX_PATH_LENGTH);

            bool bAnyExist = false;

            if (AssemblyType == AssemblyType_PCH)
            {
                const String PCHExts[] =
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
                bIsRebuild = true;
                
                StringLocal(Temp, MAX_PATH_LENGTH);
                String_BuildPath(&Temp, WorkingPath, BuildDirectory);
                if (Filesystem_DoesDirectoryExist(Temp))
                {
                    // only say this if we have a build directory but no assembly file
                    LOG("Assembly file \"%S\" does not exist. Forcing rebuild...\n", AssemblyPath);
                }
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
        if (!bIsRebuild && !bIsClean && bFoundBuildFile && Array_Num(IncludeFiles) > 0)
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
                        Filesystem_GetFilePath(Include, &Path);

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
            Filesystem_Close(&File);

        // force a rebuild if either the build directory or the intermediate directory is missing
        if (!bIsRebuild && !bIsClean)
        {
            if (!bDidBuildDirectoryExist || !bDidIntermediateDirectoryExist)
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
                Filesystem_Open(OutputDebugFile, FileMode_Read, &h);
                StringLocal(SavedCmdLine, 2048);
                Filesystem_ReadLine(h, &SavedCmdLine);
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

        // force a rebuild if any of the .h files have been modified after a build
        if (!bIsRebuild && !bIsClean)
        {
            StringLocal(AssemblyPath, MAX_PATH_LENGTH);
            String_BuildPath(&AssemblyPath, WorkingPath, BuildDirectory, AssemblyNameWithExt);

            u64 AssemblyFileTime = Filesystem_GetLastWriteTime(AssemblyPath);

            if (AssemblyFileTime > 0)
            {
                struct HeaderIterData
                {
                    u64 AssemblyFileTime;
                    bool* bShouldRebuild;
                };

                struct HeaderIterData Data = { AssemblyFileTime, &bIsRebuild };
                Filesystem_IterateDirectory_Ex(SourceDir, HeaderFileRebuildCheckDirectoryIterator, true, &Data);

                /*
                for each (File, GHeaderFiles)
                {
                    u64 HeaderFileTime = Filesystem_GetLastWriteTime(File.FullPath);

                    if (HeaderFileTime >= AssemblyFileTime)
                    {
                        bIsRebuild = true;

                        #ifndef HOOD
                        LOG("Header file \"%S\" has been modified since last build. Forcing rebuild...", File.FullPath);
                        #else
                        LOG("yo homie, dis header file \"%S\" was recently changed. gon force a rebuild...", File.FullPath);
                        #endif

                        LOG_LINE_BREAK();

                        break;
                    }
                }
                */
            }
        }

        if (bIsClean)
        {
            bIsRebuild = false;
        }

        if (bIsClean || bIsRebuild)
        {
            bool bCleanedSomething = false;

            const String Exts[] =
            {
                S(""),
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
            String Wildcard = S("");
            const String WildcardS = S("S");
            #endif

            // Delete all [Assembly]*.* files
            if (bDidBuildDirectoryExist)
            {
                #ifndef HOOD
                LOG("Cleaning %S%S%S", BuildBaseDirectory, AssemblyName, Wildcard);
                #else
                LOG("cleaning dis shit %S%S%S", BuildBaseDirectory, AssemblyName, Wildcard);
                #endif

                if (bBuildDirSameAsSource)
                {
                    for each_static (String, e, Exts)
                    {
                        StringLocal(AssemblyWildcard, MAX_PATH_LENGTH);
                        String_Append(&AssemblyWildcard, AssemblyName);
                        String_Append(&AssemblyWildcard, e);
                        Filesystem_DeleteFiles(BuildBaseDirectory, AssemblyWildcard, true);
                    }
                }
                else
                {
                    StringLocal(AssemblyWildcard, MAX_PATH_LENGTH);
                    String_Append(&AssemblyWildcard, AssemblyName);
                    String_Append(&AssemblyWildcard, Wildcard);
                    Filesystem_DeleteFiles(BuildBaseDirectory, AssemblyWildcard, true);
                    String_Empty(&AssemblyWildcard);
                    String_Append(&AssemblyWildcard, AssemblyName);
                    String_Append(&AssemblyWildcard, WildcardS);
                    Filesystem_DeleteFiles(BuildBaseDirectory, AssemblyWildcard, true);
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
                    Filesystem_DeleteDirectory(AppBundlePath);

                    String_Empty(&AppBundleName);
                    String_Append(&AppBundleName, AssemblyName);
                    String_Append(&AppBundleName, S(".app"));
                    String_Empty(&AppBundlePath);
                    String_BuildPath(&AppBundlePath, WorkingPath, BuildDirectory, AppBundleName);
                    LOG("Cleaning %S", AppBundlePath);
                    Filesystem_DeleteDirectory(AppBundlePath);
                }
                #endif

                bCleanedSomething = true;
            }

            // Delete intermediate directory based on given source directory
            if (bDidIntermediateDirectoryExist)
            {
                Wildcard = S("*");

                #ifndef HOOD
                LOG("Cleaning %S%S", IntermediateBaseDirectory, Wildcard);
                #else
                LOG("cleaning dis shit %S%S", IntermediateBaseDirectory, Wildcard);
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
                            Filesystem_DeleteFiles(IntermediateBaseDirectory, AssemblyWildcard, true);
                            Filesystem_DeleteFiles(IntermediateBaseDirectory, e, true);
                        }
                    }
                }
                else
                {
                    Filesystem_DeleteFiles(IntermediateBaseDirectory, Wildcard, true);
                }

                bCleanedSomething = true;
            }

            if (bCleanedSomething)
                LOG_LINE_BREAK();

            if (!bIsRebuild)
            {
                return 0;
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
            Filesystem_WriteLine(f, S("\n"), NULL);

            // TODO: Speed: fill a buffer first, then write to file
            for each (FileVariable, v, ExpandedVariablesDB)
            {
                StringLocal(Line, 4096);
                String_Append(&Line, v.Name);
                String_AppendSpace(&Line);
                String_Append(&Line, v.Value);
                String_AppendChar(&Line, '\n');
                Filesystem_WriteLine(f, Line, NULL);
            }
        }

        Filesystem_Close(&f);
    }

    if (Array_Num(Messages) > 0)
    {
        for each (String, m, Messages)
        {
            LOG("%S", m);
        }

        LOG_LINE_BREAK();
    }

    // find the appropriate rc program
    #if PLATFORM_WINDOWS
    String RCProgram = S("windres");
    String RCProgramFlags = S("");
    if (String_IsEqual(CompilerProgram, S("cl"), false))
    {
        RCProgram = S("rc");
        RCProgramFlags = S(" /nologo");
    }
    else if (String_IsEqual(CompilerProgram, S("clang"), false) ||
            String_IsEqual(CompilerProgram, S("clang++"), false ||
            String_IsEqual(CompilerProgram, S("clang-cl"), false)))
    {
        RCProgram = S("llvm-rc");
    }

    StringLocal(RCProgramPath, MAX_PATH_LENGTH);
    bool bHasRcProgram = Platform_FindProgram_Ex(RCProgram, &RCProgramPath);
    #endif

    if (!bExportingSomething)
    {
        if (bFoundBuildFile)
        {
            String Mode = GetCmdOptionValue(CmdOptionsDB, S("mode"));

            if (!String_IsValid(Mode))
                LOG("Build Configuration:");
            else
                LOG("Build Configuration: (%S)", Mode);

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

            const String AssemblyTypeStringTable[] =
            {
                S("None"),
                S("Executable"),
                S("Library"),
                S("Static Library"),
                S("Shared Library"),
                S("Pre Compiled Header"),
            };

            StringLocal(ExtInfo, 32);
            String_Format(&ExtInfo, S(" (%S)"), 32, Extension_Og);

            LOG("    Type:                 %S%S", AssemblyTypeStringTable[AssemblyType], Extension_Og.Length == 0 ? S("") : ExtInfo);

            LOG("    Version:              %S", Version);
            
            if (bExplicitCompilerPath)
                LOG("    Compiler:             %S", CompilerPath);
            else
                LOG("    Compiler:             %S -> \"%S\"", CompilerProgram, CompilerPath);

            if (CountData.NumAsmSources > 0)
            {
                if (bExplicitAsmPath)
                    LOG("    Assembler:            %S", AsmCompilerPath);
                else
                    LOG("    Assembler:            %S -> \"%S\"", AsmProgram, AsmCompilerPath);
            }

            #if PLATFORM_WINDOWS
            if (bHasRcProgram && (Icon.Length > 0 || CountData.NumRcSources > 0))
                LOG("    Resource Compiler:    %S -> \"%S\"", RCProgram, RCProgramPath);
            #endif

            if (CompilerFlags.Length > 0)      { LogBuildVariable(*Arena, VariablesDB, S("CompilerFlags"),      S("    Compiler Flags:       "), !bNoWordWrapLogging); }
            if (AssemblerFlags.Length > 0)     { LogBuildVariable(*Arena, VariablesDB, S("AssemblerFlags"),     S("    Assembler Flags:      "), !bNoWordWrapLogging); }
            if (IncludeFlags.Length > 0)       { LogBuildVariable(*Arena, VariablesDB, S("Includes"),           S("    Includes:             "), !bNoWordWrapLogging); }
            if (LinkerFlags.Length > 0)        { LogBuildVariable(*Arena, VariablesDB, S("LinkerFlags"),        S("    Linker Flags:         "), !bNoWordWrapLogging); }
            if (Libraries.Length > 0)          { LogBuildVariable(*Arena, VariablesDB, S("Libraries"),          S("    Libraries:            "), !bNoWordWrapLogging); }
            if (LibraryDirectories.Length > 0) { LogBuildVariable(*Arena, VariablesDB, S("LibraryDirectories"), S("    Library Directories:  "), !bNoWordWrapLogging); }
            if (Defines.Length > 0)            { LogBuildVariable(*Arena, VariablesDB, S("Defines"),            S("    Defines:              "), !bNoWordWrapLogging); }
            if (UnDefines.Length > 0)          { LogBuildVariable(*Arena, VariablesDB, S("UnDefines"),          S("    UnDefines:            "), !bNoWordWrapLogging); }
            if (LinkerDefines.Length > 0)      { LogBuildVariable(*Arena, VariablesDB, S("LinkerDefines"),      S("    Linker Defines:       "), !bNoWordWrapLogging); }

            LOG_LINE_BREAK();
        }
    }

    const String ExpandedCompilerFlags  = GetVariableValue(ExpandedVariablesDB, S("CompilerFlags"));
    const String ExpandedAssemblerFlags = GetVariableValue(ExpandedVariablesDB, S("AssemblerFlags"));
    const String ExpandedLinkerFlags    = GetVariableValue(ExpandedVariablesDB, S("LinkerFlags"));

    StringLocal(ExpandedIncludeFlags, 4096);
    StringLocal(ExpandedLibraries, 1024);
    StringLocal(ExpandedLibraryDirectories, 4096);
    StringLocal(ExpandedDefineFlags, 1024);
    StringLocal(ExpandedUnDefineFlags, 1024);
    StringLocal(ExpandedLinkerDefineFlags, 1024);

    StringLocal(FlagPrefix, 4);
    String_Append(&FlagPrefix, CompilerFlagPrefixSymbol);
    String_Append(&FlagPrefix, S("I"));

    ExpandPathFlags(*Arena, &ExpandedIncludeFlags, IncludeFlags, FlagPrefix, !bExportingSomething);

    FlagPrefix.Data[1] = 'l';
    if (String_IsEqual(CompilerProgram, S("cl"), false) ||
        String_IsEqual(CompilerProgram, S("msvc"), false)) // todo: something better
    {
        SuffixVariables(&ExpandedLibraries, Libraries, S(".lib"));
    }
    else
    {
        PrefixVariables(&ExpandedLibraries, Libraries, FlagPrefix, !bExportingSomething);
    }

    FlagPrefix.Data[1] = 'L';
    if (String_IsEqual(CompilerProgram, S("cl"), false) ||
        String_IsEqual(CompilerProgram, S("msvc"), false)) // todo: something better
    {
        ExpandPathFlags(*Arena, &ExpandedLibraryDirectories, LibraryDirectories, S("/LIBPATH:"), !bExportingSomething);
    }
    else
    {
        ExpandPathFlags(*Arena, &ExpandedLibraryDirectories, LibraryDirectories, FlagPrefix, !bExportingSomething);
    }

    FlagPrefix.Data[1] = 'D';
    PrefixVariables(&ExpandedDefineFlags, Defines, FlagPrefix, !bExportingSomething);
    PrefixVariables(&ExpandedLinkerDefineFlags, LinkerDefines, FlagPrefix, !bExportingSomething);

    FlagPrefix.Data[1] = 'U';
    PrefixVariables(&ExpandedUnDefineFlags, UnDefines, FlagPrefix, !bExportingSomething);

    if (!bExportingSomething)
    {
        LogNameValuePair(*Arena, S("Expanded Compiler  Flags: "), ExpandedCompilerFlags,      !bNoWordWrapLogging);
        LogNameValuePair(*Arena, S("Expanded Assembler Flags: "), ExpandedAssemblerFlags,     !bNoWordWrapLogging);
        LogNameValuePair(*Arena, S("Expanded Include   Flags: "), ExpandedIncludeFlags,       !bNoWordWrapLogging);
        LogNameValuePair(*Arena, S("Expanded Linker    Flags: "), ExpandedLinkerFlags,        !bNoWordWrapLogging);
        LogNameValuePair(*Arena, S("Expanded Library   Flags: "), ExpandedLibraries,          !bNoWordWrapLogging);
        LogNameValuePair(*Arena, S("Expanded Library   Paths: "), ExpandedLibraryDirectories, !bNoWordWrapLogging);
        LogNameValuePair(*Arena, S("Expanded Define    Flags: "), ExpandedDefineFlags,        !bNoWordWrapLogging);
        LogNameValuePair(*Arena, S("Expanded UnDefine  Flags: "), ExpandedUnDefineFlags,      !bNoWordWrapLogging);
        LogNameValuePair(*Arena, S("Expanded Linker  Defines: "), ExpandedLinkerDefineFlags,  !bNoWordWrapLogging);
    }

    Clock IconClock = {0};
    Clock ResourceCompileClock = {0};
    Clock BundleCompileClock = {0};

    u32 MaxLogicalCores = Platform_GetNumLogicalProcessors();
    u8 MaxCompilersAtOnce = (u8)MaxLogicalCores; // bound by max logical processors on the user's machine
    //LOG_INFO("Max logical cores: %u", MaxLogicalCores);

    // clamp to the min amount of source files vs cores
    u32 MinResult = Min(NumSources, (u32)MaxCompilersAtOnce);
    MaxCompilersAtOnce = (u8)Min(MinResult, (u32)UINT8_MAX);

    if (String_IsValid(MaxConcurrentCompilations))
    {
        u8 Num = 0;
        String_ToU8(MaxConcurrentCompilations, &Num);
        MaxCompilersAtOnce = Min(Num, (u8)MaxLogicalCores);   
    }

    if (bSingleThread)
    {
        MaxCompilersAtOnce = 1;
    }

    if (!String_IsEqual(BuildDirectory, S("."), false))
    {
        StringLocal(FullBuildDirectory, MAX_PATH_LENGTH);
        String_BuildPath(&FullBuildDirectory, WorkingPath, BuildDirectory);
        if (!Filesystem_DoesDirectoryExist(FullBuildDirectory))
        {
            if (!Filesystem_OpenDirectory(FullBuildDirectory))
                return 1;
        }
    }

    StringLocal(IntSrcDir, MAX_PATH_LENGTH);
    String_BuildPath(&IntSrcDir, IntermediateBaseDirectory, SourceDirectory);
    if (!Filesystem_DoesDirectoryExist(IntermediateBaseDirectory))
    {
        if (!Filesystem_OpenDirectory(IntermediateBaseDirectory))
            return 1;
    }

    BuildParams p = {0};
    p.Arena                         = Arena;
    p.CompilerProgram               = bExplicitCompilerPath ? CompilerPath : CompilerProgram;
    p.CompilerPath                  = CompilerPath;
    p.AsmProgram                    = AsmProgram;
    p.AsmPath                       = AsmCompilerPath;
    #if PLATFORM_WINDOWS
    p.RCProgram                     = RCProgram;
    p.RCProgramPath                 = RCProgramPath;
    p.RCProgramFlags                = RCProgramFlags;
    p.bHasRCProgram                 = bHasRcProgram;
    #endif
    p.Assembly                      = AssemblyName;
    p.AssemblyWithExt               = AssemblyNameWithExt;
    p.Extension                     = Extension;
    p.Extension_Og                  = Extension_Og;
    p.Type                          = AssemblyType;
    p.WhitelistFiles                = WhitelistArray;
    p.WhitelistDirectories          = WhitelistDirArray;
    p.BlacklistFiles                = BlacklistArray;
    p.BlacklistDirectories          = BlacklistDirArray;
    p.Processes                     = &Processes;
    //p.Pipes                         = &Pipes;
    p.RootDirectory                 = WorkingPath;
    p.SourceDirectory               = SourceDirectory;
    p.BuildDirectory                = BuildDirectory;
    p.IntermediateDirectory         = IntermediateDirectory;
    p.IntermediateBaseDirectory     = IntermediateBaseDirectory;
    p.PCHPath                       = PCHPath;
    p.PCHHeaderPath                 = PCHHeaderPath;
    p.MaxCompilersAtOnce            = MaxCompilersAtOnce;
    p.MaxErrors                     = MaxErrorsAllowed;
    p.bShouldWaitPerCompileProcess  = bShouldWaitPerCompileProcess;
    p.CompilerFlags                 = ExpandedCompilerFlags;
    p.AssemblerFlags                = ExpandedAssemblerFlags;
    p.LinkerFlags                   = ExpandedLinkerFlags;
    p.IncludeFlags                  = ExpandedIncludeFlags;
    p.DefineFlags                   = ExpandedDefineFlags;
    p.UnDefineFlags                 = ExpandedUnDefineFlags;
    p.LinkerDefineFlags             = ExpandedLinkerDefineFlags;
    p.Libraries                     = ExpandedLibraries;
    p.LibraryDirectories            = ExpandedLibraryDirectories;
    p.bIsAssemblyExe                = bIsAssemblyExe;
    p.bVerbose                      = bVerboseLog;
    p.TitleName                     = TitleName;
    p.InternalName                  = InternalName;
    p.CompanyName                   = CompanyName;
    p.Description                   = Description;
    p.Copyright                     = Copyright;
    p.Version                       = Version;
    p.NumSources                    = NumSources;
    p.NumHeaders                    = CountData.NumHeaders;
    p.NumRcSources                  = CountData.NumRcSources;
    p.bHasCppFiles                  = CountData.bHasCppFiles;


    // export feature
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
                    return 1;
                }

                char TempMemory[1024] = {0};
                LinearAllocator Temp = {0};
                LinearAllocator_Create(1024, TempMemory, &Temp);
                StringArray Vars = String_ParseIntoArray(&Temp, VarToList, ',', 0, 128);
            
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

                    if (bGenCompileCommandsJSON || bGenCompileCommandsJSONOneLine)
                    {
                        if (bQuietBuild) Logging_Enable();

                        LOG("Generating compile_commands.json ...");

                        bool bLast = CameFromBuildFile.Length == 0;

                        Clock c;
                        Clock_Start(&c);

                        if (!ExportCompileCommands(&p, ExpandedCompilerFlags, ExpandedIncludeFlags, ExpandedDefineFlags, ExpandedUnDefineFlags, bLast, bGenCompileCommandsJSONOneLine))
                        {
                            return 1;
                        }

                        Clock_Tick(&c);

                        if (bLast)
                        {
                            StringLocal(ExportTimeString, 32);
                            Clock_GetElapsedTime_ToString(&c, true, &ExportTimeString);
                            LOG("\nExport time: %S", ExportTimeString);

                            StringLocal(CompileCommandsPath, MAX_PATH_LENGTH);
                            String_BuildPath(&CompileCommandsPath, WorkingPath, S("compile_commands.json"));
                            LOG_SUCCESS("\n\"%S\"", CompileCommandsPath);
                        }

                        if (bQuietBuild) Logging_Disable();

                        bAnyExported = true;
                    }
                    else if (bGenPlist || bGenInfoPlist || bGenVersionPlist)
                    {
                        if (bQuietBuild) Logging_Enable();

                        StringLocal(ExportPath, MAX_PATH_LENGTH);
                        String_BuildPath(&ExportPath, WorkingPath, IntermediateDirectory, S("__Exports"));

                        if (!Filesystem_OpenDirectory(ExportPath))
                        {
                            return 1;
                        }

                        Clock c;
                        Clock_Start(&c);

                        if (bGenPlist || bGenInfoPlist)
                        {
                            LOG("Generating Info.plist ...");

                            StringLocal(PlistPath, MAX_PATH_LENGTH);
                            String_BuildPath(&PlistPath, ExportPath, S("Info.plist"));

                            if (!ExportInfoPlist(*Arena, &p, PlistPath, ExpandedVariablesDB, DoesBuildVarExist(ExpandedVariablesDB, S("Info.plist"))))
                            {
                                LOG_ERROR("Failed to export \"%S\". Aborting build...", PlistPath);
                                return 1;
                            }

                            LOG_SUCCESS("\n\"%S\"", PlistPath);
                        }

                        if (bGenPlist || bGenVersionPlist)
                        {
                            LOG("\nGenerating Version.plist ...");

                            StringLocal(PlistPath, MAX_PATH_LENGTH);
                            String_BuildPath(&PlistPath, ExportPath, S("Version.plist"));

                            if (!ExportVersionPlist(*Arena, &p, PlistPath, ExpandedVariablesDB, DoesBuildVarExist(ExpandedVariablesDB, S("Version.plist"))))
                            {
                                LOG_ERROR("Failed to export \"%S\". Aborting build...", PlistPath);
                                return 1;
                            }

                            LOG_SUCCESS("\n\"%S\"", PlistPath);
                        }

                        Clock_Tick(&c);

                        StringLocal(ExportTimeString, 32);
                        Clock_GetElapsedTime_ToString(&c, true, &ExportTimeString);
                        LOG("\nExport time: %S", ExportTimeString);

                        if (bQuietBuild) Logging_Disable();

                        bAnyExported = true;
                    }
                    else if (bGenPkgInfo)
                    {
                        if (bQuietBuild) Logging_Enable();

                        LOG("Generating PkgInfo ...");

                        StringLocal(ExportPath, MAX_PATH_LENGTH);
                        String_BuildPath(&ExportPath, WorkingPath, IntermediateDirectory, S("__Exports"));

                        if (!Filesystem_OpenDirectory(ExportPath))
                        {
                            return 1;
                        }

                        StringLocal(PkgInfoPath, MAX_PATH_LENGTH);
                        String_BuildPath(&PkgInfoPath, ExportPath, S("PkgInfo"));

                        Clock c;
                        Clock_Start(&c);

                        if (!ExportPkgInfo(&p, PkgInfoPath))
                        {
                            LOG_ERROR("Failed to export \"%S\". Aborting build...", PkgInfoPath);
                            return 1;
                        }

                        Clock_Tick(&c);

                        StringLocal(ExportTimeString, 32);
                        Clock_GetElapsedTime_ToString(&c, true, &ExportTimeString);
                        LOG("\nExport time: %S", ExportTimeString);

                        LOG_SUCCESS("\n\"%S\"", PkgInfoPath);

                        if (bQuietBuild) Logging_Disable();

                        bAnyExported = true;
                    }
                    else if (bGenVersionRc || bGenIconRc)
                    {
                        if (bQuietBuild) Logging_Enable();

                        LOG("Generating resource file ...");

                        StringLocal(ExportPath, MAX_PATH_LENGTH);
                        String_BuildPath(&ExportPath, WorkingPath, IntermediateDirectory, S("__Exports"));

                        if (!Filesystem_OpenDirectory(ExportPath))
                        {
                            return 1;
                        }

                        Clock c;
                        Clock_Start(&c);

                        StringLocal(RCPath, MAX_PATH_LENGTH);
                        String_BuildPath(&RCPath, ExportPath, bGenVersionRc ? S("version.rc") : S("icon.rc"));

                        if ((bGenVersionRc && !ExportVersionRC(&p, RCPath)) ||
                            (bGenIconRc && !ExportIconRC(&p, RCPath, IconFilePath)))
                        {
                            LOG_ERROR("Failed to export \"%S\". Aborting build...", RCPath);
                            return 1;
                        }

                        Clock_Tick(&c);

                        StringLocal(ExportTimeString, 32);
                        Clock_GetElapsedTime_ToString(&c, true, &ExportTimeString);
                        LOG("\nExport time: %S", ExportTimeString);

                        LOG_SUCCESS("\n\"%S\"", RCPath);

                        if (bQuietBuild) Logging_Disable();

                        bAnyExported = true;
                    }
                }
                
                if (!bAnyExported)
                {
                    LOG_INLINE("Nothing happened. No export logic was implemented for ");

                    i = 0;
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

                return 0;
            }
        }
    }

    // enforce copyright in all source files
    if (VariableHasSpecial(VariablesDB, S("Copyright")))
    {
        const String CopyrightString = GetVariableValue(ExpandedVariablesDB, S("Copyright"));
        if (CopyrightString.Length > 0)
        {
            struct { bool bSuccess; String Content; } AuxData = { true, CopyrightString };
            CompileData UserData = { EnforceCopyright, &p, NULL, 0, true, &AuxData};
            Filesystem_IterateDirectory_Ex(SourceDir, SourceFileDirectoryIterator, true, &UserData);

            if (!AuxData.bSuccess)
            {
                return 1;
            }
        }
    }

    Clock ExternalClock = {0};
    Clock_Start(&ExternalClock);

    // precompile step
    u16 NumPreCompileCmds = 0;
    u16 NumPostCompileCmds = 0;
    u16 NumPreLinkCmds = 0;
    u16 NumPostLinkCmds = 0;

    for each (FileVariable, Var, ExpandedVariablesDB)
    {
        if      (String_StartsWith(Var.Name, S("PreCompile"), false))  NumPreCompileCmds++;
        else if (String_StartsWith(Var.Name, S("PostCompile"), false)) NumPostCompileCmds++;
        else if (String_StartsWith(Var.Name, S("PreLink"), false))     NumPreLinkCmds++;
        else if (String_StartsWith(Var.Name, S("PostLink"), false))    NumPostLinkCmds++;
    }

    if (NumPreCompileCmds > 0 && !bIsClean)
    {
        #ifndef HOOD
        LOG("Running pre compile commands...");
        #else
        LOG("cool mang, gonna run some pre compile cmds...");
        #endif

        for each (FileVariable, Var, ExpandedVariablesDB)
        {
            if (String_StartsWith(Var.Name, S("PreCompile"), false))
            {
                u32 ExitCode = 0;
                bool bResult = Internal_ExecuteBuildCmd(WorkingPath, Var.Name, Var.Value, Var.bHasSpecial, &ExitCode);
                if (!bResult)
                {
                    #ifndef HOOD
                    LOG_ERROR("Pre-compile command exited with a failure result: %u", ExitCode);
                    #else
                    LOG_ERROR("brah wtf, gon have to stop you there nigga. da command we jus ran fuck'n failed on me nigga");
                    #endif

                    return 1;
                }
            }
        }

        Clock_Tick(&ExternalClock);

        LOG_LINE_BREAK();
    }


    // find the icon path (if specified)
    if (Icon.Length > 0)
    {
        u32 LastSlashIndex = 0;
        if (String_IndexOfLastPathSlash(Icon, &LastSlashIndex))
        {
            //IconName = StrShiftF(Icon, LastSlashIndex+1);
        }

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
                TArray(FileVariable) ExpandedVarsArray;
                String* IconFilePath;
                bool bSuccess;
            };

            struct Data d = {ExpandedVariablesDB, &IconFilePath, false};

            String SearchPath = WorkingPath;
            if (LastSlashIndex && !Filesystem_IsPathRelative(Icon))
            {
                SearchPath = StrSlice(Icon.Data, LastSlashIndex+1);
            }

            Filesystem_IterateDirectory_Ex(SearchPath, IconFileDirectoryIterator, true, &d);

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

    // log "Building (Assembly)" ui text
    {
        if (NumSources > 0)
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
                LOG("build'n %S\n", AssemblyNameWithExt);
            }
            else
            {
                String NextExt = StrShiftF(Extension_Og, WhitespaceIndex+1);
                LOG("build'n %S and %S%S\n", AssemblyNameWithExt, AssemblyName, NextExt);
            }
            #endif
        }

        // compile executable icon just before we link (if specified)
        #if PLATFORM_WINDOWS
        if (IconFilePath.Length > 0)
        {
            if (bHasRcProgram)
            {
                Clock_Start(&IconClock);

                u32 LastSlashIndex = 0;
                String_IndexOfLastPathSlash(IconFilePath, &LastSlashIndex);

                String BasePath = StrSlice(IconFilePath.Data, LastSlashIndex);

                StringLocal(RcFilePath, MAX_PATH_LENGTH);
                if (Filesystem_IsPathRelative(IconFilePath))
                {
                    String_BuildPath(&RcFilePath, WorkingPath, BasePath, S("icon.rc"));
                }
                else
                {
                    String_BuildPath(&RcFilePath, BasePath, S("icon.rc"));
                }

                if (ExportIconRC(&p, RcFilePath, IconFilePath))
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
                LOG_WARNING("Unable to build icon. \"%S\" tool does not exist."
                " Download \"%S\" and add a new environment path that points to it."
                " Skipping icon build...", RCProgram, RCProgram);
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
                if (ExportVersionRC(&p, VersionRCPath))
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
                LOG_WARNING("Unable to build version resource file. \"%S\" tool does not exist."
                " Download \"%S\" and add a new environment path that points to it."
                " Skipping resource build...", RCProgram, RCProgram);
            }
        }
        #endif
    }

    p.IconResFilePath    = IconResFilePath;
    p.VersionResFilePath = VersionResFilePath;

    if (IconResFilePath.Length > 0 || VersionResFilePath.Length > 0)
    {
        LOG_LINE_BREAK();
    }

    bool bSuccess = false;

    Clock CompileClock;

    // switch between different compiler backends
    if (String_IsEqual(CompilerProgram, S("cl"), false) ||
        String_IsEqual(CompilerProgram, S("msvc"), false))
    {
        Clock_Start(&CompileClock);
        bSuccess = MSVC_Compile(&p, &NumCompiled);
    }
    else
    {
        Clock_Start(&CompileClock);
        bSuccess = C_Compile(&p, &NumCompiled);
    }

    Platform_WaitForMultipleHandles(Processes, (u32)Array_Num(Processes), -1, true);

    Clock_Tick(&CompileClock);

    if (!bSuccess)
    {
        return 1;
    }

    if (NumCompiled == 0)
    {
        if (bRunPostBuildWhenWorkWasDone)
        {
            goto End;
        }

        goto PostBuild;
    }

    // postcompile step
    if (NumPostCompileCmds > 0 && !bIsClean)
    {
        #ifndef HOOD
        LOG("\nRunning post compile commands...");
        #else
        LOG("cool mang, gonna run some post compile cmds...");
        #endif

        f64 ElapsedSoFar = ExternalClock.ElapsedTime;
        Clock_Start(&ExternalClock);

        for each (FileVariable, Var, ExpandedVariablesDB)
        {
            if (String_StartsWith(Var.Name, S("PostCompile"), false))
            {
                u32 ExitCode = 0;
                bool bResult = Internal_ExecuteBuildCmd(WorkingPath, Var.Name, Var.Value, Var.bHasSpecial, &ExitCode);
                if (!bResult)
                {
                    #ifndef HOOD
                    LOG_ERROR("Post-compile command exited with a failure result: %u", ExitCode);
                    #else
                    LOG_ERROR("brah wtf, gon have to stop you there nigga. da command we jus ran fuck'n failed on me nigga");
                    #endif

                    return 1;
                }
            }
        }

        Clock_Tick(&ExternalClock);
        ExternalClock.ElapsedTime += ElapsedSoFar;

        LOG_LINE_BREAK();
    }


    // prelink step
    if (NumPreLinkCmds > 0 && !bIsClean)
    {
        #ifndef HOOD
        LOG("Running pre link commands...");
        #else
        LOG("cool mang, gonna run some pre link cmds...");
        #endif

        f64 ElapsedSoFar = ExternalClock.ElapsedTime;
        Clock_Start(&ExternalClock);

        for each (FileVariable, Var, ExpandedVariablesDB)
        {
            if (String_StartsWith(Var.Name, S("PreLink"), false))
            {
                u32 ExitCode = 0;
                bool bResult = Internal_ExecuteBuildCmd(WorkingPath, Var.Name, Var.Value, Var.bHasSpecial, &ExitCode);
                if (!bResult)
                {
                    #ifndef HOOD
                    LOG_ERROR("Pre-link command exited with a failure result: %u", ExitCode);
                    #else
                    LOG_ERROR("brah wtf, gon have to stop you there nigga. da command we jus ran fuck'n failed on me nigga");
                    #endif

                    return 1;
                }
            }
        }

        Clock_Tick(&ExternalClock);
        ExternalClock.ElapsedTime += ElapsedSoFar;

        LOG_LINE_BREAK();
    }

    Clock LinkClock;

    if (String_IsEqual(CompilerProgram, S("cl"), false) ||
        String_IsEqual(CompilerProgram, S("msvc"), false))
    {
        Clock_Start(&LinkClock);
        bSuccess = MSVC_Link(&p);
    }
    else // if unrecognized, treat as clang/gcc style compiler
    {
        Clock_Start(&LinkClock);
        bSuccess = C_Link(&p);
    }

    Clock_Tick(&LinkClock);

    if (!bSuccess)
    {
        return 1;
    }

    // postlink step
    if (NumPostLinkCmds > 0 && !bIsClean)
    {
        #ifndef HOOD
        LOG("\nRunning post link commands...");
        #else
        LOG("cool mang, gonna run some pre link cmds...");
        #endif

        f64 ElapsedSoFar = ExternalClock.ElapsedTime;
        Clock_Start(&ExternalClock);

        for each (FileVariable, Var, ExpandedVariablesDB)
        {
            if (String_StartsWith(Var.Name, S("PostLink"), false))
            {
                u32 ExitCode = 0;
                bool bResult = Internal_ExecuteBuildCmd(WorkingPath, Var.Name, Var.Value, Var.bHasSpecial, &ExitCode);
                if (!bResult)
                {
                    #ifndef HOOD
                    LOG_ERROR("Post-link command exited with a failure result: %u", ExitCode);
                    #else
                    LOG_ERROR("brah wtf, gon have to stop you there nigga. da command we jus ran fuck'n failed on me nigga");
                    #endif

                    return 1;
                }
            }
        }

        Clock_Tick(&ExternalClock);
        ExternalClock.ElapsedTime += ElapsedSoFar;
    }


    #if PLATFORM_APPLE
    // compile the .app bundle (if desired)
    if (bBundleApp && bIsAssemblyExe)
    {
        Clock_Start(&BundleCompileClock);

        StringLocal(AssemblyPath, MAX_PATH_LENGTH);
        String_BuildPath(&AssemblyPath, WorkingPath, BuildDirectory, AssemblyNameWithExt);

        StringLocal(AppBundleName, 256);
        String_Append(&AppBundleName, TitleName.Length == 0 ? AssemblyName : TitleName);
        String_Append(&AppBundleName, S(".app"));

        LOG("\nBundling %S", AppBundleName);

        StringLocal(AppBundlePath, MAX_PATH_LENGTH);
        String_BuildPath(&AppBundlePath, WorkingPath, BuildDirectory, AppBundleName);

        StringLocal(BuildPath, MAX_PATH_LENGTH);
        String_BuildPath(&BuildPath, WorkingPath, BuildDirectory);

        // todo: delete old .app directory if we happen to change the assembly name between builds

        if (Filesystem_DoesDirectoryExist(AppBundlePath))
            Filesystem_DeleteDirectory(AppBundlePath);

        bSuccess = Filesystem_OpenDirectory(AppBundlePath);
        if (!bSuccess) goto BundleDirectoryError;

        StringLocal(TempPath, MAX_PATH_LENGTH);
        String_BuildPath(&TempPath, AppBundlePath, S("Contents"));
        bSuccess = Filesystem_OpenDirectory(TempPath);
        if (!bSuccess) goto BundleDirectoryError;
        String_Empty(&TempPath);

        String_BuildPath(&TempPath, AppBundlePath, S("Contents/MacOS"));
        bSuccess = Filesystem_OpenDirectory(TempPath);
        if (!bSuccess) goto BundleDirectoryError;
        String_Empty(&TempPath);

        String_BuildPath(&TempPath, AppBundlePath, S("Contents/Resources"));
        bSuccess = Filesystem_OpenDirectory(TempPath);
        if (!bSuccess) goto BundleDirectoryError;
        String_Empty(&TempPath);

        String_BuildPath(&TempPath, AppBundlePath, S("Contents/Frameworks"));
        bSuccess = Filesystem_OpenDirectory(TempPath);
        String_Empty(&TempPath);

        BundleDirectoryError:
        if (!bSuccess)
        {
            LOG_ERROR("Failed to create app bundle directory. Aborting build...");
            return 1;
        }

        if (IconFilePath.Length > 0)
        {
            // create the .iconset directory and compile the icon into different sizes
            StringLocal(IconsetName, 256);
            String_Append(&IconsetName, AssemblyName);
            String_Append(&IconsetName, S(".iconset"));
            StringLocal(IconsetPath, MAX_PATH_LENGTH);
            String_BuildPath(&IconsetPath, WorkingPath, IntermediateDirectory, IconsetName);

            if (Filesystem_DoesDirectoryExist(IconsetPath))
                Filesystem_DeleteDirectory(IconsetPath);

            bSuccess = Filesystem_OpenDirectory(IconsetPath);
            if (!bSuccess)
            {
                LOG_ERROR("Failed to create iconset directory. Aborting build...");
                return 1;
            }

            PlatformHandle Handles[6] = {0};
            u16 Size = 16;
            for (u8 i = 0; i < 6; i++)
            {
                StringLocal(CmdLine, 2048);
                String_Format(&CmdLine, S("sips -z %u %u \"%S\" --out \"%S/icon_%ux%u.png\" > /dev/null"), CmdLine.Capacity, Size, Size, IconFilePath, IconsetPath, Size, Size);
                if (bVerboseLog) LOG("    %S", CmdLine);

                Size *= 2;

                Handles[i] = Platform_RunCommand(CmdLine, WorkingPath, String_Null());
            }

            u32 ExitCode = Platform_WaitForMultipleHandles(Handles, SArray_Capacity(Handles), -1, true);
            if (ExitCode != 0)
            {
                LOG_ERROR("Failed to build iconset for \"%S\". Aborting build...", IconFilePath);
                return 1;
            }

            StringLocal(IcnsName, 256);
            String_Append(&IcnsName, AssemblyName);
            String_Append(&IcnsName, S(".icns"));

            StringLocal(IcnsPath, MAX_PATH_LENGTH);
            String_BuildPath(&IcnsPath, WorkingPath, IntermediateDirectory, IcnsName);

            StringLocal(CmdLine, 2048);
            String_Format(&CmdLine, S("iconutil -c icns -o \"%S\" \"%S\""), CmdLine.Capacity, IcnsPath, IconsetPath);
            if (bVerboseLog) LOG("    %S", CmdLine);
            PlatformHandle H = Platform_RunCommand(CmdLine, WorkingPath, String_Null());
            ExitCode = Platform_WaitForProcessAndGetExitCode(H);
            if (ExitCode != 0)
            {
                LOG_ERROR("Failed to build \"%S\". Aborting build...", IcnsPath);
                return 1;
            }

            String_BuildPath(&TempPath, AppBundlePath, S("Contents/Resources"), IcnsName);
            bSuccess = Filesystem_Copy(IcnsPath, TempPath);
            if (!bSuccess) goto CopyError;
            String_Empty(&TempPath);
        }

        // copy Info.plist, version.plist and PkgInfo files into Contents directory
        StringLocal(ResourcePath, MAX_PATH_LENGTH);

        if (CustomInfoPlist.Length > 0)
        {
            if (Filesystem_IsPathRelative(CustomInfoPlist))
            {
                String_BuildPath(&ResourcePath, WorkingPath, CustomInfoPlist);
            }
            else
            {
                String_Copy(&ResourcePath, CustomInfoPlist);
            }

            if (!String_EndsWith(ResourcePath, S(".plist"), false))
            {
                LOG_ERROR("%S: Bundle.InfoPlist: file must end with \".plist\". Aborting build...", BuildFileName);
                return 1;
            }

            // todo: if no explicit path given, search for it
        }
        else
        {
            String_BuildPath(&ResourcePath, WorkingPath, IntermediateDirectory, S("Info.plist"));

            // generate Info.plist
            if (bVerboseLog) LOG("    Generating %S", ResourcePath);

            if (!ExportInfoPlist(*Arena, &p, ResourcePath, ExpandedVariablesDB, DoesBuildVarExist(ExpandedVariablesDB, S("Info.plist"))))
            {
                return 1;
            }
        }

        String_BuildPath(&TempPath, AppBundlePath, S("Contents/Info.plist"));
        bSuccess = Filesystem_Copy(ResourcePath, TempPath);
        if (!bSuccess) goto CopyError;
        String_Empty(&TempPath);
        String_Empty(&ResourcePath);

        if (CustomVersionPlist.Length > 0)
        {
            String_BuildPath(&ResourcePath, WorkingPath, CustomVersionPlist);

            if (!String_EndsWith(ResourcePath, S(".plist"), false))
            {
                LOG_ERROR("%S: Bundle.VersionPlist: file must end with \".plist\". Aborting build...", BuildFileName);
                return 1;
            }

            // todo: if no explicity path given, search for it
        }
        else
        {
            String_BuildPath(&ResourcePath, WorkingPath, IntermediateDirectory, S("Version.plist"));

            // generate version.plist

            if (bVerboseLog) LOG("    Generating %S", ResourcePath);

            if (!ExportVersionPlist(*Arena, &p, ResourcePath, ExpandedVariablesDB, DoesBuildVarExist(ExpandedVariablesDB, S("Version.plist"))))
            {
                return 1;
            }
        }

        String_BuildPath(&TempPath, AppBundlePath, S("Contents/Version.plist"));
        bSuccess = Filesystem_Copy(ResourcePath, TempPath);
        if (!bSuccess) goto CopyError;
        String_Empty(&TempPath);
        String_Empty(&ResourcePath);

        if (CustomPkgInfo.Length > 0)
        {
            String_BuildPath(&ResourcePath, WorkingPath, CustomPkgInfo);

            if (!String_EndsWith(ResourcePath, S("PkgInfo"), true))
            {
                LOG_ERROR("%S: Bundle.PkgInfo: file must be named \"PkgInfo\" (case sensitive). Aborting build...", BuildFileName);
                return 1;
            }

            // todo: if no explicity path given, search for it
        }
        else
        {
            String_BuildPath(&ResourcePath, WorkingPath, IntermediateDirectory, S("PkgInfo"));

            // generate PkgInfo

            if (bVerboseLog) LOG("    Generating %S", ResourcePath);

            if (!ExportPkgInfo(&p, ResourcePath))
            {
                return 1;
            }
        }

        String_BuildPath(&TempPath, AppBundlePath, S("Contents/PkgInfo"));
        bSuccess = Filesystem_Copy(ResourcePath, TempPath);
        if (!bSuccess) goto CopyError;
        String_Empty(&TempPath);
        String_Empty(&ResourcePath);

        // todo: support
        /*
            #Bundle.Plugins                    
            #Bundle.Frameworks                 
            #Bundle.Libraries                  
            #Bundle.CodeResources              
        */

        // copy the executable into the MacOS directory
        // do something special if this is a terminal only app
        if (bBundleAppIsTerminal)
        {
            StringLocal(NewAssemblyName, 256);
            String_Append(&NewAssemblyName, AssemblyName);
            String_Append(&NewAssemblyName, S("-bin"));
            String_Append(&NewAssemblyName, Extension);

            // Step 1 ----------------
            String_BuildPath(&TempPath, AppBundlePath, S("Contents/MacOS"), AssemblyName);

            if (bVerboseLog) LOG("    Generating terminal script %S", TempPath);

            FileHandle f = {0};
            if (!Filesystem_Open(TempPath, FileMode_Write, &f))
            {
                LOG_ERROR("Failed to create terminal script \"%S\". Aborting build...", TempPath);
                return 1;
            }

            StringLocal(RealBinaryPath, 1024);
            String_BuildPath(&RealBinaryPath, AppBundlePath, S("Contents/MacOS"), NewAssemblyName);

            Filesystem_WriteLine(f, S("#!/bin/sh\n\n"), NULL);
            //Filesystem_WriteLineFormatted(f, S("open -a Terminal \"%S\"\n"), NULL, RealBinaryPath);
            Filesystem_WriteLineFormatted(f, S("cd \"${0%%/*}\"\nopen %S"), NULL, NewAssemblyName);
            Filesystem_Close(&f);

            // Step 2 ----------------
            StringLocal(CmdLine, 2048);
            String_Format(&CmdLine, S("chmod +x \"%S\""), CmdLine.Capacity, TempPath);
            if (bVerboseLog) LOG("    %S", CmdLine);
            Platform_RunCommand(CmdLine, WorkingPath, String_Null());
            String_Empty(&TempPath);

            // Step 3 ----------------
            String_BuildPath(&TempPath, AppBundlePath, S("Contents/MacOS"), NewAssemblyName);
            if (bVerboseLog) LOG("    Copying binary executable %S", TempPath);
            bSuccess = Filesystem_Copy(AssemblyPath, TempPath);
            if (!bSuccess) goto CopyError;
            String_Empty(&TempPath);
        }
        else
        {
            String_BuildPath(&TempPath, AppBundlePath, S("Contents/MacOS"), AssemblyNameWithExt);
            if (bVerboseLog) LOG("    Copying binary executable %S", TempPath);
            bSuccess = Filesystem_Copy(AssemblyPath, TempPath);
            if (!bSuccess) goto CopyError;
            String_Empty(&TempPath);
        }

        CopyError:
        if (!bSuccess)
        {
            LOG_ERROR("Failed to copy \"%S\" into the app bundle. Aborting build...", TempPath);
            return 1;
        }

        Clock_Tick(&BundleCompileClock);
    }

    // build icon for mach-o executables
    if (IconFilePath.Length > 0)
    {
        // embed exe icon into the actual executable
        {
            Clock_Start(&IconClock);

            LOG("\nCompiling icon \"%S\"", IconFilePath);

            StringLocal(CmdLine, 4096);

            StringLocal(AssemblyPath, MAX_PATH_LENGTH);
            String_BuildPath(&AssemblyPath, WorkingPath, BuildDirectory, AssemblyNameWithExt);

            u32 LastSlashIndex = 0;
            String_IndexOfLastPathSlash(IconFilePath, &LastSlashIndex);

            // Step 1 ------------------
            StringLocal(RsrcFilePath, MAX_PATH_LENGTH);
            StringLocal(RsrcFileName, 256);
            String_Append(&RsrcFileName, StrShiftF(IconFilePath, LastSlashIndex == 0 ? 0 : LastSlashIndex+1));
            String_Append(&RsrcFileName, S("-icns.rsrc"));
            String_BuildPath(&RsrcFilePath, WorkingPath, IntermediateDirectory, RsrcFileName);

            String_BuildSeparator(&CmdLine, ' ', S("derez -only icns"), IconFilePath, S(">"), RsrcFilePath);

            if (bVerboseLog) LOG("    %S", CmdLine);

            PlatformHandle h = Platform_RunCommand(CmdLine, WorkingPath, String_Null());
            u32 ExitCode = Platform_WaitForProcessAndGetExitCode(h);
            if (ExitCode != 0)
            {
                LOG_ERROR("Failed to build icon \"%S\" for %S. Aborting build...", IconFilePath, AssemblyNameWithExt);
                return 1;
            }

            String_Empty(&CmdLine);

            // Step 2 ------------------
            String_BuildSeparator(&CmdLine, ' ', S("rez -append"), RsrcFilePath, S("-o"), AssemblyPath);

            if (bVerboseLog) LOG("    %S", CmdLine);

            h = Platform_RunCommand(CmdLine, WorkingPath, String_Null());
            ExitCode = Platform_WaitForProcessAndGetExitCode(h);
            if (ExitCode != 0)
            {
                LOG_ERROR("Failed to build icon \"%S\" for %S. Aborting build...", IconFilePath, AssemblyNameWithExt);
                return 1;
            }

            String_Empty(&CmdLine);

            // Step 3 ------------------
            String_BuildSeparator(&CmdLine, ' ', S("SetFile -a C"), AssemblyPath);

            if (bVerboseLog) LOG("    %S", CmdLine);

            h = Platform_RunCommand(CmdLine, WorkingPath, String_Null());
            ExitCode = Platform_WaitForProcessAndGetExitCode(h);
            if (ExitCode != 0)
            {
                LOG_ERROR("Failed to build icon \"%S\" for %S. Aborting build...", IconFilePath, AssemblyNameWithExt);
                return 1;
            }

            Clock_Tick(&IconClock);
        }
    }
    #endif

    // build icon for linux executables
    #if PLATFORM_LINUX || PLATFORM_BSD
    if (IconFilePath.Length > 0)
    {
        Clock_Start(&IconClock);
        LOG("\nCompiling icon \"%S\"", IconFilePath);

        StringLocal(AssemblyPath, MAX_PATH_LENGTH);
        String_BuildPath(&AssemblyPath, WorkingPath, BuildDirectory, AssemblyNameWithExt);

        StringLocal(UserDirectory, MAX_PATH_LENGTH);
        bool bGotUsrDir = Platform_GetUserDirectory(&UserDirectory);

        if (bIsAssemblyExe)
        {
            // build a .desktop file
            StringLocal(DotDesktopFilePath, MAX_PATH_LENGTH);
            StringLocal(DesktopFileName, 512);
            String_Append(&DesktopFileName, AssemblyName);
            String_Append(&DesktopFileName, S(".desktop"));
            
            if (bGotUsrDir)
            {
                StringLocal(LocalAppsDirectory, MAX_PATH_LENGTH);
                String_BuildPath(&LocalAppsDirectory, UserDirectory, S(".local/share/applications"));
                if (!Filesystem_OpenDirectory(LocalAppsDirectory))
                    return 1;

                String_BuildPath(&DotDesktopFilePath, UserDirectory, S(".local/share/applications/"), DesktopFileName);
            }
            else
            {
                String_BuildPath(&DotDesktopFilePath, WorkingPath, IntermediateDirectory, DesktopFileName);
            }

            FileHandle f = {0};
            if (Filesystem_Open(DotDesktopFilePath, FileMode_Write, &f))
            {
                StringLocal(ExecCmd, 4096);
                #if PLATFORM_NET_BSD
                String_Format(&ExecCmd, S("sh -c 'cd \"$(realpath -q \"$0\"/ || dirname \"$1\")\" && %S --from-desktop' %%U"), ExecCmd.Capacity, AssemblyPath);
                #else
                String_Format(&ExecCmd, S("sh -c 'cd \"$(realpath -q \"$0\"/ || dirname \"$0\")\" && %S --from-desktop' %%U"), ExecCmd.Capacity, AssemblyPath);
                #endif

                StringLocal(FileData, 4096);
                String_Format(&FileData,
                    S("# Generated by riftbuild on %S\n"
                        "[Desktop Entry]\n"
                        "Name=%S\n"
                        "TryExec=%S\n"
                        "Exec=%S\n"
                        "Icon=%S\n"
                        "Terminal=true\n"
                        "Type=Application\n"
                        "StartupNotify=false\n"),
                        4096, 
                        TimeStamp,
                        TitleName.Length == 0 ? AssemblyName : TitleName,
                        AssemblyPath,
                        ExecCmd,
                        IconFilePath);

                if (bVerboseLog) LOG("    Writing %S ...", DotDesktopFilePath);

                Filesystem_Write(f, FileData.Length, FileData.Data, NULL);
                Filesystem_Close(&f);
            }

            // TODO: update or generate mimeapps.list config... i wanna cry
            // first copy the mimeapps.list if it exist, if this fails, stop and skip this procedure
            // reconstruct the mimeapps.list contents and add our new ones in the appropriate sections

            // try to natively override the default icon for the actual executable
            // currently only supporting GNOME and KDE desktop environments
            // todo: get rid of those
            #if PLATFORM_LINUX_GNOME || PLATFORM_LINUX_KDE || PLATFORM_BSD
            {
                StringLocal(CmdLine, 4096);

                StringLocal(XmlFilePath, MAX_PATH_LENGTH);
                StringLocal(XmlFileName, 512);
                String_Append(&XmlFileName, S("application-"));
                String_Append(&XmlFileName, AssemblyName);
                String_Append(&XmlFileName, S(".xml"));
                
                if (bGotUsrDir)
                {
                    StringLocal(MimeDirectory, MAX_PATH_LENGTH);
                    String_BuildPath(&MimeDirectory, UserDirectory, S(".local/share/mime/packages"));
                    if (!Filesystem_OpenDirectory(MimeDirectory))
                        return 1;

                    String_BuildPath(&XmlFilePath, UserDirectory, S(".local/share/mime/packages"), XmlFileName);
                }
                else
                {
                    String_BuildPath(&XmlFilePath, WorkingPath, IntermediateDirectory, XmlFileName);
                }

                // todo: remove defines, use runtime check for gnome/xfce4
                #if PLATFORM_LINUX_GNOME
                u32 LastSlash = 0, LastDot = 0;
                String_IndexOfLastPathSlash(IconFilePath, &LastSlash);
                String_IndexOfLastChar(StrShiftF(IconFilePath, LastSlash+1), '.', &LastDot);

                const String IconName = StrSlice(StrShiftF(IconFilePath, LastSlash+1).Data, LastDot);
                #endif

                if (Filesystem_Open(XmlFilePath, FileMode_Write, &f))
                {
                    StringLocal(FileData, 4096);
                    String_Format(&FileData,
                        #if PLATFORM_LINUX_GNOME
                        S("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                        "  <mime-info xmlns='http://www.freedesktop.org/standards/shared-mime-info'>\n"
                        "    <mime-type type=\"application/%S\">\n"
                        "      <comment>%S</comment>\n"
                        "      <expanded-acronym>%S</expanded-acronym>\n"
                        "      <glob pattern=\"%S\"/>\n"
                        "      <generic-icon name=\"%S\"/>\n"
                        "    </mime-type>\n"
                        "  </mime-info>\n"),
                        4096,
                        AssemblyName, Description, TitleName.Length == 0 ? AssemblyName : TitleName,
                        AssemblyName, IconName
                        #elif PLATFORM_LINUX_KDE || PLATFORM_BSD
                        S("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                        "  <mime-info xmlns='http://www.freedesktop.org/standards/shared-mime-info'>\n"
                        "    <mime-type type=\"application/%S\">\n"
                        "      <comment>%S</comment>\n"
                        "      <expanded-acronym>%S</expanded-acronym>\n"
                        "      <glob pattern=\"%S\"/>\n"
                        "      <icon name=\"%S\"/>\n"
                        "    </mime-type>\n"
                        "  </mime-info>\n"),
                        4096,
                        AssemblyName, Description, TitleName.Length == 0 ? AssemblyName : TitleName,
                        AssemblyName, IconFilePath
                        #endif
                    );

                    if (bVerboseLog) LOG("    Writing %S ...", XmlFilePath);

                    Filesystem_Write(f, FileData.Length, FileData.Data, NULL);
                    Filesystem_Close(&f);

                    // update the databases

                    //xdg-mime install --mode user ~/.local/share/mime/packages/application-riftbuild.xml 
                    PlatformHandle H = {0};
                    u32 ExitCode = 0;

                    // todo: remove defines, use runtime check for gnome/xfce4
                    #if PLATFORM_LINUX_GNOME
                    if (Platform_FindProgram(S("xdg-mime")))
                    {
                        String_Append(&CmdLine, S("xdg-mime install --mode user "));
                        String_Append(&CmdLine, XmlFilePath);
                        if (bVerboseLog) LOG("    %S", CmdLine);

                        H = Platform_RunCommand(CmdLine, WorkingPath, String_Null());
                        ExitCode = Platform_WaitForProcessAndGetExitCode(H);
                        if (ExitCode != 0)
                        {
                            LOG_ERROR("Failed to build icon \"%S\" for %S. Aborting build...", IconFilePath, AssemblyNameWithExt);
                            return 1;
                        }
                    }
                    else
                    {
                        LOG_WARNING("xdg-mime not found. Skipping icon database update...");
                    }

                    String_Empty(&CmdLine);

                    //xdg-icon-resource install --context mimetypes --novendor --size 32 Source/Resources/riftbuild.png riftbuild
                    if (Platform_FindProgram(S("xdg-icon-resource")))
                    {
                        String_Append(&CmdLine, S("xdg-icon-resource install --context mimetypes --novendor --size 32 "));
                        String_Append(&CmdLine, IconFilePath);
                        String_AppendSpace(&CmdLine);
                        String_Append(&CmdLine, AssemblyName);
                        if (bVerboseLog) LOG("    %S", CmdLine);

                        H = Platform_RunCommand(CmdLine, WorkingPath, String_Null());
                        ExitCode = Platform_WaitForProcessAndGetExitCode(H);
                        if (ExitCode != 0)
                        {
                            LOG_ERROR("Failed to build icon \"%S\" for %S. Aborting build...", IconFilePath, AssemblyNameWithExt);
                            return 1;
                        }
                    }
                    else
                    {
                        LOG_WARNING("xdg-icon-resource not found. Skipping icon database update...");
                    }

                    String_Empty(&CmdLine);
                    #endif

                    //update-desktop-database ~/.local/share/applications
                    if (Platform_FindProgram(S("update-desktop-database")))
                    {
                        String_Copy(&CmdLine, S("update-desktop-database ~/.local/share/applications"));
                        if (bVerboseLog) LOG("    %S", CmdLine);

                        H = Platform_RunCommand(CmdLine, WorkingPath, String_Null());
                        ExitCode = Platform_WaitForProcessAndGetExitCode(H);
                        if (ExitCode != 0)
                        {
                            LOG_ERROR("Failed to build icon \"%S\" for %S. Aborting build...", IconFilePath, AssemblyNameWithExt);
                            return 1;
                        }
                    }
                    else
                    {
                        LOG_WARNING("update-desktop-database not found. Skipping desktop database update...");
                    }

                    //update-mime-database ~/.local/share/mime
                    if (Platform_FindProgram(S("update-mime-database")))
                    {
                        String_Copy(&CmdLine, S("update-mime-database ~/.local/share/mime"));
                        if (bVerboseLog) LOG("    %S", CmdLine);

                        H = Platform_RunCommand(CmdLine, WorkingPath, String_Null());
                        ExitCode = Platform_WaitForProcessAndGetExitCode(H);
                        if (ExitCode != 0)
                        {
                            LOG_ERROR("Failed to build icon \"%S\" for %S. Aborting build...", IconFilePath, AssemblyNameWithExt);
                            return 1;
                        }
                    }
                    else
                    {
                        LOG_WARNING("update-mime-database not found. Skipping mime database update...");
                    }

                    String_Empty(&CmdLine);

                    //update-icon-caches ~/.local/share/icons/

            /*
                    #if PLATFORM_LINUX_GNOME
                    String_Copy(&CmdLine, S("update-icon-caches ~/.local/share/icons"));
                    if (bVerboseLog) LOG("    %S", CmdLine);

                    H = Platform_RunCommand(CmdLine, WorkingPath);
                    ExitCode = Platform_WaitForProcessAndGetExitCode(H);
                    if (ExitCode != 0)
                    {
                        LOG_ERROR("Failed to build icon \"%S\" for %S. Aborting build...", IconFilePath, AssemblyNameWithExt);
                        return 1;
                    }
                    #endif
            */

                    /*
                    String_Empty(&CmdLine);

                    const String Cmd = S("update-mime-database ~/.local/share/mime");
                    if (bVerboseLog) LOG("    %S", Cmd);

                    PlatformHandle H = Platform_RunCommand(Cmd, WorkingPath);
                    u32 ExitCode = Platform_WaitForProcessAndGetExitCode(H);
                    if (ExitCode != 0)
                    {
                        LOG_ERROR("Failed to build icon \"%S\" for %S. Aborting build...", IconFilePath, AssemblyNameWithExt);
                        return 1;
                    }
                    */
                }
            }
            #endif
        }

        Clock_Tick(&IconClock);
    }
    else
    {
        // clean up icon files if we previously built one
        // todo: store old assembly name in case that was renamed, read the build_generated file to find the old assembly name before doing a clean
        // todo: if riftbuild clean was executed, also clean up these files
        StringLocal(UserDirectory, MAX_PATH_LENGTH);
        bool bGotUsrDir = Platform_GetUserDirectory(&UserDirectory);

        StringLocal(XmlFilePath, MAX_PATH_LENGTH);
        StringLocal(XmlFileName, 512);
        String_Append(&XmlFileName, S("application-"));
        String_Append(&XmlFileName, AssemblyName);
        String_Append(&XmlFileName, S(".xml"));

        StringLocal(DotDesktopFilePath, MAX_PATH_LENGTH);
        StringLocal(DesktopFileName, 512);
        String_Append(&DesktopFileName, AssemblyName);
        String_Append(&DesktopFileName, S(".desktop"));
        
        if (bGotUsrDir)
        {
            StringLocal(MimeDirectory, MAX_PATH_LENGTH);
            String_BuildPath(&MimeDirectory, UserDirectory, S(".local/share/mime/packages"));
            if (!Filesystem_OpenDirectory(MimeDirectory))
                return 1;

            String_BuildPath(&XmlFilePath, UserDirectory, S(".local/share/mime/packages"), XmlFileName);

            // ================================

            StringLocal(LocalAppsDirectory, MAX_PATH_LENGTH);
            String_BuildPath(&LocalAppsDirectory, UserDirectory, S(".local/share/applications"));
            if (!Filesystem_OpenDirectory(LocalAppsDirectory))
                return 1;

            String_BuildPath(&DotDesktopFilePath, UserDirectory, S(".local/share/applications/"), DesktopFileName);
        }
        else
        {
            String_BuildPath(&XmlFilePath, WorkingPath, IntermediateDirectory, XmlFileName);

            String_BuildPath(&DotDesktopFilePath, WorkingPath, IntermediateDirectory, DesktopFileName);
        }
        
        const bool bHaveXml        = Filesystem_DoesFileExist(XmlFilePath);
        const bool bHaveDotDesktop = Filesystem_DoesFileExist(DotDesktopFilePath);
        if (bHaveXml || bHaveDotDesktop)
        {
            LOG("\nNo icon specified - cleaning up icon/resource files ...");

            if (bHaveXml)
            {
                if (bVerboseLog) LOG("    Deleting %S ...", XmlFilePath);
                Filesystem_DeleteFile(XmlFilePath);
            }

            if (bHaveDotDesktop)
            {
                if (bVerboseLog) LOG("    Deleting %S ...", DotDesktopFilePath);
                Filesystem_DeleteFile(DotDesktopFilePath);
            }

            // update databases
            {
                StringLocal(CmdLine, 128);

                if (Platform_FindProgram(S("update-desktop-database")))
                {
                    String_Copy(&CmdLine, S("update-desktop-database ~/.local/share/applications"));
                    if (bVerboseLog) LOG("    %S", CmdLine);

                    PlatformHandle H = Platform_RunCommand(CmdLine, WorkingPath, String_Null());
                    (void)Platform_WaitForProcessAndGetExitCode(H);
                }
                else
                {
                    LOG_WARNING("update-desktop-database not found. Skipping desktop database update...");
                }

                String_Empty(&CmdLine);

                if (Platform_FindProgram(S("update-mime-database")))
                {
                    String_Copy(&CmdLine, S("update-mime-database ~/.local/share/mime"));
                    if (bVerboseLog) LOG("    %S", CmdLine);

                    PlatformHandle H = Platform_RunCommand(CmdLine, WorkingPath, String_Null());
                    (void)Platform_WaitForProcessAndGetExitCode(H);
                }
                else
                {
                    LOG_WARNING("update-mime-database not found. Skipping mime database update...");
                }
            }
        }
    }
    #endif

    Clock_Tick(&BuildRuntime);

    if (bQuietBuild) Logging_Enable();

    StringLocal(TimeString, 32);

    Clock_GetElapsedTime_ToString(&CompileClock, true, &TimeString);
    LOG("\nCompile     time: %S", TimeString);

    Clock_GetElapsedTime_ToString(&LinkClock, true, &TimeString);
    LOG("Link        time: %S", TimeString);

    if (IconClock.StartTime > 0)
    {
        Clock_GetElapsedTime_ToString(&IconClock, true, &TimeString);
        LOG("Icon        time: %S", TimeString);
    }

    if (ResourceCompileClock.StartTime > 0)
    {
        Clock_GetElapsedTime_ToString(&ResourceCompileClock, true, &TimeString);
        LOG("Resource    time: %S", TimeString);
    }

    if (BundleCompileClock.StartTime > 0)
    {
        Clock_GetElapsedTime_ToString(&BundleCompileClock, true, &TimeString);
        LOG("Bundle      time: %S", TimeString);
    }

    if (bFoundBuildFile)
    {
        Clock_GetElapsedTime_ToString(&BuildFileParseClock, true, &TimeString);
        LOG("Build parse time: %S", TimeString);
    }

    if (MSVCInitClock.StartTime > 0)
    {
        Clock_GetElapsedTime_ToString(&MSVCInitClock, true, &TimeString);
        LOG("MSVC init   time: %S", TimeString);
    }

    if (DependencyBuildClock.ElapsedTime > 0)
    {
        Clock_GetElapsedTime_ToString(&DependencyBuildClock, true, &TimeString);
        LOG("Dependency  time: %S", TimeString);
    }

    if (ExternalClock.ElapsedTime > 0)
    {
        Clock_GetElapsedTime_ToString(&ExternalClock, true, &TimeString);
        LOG("External    time: %S", TimeString);
    }

    // calculate the overhead time
    f64 TotalElapsedTime = CompileClock.ElapsedTime +
                           LinkClock.ElapsedTime +
                           IconClock.ElapsedTime +
                           ResourceCompileClock.ElapsedTime +
                           BundleCompileClock.ElapsedTime +
                           BuildFileParseClock.ElapsedTime +
                           MSVCInitClock.ElapsedTime +
                           DependencyBuildClock.ElapsedTime +
                           ExternalClock.ElapsedTime;

    Clock OverheadClock = {0};
    OverheadClock.ElapsedTime = BuildRuntime.ElapsedTime - TotalElapsedTime;

    Clock_GetElapsedTime_ToString(&OverheadClock, true, &TimeString);
    LOG("Overhead    time: %S", TimeString);

    Clock_GetElapsedTime_ToString(&BuildRuntime, true, &TimeString);
    LOG("Total build time: %S", TimeString);

    StringLocal(OutputPath, MAX_PATH_LENGTH);
    String_AppendChar(&OutputPath, '"');
    String_Append(&OutputPath, BuildBaseDirectory);
    String_Append(&OutputPath, AssemblyNameWithExt);
    String_AppendChar(&OutputPath, '"');

    LOG_LINE_BREAK();

    u32 WhitespaceIndex = 0;
    bool bHasSpace = String_IndexOfFirstWhitespace(Extension_Og, &WhitespaceIndex);

    if (bIsAssemblyExe || !bHasSpace)
    {
        #ifndef HOOD
        LOG_SUCCESS("Build complete: %S", OutputPath);
        #else
        LOG_SUCCESS("lessss goooo: %S", OutputPath);
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
        LOG_SUCCESS("lessss goooo: %S\n                         %S", OutputPath, OutputPath2);
        #endif
    }

    #if !PLATFORM_WINDOWS
    //LOG_LINE_BREAK();
    #endif

    // run post build commands (if specified)
PostBuild:
    if (bQuietBuild) Logging_Enable();

    for each (FileVariable, Var, ExpandedVariablesDB)
    {
        if (String_StartsWith(Var.Name, S("PostBuild"), false))
        {
            NumPostBuildCmds++;
        }
    }

    if (NumPostBuildCmds > 0)
    {
        //#if PLATFORM_WINDOWS
        LOG_LINE_BREAK();
        //#endif

        #ifndef HOOD
        LOG("Running post build commands...");
        #else
        LOG("cool mang, gonna run some post build cmds...");
        #endif

        for each (FileVariable, Var, ExpandedVariablesDB)
        {
            if (String_StartsWith(Var.Name, S("PostBuild"), false))
            {
                u32 ExitCode = 0;
                bool bResult = Internal_ExecuteBuildCmd(WorkingPath, Var.Name, Var.Value, Var.bHasSpecial, &ExitCode);
                if (!bResult)
                {
                    #ifndef HOOD
                    LOG_ERROR("Post-build command exited with a failure result: %u", ExitCode);
                    #else
                    LOG_ERROR("brah wtf, gon have to stop you there nigga. da command we jus ran fuck'n failed on me nigga");
                    #endif

                    return 1;
                }
            }
        }

        LOG_LINE_BREAK();
    }

End:
    // run the assembly (if an executable)
    if (bIsAssemblyExe)
    {
        // todo:if we have run assembly's ignore this shit
        if (StringArray_Contains(Parameters, S("Run"), false))
        {
            // todo: args like runassembly key
            
            Internal_RunAssembly(*Arena, WorkingPath, BuildBaseDirectory, AssemblyNameWithExt, String_Null());
        }

        for each (FileVariable, v, ExpandedVariablesDB)
        {
            if (!String_IsEqual(v.Name, S("RunAssembly"), false))
                continue;

            if (v.bHasSpecial)
            {
                if (NumCompiled == 0)
                    continue;
            }

            Internal_RunAssembly(*Arena, WorkingPath, BuildBaseDirectory, AssemblyNameWithExt, v.Value);
        }
    }

    if (bQuietBuild) Logging_Disable();

    if (String_IsValid(CameFromBuildFile) && NumCompiled > 0)
    {
        // special exit code to let the parent build know this child build finished successfully (and that it did some work)
        return 2;
    }

    return 0;
}

internal u32 RiftBuild(LinearAllocator* Arena, const StringArray Arguments)
{
    if (NEVER(Arena == NULL)) return 1;

    StringLocal(BuildFileName, 128);
    StringLocal(BuildFilePath, MAX_PATH_LENGTH);

    StringLocal(WorkingDirectory, MAX_PATH_LENGTH);
    Platform_GetWorkingDirectory(&WorkingDirectory);
    
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
            if (IsBuildFile(Arguments.List[i]))
            {
                BuildFileIndex = (i8)i;
                break;
            }
        }

        // find the root path (if specified)
        for (u8 i = 0; i < Arguments.Num; i++)
        {
            if (i == BuildFileIndex)
                continue;

            if (IsBuildFile(Arguments.List[i]) ||
                IsBuildBatchFile(Arguments.List[i]))
                continue;

            if (String_StartsWith(Arguments.List[i], S("override:"), false) ||
                String_StartsWith(Arguments.List[i], S("export:"), false) ||
                String_StartsWith(Arguments.List[i], S("preset:"), false) ||
                String_StartsWith(Arguments.List[i], S("list:"), false))
                continue;
        
            if (String_IndexOfChar(Arguments.List[i], '\\', NULL) ||
                String_IndexOfChar(Arguments.List[i], '/', NULL))
            {
                RootPathIndex = (i8)i;
                break;
            }
        }

        if (BuildFileIndex == -1)
            bNoBuildFileSpecifiedInCmd = true;

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
                String_Copy(&BuildFilePath, Arguments.List[BuildFileIndex]);

                if (!String_EndsWith(BuildFilePath, S(".build"), false))
                    String_Append(&BuildFilePath, S(".build"));

                if (!String_EndsWith(BuildFileName, S(".build"), false))
                    String_Append(&BuildFileName, S(".build"));

                bBuildPathGivenInCmdLine = true;
            }
            else
            {
                String_Copy(&BuildFileName, Arguments.List[BuildFileIndex]);
            }
        }
    }

    String_EatPathSeparatorsInlineFromEnd(&WorkingDirectory);
    Filesystem_ConvertRelativeToAbsolutePath(&WorkingDirectory);
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

    // prevent riftbuild from running in a root drive directory like C:/ (or / on linux).
    // it's non-sensical anyway, it has no business running in those places
    {
        StringLocal(RootCopy, MAX_PATH_LENGTH);
        String_Copy(&RootCopy, WorkingDirectory);
        String_EatSpacesInlineFromEnd(&RootCopy);
        String_EatPathSeparatorsInlineFromEnd(&RootCopy);
        String_AppendPathSeparator(&RootCopy);

        u32 NumPathSeparators = String_CountPathSeparators(RootCopy);
        if (NumPathSeparators <= 1)
        {
            LOG_ERROR("%S is too shallow of a directory.\n", RootCopy);
            LOG("    Create a new directory from here and then run riftbuild again from the new directory");

            return 1;
        }
    }

    String_EatPathSeparatorsInlineFromEnd(&WorkingDirectory);

    // TODO: if "help" was given just log the .build files' custom help message

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

    if (StringArray_Contains(Arguments, S("-t"), false))
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
    bIsClean      = StringArray_Contains(Arguments, S("clean"), false);
    bIsRebuild    = StringArray_Contains(Arguments, S("rebuild"), false);
    bVerboseLog   = StringArray_Contains(Arguments, S("-v"), false);
    bSingleThread = StringArray_Contains(Arguments, S("-singlethread"), false) ||
                    StringArray_Contains(Arguments, S("-s"), false);

    BuildFileDirectoryIteratorData Data = {0};
    Data.bNoBuildFileSpecifiedInCmd = bNoBuildFileSpecifiedInCmd;
    Data.BuildFileIndex = BuildFileIndex;
    Data.RootPathIndex = RootPathIndex;
    Data.Name = &BuildFileName;
    Data.Path = &BuildFilePath;
    Data.Arguments = Arguments;
    Data.bSearchOnlyBuildBatch = true;

    // first, find .buildbatch files
    {
        for (u8 i = 0; i < Arguments.Num; i++)
        {
            if (IsBuildBatchFile(Arguments.List[i]))
            {
                String_Copy(&BuildFileName, Arguments.List[i]);
                Filesystem_IterateDirectory_Ex(WorkingDirectory, BuildFileDirectoryIterator, false, &Data);
                break;
            }
        }

        if ((!Data.bFoundBuildFile || Data.NumBuildFilesFound > 1) && Arguments.Num > 0)
        {
            Filesystem_IterateDirectory_Ex(WorkingDirectory, BuildFileDirectoryIterator_Args, true, &Data);
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

                while (Filesystem_ReadLine(f, &Line))
                {
                    String Trimmed = String_EatSpaces(Line);

                    if (Trimmed.Length == 0)
                        continue;

                    // multiline comment
                    if (Trimmed.Data[0] == '#' && Trimmed.Data[1] == '#')
                    {
                        bInMultiLineComment = !bInMultiLineComment;
                        continue;
                    }

                    if (bInMultiLineComment)
                        continue;

                    // single line comment
                    if (Trimmed.Data[0] == '#')
                        continue;

                    StringList List = String_SplitIntoList(Arena, Trimmed, ' ', true);
                    u16 Num = 0;
                    for each_str_list (List) Num++;

                    if (bWantsRebuild) Num++;
                    if (bWantsClean)   Num++;

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
                            //i++;
                        }
                    }

                    u32 ReturnValue = RiftBuild(Arena, NewArguments);
                    if (ReturnValue != 0)
                    {
                        return ReturnValue;
                    }

                    if (bQuietBuild) Logging_Enable();

                    LOG_LINE_BREAK();

                    u32 Rows = 0, Cols = 0;
                    if (Platform_GetTerminalDimensions(&Rows, &Cols))
                    {
                        char Separator[256] = {'='};
                        for (u32 i = 0; i < Min(Cols, 255); i++)
                        {
                            Separator[i] = '=';
                        }

                        LOG("%S\n", StrSlice(Separator, Cols));
                    }

                    if (bQuietBuild) Logging_Disable();

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
            Filesystem_IterateDirectory_Ex(WorkingDirectory, BuildFileDirectoryIterator, false, &Data);
            if (Data.NumBuildFilesFound == 1)
            {
                Data.bFoundBuildFile = true;
            }

            if ((!Data.bFoundBuildFile || Data.NumBuildFilesFound > 1) && Arguments.Num > 0)
            {
                Filesystem_IterateDirectory_Ex(WorkingDirectory, BuildFileDirectoryIterator_Args, true, &Data);
            }
        }
        else
        {
            Filesystem_IterateDirectory_Ex(WorkingDirectory, BuildFileDirectoryIterator, true, &Data);
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

            Filesystem_IterateDirectory(WorkingDirectory, MultipleBuildFileDirectoryIterator, true);

            return 1;
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
            String_BuildPath(&BuildFilePathFull, WorkingDirectory, BuildFilePath);
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
                    continue;

                // multiline comment
                if (Trimmed.Data[0] == '#' && Trimmed.Data[1] == '#')
                {
                    bInMultiLineComment = !bInMultiLineComment;
                    continue;
                }

                if (bInMultiLineComment)
                    continue;

                // single line comment
                if (Trimmed.Data[0] == '#')
                    continue;

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

            Filesystem_SeekToBeginning(BuildFileHandle);
        
            if (bFoundPreset)
            {
                StringList List = String_SplitIntoList(Arena, PresetArgumentLine, ' ', true);
                u16 Num = 0;
                for each_str_list (List) Num++;

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
    u32 ExitCode = BuildTarget(Arena, BuildFileHandle, &BuildMutex, WorkingDirectory, BuildArguments, S(""), BuildFileIndex, RootPathIndex);
    if (BuildMutex.Handle) Platform_ReleaseMutex(&BuildMutex);

    Filesystem_Close(&BuildFileHandle);

    return ExitCode;
}

internal void InitInternalVars(LinearAllocator* Arena)
{
    const u32 MaxInternalVars = 256;
    const u32 MaxSize = MaxInternalVars * sizeof(InternalVariable); // 8192 bytes
    InternalVariablesDB = _ArrayCreateStatic(LinearAllocator_Allocate(Arena, MaxSize), MaxInternalVars, sizeof(InternalVariable));

    // store internal options. like platform, native os .lib's, etc..
    AddInternalVariable(S(PLATFORM_STRING), S(""));

    // TODO: POSIX
    // TODO: _POSIX = "version"

    #if PLATFORM_WINDOWS
    AddInternalVariable(S("_Platform"), S("Windows"));
    AddInternalVariable(S("Windows"), S(""));
    AddInternalVariable(S("Win32"), S(""));
    #if PLATFORM_64_BIT
    AddInternalVariable(S("Win64"), S(""));
    #endif
    #elif PLATFORM_MAC
    AddInternalVariable(S("_Platform"), S("macOS"));
    AddInternalVariable(S("Apple"), S(""));
    AddInternalVariable(S("Macintosh"), S(""));
    AddInternalVariable(S("Mac"), S(""));
    AddInternalVariable(S("macOS"), S(""));
    AddInternalVariable(S("Unix"), S(""));
    #elif PLATFORM_LINUX
    AddInternalVariable(S("_Platform"), S("Linux"));
    AddInternalVariable(S("Linux"), S(""));
    AddInternalVariable(S("Unix"), S(""));

    // TODO: add more linux distros (look in /etc/os-release for distro name)
    /*
    AddInternalVariable(S("_Distribution"), S(""));
    AddInternalVariable(S("_Distro"), S(""));
    AddInternalVariable(S("Arch"), S(""));
    AddInternalVariable(S("Ubuntu"), S(""));
    AddInternalVariable(S("Mint"), S(""));
    AddInternalVariable(S("Debian"), S(""));
    */

    // dynamically add one from /etc/os-release or /usr/lib/os-release
    //AddInternalVariable(, S(""));

    #if PLATFORM_LINUX_GNOME
    AddInternalVariable(S("GNOME"), S(""));
    AddInternalVariable(S("_DesktopEnvironment"), S("GNOME"));
    AddInternalVariable(S("_DesktopEnv"), S("GNOME"));
    AddInternalVariable(S("_DE"), S("GNOME"));
    #elif PLATFORM_LINUX_KDE
    AddInternalVariable(S("KDE"), S(""));
    AddInternalVariable(S("_DesktopEnvironment"), S("KDE"));
    AddInternalVariable(S("_DesktopEnv"), S("KDE"));
    AddInternalVariable(S("_DE"), S("KDE"));
    #elif PLATFORM_LINUX_CINNAMON
    AddInternalVariable(S("Cinnamon"), S(""));
    AddInternalVariable(S("_DesktopEnvironment"), S("Cinnamon"));
    AddInternalVariable(S("_DesktopEnv"), S("Cinnamon"));
    AddInternalVariable(S("_DE"), S("Cinnamon"));
    #endif
    
    #elif PLATFORM_BSD
    AddInternalVariable(S("_Platform"), S("BSD " PLATFORM_STRING));
    AddInternalVariable(S("BSD"), S(""));
    #else
    AddInternalVariable(S("_Platform"), S("Unix"));
    AddInternalVariable(S("Unix"), S(""));
    #endif

    String Win32Libs = S("kernel32 user32 opengl32 shell32 gdi32 comdlg32 comctl32 ws2_32 winmm netapi32 ole32 advapi32 "
                         "wldap32 crypt32 rpcrt4 shlwapi dbghelp bcrypt version imm32 cfgmgr32 setupapi oleaut32 "
                         "uuid odbc32 odbccp32 delayimp userenv");

    String LinuxLibs = S("m");

    AddInternalVariable(S("_Win32Libs"), Win32Libs);
    AddInternalVariable(S("_LinuxLibs"), LinuxLibs);

    #if PLATFORM_WINDOWS
    AddInternalVariable(S("_NativeLibs"), Win32Libs);
    #elif PLATFORM_LINUX || PLATFORM_UNIX
    AddInternalVariable(S("_NativeLibs"), LinuxLibs);
    #endif

    AddInternalVariable(S("_Arch"), S(CPU_ARCHITECTURE_STRING));
    
    #if PLATFORM_64_BIT
    AddInternalVariable(S("_Bit"), S("64"));
    AddInternalVariable(S("64-bit"), S(""));
    #else
    AddInternalVariable(S("_Bit"), S("32"));
    AddInternalVariable(S("32-bit"), S(""));
    #endif

    Uuid ID = UUID_Generate();
    StringLocal(UuidString, 64);
    UUID_ToString(ID, &UuidString);
    AddInternalVariable(S("_UUID"), String_Create(Arena, UuidString));

    const CpuInfo CPUInfo = Platform_QueryCPUInfo();

    String CpuBrandName = S("Unknown");
    StringLocal(CPU, 64);
    if (Platform_GetCpuBrandName(&CPU))
    {
        CpuBrandName = String_Create(Arena, CPU);
        AddInternalVariable(S("_CPUBrand"), CpuBrandName);

        String_ReplaceCharInline(&CPU, ' ', '_');
        CpuBrandName = String_Create(Arena, CPU);
        AddInternalVariable(CpuBrandName, S("1"));
    }
    else
    {
        AddInternalVariable(S("_CPUBrand"), CpuBrandName);
    }

    String CpuFullName = S("Unknown");
    if (Platform_GetFullCpuName(&CPU))
    {
        String_ReplaceCharInline(&CPU, '@', '|');

        CpuFullName = String_Create(Arena, CPU);
    }

    if (CPUInfo.Intel)
    {
        AddInternalVariable(S("_CPUVendor"), S("Intel"));
        AddInternalVariable(S("_CPU"), CpuFullName);

        AddInternalVariable(S("Intel"), S("1"));
    }

    if (CPUInfo.AMD)
    {
        AddInternalVariable(S("_CPUVendor"), S("AMD"));
        AddInternalVariable(S("_CPU"), CpuFullName);

        AddInternalVariable(S("AMD"), S("1"));
    }

    if (CPUInfo.Apple)
    {
        AddInternalVariable(S("_CPUVendor"), S("Apple"));
        AddInternalVariable(S("_CPU"), CpuFullName);
    }

    #if __CPU_X64
    AddInternalVariable(S("x86"), S("1"));
    AddInternalVariable(S("x64"), S("1"));
    #elif __CPU_X86
    AddInternalVariable(S("x86"), S("1"));
    #elif __CPU_ARM64
    AddInternalVariable(S("ARM"), S("1"));
    AddInternalVariable(S("ARM32"), S("1"));
    AddInternalVariable(S("ARM64"), S("1"));
    #elif __CPU_ARM
    AddInternalVariable(S("ARM"), S("1"));
    AddInternalVariable(S("ARM32"), S("1"));
    #elif __CPU_PPC64
    AddInternalVariable(S("PPC"), S("1"));
    AddInternalVariable(S("PPC64"), S("1"));
    #elif __CPU_PPC
    AddInternalVariable(S("PPC"), S("1"));
    #endif

    #define AddInstruction(Instruction) AddInternalVariable(S("_" #Instruction), CPUInfo.Instruction ? S("1") : S("0"))

    // x86
    if (CPUInfo.x86 || CPUInfo.x64)
    {
        AddInternalVariable(S("_MMX"),             CPUInfo.MMX             ? S("1") : S("0"));
        AddInternalVariable(S("_SSE"),             CPUInfo.SSE             ? S("1") : S("0"));
        AddInternalVariable(S("_SSE2"),            CPUInfo.SSE2            ? S("1") : S("0"));
        AddInternalVariable(S("_SSE3"),            CPUInfo.SSE3            ? S("1") : S("0"));
        AddInternalVariable(S("_SSSE3"),           CPUInfo.SSSE3           ? S("1") : S("0"));
        AddInternalVariable(S("_SSE4"),            CPUInfo.SSE4            ? S("1") : S("0"));
        AddInternalVariable(S("_SSE4.1"),          CPUInfo.SSE41           ? S("1") : S("0"));
        AddInternalVariable(S("_SSE4.2"),          CPUInfo.SSE42           ? S("1") : S("0"));
        AddInternalVariable(S("_AES"),             CPUInfo.AES             ? S("1") : S("0"));
        AddInternalVariable(S("_FMA3"),            CPUInfo.FMA3            ? S("1") : S("0"));
        AddInternalVariable(S("_AVX"),             CPUInfo.AVX             ? S("1") : S("0"));
        AddInternalVariable(S("_AVX2"),            CPUInfo.AVX2            ? S("1") : S("0"));
        AddInternalVariable(S("_F16C"),            CPUInfo.F16C            ? S("1") : S("0"));
        AddInternalVariable(S("_BMI1"),            CPUInfo.BMI1            ? S("1") : S("0"));
        AddInternalVariable(S("_BMI2"),            CPUInfo.BMI2            ? S("1") : S("0"));
        AddInternalVariable(S("_LZCNT"),           CPUInfo.LZCNT           ? S("1") : S("0"));
        AddInternalVariable(S("_TZCNT"),           CPUInfo.TZCNT           ? S("1") : S("0"));
        AddInternalVariable(S("_ADX"),             CPUInfo.ADX             ? S("1") : S("0"));
        AddInternalVariable(S("_MPX"),             CPUInfo.MPX             ? S("1") : S("0"));
        AddInternalVariable(S("_SHA"),             CPUInfo.SHA             ? S("1") : S("0"));
        AddInternalVariable(S("_RDRAND"),          CPUInfo.RDRAND          ? S("1") : S("0"));
        AddInternalVariable(S("_PCLMULQDQ"),       CPUInfo.PCLMULQDQ       ? S("1") : S("0"));
        AddInternalVariable(S("_DTES64"),          CPUInfo.DTES64          ? S("1") : S("0"));
        AddInternalVariable(S("_MONITOR"),         CPUInfo.MONITOR         ? S("1") : S("0"));
        AddInternalVariable(S("_DSCPL"),           CPUInfo.DSCPL           ? S("1") : S("0"));
        AddInternalVariable(S("_VMX"),             CPUInfo.VMX             ? S("1") : S("0"));
        AddInternalVariable(S("_SMX"),             CPUInfo.SMX             ? S("1") : S("0"));
        AddInternalVariable(S("_EIST"),            CPUInfo.EIST            ? S("1") : S("0"));
        AddInternalVariable(S("_TM2"),             CPUInfo.TM2             ? S("1") : S("0"));
        AddInternalVariable(S("_CNXTID"),          CPUInfo.CNXTID          ? S("1") : S("0"));
        AddInternalVariable(S("_SDBG"),            CPUInfo.SDBG            ? S("1") : S("0"));
        AddInternalVariable(S("_CX16"),            CPUInfo.CX16            ? S("1") : S("0"));
        AddInternalVariable(S("_XTPR"),            CPUInfo.XTPR            ? S("1") : S("0"));
        AddInternalVariable(S("_PDCM"),            CPUInfo.PDCM            ? S("1") : S("0"));
        AddInternalVariable(S("_PCID"),            CPUInfo.PCID            ? S("1") : S("0"));
        AddInternalVariable(S("_DCA"),             CPUInfo.DCA             ? S("1") : S("0"));
        AddInternalVariable(S("_X2APIC"),          CPUInfo.X2APIC          ? S("1") : S("0"));
        AddInternalVariable(S("_MOVBE"),           CPUInfo.MOVBE           ? S("1") : S("0"));
        AddInternalVariable(S("_POPCNT"),          CPUInfo.POPCNT          ? S("1") : S("0"));
        AddInternalVariable(S("_TSCDEADLINE"),     CPUInfo.TSCDEADLINE     ? S("1") : S("0"));
        AddInternalVariable(S("_XSAVE"),           CPUInfo.XSAVE           ? S("1") : S("0"));
        AddInternalVariable(S("_OSXSAVE"),         CPUInfo.OSXSAVE         ? S("1") : S("0"));
        AddInternalVariable(S("_HYPERVISOR"),      CPUInfo.HYPERVISOR      ? S("1") : S("0"));


        AddInstruction(FPU);
        AddInstruction(VME);
        AddInstruction(DE);
        AddInstruction(PSE);
        AddInstruction(TSC);
        AddInstruction(MSR);
        AddInstruction(PAE);
        AddInstruction(MCE);
        AddInstruction(CX8);
        AddInstruction(APIC);
        AddInstruction(SEP);
        AddInstruction(MTRR);
        AddInstruction(PGE);
        AddInstruction(MCA);
        AddInstruction(CMOV);
        AddInstruction(PAT);
        AddInstruction(PSE36);
        AddInstruction(PSN);
        AddInstruction(CLFLUSH);
        AddInstruction(DS);
        AddInstruction(ACPI);
        AddInstruction(FXSR);
        AddInstruction(SS);
        AddInstruction(HTT);
        AddInstruction(TM);
        AddInstruction(PBE);

        AddInternalVariable(S("_RDSEED"),           CPUInfo.RDSEED            ? S("1") : S("0"));
        AddInternalVariable(S("_PREFETCHWT1"),      CPUInfo.PREFETCHWT1       ? S("1") : S("0"));
        AddInternalVariable(S("_RDPID"),            CPUInfo.RDPID             ? S("1") : S("0"));
        AddInternalVariable(S("_AVX512F"),          CPUInfo.AVX512F           ? S("1") : S("0"));
        AddInternalVariable(S("_AVX512DQ"),         CPUInfo.AVX512DQ          ? S("1") : S("0"));
        AddInternalVariable(S("_AVX512IFMA"),       CPUInfo.AVX512IFMA        ? S("1") : S("0"));
        AddInternalVariable(S("_AVX512PF"),         CPUInfo.AVX512PF          ? S("1") : S("0"));
        AddInternalVariable(S("_AVX512ER"),         CPUInfo.AVX512ER          ? S("1") : S("0"));
        AddInternalVariable(S("_AVX512CD"),         CPUInfo.AVX512CD          ? S("1") : S("0"));
        AddInternalVariable(S("_AVX512BW"),         CPUInfo.AVX512BW          ? S("1") : S("0"));
        AddInternalVariable(S("_AVX512VL"),         CPUInfo.AVX512VL          ? S("1") : S("0"));
        AddInternalVariable(S("_AVX512"),           CPUInfo.AVX512            ? S("1") : S("0"));
        AddInternalVariable(S("_AVX512VBMI"),       CPUInfo.AVX512VBMI        ? S("1") : S("0"));
        AddInternalVariable(S("_AVX512VBMI2"),      CPUInfo.AVX512VBMI2       ? S("1") : S("0"));
        AddInternalVariable(S("_AVX512VPCLMUL"),    CPUInfo.AVX512VPCLMUL     ? S("1") : S("0"));
        AddInternalVariable(S("_AVX512VNNI"),       CPUInfo.AVX512VNNI        ? S("1") : S("0"));
        AddInternalVariable(S("_AVX512BITALG"),     CPUInfo.AVX512BITALG      ? S("1") : S("0"));
        AddInternalVariable(S("_AVX512VPOPCNTDQ"),  CPUInfo.AVX512VPOPCNTDQ   ? S("1") : S("0"));
        AddInternalVariable(S("_AVX5124VNNIW"),     CPUInfo.AVX512VNNI        ? S("1") : S("0"));
        AddInternalVariable(S("_AVX5124FMAPS"),     CPUInfo.AVX5124FMAPS      ? S("1") : S("0"));
        AddInternalVariable(S("_AVX512BF16"),       CPUInfo.AVX512BF16        ? S("1") : S("0"));
        AddInternalVariable(S("_AVX512FP16"),       CPUInfo.AVX512FP16        ? S("1") : S("0"));
        AddInternalVariable(S("_GFNI"),             CPUInfo.GFNI              ? S("1") : S("0"));
        AddInternalVariable(S("_VAES"),             CPUInfo.VAES              ? S("1") : S("0"));
        AddInternalVariable(S("_FSGSBASE"),         CPUInfo.FSGSBASE          ? S("1") : S("0"));
        AddInternalVariable(S("_TSCADJUST"),        CPUInfo.TSCADJUST         ? S("1") : S("0"));
        AddInternalVariable(S("_SGX"),              CPUInfo.SGX               ? S("1") : S("0"));
        AddInternalVariable(S("_HLE"),              CPUInfo.HLE               ? S("1") : S("0"));
        AddInternalVariable(S("_FDP_EXCEPTN_ONLY"), CPUInfo.FDP_EXCEPTN_ONLY  ? S("1") : S("0"));
        AddInternalVariable(S("_SMEP"),             CPUInfo.SMEP              ? S("1") : S("0"));
        AddInternalVariable(S("_ERMS"),             CPUInfo.ERMS              ? S("1") : S("0"));
        AddInternalVariable(S("_INVPCID"),          CPUInfo.INVPCID           ? S("1") : S("0"));
        AddInternalVariable(S("_RTM"),              CPUInfo.RTM               ? S("1") : S("0"));
        AddInternalVariable(S("_PQM"),              CPUInfo.PQM               ? S("1") : S("0"));
        AddInternalVariable(S("_FPU_DEPR"),         CPUInfo.FPU_DEPR          ? S("1") : S("0"));
        AddInternalVariable(S("_PQE"),              CPUInfo.PQE               ? S("1") : S("0"));
        AddInternalVariable(S("_SMAP"),             CPUInfo.SMAP              ? S("1") : S("0"));
        AddInternalVariable(S("_PCOMMIT"),          CPUInfo.PCOMMIT           ? S("1") : S("0"));
        AddInternalVariable(S("_CLFLUSHOPT"),       CPUInfo.CLFLUSHOPT        ? S("1") : S("0"));
        AddInternalVariable(S("_CLWB"),             CPUInfo.CLWB              ? S("1") : S("0"));
        AddInternalVariable(S("_INTELPT"),          CPUInfo.INTELPT           ? S("1") : S("0"));
        AddInternalVariable(S("_UMIP"),             CPUInfo.UMIP              ? S("1") : S("0"));
        AddInternalVariable(S("_PKU"),              CPUInfo.PKU               ? S("1") : S("0"));
        AddInternalVariable(S("_OSPKE"),            CPUInfo.OSPKE             ? S("1") : S("0"));
        AddInternalVariable(S("_WAITPKG"),          CPUInfo.WAITPKG           ? S("1") : S("0"));
        AddInternalVariable(S("_CET_SS"),           CPUInfo.CET_SS            ? S("1") : S("0"));
        AddInternalVariable(S("_VPCLMULQDQ"),       CPUInfo.VPCLMULQDQ        ? S("1") : S("0"));
        AddInternalVariable(S("_TME"),              CPUInfo.TME               ? S("1") : S("0"));
        AddInternalVariable(S("_LA57"),             CPUInfo.LA57              ? S("1") : S("0"));
        AddInternalVariable(S("_KL"),               CPUInfo.KL                ? S("1") : S("0"));
        AddInternalVariable(S("_CLDEMOTE"),         CPUInfo.CLDEMOTE          ? S("1") : S("0"));
        AddInternalVariable(S("_MOVDIRI"),          CPUInfo.MOVDIRI           ? S("1") : S("0"));
        AddInternalVariable(S("_MOVDIR64B"),        CPUInfo.MOVDIR64B         ? S("1") : S("0"));
        AddInternalVariable(S("_ENQCMD"),           CPUInfo.ENQCMD            ? S("1") : S("0"));
        AddInternalVariable(S("_SGXLC"),            CPUInfo.SGXLC             ? S("1") : S("0"));
        AddInternalVariable(S("_BUSLOCKDETECT"),    CPUInfo.BUSLOCKDETECT     ? S("1") : S("0"));
    }

    // Arm
    if (CPUInfo.ARM || CPUInfo.ARM64)
    {
        AddInternalVariable(S("_NEON"),             CPUInfo.NEON              ? S("1") : S("0"));
        AddInternalVariable(S("_NEON_HPFP"),        CPUInfo.NEON_HPFP         ? S("1") : S("0"));
        AddInternalVariable(S("_NEON_FP16"),        CPUInfo.NEON_FP16         ? S("1") : S("0"));
        AddInternalVariable(S("_ARMV8_1_ATOMICS"),  CPUInfo.ARMV8_1_ATOMICS   ? S("1") : S("0"));
        AddInternalVariable(S("_ARMV8_2_FHM"),      CPUInfo.ARMV8_2_FHM       ? S("1") : S("0"));
        AddInternalVariable(S("_ARMV8_2_SHA512"),   CPUInfo.ARMV8_2_SHA512    ? S("1") : S("0"));
        AddInternalVariable(S("_ARMV8_2_SHA3"),     CPUInfo.ARMV8_2_SHA3      ? S("1") : S("0"));
        AddInternalVariable(S("_ARMV8_3_COMPNUM"),  CPUInfo.ARMV8_3_COMPNUM   ? S("1") : S("0"));
        AddInternalVariable(S("_ARMV8_CRC32"),      CPUInfo.ARMV8_CRC32       ? S("1") : S("0"));
        AddInternalVariable(S("_ARMV8_GPI"),        CPUInfo.ARMV8_GPI         ? S("1") : S("0"));
        AddInternalVariable(S("_AdvSIMD"),          CPUInfo.AdvSIMD           ? S("1") : S("0"));
        AddInternalVariable(S("_AdvSIMD_HPFPCvt"),  CPUInfo.AdvSIMD_HPFPCVT   ? S("1") : S("0"));
        AddInternalVariable(S("_UCNORMAL_MEM"),     CPUInfo.UCNORMAL_MEM      ? S("1") : S("0"));
        AddInternalVariable(S("_M1"),               CPUInfo.FLAGM             ? S("1") : S("0"));
        AddInternalVariable(S("_M2"),               CPUInfo.FLAGM2            ? S("1") : S("0"));
        AddInternalVariable(S("_M3"),               CPUInfo.FLAGM3            ? S("1") : S("0"));
        AddInternalVariable(S("_M4"),               CPUInfo.FLAGM4            ? S("1") : S("0"));
        AddInternalVariable(S("_FHM"),              CPUInfo.FHM               ? S("1") : S("0"));
        AddInternalVariable(S("_DOTPROD"),          CPUInfo.DOTPROD           ? S("1") : S("0"));
        AddInternalVariable(S("_SHA3"),             CPUInfo.SHA3              ? S("1") : S("0"));
        AddInternalVariable(S("_RDM"),              CPUInfo.RDM               ? S("1") : S("0"));
        AddInternalVariable(S("_LSE"),              CPUInfo.LSE               ? S("1") : S("0"));
        AddInternalVariable(S("_SHA256"),           CPUInfo.SHA256            ? S("1") : S("0"));
        AddInternalVariable(S("_SHA512"),           CPUInfo.SHA512            ? S("1") : S("0"));
        AddInternalVariable(S("_SHA1"),             CPUInfo.SHA1              ? S("1") : S("0"));
        AddInternalVariable(S("_AES"),              CPUInfo.AES               ? S("1") : S("0"));
        AddInternalVariable(S("_PMULL"),            CPUInfo.PMULL             ? S("1") : S("0"));
        AddInternalVariable(S("_SPECRES"),          CPUInfo.SPECRES           ? S("1") : S("0"));
        AddInternalVariable(S("_SB"),               CPUInfo.SB                ? S("1") : S("0"));
        AddInternalVariable(S("_FRINTTS"),          CPUInfo.FRINTTS           ? S("1") : S("0"));
        AddInternalVariable(S("_LRCPC"),            CPUInfo.LRCPC             ? S("1") : S("0"));
        AddInternalVariable(S("_LRCPC2"),           CPUInfo.LRCPC2            ? S("1") : S("0"));
        AddInternalVariable(S("_FCMA"),             CPUInfo.FCMA              ? S("1") : S("0"));
        AddInternalVariable(S("_JSCVT"),            CPUInfo.JSCVT             ? S("1") : S("0"));
        AddInternalVariable(S("_PAUTH"),            CPUInfo.PAUTH             ? S("1") : S("0"));
        AddInternalVariable(S("_PAUTH2"),           CPUInfo.PAUTH2            ? S("1") : S("0"));
        AddInternalVariable(S("_FPAC"),             CPUInfo.FPAC              ? S("1") : S("0"));
        AddInternalVariable(S("_DPB"),              CPUInfo.DPB               ? S("1") : S("0"));
        AddInternalVariable(S("_DPB2"),             CPUInfo.DPB2              ? S("1") : S("0"));
        AddInternalVariable(S("_BF16"),             CPUInfo.BF16              ? S("1") : S("0"));
        AddInternalVariable(S("_I8MM"),             CPUInfo.I8MM              ? S("1") : S("0"));
        AddInternalVariable(S("_ECV"),              CPUInfo.ECV               ? S("1") : S("0"));
        AddInternalVariable(S("_LSE2"),             CPUInfo.LSE2              ? S("1") : S("0"));
        AddInternalVariable(S("_CSV2"),             CPUInfo.CSV2              ? S("1") : S("0"));
        AddInternalVariable(S("_CSV3"),             CPUInfo.CSV3              ? S("1") : S("0"));
        AddInternalVariable(S("_DIT"),              CPUInfo.DIT               ? S("1") : S("0"));
        AddInternalVariable(S("_FP16"),             CPUInfo.FP16              ? S("1") : S("0"));
        AddInternalVariable(S("_SSBS"),             CPUInfo.SSBS              ? S("1") : S("0"));
        AddInternalVariable(S("_BTI"),              CPUInfo.BTI               ? S("1") : S("0"));
    }

    // todo: store all supported extensions
    //AddInternalVariable(S("_CPUExtensions"), S(""));

    StringLocal(CacheLineSize, 8);
    String_Format(&CacheLineSize, S("%u"), 7, Platform_GetCpuCacheLineSize());
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
    Platform_GetAccountName(&AccountName);
    String Allocated = String_Create(Arena, AccountName);
    AddInternalVariable(S("_Account"), Allocated);
    AddInternalVariable(S("_AccountName"), Allocated);

    StringLocal(UserName, 256);
    Platform_GetUserName(&UserName);
    Allocated = String_Create(Arena, UserName);
    AddInternalVariable(S("_User"), Allocated);
    AddInternalVariable(S("_UserName"), Allocated);

    StringLocal(UserDirectory, MAX_PATH_LENGTH);
    Platform_GetUserDirectory(&UserDirectory);
    String_EatPathSeparatorsInlineFromEnd(&UserDirectory);
    Allocated = String_Create(Arena, UserDirectory);
    AddInternalVariable(S("_UserDirectory"), Allocated);
    AddInternalVariable(S("_HomeDirectory"), Allocated);
    AddInternalVariable(S("_Home"),          Allocated);
}

u32 RunApplication(const StringArray Arguments)
{
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
    #ifdef DEVELOPER
    String_BuildSeparator(&ExtraFlags, ' ', S("[DEBUG]"));
    #endif
    #ifdef RIFT_ASAN
    String_BuildSeparator(&ExtraFlags, ' ', S("[ASAN]"));
    #endif
    #ifdef HOOD
    String_BuildSeparator(&ExtraFlags, ' ', S("- (HOOD EDITION)"));
    #endif

    LOG("\nRift Build System v%S (%S %S) %S\n", S(RIFTBUILD_VERSION_STRING), S(PLATFORM_STRING), S(CPU_ARCHITECTURE_STRING), ExtraFlags);

    #ifdef HOOD
    LOG("\nwasssup yo. les get build'n...\n");
    #endif

    LinearAllocator ProgramArena = {0};
    char ProgramMemory[Kilobytes(512)] = {0};
    LinearAllocator_Create(Kilobytes(512), ProgramMemory, &ProgramArena);

    InitInternalVars(&ProgramArena);

    u32 ExitCode = RiftBuild(&ProgramArena, Arguments);

    #if !PLATFORM_MAC
    const bool bLaunchedFromDesktop = StringArray_Contains(Arguments, S("--from-desktop"), false);
    if (Platform_GetConsoleProcessCount() == 1 || bLaunchedFromDesktop)
    {
        LOG_INLINE_WARNING("\nLaunched outside an existing terminal, pausing until user exit.\nPress any key to exit ... ");

        Platform_BeginNonBlockingMode();
        while (1)
        {
            Platform_Sleep(10);
            if (Platform_AnyKeyPressed())
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
