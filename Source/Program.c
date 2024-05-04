#include "EntryPoint.h"

u64 GEngineMemoryAmount  = Kibibytes(512);
u64 GEngineScratchAmount = Kibibytes(32);

#include "Platform/Filesystem.h"
#include "String/StringUtils.h"
#include "Structures/Array.h"
#include "Uuid.h"

#include "Backend.h"

TArray(InternalVariable) InternalVariablesDB = NULL;

STRUCT(BuildFileDirectoryIteratorData)
{
    bool bFoundBuildFile;
    bool bNoBuildFileSpecifiedInCmd;
    i8 BuildFileIndex;
    i8 RootPathIndex;
    u8 NumBuildFilesFound;
    String* Name;
    String* Path;
    StringArray Arguments;
};

STRUCT(Context)
{
    String WorkingDirectory;
    String BuildFilePath;
    String BuildFileName;
    StringArray Arguments;
    LinearAllocator Arena;
    FileHandle BuildFileHandle;
};

internal bool IsBuildFile(const String FilePath)
{
    return String_EndsWith(FilePath, StrLit(".build"), false);
}

bool DoesCmdVarExist(TArray(CmdOption) CmdOptionsDB, const String Name)
{
    for each (o, CmdOptionsDB)
    {
        if (String_IsEqual(o.Name, Name, false))
        {
            return true;
        }
    }

    return false;
}

String GetCmdOptionValue(TArray(CmdOption) CmdOptionsDB, const String Name)
{
    for each (o, CmdOptionsDB)
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
    for each (Var, VariablesDB)
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

    for each (Var, VariablesDB)
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

internal String GetVariableValue(TArray(FileVariable) VariablesDB, const String Name)
{
    for each (Var, VariablesDB)
    {
        if (String_IsEqual(Var.Name, Name, false))
        {
            return Var.Value;
        }
    }

    return String_Null();
}

String GetExpandedVariableValue(TArray(FileVariable) ExpandedVariablesDB, const String Name)
{
    for each (Var, ExpandedVariablesDB)
    {
        if (String_IsEqual(Var.Name, Name, false))
        {
            return Var.Value;
        }
    }

    return String_Null();
}

internal void PrefixVariables(String* Dest, String VariableValue, const String Prefix)
{
    bool bInsideQuote = false;
    bool bSawSpace = false;

    if (VariableValue.Length > 0)
    {
        String_Append(Dest, Prefix);

        #if PLATFORM_LINUX
        if (String_StartsWith(VariableValue, StrLit("lib"), false) && // TODO: only care about extension??
            String_IsEqual(Prefix, StrLit("-l"), true))// &&
            //String_IndexOfChar(VariableValue, '.', NULL))
        {
            String_AppendChar(Dest, ':');
        }
        #endif
    }

    for (u32 i = 0; i < VariableValue.Length; i++)
    {
        char C = VariableValue.Data[i];

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
                    String_Append(Dest, Prefix);

                    #if PLATFORM_LINUX
                    if (String_StartsWith(StrShiftF(VariableValue, i), StrLit("lib"), false) &&
                        String_IsEqual(Prefix, StrLit("-l"), true))
                    {
                        String_AppendChar(Dest, ':');
                    }
                    #endif
                }
            }
        }

        if (C == '"')
        {
            bInsideQuote = !bInsideQuote;
        }

        String_AppendChar(Dest, C);
    }
}

internal void SuffixVariables(String* Dest, String VariableValue, const String Suffix)
{
    bool bInsideQuote = false;
    bool bSawSpace = false;

    for (u32 i = 0; i < VariableValue.Length; i++)
    {
        char C = VariableValue.Data[i];

        if (C == ' ')
        {
            bSawSpace = true;
        }
        else
        {
            if (bSawSpace)
            {
                bSawSpace = false;
            }
        }

        if (C == '"')
        {
            bInsideQuote = !bInsideQuote;
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

bool ExtensionHas(const String ExtensionString, const String Ext)
{
    TEMP_SCRATCH(Ext)
    {
        StringArray Options = String_ParseIntoArray(Scratch_Ext.Allocator, ExtensionString, ' ', 0, 8);

        for each_str (s, Options)
        {
            String e = String_EatChar(*s, '.');

            if (String_IsEqual(e, Ext, false))
            {
                return true;
            }
        }
    }

    return false;
}

#if PLATFORM_WINDOWS
internal bool IconFileDirectoryIterator(const String FullPath, const String RelativePath, const String FileName, u64 FileSize, bool bIsDirectory, void* UserData)
{
    if (FileSize > 0)
    {
        struct Data
        {
            TArray(FileVariable) ExpandedVarsArray;
            String* IconFilePath;
        };

        struct Data* D = (struct Data*)UserData;

        if (String_IsEqual(FileName, GetExpandedVariableValue(D->ExpandedVarsArray, StrLit("Icon")), false))
        {
            String_Copy(D->IconFilePath, RelativePath);
            return false;
        }
    }

    return true;
}

/*
internal bool ResourceFileDirectoryIterator(const String FullPath, const String RelativePath, const String FileName, u64 FileSize, bool bIsDirectory)
{
    if (FileSize > 0)
    {
        if (String_IsEqual(FileName, GetExpandedVariableValue(StrLit("Resource")), false))
        {
            String_Copy(&GResourceFilePath, RelativePath);
            return false;
        }
    }

    return true;
}
*/
#endif

// TODO: get rid of these
bool IsSource(const String Ext)
{
    return C_IsSource(Ext);
}

bool IsHeader(const String Ext)
{
    return C_IsHeader(Ext);
}

internal bool SourceFileCounterDirectoryIterator(const String FullPath, const String RelativePath, const String FileName, u64 FileSize, bool bIsDirectory, void* UserData)
{
    if (FileSize > 0)
    {
        if (String_StartsWith(FileName, StrLit("__"), false))
        {
            return true;
        }

        struct SourceCountData
        {
            u32 NumSources;
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
        };

        struct SourceCountData* Data = (struct SourceCountData*)UserData;

        u32 DotIndex = 0;
        String_IndexOfLastChar(FileName, '.', &DotIndex);

        const String Extension = StrShiftF(FileName, DotIndex);

        if (IsSource(Extension))
        {
            if (String_IsEqual(Extension, StrLit(".rc"), false))
            {
                // we will build this later
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

                if (!Data->bHasCppFiles)
                {
                    if (String_EndsWith(RelativePath, StrLit(".cpp"), false) ||
                        String_EndsWith(RelativePath, StrLit(".c++"), false) ||
                        String_EndsWith(RelativePath, StrLit(".cc"), false) ||
                        String_EndsWith(RelativePath, StrLit(".cxx"), false) ||
                        String_EndsWith(RelativePath, StrLit(".hpp"), false) ||
                        String_EndsWith(RelativePath, StrLit(".h++"), false) ||
                        String_EndsWith(RelativePath, StrLit(".hh"), false) ||
                        String_EndsWith(RelativePath, StrLit(".hxx"), false))
                    {
                        Data->bHasCppFiles = true;
                    }
                }
            }
        }
        else if (IsHeader(Extension))
        {
            Data->NumHeaders++;
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
            struct HeaderIterData
            {
                u64 AssemblyFileTime;
                bool* bShouldRebuild;
            };

            struct HeaderIterData* Data = (struct HeaderIterData*)UserData;

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

    return true;
}

/*
internal bool SourceFileDirectoryIterator(const String FullPath, const String RelativePath, const String FileName, u64 FileSize, bool bIsDirectory, void* UserData)
{
    if (FileSize > 0)
    {
        //todo: remove
        if (String_StartsWith(RelativePath, StrLit("Intermediate"), true) ||
            String_StartsWith(RelativePath, StrLit("Build"), true))
        {
            return true;
        }

        // todo: actual build path
        if (String_StartsWith(FullPath, GIntermediateBaseDirectory, false))
        {
            return true;
        }

        if (String_StartsWith(FileName, StrLit("__"), false))
        {
            return true;
        }

        u32 DotIndex = 0;
        String_IndexOfLastChar(FileName, '.', &DotIndex);

        const String Extension = StrShiftF(FileName, DotIndex);

        if (IsSource(Extension))
        {
            SourceFileData Data;
            Data.FullPath = String_Create(&GSourceFilePathAllocator, FullPath);
            Data.RelativePath = String_Create(&GSourceFilePathAllocator, RelativePath);

            Array_Add(GSourceFiles, Data);
        }
        else if (IsHeader(Extension))
        {
            SourceFileData Data;
            Data.FullPath = String_Create(&GSourceFilePathAllocator, FullPath);
            Data.RelativePath = String_Create(&GSourceFilePathAllocator, RelativePath);

            Array_Add(GHeaderFiles, Data);
        }
    }

    return true;
}
*/

internal bool BuildFileDirectoryIterator_Args(const String FullPath, const String RelativePath, const String FileName, u64 FileSize, bool bIsDirectory, void* UserData)
{
    if (FileSize > 0)
    {
        if (IsBuildFile(FileName))
        {
            BuildFileDirectoryIteratorData* Data = (BuildFileDirectoryIteratorData*)UserData;

            bool bFoundFromNameSearch = false;
            //const StringArray Args = Platform_GetCommandLineArgs();
            for (u32 i = 0; i < Data->Arguments.Num; i++)
            {
                if (Data->BuildFileIndex == (i8)i || Data->RootPathIndex == (i8)i)
                {
                    continue;
                }

                StringLocal(Temp, MAX_PATH_LENGTH);
                String_Copy(&Temp, Data->Arguments.List[i]);
                String_Append(&Temp, StrLit(".build"));
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
    bFileMatch = String_EndsWith(FileName, StrLit(".lib"), false);
    #elif PLATFORM_APPLE
    bFileMatch = String_EndsWith(FileName, StrLit(".dylib"), false) ||
                 String_EndsWith(FileName, StrLit(".a"), false);
    #else
    bFileMatch = String_EndsWith(FileName, StrLit(".so"), false) ||
                 String_EndsWith(FileName, StrLit(".a"), false);
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
        if (IsBuildFile(FileName))
        {
            BuildFileDirectoryIteratorData* Data = (BuildFileDirectoryIteratorData*)UserData;

            if (String_StartsWith(FileName, StrLit("__"), false))
            {
                if (Data->bNoBuildFileSpecifiedInCmd) // maybe people wanna explicity specify the build file if they type it in the command line, so dont ignore it
                {
                    return true;
                }
            }

            if (Data->bNoBuildFileSpecifiedInCmd ||
                !String_IsValid(*Data->Name) ||
                String_IsEqual(FileName, *Data->Name, false))
            {
                Data->bFoundBuildFile = true; // found build file?

                if (Data->NumBuildFilesFound > 0)
                {
                    String_Empty(Data->Name);

                    if (Data->Path)
                        String_Empty(Data->Path);
                }
                else
                {
                    String_Copy(Data->Name, FileName);

                    if (Data->Path)
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

// the problem is that we want to separate one long collection of paths into an array of paths,
// but we cant just do a simple split on the space character because some paths have spaces in them,
// so we need to be able to detect when a space is inside of a " " and ignore it. sigh...
internal StringList Internal_ParseStringIntoList(LinearAllocator* Arena, const String Value)
{
    StringList List = {0};
    List.Next = NULL;

    bool bInsideQuote = false;
    bool bSawSpace = false;

    if (Value.Length > 0)
    {
        u32 Offset = 0;
        u32 CurrentLength = 0;
        for (u32 i = 0; i < Value.Length+1; i++)
        {
            char C = i < Value.Length ? Value.Data[i] : 0;

            bool bLastChar = i == Value.Length-1;
            if (C == ' ' || bLastChar)
            {
                bSawSpace = true;

                if (bLastChar)
                {
                    CurrentLength++;
                }
            }
            else
            {
                if (bSawSpace)
                {
                    bSawSpace = false;

                    if (!bInsideQuote)
                    {
                        String Slice = String_EatSpacesFromEnd(StrSlice(Value.Data+Offset, CurrentLength-1));

                        if (List.String.Data == NULL)
                        {
                            List.String = String_Create(Arena, Slice);
                        }
                        else
                        {
                            StringList* Entry = LinearAllocator_Allocate(Arena, sizeof(StringList));
                            Entry->String = String_Create(Arena, Slice);
                            Entry->Next = NULL;

                            StringList** Next = &List.Next;
                            while (*Next)
                            {
                                Next = &(*Next)->Next;
                            }

                            *Next = Entry;
                        }

                        Offset += CurrentLength;
                        CurrentLength = 0;
                    }
                }
            }

            if (C == '"')
            {
                bInsideQuote = !bInsideQuote;
            }

            CurrentLength++;
        }
    }

    return List;
}

bool LogStringList_WordWrapped(const String Name, const StringList List)
{
    StringList History = {0};
    u32 ParentCount = 0;
    bool bLogged = false;

    TEMP_SCRATCH(Log)
    {
        for each_str_list (List)
        {
            StringList ValueList = Internal_ParseStringIntoList(Scratch_Log.Allocator, It.String);
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

                    String NameCopy = String_Reserve(Scratch_Log.Allocator, Name.Length);
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

                StringList* Entry = LinearAllocator_Allocate(Scratch_Log.Allocator, sizeof(StringList));
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
    }

    return bLogged;
}

internal void LogBuildVariable(TArray(FileVariable) VariablesDB, const String Name, const String DisplayName)
{
    LOG_INLINE("%S", DisplayName);

    TEMP_SCRATCH(Log)
    {
        StringList List = GetVariableValueList(Scratch_Log.Allocator, VariablesDB, Name);
        LogStringList_WordWrapped(DisplayName, List);

        /*
        LOG_INLINE("%S", DisplayName);
        for each_str_list (List)
        {
            if (String_IsValid(It.String))
            {
                LOG_INLINE(" %S", It.String);
            }
        }
        */
    }

    LOG_LINE_BREAK();
}

bool LogString_WordWrapped(const String Name, const String Value, const bool bAddNewLine)
{
    if (Value.Length > 0)
    {
        LOG_INLINE("%S", Name);

        const StringList l = {Value, NULL};
        if (LogStringList_WordWrapped(Name, l))
        {
            LOG_LINE_BREAK();
            if (bAddNewLine) LOG_LINE_BREAK();
            return true;
        }
    }

    return false;
}

internal void ListVariables(const String Name, TArray(FileVariable) ExpandedVariablesDB) 
{
    const String Exclusions[] =
    {
        StrLit("AssertProgramExists"),
        StrLit("AssertLibExists"),
        StrLit("AssertBuildVarExists"),
        StrLit("AssertWorkingDirectory"),
        StrLit("AssertCmdVarExists"),
        StrLit("AssertEnvVarExists"),
        StrLit("AssertPlatform"),
        StrLit("PreBuild"),
        StrLit("PostBuild"),
        StrLit("RunAssembly"),
        StrLit("Depends"),
        StrLit("Assembly"),
        StrLit("Extension"),
        StrLit("Type"),
        StrLit("TitleName"),
        StrLit("Description"),
        StrLit("CompanyName"),
        StrLit("Version"),
        StrLit("Copyright"),
        StrLit("SourceDirectory"),
        StrLit("IntermediateDirectory"),
        StrLit("BuildDirectory"),
        StrLit("Icon")
    };

    for each (v, ExpandedVariablesDB)
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
            
            if (bOneLine)
            {
                LOG("    %S", v.Value);
            }
            else
            {
                TEMP_SCRATCH(_)
                {
                    StringList Values = Internal_ParseStringIntoList(Scratch__.Allocator, v.Value);
                    for each_str_list (Values)
                    {
                        LOG("    %S", It.String);
                    }
                }
            }
        
            LOG_LINE_BREAK();
        }
    }
}

internal bool Internal_ExecuteBuildCmd(const String WorkingDirectory, const String Name, const String Value, u32* ExitCode)
{
    if (!String_IsValid(Value))
        return true;

    if (String_EndsWith(Name, StrLit("Cmd"), false))
    {
        const String Cmd = Value;

        StringLocal(CmdLine, 8192);

        #if PLATFORM_WINDOWS
        String_Append(&CmdLine, StrLit("cmd.exe /c \""));
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

        bool bIgnoreErrors = String_EndsWith(Name, StrLit("_Cmd"), false);

        PlatformHandle Handle = Platform_RunCommand(CmdLine, WorkingDirectory);
        if (!Platform_IsValidHandle(Handle) && !bIgnoreErrors)
        {
            *ExitCode = 1;
            return false;
        }

        *ExitCode = Platform_WaitForProcessAndGetExitCode(Handle);
        if (*ExitCode != 0 && !bIgnoreErrors)
        {
            return false;
        }
    }
    else if (String_EndsWith(Name, StrLit("Copy"), false))
    {
        const String Cmd = Value;

        u32 FirstSpace = 0;
        String_IndexOfFirstWhitespace(Cmd, &FirstSpace);

        const String SourceFile           = StrSlice(Cmd.Data, FirstSpace);
        const String DestinationDirectory = String_EatSpaces(StrShiftF(Cmd, FirstSpace+1));

        LOG("Copy: %S", Cmd);

        bool bIgnoreErrors = String_EndsWith(Name, StrLit("_Copy"), false);

        if (!Filesystem_Copy(SourceFile, DestinationDirectory) && !bIgnoreErrors)
        {
            *ExitCode = 1;
            return false;
        }
    }

    return true;
}

internal void Internal_SetDefaultBuildVariables(LinearAllocator* Arena, FileHandle* BuildFileHandle, TArray(FileVariable) VariablesDB, TArray(FileVariable) ExpandedVariablesDB)
{
    if (!DoesBuildVarExist(VariablesDB, StrLit("Assembly")))
    {
        String Name = StrLit("Untitled");

        if (IsValidFileHandle(BuildFileHandle))
        {
            StringLocal(Path, MAX_PATH_LENGTH);
            Filesystem_GetFilePath(BuildFileHandle, &Path);

            u32 LastSlash = 0;
            String_IndexOfLastPathSlash(Path, &LastSlash);

            u32 LastDot = 0;
            Name = StrShiftF(Path, LastSlash+1);
            String_IndexOfLastChar(Name, '.', &LastDot);
            Name = StrSlice(Name.Data, LastDot);
        }

        FileVariable Expanded;
        Expanded.Name = StrLit("Assembly");
        Expanded.Value = String_Create(Arena, Name);

        Array_Add(VariablesDB, Expanded);
        Array_Add(ExpandedVariablesDB, Expanded);
    }

    const String Type = GetExpandedVariableValue(ExpandedVariablesDB, StrLit("Type"));
    if (String_IsValid(Type))
    {
        String Extension = StrLit("");
        
        FileVariable Expanded;
        Expanded.Name = StrLit("Extension");
        Expanded.Value = StrLit("");

        if (String_IsEqual(Type, StrLit("lib"), false))
        {
            #if PLATFORM_WINDOWS
                Extension = StrLit(".dll .lib");
            #elif PLATFORM_APPLE
                Extension = StrLit(".dylib .a");
            #elif PLATFORM_LINUX || PLATFORM_UNIX
                Extension = StrLit(".so .a");
            #endif

            Expanded.Value = Extension;
        }
        else if (String_IsEqual(Type, StrLit("static_lib"), false))
        {
            #if PLATFORM_WINDOWS
                Extension = StrLit(".lib");
            #elif PLATFORM_APPLE
                Extension = StrLit(".a");
            #elif PLATFORM_LINUX || PLATFORM_UNIX
                Extension = StrLit(".a");
            #endif

            Expanded.Value = Extension;
        }
        else if (String_IsEqual(Type, StrLit("dynamic_lib"), false))
        {
            #if PLATFORM_WINDOWS
                Extension = StrLit(".dll");
            #elif PLATFORM_APPLE
                Extension = StrLit(".dylib");
            #elif PLATFORM_LINUX || PLATFORM_UNIX
                Extension = StrLit(".so");
            #endif

            Expanded.Value = Extension;
        }
        else if (String_IsEqual(Type, StrLit("app"), false) ||
                String_IsEqual(Type, StrLit("exe"), false))
        {
            #if PLATFORM_WINDOWS
                Extension = StrLit(".exe");
            #elif PLATFORM_APPLE
                Extension = StrLit("");
            #elif PLATFORM_LINUX || PLATFORM_UNIX
                Extension = StrLit("");
            #endif

            Expanded.Value = Extension;
        }

        Array_Add(VariablesDB, Expanded);
        Array_Add(ExpandedVariablesDB, Expanded);
    }

    if (!DoesBuildVarExist(VariablesDB, StrLit("Extension")))
    {
        FileVariable Expanded;
        Expanded.Name = StrLit("Extension");

        #if PLATFORM_WINDOWS
            Expanded.Value = StrLit(".exe");
        #elif PLATFORM_APPLE
            Expanded.Value = StrLit("");
        #elif PLATFORM_LINUX || PLATFORM_UNIX
            Expanded.Value = StrLit("");
        #endif

        Array_Add(VariablesDB, Expanded);
        Array_Add(ExpandedVariablesDB, Expanded);
    }

    if (!DoesBuildVarExist(VariablesDB, StrLit("Compiler")))
    {
        //bNoCompilerProgramExplicityGiven = true;

        FileVariable Expanded;
        Expanded.Name = StrLit("Compiler");
        Expanded.Value = StrLit("clang");

        Array_Add(VariablesDB, Expanded);
        Array_Add(ExpandedVariablesDB, Expanded);
    }

    if (!DoesBuildVarExist(VariablesDB, StrLit("Version")))
    {
        //#if PLATFORM_WINDOWS
        //bFallbackVersion = true;
        //#endif

        FileVariable Expanded;
        Expanded.Name = StrLit("Version");
        Expanded.Value = StrLit("1.0.0");

        Array_Add(VariablesDB, Expanded);
        Array_Add(ExpandedVariablesDB, Expanded);
    }

    if (!DoesBuildVarExist(VariablesDB, StrLit("BuildDirectory")))
    {
        FileVariable Expanded;
        Expanded.Name = StrLit("BuildDirectory");
        Expanded.Value = StrLit("Build");

        Array_Add(VariablesDB, Expanded);
        Array_Add(ExpandedVariablesDB, Expanded);
    }

    if (!DoesBuildVarExist(VariablesDB, StrLit("IntermediateDirectory")))
    {
        FileVariable Expanded;
        Expanded.Name = StrLit("IntermediateDirectory");
        Expanded.Value = StrLit("Intermediate");

        Array_Add(VariablesDB, Expanded);
        Array_Add(ExpandedVariablesDB, Expanded);
    }

    if (!DoesBuildVarExist(VariablesDB, StrLit("SourceDirectory")))
    {
        FileVariable Expanded;
        Expanded.Name = StrLit("SourceDirectory");
        Expanded.Value = StrLit("");

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

internal void CheckForBuildVariableOverrides(TArray(FileVariable) VariablesDB, TArray(CmdOption) CmdOptionsDB)
{
    // check if the user wants to override a build variable
    for each (o, CmdOptionsDB)
    {
        if (String_StartsWith(o.Name, StrLit("override:"), false))
        {
            String VarToOverride = StrShiftF(o.Name, 9);

            for each (Var, VariablesDB)
            {
                if (String_IsEqual(Var.Name, VarToOverride, false))
                {
                    LOG("Overriding build variable \"%S\" from \"%S\" to \"%S\"", Var.Name, Var.Value, o.Value);
                    Var_->Value = o.Value;
                    break;
                }
            }
        }
    }
}

internal void LogPathEnvVarTutorialSteps(void)
{
    #ifdef HOOD
    LOG("aight lisen up dawg, this is how you put a new entry to the path env var on yo system:");
    #else
    LOG("Here is how to add a new entry to the path environment variable:");
    #endif

    #if PLATFORM_WINDOWS
    LOG("    1. Open the start menu and type \"Environment Variables\"");
    LOG("    2. Click on \"Edit the system environment variables\"");
    LOG("    3. Click on \"Environment Variables\"");
    LOG("    4. In the \"System variables\" section, scroll down and select \"Path\"");
    LOG("    5. Click on \"Edit...\"");
    LOG("    6. Click on \"New\"");
    LOG("    7. Type in the path to the compiler executable. For example: \"C:\\MyCompiler\\bin\"");
    LOG("    8. Click on \"OK\" until all windows are closed");
    LOG("    9. Restart the command prompt (by closing and opening it again) for changes to take effect");
    #elif PLATFORM_APPLE
    LOG("    1. Open the terminal");
    LOG("    2. Type in \"sudo nano /etc/paths\"");
    LOG("    3. Enter your password");
    LOG("    4. Go to the bottom of the list and add the path to the compiler executable. For example: \"/Users/Bob/MyCompiler/bin\"");
    LOG("    5. Press \"Ctrl + X\" to exit");
    LOG("    6. Press \"Y\" to save changes");
    LOG("    7. Press \"Enter\" to confirm");
    LOG("    8. Restart the terminal (by closing and opening it again) for changes to take effect");
    #else
    LOG("    1. Open the terminal");
    LOG("    2. Type in \"nano ~/.bashrc\"");
    LOG("    3. Go to the bottom of the file and add the path to the compiler executable. For example: \"export PATH=${PATH}:/Users/Bob/MyCompiler/bin\"");
    LOG("    4. Press \"Ctrl + X\" to exit");
    LOG("    5. Press \"Y\" to save changes");
    LOG("    6. Press \"Enter\" to confirm");
    LOG("    7. Restart the terminal (by closing and opening it again) for changes to take effect");
    #endif
}

internal void LogRegularEnvVarTutorialSteps(void)
{
    #ifdef HOOD
    LOG("aight lisen up dawg, this is how you put a new env var on yo system:");
    #else
    LOG("Here is how to add a new environment variable:");
    #endif

    #if PLATFORM_WINDOWS
    LOG("    1. Open the start menu and type \"Environment Variables\"");
    LOG("    2. Click on \"Edit the system environment variables\"");
    LOG("    3. Click on \"Environment Variables\"");
    LOG("    4. In either the \"User variables\" or \"System variables\" section, click on \"New\"");
    LOG("    5. Type in the name of the variable. For example: \"MY_COOL_VARIABLE\"");
    LOG("    6. Type in the value of the variable. For example: \"Some_useful_value\"");
    LOG("    7. Click on \"OK\" until all windows are closed");
    LOG("    8. Restart the command prompt (by closing and opening it again) for changes to take effect");
    #elif PLATFORM_APPLE
    LOG("    1. Open a terminal");
    LOG("    2. Type in \"sudo nano ~/.zshrc\"");
    LOG("    3. Enter your password");
    LOG("    4. Go to the bottom of the file and add the variable like so -> \"export MY_COOL_VARIABLE=Some_useful_value\"");
    LOG("    5. Press \"Ctrl + X\" to exit");
    LOG("    6. Press \"Y\" to save changes");
    LOG("    7. Press \"Enter\" to confirm");
    LOG("    8. Restart the terminal (by closing and opening it again) for changes to take effect");
    #else
    LOG("    1. Open a terminal");
    LOG("    2. Type in \"nano ~/.bashrc\"");
    LOG("    3. Go to the bottom of the file and add the variable like so -> \"export MY_COOL_VARIABLE=Some_useful_value\"");
    LOG("    4. Press \"Ctrl + X\" to exit");
    LOG("    5. Press \"Y\" to save changes");
    LOG("    6. Press \"Enter\" to confirm");
    LOG("    7. Restart the terminal (by closing and opening it again) for changes to take effect");
    #endif
}

bool FilterSourceFile(const String WorkingDirectory, const String SourceDirectory,
                      const String FullPath, const String RelativePath,
                      StringList WhitelistFiles, StringList BlacklistFiles,
                      StringList WhitelistDirectories, StringList BlacklistDirectories)
{
    String TrimmedFileName = String_Null();
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
        if (String_IsEqual(It.String, StrLit("*"), false))
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
                if (Index < TrimmedFileName.Length)
                {
                    if ((Index == 0 || String_IsEqual(Left, StrSlice(TrimmedFileName.Data, Index), true)) &&
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
                    bIsBlacklisted = true;
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

internal u32 BuildTarget(LinearAllocator* Arena,
                        FileHandle* BuildFileHandle,
                        const String WorkingPath, const StringArray Parameters, const String CameFromBuildFile,
                        i8 BuildFileIndex, i8 RootPathIndex, bool bSingleThread)
{
    Clock BuildRuntime;
    Clock_Start(&BuildRuntime);

    StringLocal(RiftCmdLine, 2048);
    for (u8 i = 0; i < Parameters.Num; i++)
    {
        String_Append     (&RiftCmdLine, Parameters.List[i]);
        String_AppendSpace(&RiftCmdLine);
    }
    String_EatSpacesInlineFromEnd(&RiftCmdLine);


    bool bIsClean    = StringArray_Contains(Parameters, StrLit("clean"), false);
    bool bIsRebuild  = StringArray_Contains(Parameters, StrLit("rebuild"), false);
    bool bVerboseLog = StringArray_Contains(Parameters, StrLit("-v"), false);
    //bool bGenCompileCommandsJSON = StringArray_Contains(Arguments, StrLit("gen_compile_commands"), false);
    //bool bGenVisualStudio = StringArray_Contains(Arguments, StrLit("gen_visual_studio_project"), false);
    //bool bGenXCode = StringArray_Contains(Arguments, StrLit("gen_xcode_project"), false);
    
    bool bFoundBuildFile = IsValidFileHandle(BuildFileHandle);

    #if PLATFORM_WINDOWS
    StringLocal(IconFilePath, MAX_PATH_LENGTH);
    //StringLocal(ResourceFilePath, MAX_PATH_LENGTH);
    #endif

    const u64 MemAmount                      = _ArrayCalculateMemRequirement(256, sizeof(FileVariable)); // 8192 bytes * 2 = 16KiB
    TArray(FileVariable) VariablesDB         = Array_CreateStatic(FileVariable, 256, LinearAllocator_Allocate(Arena, MemAmount));
    TArray(FileVariable) ExpandedVariablesDB = Array_CreateStatic(FileVariable, 256, LinearAllocator_Allocate(Arena, MemAmount));

    const u64 MemAmount_IncludeFiles         = _ArrayCalculateMemRequirement(64, sizeof(FileHandle)); // 1024 bytes
    TArray(FileHandle) IncludeFiles          = Array_CreateStatic(FileHandle, 64, LinearAllocator_Allocate(Arena, MemAmount_IncludeFiles));

    const u64 MemAmount_CmdOptions           = _ArrayCalculateMemRequirement(128, sizeof(CmdOption)); // 4608 bytes
    TArray(CmdOption) CmdOptionsDB           = Array_CreateStatic(CmdOption, 128, LinearAllocator_Allocate(Arena, MemAmount_CmdOptions));

    const u64 MemAmount_Messages             = _ArrayCalculateMemRequirement(128, sizeof(String)); // 2048 bytes
    TArray(String) Messages                  = Array_CreateStatic(String, 128, LinearAllocator_Allocate(Arena, MemAmount_Messages));

    // 256 is a reasonable max number of compilers to run in parrallel. if you have more than 256 cores then what the fuck lol
    const u64 MemAmount_Processes            = _ArrayCalculateMemRequirement(256, sizeof(PlatformHandle)); // 2048 bytes
    TArray(PlatformHandle) Processes         = Array_CreateStatic(PlatformHandle, 256, LinearAllocator_Allocate(Arena, MemAmount_Processes));

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

    String BuildFileName;
    StringLocal(BuildFilePath, MAX_PATH_LENGTH);
    {
        u32 LastSlash = 0;
        String_IndexOfLastPathSlash(BuildFilePathFull, &LastSlash);

        u32 LastDot = 0;
        BuildFileName = StrShiftF(BuildFilePathFull, LastSlash+1);
        
        const String NameCopy = String_Create(Arena, BuildFileName);
        String_IndexOfLastChar(NameCopy, '.', &LastDot);

        AddCmdOption(&CmdOptionsDB, StrLit("_FileName"), StrSlice(NameCopy.Data, LastDot));
        AddCmdOption(&CmdOptionsDB, StrLit("_FileNameExt"), NameCopy);

        const String PathFull = String_Create(Arena, StrSlice(BuildFilePathFull.Data, LastSlash));
        AddCmdOption(&CmdOptionsDB, StrLit("_FileDirectoryFull"), PathFull);

        const String PathRelative = StrShiftF(StrSlice(BuildFilePathFull.Data, LastSlash), WorkingPath.Length+1);

        String_BuildPath(&BuildFilePath, PathRelative, BuildFileName);

        AddCmdOption(&CmdOptionsDB, StrLit("_FileDirectory"), String_Create(Arena, PathRelative));

        AddCmdOption(&CmdOptionsDB, StrLit("_WorkingDirectory"), WorkingPath);
    }

    SystemTime TimeNow = Platform_GetSystemLocalTime();
    StringLocal(TimeStamp, 64);
    String_Format(&TimeStamp, StrLit("%hu-%.2hu-%.2hu %.2hu:%.2hu:%.2hu"), TimeStamp.Capacity, TimeNow.Year, TimeNow.Month, TimeNow.Day, TimeNow.Hour, TimeNow.Minute, TimeNow.Second);

    StringLocal(TimeStampVar, 64);
    String_Format(&TimeStampVar, StrLit("%hu.%.2hu.%.2hu.%.2hu.%.2hu.%.2hu"), TimeStamp.Capacity, TimeNow.Year, TimeNow.Month, TimeNow.Day, TimeNow.Hour, TimeNow.Minute, TimeNow.Second);
    AddCmdOption(&CmdOptionsDB, StrLit("_Timestamp"), TimeStampVar);

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

        AddCmdOption(&CmdOptionsDB, StrLit("%"), RiftBuildArgs);
        AddCmdOption(&CmdOptionsDB, StrLit("_Args"), RiftBuildArgs);
    }

    if (bFoundBuildFile)
    {
        #ifndef HOOD
        LOG("Using build file:  %S", BuildFilePath);
        LOG("Working Directory: %S", WorkingPath);
        LOG("Timestamp:         %S\n", TimeStamp);
        #else
        LOG("alright sweet, using this build file btw: %S", BuildFilePath);
        LOG("dis da work'n directory bro: %S", WorkingDirectory);
        LOG("Timestamp: %S\n", TimeStamp);
        #endif
    }

    bool bIsAssemblyExe = false;
    bool bShouldWaitPerCompileProcess = false;
    bool bNoCompilerProgramExplicityGiven = false;

    #if PLATFORM_WINDOWS
    bool bFallbackVersion = false;
    #endif

    Clock BuildFileParseClock = {0};

    if (bFoundBuildFile)
    {
        Clock_Start(&BuildFileParseClock);

        if (!ParseBuildFile(Arena, BuildFileHandle, BuildFilePath, WorkingPath, VariablesDB, ExpandedVariablesDB,
                                    CmdOptionsDB, Messages, IncludeFiles,
                                    NULL, false, NULL, false))
        {
            return 1;
        }

        CheckForBuildVariableOverrides(VariablesDB, CmdOptionsDB);

        // first expand Type and Extension. so on linux we can tell if its an assembly exe and not a library
        for each (v, VariablesDB)
        {
            if (String_IsEqual(v.Name, StrLit("Extension"), false) ||
                String_IsEqual(v.Name, StrLit("Type"), false))
            {
                StringLocal(ExpandedVar, 64);

                TEMP_SCRATCH(Exp)
                {
                    StringList List = GetVariableValueList(Scratch_Exp.Allocator, VariablesDB, v.Name);
                    for each_str_list (List)
                    {
                        if (!ExpandBuildVariable(VariablesDB, CmdOptionsDB, &ExpandedVar, v.Name, It.String, v.Name, false, bIsAssemblyExe))
                        {
                            Memory_ReleaseScratch(&Scratch_Exp);
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
                if (String_IsEqual(v.Name, StrLit("Extension"), false) && SanitizedVar.Length > 0)
                {
                    PrefixVariables(&Extension, SanitizedVar, StrLit("."));
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

        const String Ext = GetExpandedVariableValue(ExpandedVariablesDB, StrLit("Extension"));
        const String Type = GetExpandedVariableValue(ExpandedVariablesDB, StrLit("Type"));

        bIsAssemblyExe = Type.Length == 0 && Ext.Length == 0;
        if (bIsAssemblyExe)
        {
            FileVariable Var;
            Var.Name = StrLit("Type");
            Var.Value = StrLit("app");
            Array_Add(ExpandedVariablesDB, Var);
        }

        if (!bIsAssemblyExe)
        {
            bIsAssemblyExe = String_IsEqual(Type, StrLit("app"), false);
        }

        if (!bIsAssemblyExe && Type.Length == 0)
        {
            bIsAssemblyExe = Ext.Length == 0 || String_IsEqual(Ext, StrLit(".out"), false) || String_IsEqual(Ext, StrLit(".exe"), false);
        }

        String AssemblyKey = StrLit("Assembly");
        if (DoesBuildVarExist(VariablesDB, AssemblyKey))
        {
            StringLocal(ExpandedVar, 256);
            if (!ExpandBuildVariable(VariablesDB, CmdOptionsDB, &ExpandedVar, AssemblyKey, GetVariableValue(VariablesDB, AssemblyKey), AssemblyKey, false, bIsAssemblyExe))
            {
                return 1;
            }

            String_EatSpacesInlineFromEnd(&ExpandedVar);

            FileVariable Expanded;
            Expanded.Name = AssemblyKey;
            Expanded.Value = String_Create(Arena, ExpandedVar);
            Array_Add(ExpandedVariablesDB, Expanded);
        }

        if (!DoesBuildVarExist(VariablesDB, S("Compiler")))
        {
            bNoCompilerProgramExplicityGiven = true;
        }

        String VersionKey = StrLit("Version");
        bool bDoesVersionVarExist = DoesBuildVarExist(VariablesDB, VersionKey);

        #if PLATFORM_WINDOWS
        if (!bDoesVersionVarExist)
        {
            bFallbackVersion = true;
        }
        #endif

        // set defaults for a few key build variables
        Internal_SetDefaultBuildVariables(Arena, BuildFileHandle, VariablesDB, ExpandedVariablesDB);
        CheckForBuildVariableOverrides(VariablesDB, CmdOptionsDB);

        // try expand Version (if it exists)
        if (bDoesVersionVarExist)
        {
            StringLocal(ExpandedVar, 256);
            if (!ExpandBuildVariable(VariablesDB, CmdOptionsDB, &ExpandedVar, VersionKey, GetVariableValue(VariablesDB, VersionKey), VersionKey, false, bIsAssemblyExe))
            {
                return 1;
            }

            String_EatSpacesInlineFromEnd(&ExpandedVar);
            String_ReplaceNonAlphaNumericCharInline(&ExpandedVar, '.');

            if (ExpandedVar.Length > 0)
            {
                FileVariable Expanded;
                Expanded.Name = VersionKey;
                Expanded.Value = String_Create(Arena, ExpandedVar);
                Array_Add(ExpandedVariablesDB, Expanded);

                // add the defines (if desired)
                if (!DoesBuildVarExist(VariablesDB, StrLit("NoVersionDefines")))
                {
                    const String VersionLevels[3] = 
                    {
                        StrLit("MAJOR"),
                        StrLit("MINOR"),
                        StrLit("PATCH")
                    };

                    StringLocal(AssemblyNameUpper, 128);
                    String_Copy(&AssemblyNameUpper, GetExpandedVariableValue(ExpandedVariablesDB, StrLit("Assembly")));
                    String_ReplaceCharInline(&AssemblyNameUpper, '-', '_');
                    String_ToUpper(&AssemblyNameUpper);

                    {
                        StringLocal(VersionDefineString, 256);
                        String_Format(&VersionDefineString, StrLit("%S_VERSION_STRING=\\\"%S\\\""), VersionDefineString.Capacity, AssemblyNameUpper, ExpandedVar);

                        FileVariable Var;
                        Var.Name = StrLit("Defines");
                        Var.Value = String_Create(Arena, VersionDefineString);
                        Array_Add(VariablesDB, Var);
                    }

                    const u32 NumDots = String_CountChar(ExpandedVar, '.');
                    if (NumDots > 0)
                    {
                        TEMP_SCRATCH(_)
                        {
                            StringArray Versions = String_ParseIntoArray(Scratch__.Allocator, ExpandedVar, '.', 0, 128);

                            u8 i = 0;
                            for each_str (v, Versions)
                            {
                                if (v->Length > 0)
                                {
                                    StringLocal(VersionDefine, 256);
                                    if (i < 3)
                                    {
                                        String_Format(&VersionDefine, StrLit("%S_%S_VERSION=%S"), VersionDefine.Capacity, AssemblyNameUpper, VersionLevels[i], *v);
                                    }
                                    else
                                    {
                                        String_Format(&VersionDefine, StrLit("%S_DETAIL_VERSION_%hhu=%S"), VersionDefine.Capacity, AssemblyNameUpper, i-3, *v);
                                    }

                                    FileVariable Var;
                                    Var.Name = StrLit("Defines");
                                    Var.Value = String_Create(Arena, VersionDefine);
                                    Array_Add(VariablesDB, Var);

                                    i++;
                                }
                            }
                        }
                    }
                    else
                    {
                        StringLocal(VersionDefine, 256);
                        String_Format(&VersionDefine, StrLit("%S_VERSION=%S"), VersionDefine.Capacity, AssemblyNameUpper, ExpandedVar);

                        FileVariable Var;
                        Var.Name = StrLit("Defines");
                        Var.Value = String_Create(Arena, VersionDefine);
                        Array_Add(VariablesDB, Var);
                    }
                }
            }
        }

        // expand all build variables
        for each (v, VariablesDB)
        {
            // already expanded
            if (String_IsEqual(v.Name, StrLit("Extension"), false) ||
                String_IsEqual(v.Name, StrLit("Assembly"), false) ||
                String_IsEqual(v.Name, StrLit("Version"), false) ||
                String_IsEqual(v.Name, StrLit("Type"), false))
            {
                continue;
            }

            StringLocal(ExpandedVar, 4096);

            const String Exclusions[] =
            {
                StrLit("AssertProgramExists"),
                StrLit("AssertBuildVarExists"),
                StrLit("AssertLibExists"),
                StrLit("AssertWorkingDirectory"),
                StrLit("AssertCmdVarExists"),
                StrLit("AssertEnvVarExists"),
                StrLit("AssertPlatform"),
                StrLit("PreBuild"),
                StrLit("PostBuild"),
                StrLit("Depends"),
                StrLit("RunAssembly"),
            };

            // do not join the above variables into one long string basically, is what this is for
            bool bIsExcludedFromMultiVarDeclarations = false;
            for (u8 i = 0; i < (sizeof Exclusions / sizeof(String)); i++)
            {
                if (String_StartsWith(v.Name, Exclusions[i], false))
                {
                    bIsExcludedFromMultiVarDeclarations = true;
                    break;
                }
            }

            if (!bIsExcludedFromMultiVarDeclarations)
            {
                bool bAlreadyExpanded = GetExpandedVariableValue(ExpandedVariablesDB, v.Name).Length > 0;
                if (bAlreadyExpanded)
                    continue;

                TEMP_SCRATCH(Exp)
                {
                    StringList List = GetVariableValueList(Scratch_Exp.Allocator, VariablesDB, v.Name);
                    for each_str_list (List)
                    {
                        if (!ExpandBuildVariable(VariablesDB, CmdOptionsDB, &ExpandedVar, v.Name, It.String, v.Name, false, bIsAssemblyExe))
                        {
                            Memory_ReleaseScratch(&Scratch_Exp);
                            return 1;
                        }

                        if (ExpandedVar.Length > 0)
                            String_AppendSpace(&ExpandedVar);
                    }
                }
            }
            else
            {
                if (!ExpandBuildVariable(VariablesDB, CmdOptionsDB, &ExpandedVar, v.Name, v.Value, v.Name, false, bIsAssemblyExe))
                {
                    return 1;
                }
            }

            String_EatSpacesInlineFromEnd(&ExpandedVar);

            FileVariable Expanded;
            Expanded.Name = v.Name;
            Expanded.Value = String_Create(Arena, ExpandedVar);

            Array_Add(ExpandedVariablesDB, Expanded);
        }

        Clock_Tick(&BuildFileParseClock);
    }
    else
    {
        bNoCompilerProgramExplicityGiven = true;

        #if PLATFORM_WINDOWS
        bFallbackVersion = true;
        #endif

        // set defaults for a few key build variables
        FileHandle f = {0};
        Internal_SetDefaultBuildVariables(Arena, &f, VariablesDB, ExpandedVariablesDB);
        CheckForBuildVariableOverrides(VariablesDB, CmdOptionsDB);
    }

    // build file variable listing feature. list:all or list:varname
    for (u8 i = 0; i < Parameters.Num; i++)
    {
        const String Arg = Parameters.List[i];

        if (String_StartsWith(Arg, StrLit("list:"), false))
        {
            u32 Colon = 0;
            if (String_IndexOfChar(Arg, ':', &Colon))
            {
                const String VarToList = StrShiftF(Arg, Colon+1);

                if (VarToList.Length == 0)
                {
                    LOG_ERROR("Failed to list build variable. No variable name was given after ':'");
                    LOG("\nUsage:");
                    LOG("     list:all");
                    LOG("     list:varname");
                    LOG("     list:varname,othername,anotherone");
                    return 1;
                }

                TEMP_SCRATCH(_)
                {
                    StringArray Vars = String_ParseIntoArray(Scratch__.Allocator, VarToList, ',', 0, 128);
                
                    for each_str (var, Vars)
                    {
                        if (String_IsEqual(*var, StrLit("all"), false))
                        {
                            LOG("Listing all build variables...\n");

                            ListVariables(String_Null(), ExpandedVariablesDB);
                        }
                        else
                        {
                            if (!DoesBuildVarExist(VariablesDB, *var))
                            {
                                LOG_ERROR("Failed to list \"%S\". It does not exist in \"%S\" (within the context of the given build parameters)", *var, BuildFilePath);
                                return 1;
                            }

                            ListVariables(*var, ExpandedVariablesDB);
                        }
                    }
                }

                return 0;
            }
        }
    }

    String AssemblyName                     = GetExpandedVariableValue(ExpandedVariablesDB, StrLit("Assembly"));
    String Extension                        = GetExpandedVariableValue(ExpandedVariablesDB, StrLit("Extension"));
    String Type                             = GetExpandedVariableValue(ExpandedVariablesDB, StrLit("Type"));
    String CompilerProgram                  = GetExpandedVariableValue(ExpandedVariablesDB, StrLit("Compiler"));
    String CompilerFlagPrefixSymbol         = StrLit("-");
    const String CompilerFlags              = GetExpandedVariableValue(ExpandedVariablesDB, StrLit("CompilerFlags"));
    String IncludeFlags                     = GetExpandedVariableValue(ExpandedVariablesDB, StrLit("Includes"));
    const String Libraries                  = GetExpandedVariableValue(ExpandedVariablesDB, StrLit("Libraries"));
    String LibraryDirectories               = GetExpandedVariableValue(ExpandedVariablesDB, StrLit("LibraryDirectories"));
    String LinkerFlags                      = GetExpandedVariableValue(ExpandedVariablesDB, StrLit("LinkerFlags"));
    const String Defines                    = GetExpandedVariableValue(ExpandedVariablesDB, StrLit("Defines"));
    const String UnDefines                  = GetExpandedVariableValue(ExpandedVariablesDB, StrLit("UnDefines"));
    const String LinkerDefines              = GetExpandedVariableValue(ExpandedVariablesDB, StrLit("LinkerDefines"));
    const String AssertPlatforms            = GetExpandedVariableValue(ExpandedVariablesDB, StrLit("AssertPlatform"));
    const String AssertArchitecture         = GetExpandedVariableValue(ExpandedVariablesDB, StrLit("AssertArchitecture"));
    const String AssertPrograms             = GetExpandedVariableValue(ExpandedVariablesDB, StrLit("AssertProgramExists"));
    const String AssertEnvVars              = GetExpandedVariableValue(ExpandedVariablesDB, StrLit("AssertEnvVarExists"));
    const String AssertBuildVars            = GetExpandedVariableValue(ExpandedVariablesDB, StrLit("AssertBuildVarExists"));
    const String AssertLibs                 = GetExpandedVariableValue(ExpandedVariablesDB, StrLit("AssertLibExists"));
    String AssertWorkingDirectory           = GetExpandedVariableValue(ExpandedVariablesDB, StrLit("AssertWorkingDirectory"));
    String IncludedSourceFiles              = GetExpandedVariableValue(ExpandedVariablesDB, StrLit("IncludedSourceFiles"));
    String ExcludedSourceFiles              = GetExpandedVariableValue(ExpandedVariablesDB, StrLit("ExcludedSourceFiles"));
    const String IncludedSourceDir          = GetExpandedVariableValue(ExpandedVariablesDB, StrLit("IncludedSourceDirectories"));
    const String ExcludedSourceDir          = GetExpandedVariableValue(ExpandedVariablesDB, StrLit("ExcludedSourceDirectories"));
    const String MaxConcurrentCompilations  = GetExpandedVariableValue(ExpandedVariablesDB, StrLit("MaxConcurrentCompilations"));
    //const String OutsideSourceDirectories   = GetExpandedVariableValue(ExpandedVariablesDB, StrLit("ExternalSourceDirectories"));
    //const String MultiThread                = GetExpandedVariableValue(ExpandedVariablesDB, StrLit("MultiThread"));
    String Icon                             = GetExpandedVariableValue(ExpandedVariablesDB, StrLit("Icon"));
    const String PostBuildSetting           = GetExpandedVariableValue(ExpandedVariablesDB, StrLit("RunPostBuildOnChange"));
    const String MaxCompilerErrors          = GetExpandedVariableValue(ExpandedVariablesDB, StrLit("MaxCompilerErrors"));

    #if PLATFORM_WINDOWS
    const String TitleName                  = GetExpandedVariableValue(ExpandedVariablesDB, StrLit("TitleName"));
    const String Description                = GetExpandedVariableValue(ExpandedVariablesDB, StrLit("Description"));
    const String CompanyName                = GetExpandedVariableValue(ExpandedVariablesDB, StrLit("CompanyName"));
    const String Copyright                  = GetExpandedVariableValue(ExpandedVariablesDB, StrLit("Copyright"));
    #endif

    String Version                          = GetExpandedVariableValue(ExpandedVariablesDB, StrLit("Version"));

    const bool bNoRebuildOnDependencyChange = String_ToBool(GetExpandedVariableValue(ExpandedVariablesDB, StrLit("NoRebuildOnDependencyChange")));

    bShouldWaitPerCompileProcess = bSingleThread;

    bool bRunPostBuildOnlyWhenWorkWasDone = false;
    if (String_IsValid(PostBuildSetting))
    {
        bRunPostBuildOnlyWhenWorkWasDone = String_ToBool(PostBuildSetting);
    }

    u8 MaxErrorsAllowed = 1; // default to 1 error (for the people's sanity)
    if (String_IsValid(MaxCompilerErrors))
    {
        String_ToU8(MaxCompilerErrors, &MaxErrorsAllowed);
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

    if (!String_IsValid(Version))
    {
        #if PLATFORM_WINDOWS
        bFallbackVersion = true;
        #endif

        Version = StrLit("1.0.0");
    }

    StringLocal(VersionCommas, 64);
    String_Copy(&VersionCommas, Version);
    String_ReplaceCharInline(&VersionCommas, '.', ',');
    // .rc files can only have 4 version numbers. sigh...
    u8 CommaCount = 0;
    for (u8 i = 0; i < VersionCommas.Length; i++)
    {
        if (VersionCommas.Data[i] == ',')
        {
            CommaCount++;
            if (CommaCount == 4)
            {
                VersionCommas.Length = i;
                break;
            }
        }
    }

    StringLocal(CompilerPath, MAX_PATH_LENGTH);

    bool bExplicitProgramPath = false;
    if (String_IndexOfFirstPathSlash(CompilerProgram, NULL))
    {
        if (Filesystem_DoesFileExist(CompilerProgram))
        {
            bExplicitProgramPath = true;
            String_Copy(&CompilerPath, CompilerProgram);
        }
        else
        {
            LOG_ERROR("Compiler program \"%S\" does not exist", CompilerProgram);
            return 1;
        }
    }

    // does the compiler program exist on the user's machine
    if (!bExplicitProgramPath)
    {
        bool bCompilerProgramFound = Platform_FindProgram_Ex(CompilerProgram, &CompilerPath);

        if (!bCompilerProgramFound && bNoCompilerProgramExplicityGiven)
        {
            const String CompilerPrograms[] =
            {
                StrLit("gcc"),
                StrLit("x86_64-w64-mingw32-gcc"),
                StrLit("g++"),
                StrLit("clang++"),
                StrLit("cl"),
            };

            for (u8 i = 0; i < SArray_Capacity(CompilerPrograms); i++)
            {
                const bool bFound = Platform_FindProgram_Ex(CompilerPrograms[i], &CompilerPath);
                if (bFound)
                {
                    bCompilerProgramFound = true;
                    break;
                }
            }
        }

        if (!bCompilerProgramFound)
        {
            if (bNoCompilerProgramExplicityGiven)
            {
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
            if (String_IsEqual(CompilerProgram, StrLit("cl"), false))
            {
                #if PLATFORM_WINDOWS
                LOG_ERROR(
                    "Compiler program \"%S\" does not exist. Make sure that you have the Visual Studio build tools installed"
                    " and that you run riftbuild from a different terminal application named"
                    " \"x64 (or x86) Native Tools Command Prompt for VS\"."
                    " This can be found through the windows search. Aborting build...", CompilerProgram);
                #else
                LOG_ERROR("Compiler program \"cl\" does not exist on non-Windows platforms. Use a different compiler. Aborting build...");
                #endif
            }
            else
            {
                LOG_ERROR("Compiler program \"%S\" does not exist. Make sure that it is installed and added to the path environment. Alternatively, you can specify the full path to the compiler executable instead. Aborting build...\n", CompilerProgram);

                LogPathEnvVarTutorialSteps();
            }
            #else
            LOG_ERROR(
                "yo dat compiler program \"%S\" don exist cuh."
                " need to be installed and set in da path ma nigga", CompilerProgram);
            #endif

            return 1;
        }
    }

    //ECompiler Compiler = Compiler_Clang;

    if (String_IsEqual(CompilerProgram, StrLit("cl"), false) ||
        String_IsEqual(CompilerProgram, StrLit("clang-cl"), false) ||
        String_IsEqual(CompilerProgram, StrLit("msvc"), false) || // todo: detect msvc and chang "Compiler" value to "cl"
        String_IsEqual(CompilerProgram, StrLit("clang"), false) ||
        String_IsEqual(CompilerProgram, StrLit("clang++"), false) ||
        String_IsEqual(CompilerProgram, StrLit("gcc"), false) ||
        String_IsEqual(CompilerProgram, StrLit("x86_64-w64-mingw32-gcc"), false) ||
        String_IsEqual(CompilerProgram, StrLit("g++"), false))
    {
        if (String_IsEqual(CompilerProgram, StrLit("cl"), false) ||
            String_IsEqual(CompilerProgram, StrLit("clang-cl"), false) ||
            String_IsEqual(CompilerProgram, StrLit("msvc"), false))
        {
            CompilerFlagPrefixSymbol = StrLit("/");
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
        if (i == 0 || (i > 0 && IsWhitespace(CompilerFlags.Data[i-1])))
        {
            if (CompilerFlags.Data[i] == '-' || CompilerFlags.Data[i] == '/')
            {
                CompilerFlags.Data[i] = CompilerFlagPrefixSymbol.Data[0];
            }
        }
    }

    // run through the assert lists
    TEMP_SCRATCH(Assert)
    {
        StringArray ProgramsArray      = String_ParseIntoArray(Scratch_Assert.Allocator, AssertPrograms, ' ', 0, 128);
        StringArray EnvVarsArray       = String_ParseIntoArray(Scratch_Assert.Allocator, AssertEnvVars, ' ', 0, 128);
        StringArray BuildVarsArray     = String_ParseIntoArray(Scratch_Assert.Allocator, AssertBuildVars, ' ', 0, 128);
        StringArray PlatformsArray     = String_ParseIntoArray(Scratch_Assert.Allocator, AssertPlatforms, ' ', 0, 128);
        StringArray ArchitecturesArray = String_ParseIntoArray(Scratch_Assert.Allocator, AssertArchitecture, ' ', 0, 128);

        for each_str (S, ProgramsArray)
        {
            String Trimmed = String_EatSpaces(*S);

            bool bFound = Platform_FindProgram(Trimmed);

            if (!bFound)
            {
                #ifndef HOOD
                LOG_ERROR("Build assertion failure. Program \"%S\" does not exist. Make sure that \"%S\" is installed and that its directory has been set in the path environment variable. Aborting build...\n", Trimmed, Trimmed);
                LogPathEnvVarTutorialSteps();
                #else
                LOG_ERROR("yo dis program \"%S\" don exist cuh. need to be installed and set in da path ma nigga", CompilerProgram);
                #endif

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
                LOG_ERROR("Build assertion failure. Environment variable \"%S\" does not exist. Aborting build...\n", Trimmed);
                #else
                LOG_ERROR("yo da environment var \"%S\" don exist cuh. need to be setup n' shit ma nigga", Trimmed);
                #endif

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
                LOG_ERROR("Build assertion failure. Build variable \"%S\" does not exist. Aborting build...", Trimmed);
                #else
                LOG_ERROR("yo da build var \"%S\" don exist cuh. dat shit not there nigga", Trimmed);
                #endif

                return 1;
            }
        }

        StringLocal(PlatformsLogString, 128);
        {
            u8 i = 0;
            for each_str_i (i, S, PlatformsArray)
            {
                String_Append(&PlatformsLogString, *S);
                if (PlatformsArray.Num > 1 && i != PlatformsArray.Num-1)
                {
                    if (i == PlatformsArray.Num-2)
                    {
                        String_Append(&PlatformsLogString, StrLit(" and "));
                    }
                    else
                    {
                        String_AppendChar(&PlatformsLogString, ',');
                        String_AppendSpace(&PlatformsLogString);
                    }
                }
            }
        }

        if (PlatformsArray.Num > 0)
        {
            bool bAnyPlatformMatch = false;
            for each_str (S, PlatformsArray)
            {
                String Trimmed = String_EatSpaces(*S);

                #if PLATFORM_WINDOWS
                const String HostPlatform = StrLit("Windows");
                #elif PLATFORM_MAC
                const String HostPlatform = StrLit("Apple MacOS Unix");
                #elif PLATFORM_LINUX
                const String HostPlatform = StrLit("Linux Unix");
                #else
                const String HostPlatform = StrLit("Unix");
                #endif

                bool bMatch = String_IsEqual(Trimmed, HostPlatform, false);
                if (bMatch)
                {
                    bAnyPlatformMatch = true;
                    break;
                }

                StringArray AdditionalPlatforms = String_ParseIntoArray(Scratch_Assert.Allocator, HostPlatform, ' ', 0, 128);
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
                LOG_ERROR("Build assertion failure. %S can only be used on %S. You are on %S. Aborting build...", BuildFileName, PlatformsLogString, StrLit(PLATFORM_STRING));
                #else
                LOG_ERROR("yo u cant build on dis platform nigga");
                #endif

                return 1;
            }
        }

        StringLocal(ArchitecturesLogString, 128);
        {
            u8 i = 0;
            for each_str_i (i, S, ArchitecturesArray)
            {
                String_Append(&ArchitecturesLogString, *S);
                if (ArchitecturesArray.Num > 1 && i != ArchitecturesArray.Num-1)
                {
                    if (i == ArchitecturesArray.Num-2)
                    {
                        String_Append(&ArchitecturesLogString, StrLit(" and "));
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

                bool bMatch = String_IsEqual(Trimmed, StrLit(CPU_ARCHITECTURE_STRING), false);
                if (bMatch)
                {
                    bAnyArchMatch = true;
                    break;
                }

                StringArray AdditionalArchs = String_ParseIntoArray(Scratch_Assert.Allocator, StrLit(CPU_ARCHITECTURE_STRING), ' ', 0, 128);
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
                LOG_ERROR("Build assertion failure. %S can only be used on %S architectures. You are on %S. Aborting build...", BuildFileName, ArchitecturesLogString, StrLit(CPU_ARCHITECTURE_STRING));
                #else
                LOG_ERROR("yo u cant build on dis platform nigga");
                #endif

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
                LOG_ERROR("Build assertion failure. RiftBuild must be ran from this directory -> \"%S\" but we are in \"%S\". Aborting build...", AssertPath, WorkingPath);
                #else
                LOG_ERROR("yo we cant run from this dir cuh \"%S\" you gotta run from \"%S\"", WorkingPath, AssertPath);
                #endif

                return 1;
            }
        }
    }

    /*
    if (bIsAssemblyExe)
    {
        // TODO: why the fuck is this allocating memory
        if (Platform_IsProgramRunning(AssemblyName))
        {
            LOG_ERROR("Assembly \"%S\" is currently running. Close all instances of \"%S.exe\" to continue with the build process. Aborting build...", AssemblyName, AssemblyName);
            return 1;
        }
    }
    */

    // run build depenencies
    bool bRanAnyDependencies = false;
    for each (Var, ExpandedVariablesDB)
    {
        if (String_IsEqual(Var.Name, StrLit("Depends"), false))
        {
            String Value = Var.Value;

            String BuildFile = String_Null();
            String SpecifiedParams = String_Null();

            u32 PipeIndex = 0;
            if (String_IndexOfChar(Value, '|', &PipeIndex))
            {
                // we have params that we need to pass in
                SpecifiedParams = String_EatSpaces(StrSlice(Value.Data+PipeIndex+1, Value.Length-PipeIndex-1));
            }

            u32 SpaceIndex = 0;
            if (String_IndexOfFirstWhitespace(Value, &SpaceIndex))
            {
                BuildFile = StrSlice(Value.Data, SpaceIndex);
            }
            else
            {
                BuildFile = Value;
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
                String_Copy(&CustomWorkingPath, WorkingPath);
            }

            String_EatPathSeparatorsInlineFromEnd(&CustomWorkingPath);

            StringLocal(BuildFileNameWithExt, 128);
            String_Append(&BuildFileNameWithExt, BuildFile);

            if (!String_EndsWith(BuildFile, StrLit(".build"), false))
                String_Append(&BuildFileNameWithExt, StrLit(".build"));

            String BuildType = String_Null();
            if (bIsRebuild)
            {
                BuildType = S("rebuild");
            }
            else if (bIsClean)
            {
                BuildType = S("clean");
            }

            String VerboseFlag = String_Null();
            if (bVerboseLog)
            {
                VerboseFlag = S("-v");
            }

            void* ArenaMemory = Platform_MemAllocZero(Kibibytes(512));
            if (!ArenaMemory)
            {
                LOG_ERROR("Failed to allocate memory from the operating system for %S", BuildFileNameWithExt);
                return 1;
            }

            LinearAllocator NewArena = {0};
            LinearAllocator_Create(Kibibytes(512), ArenaMemory, &NewArena);

            StringLocal(CmdLine, 1024);
            String_BuildSeparator(&CmdLine, ' ', SpecifiedParams, BuildType, VerboseFlag);
            StringList List = Internal_ParseStringIntoList(&NewArena, CmdLine);
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
            
            LOG("Depend -> %S\n", BuildFileNameWithExt);
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
                LOG_ERROR("Failed to find %S in %S", BuildFileNameWithExt, CustomWorkingPath);
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

            u32 ExitCode = BuildTarget(&NewArena, &f, CustomWorkingPath, NewParams, BuildFileName, -1, -1, bSingleThread);

            Filesystem_Close(&f);

            LinearAllocator_Destroy(&NewArena);
            Platform_MemFree(ArenaMemory);

            if (ExitCode == 2)
            {
                if (!bIsRebuild && !bNoRebuildOnDependencyChange)
                {
                    LOG("\nDependency \"%S\" was modified. Forcing rebuild...", BuildFile);
                    bIsRebuild = true;
                }
            }
            else if (ExitCode != 0)
            {
                #ifndef HOOD
                LOG_ERROR("Dependency build \"%S\" failed. Aborting build...", BuildFile);
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
        LOG("[All build dependencies complete. Continuing with %S]\n", BuildFileName);
    }

    u16 NumPreBuildCmds = 0;
    u16 NumPostBuildCmds = 0;

    for each (Var, ExpandedVariablesDB)
    {
        if (String_StartsWith(Var.Name, StrLit("PreBuild"), false))
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
        for each (Var, ExpandedVariablesDB)
        {
            if (String_StartsWith(Var.Name, StrLit("PreBuild"), false))
            {
                u32 ExitCode = 0;
                bool bResult = Internal_ExecuteBuildCmd(WorkingPath, Var.Name, Var.Value, &ExitCode);
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
    TEMP_SCRATCH(LibTests)
    {
        // TODO: support native libraries. windows kits, msvc lib paths, etc
        StringArray LibsArray = String_ParseIntoArray(Scratch_LibTests.Allocator, AssertLibs, ' ', 0, 128);
        for each_str (S, LibsArray)
        {
            const String Trimmed = String_EatSpaces(*S);

            StringLocal(TrimmedCopy, MAX_PATH_LENGTH);
            #if !PLATFORM_WINDOWS
            String_Append(&TrimmedCopy, StrLit("lib"));
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

            StringList DirList = Internal_ParseStringIntoList(Scratch_LibTests.Allocator, LibraryDirectories);
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
                    bAnyFound = Platform_FindFile(TrimmedCopy, StrLit(".lib"));
                    #else
                    bAnyFound = Platform_FindFile(TrimmedCopy, StrLit(".a"));
                    if (!bAnyFound)
                        bAnyFound = Platform_FindFile(TrimmedCopy, StrLit(".so"));
                    #endif
                }
            }

            if (!bAnyFound)
            {
                #ifndef HOOD
                if (bExactFile)
                {
                    LOG_ERROR("Build assertion failure. Library \"%S\" does not exist. Aborting build...\n", TrimmedCopy);
                }
                else
                {
                    #if PLATFORM_WINDOWS
                    LOG_ERROR("Build assertion failure. Library \"%S.lib\" does not exist. Aborting build...\n", TrimmedCopy);
                    #elif PLATFORM_APPLE
                    LOG_ERROR("Build assertion failure. Library \"%S(.dylib/.a)\" does not exist. Aborting build...\n", TrimmedCopy);
                    #else
                    LOG_ERROR("Build assertion failure. Library \"%S(.so/.a)\" does not exist. Aborting build...\n", TrimmedCopy);
                    #endif
                }
                #else
                LOG_ERROR("cant find this library nigga \"%S\". i searched fkn everywhere bro", TrimmedCopy);
                #endif

                StringLocal(PathValue, Kibibytes(32));
                #if PLATFORM_WINDOWS
                Platform_GetEnvironmentVariableValue(StrLit("Path"), &PathValue);
                #else
                Platform_GetEnvironmentVariableValue(StrLit("PATH"), &PathValue);
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
                StringArray Values = String_ParseIntoArray(Scratch_LibTests.Allocator, PathValue, ';', 0, 128);
                #else
                StringArray Values = String_ParseIntoArray(Scratch_LibTests.Allocator, PathValue, ':', 0, 128);
                #endif
                for each_str (v, Values)
                {
                    LOG("    %S", *v);
                }

                return 1;
            }
        }
    }

    String SourceDirectory       = GetExpandedVariableValue(ExpandedVariablesDB, StrLit("SourceDirectory"));
    String BuildDirectory        = GetExpandedVariableValue(ExpandedVariablesDB, StrLit("BuildDirectory"));
    String IntermediateDirectory = GetExpandedVariableValue(ExpandedVariablesDB, StrLit("IntermediateDirectory"));

    String_ConvertSlashToPlatformSlash(&SourceDirectory);
    String_ConvertSlashToPlatformSlash(&IntermediateDirectory);
    String_ConvertSlashToPlatformSlash(&BuildDirectory);

    StringLocal(RelativeIntermediateDirectory, MAX_PATH_LENGTH);
    String_BuildPath(&RelativeIntermediateDirectory, IntermediateDirectory);

    StringLocal(IntermediateBaseDirectory, MAX_PATH_LENGTH);
    String_BuildPath(&IntermediateBaseDirectory, WorkingPath, RelativeIntermediateDirectory);
    String_EatPathSeparatorsInlineFromEnd(&IntermediateBaseDirectory);
    String_AppendPathSeparator(&IntermediateBaseDirectory);
    String_ConvertSlashToPlatformSlash(&IntermediateBaseDirectory);
    //GIntermediateBaseDirectory = IntermediateBaseDirectory;

    StringLocal(SourceDir, MAX_PATH_LENGTH);
    String_BuildPath(&SourceDir, WorkingPath, SourceDirectory);

    // assert that the given directories exist before proceeding with the build
    // ignoring build/intermediate since they will be created if they don't exist
    TEMP_SCRATCH(DirTests)
    {
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
        StringList DirList = Internal_ParseStringIntoList(Scratch_DirTests.Allocator, LibraryDirectories);
        for each_str_list (DirList)
        {
            StringLocal(DirPath, MAX_PATH_LENGTH);

            StringLocal(DirCopy, MAX_PATH_LENGTH);
            Filesystem_SanitizeQuotes(&DirCopy, It.String);

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
        TEMP_SCRATCH(Search)
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
    }
    */

    StringList WhitelistArray    = Internal_ParseStringIntoList(Arena, IncludedSourceFiles);
    StringList BlacklistArray    = Internal_ParseStringIntoList(Arena, ExcludedSourceFiles);
    StringList WhitelistDirArray = Internal_ParseStringIntoList(Arena, IncludedSourceDir);
    StringList BlacklistDirArray = Internal_ParseStringIntoList(Arena, ExcludedSourceDir);

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
    };

    StringLocal(FirstSourceFileName, 256);
    struct SourceCountData CountData = { 0, 0, 0, &FirstSourceFileName, WorkingPath, SourceDirectory, WhitelistArray, BlacklistArray, WhitelistDirArray, BlacklistDirArray, false };

    Filesystem_IterateDirectory_Ex(SourceDir, SourceFileCounterDirectoryIterator, true, &CountData);

    //u32 NumSourceFiles = (u32)Array_Num(GSourceFiles);
    //TArray(SourceFileData*) SourceFilesFiltered = Array_Reserve(SourceFileData*, NumSourceFiles > 0 ? NumSourceFiles : 2);

    if (CountData.NumSources == 0)
    //if (Array_Num(GSourceFiles) == 0)
    {
        #ifndef HOOD
        LOG("Nothing to compile");
        #else
        LOG("no work to do homie");
        #endif

        #if !PLATFORM_WINDOWS
        LOG_LINE_BREAK();
        #endif

        goto End;
    }

    // use the first source file as the assembly name (if none provided or if "untitled" was set)
    //if (Array_Num(GSourceFiles) == 1)
    if (CountData.NumSources == 1)
    {
        if (!String_IsValid(AssemblyName) ||
            String_IsEqual(AssemblyName, StrLit("Untitled"), false))
        {
            String TrimmedFileName = FirstSourceFileName;

            //SourceFileData SFile = GSourceFiles[0];

            // extract the name only, remove the path prefixes
            /*
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
            */

            // todo: left chop function
            u32 DotIndex = 0;
            if (String_IndexOfLastChar(TrimmedFileName, '.', &DotIndex))
            {
                TrimmedFileName.Length -= TrimmedFileName.Length-DotIndex;
            }

            AssemblyName = TrimmedFileName;
        }
    }

    // automatically switch to a c++ compiler if we have c++ source code files
    if (bNoCompilerProgramExplicityGiven)
    {
        const String CppCompilers[] =
        {
            StrLit("clang++"),
            StrLit("g++"),
            StrLit("cl")
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
                    TEMP_SCRATCH(Search)
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
        String_Append(&AssemblyNameCopy, StrLit("lib"));
        String_Append(&AssemblyNameCopy, AssemblyName);
        AssemblyName = AssemblyNameCopy;
    }
    #endif

    StringLocal(AssemblyNameWithExt, 128);
    String_Copy(&AssemblyNameWithExt, AssemblyName);
    if (Extension.Length > 0)
    {
        //String_AppendChar(&AssemblyNameWithExt, '.');
        String_Append(&AssemblyNameWithExt, Extension);
    }

    #if !PLATFORM_WINDOWS
    if (bIsAssemblyExe)
    {
        String_ToLower(&AssemblyNameWithExt);
    }
    #endif

    // force a rebuild if the .build file has been modified
    if (!bIsRebuild && !bIsClean && bFoundBuildFile)
    {
        // build the full source directory path
        StringLocal(AssemblyPath, MAX_PATH_LENGTH);
        String_BuildPath(&AssemblyPath, WorkingPath, BuildDirectory, AssemblyNameWithExt);

        if (!Filesystem_DoesFileExist(AssemblyPath))
        {
            LOG("Assembly file \"%S\" does not exist. Forcing rebuild...\n", AssemblyPath);
            bIsRebuild = true;
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
            for each (Include, IncludeFiles)
            {
                u64 IncludeFileTime = Filesystem_GetLastWriteTimeH(&Include);

                if (IncludeFileTime >= AssemblyFileTime)
                {
                    bIsRebuild = true;

                    StringLocal(Path, MAX_PATH_LENGTH);
                    Filesystem_GetFilePath(&Include, &Path);

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

    for each (File, IncludeFiles)
        Filesystem_Close(&File);

    // force a rebuild if either the build directory or the intermediate directory is missing
    if (!bIsRebuild && !bIsClean)
    {
        StringLocal(FullBuildDirectory, MAX_PATH_LENGTH);
        String_BuildPath(&FullBuildDirectory, WorkingPath, BuildDirectory);

        if (!Filesystem_DoesDirectoryExist(FullBuildDirectory) ||
            !Filesystem_DoesDirectoryExist(IntermediateBaseDirectory))
        {
            bIsRebuild = true;
        }
    }

    // force a rebuild if the cmd line given to this program was different than the previous run
    /// TODO: fix this, idk what to do
    if (!bIsRebuild && !bIsClean && !String_IsValid(CameFromBuildFile))
    {
        StringLocal(OutputDebugFile, MAX_PATH_LENGTH);
        StringLocal(GenFileName, 256);
        String_Append(&GenFileName, BuildFileName);
        String_Append(&GenFileName, StrLit("_generated.txt"));
        String_ToLower(&GenFileName);
        String_BuildPath(&OutputDebugFile, IntermediateBaseDirectory, GenFileName);

        bool bFileExists = Filesystem_DoesFileExist(OutputDebugFile);
        if ((!bFileExists && RiftCmdLine.Length > 0) ||
            bFileExists)
        {
            FileHandle h = {0};
            Filesystem_Open(OutputDebugFile, FileMode_Read, &h);
            StringLocal(SavedCmdLine, 2048);
            Filesystem_ReadLine(&h, &SavedCmdLine);
            if (SavedCmdLine.Length > 0 && RiftCmdLine.Length > 0)
            {
                if (!String_IsEqual(SavedCmdLine, RiftCmdLine, false))
                {
                    LOG("Different command line given. Forcing rebuild...");
                    LOG("    Previous: %S", SavedCmdLine);
                    LOG("    Current:  %S", RiftCmdLine);
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

        StringLocal(BuildDirectoryPath, MAX_PATH_LENGTH);
        String_BuildPath(&BuildDirectoryPath, WorkingPath, BuildDirectory);
        String_EatPathSeparatorsInlineFromEnd(&BuildDirectoryPath);
        String_AppendPathSeparator(&BuildDirectoryPath);

        #if PLATFORM_WINDOWS
        String Wildcard = StrLit(".*");
        const String WildcardS = StrLit("S.*");
        #else
        String Wildcard = StrLit("");
        const String WildcardS = StrLit("S");
        #endif

        // Delete all [Assembly]*.* files
        if (Filesystem_DoesDirectoryExist(BuildDirectoryPath))
        {
            #ifndef HOOD
            LOG("Cleaning %S%S%S", BuildDirectoryPath, AssemblyName, Wildcard);
            #else
            LOG("cleaning dis shit %S%S%S", BuildDirectoryPath, AssemblyName, Wildcard);
            #endif

            StringLocal(AssemblyWildcard, MAX_PATH_LENGTH);
            String_Append(&AssemblyWildcard, AssemblyName);
            String_Append(&AssemblyWildcard, Wildcard);
            Filesystem_DeleteFiles(BuildDirectoryPath, AssemblyWildcard, true);
            String_Empty(&AssemblyWildcard);
            String_Append(&AssemblyWildcard, AssemblyName);
            String_Append(&AssemblyWildcard, WildcardS);
            Filesystem_DeleteFiles(BuildDirectoryPath, AssemblyWildcard, true);

            bCleanedSomething = true;
        }

        StringLocal(IntermediateDirectoryPath, MAX_PATH_LENGTH);
        String_BuildPath(&IntermediateDirectoryPath, IntermediateBaseDirectory/*, SourceDirectory*/);

        // Delete intermediate directory based on given source directory
        if (Filesystem_DoesDirectoryExist(IntermediateDirectoryPath))
        {
            Wildcard = StrLit("*");

            #ifndef HOOD
            LOG("Cleaning %S%S", IntermediateDirectoryPath, Wildcard);
            #else
            LOG("cleaning dis shit %S%S", IntermediateDirectoryPath, Wildcard);
            #endif

            Filesystem_DeleteFiles(IntermediateDirectoryPath, Wildcard, true);

            bCleanedSomething = true;
        }

        if (bCleanedSomething)
            LOG_LINE_BREAK();

        if (!bIsRebuild)
        {
            Filesystem_Close(BuildFileHandle);

            return 0;
        }
    }

    if (bFoundBuildFile)
    {
        StringLocal(OutputDebugFile, MAX_PATH_LENGTH);
        StringLocal(GenFileName, 256);
        String_Append(&GenFileName, BuildFileName);
        String_Append(&GenFileName, StrLit("_generated.txt"));
        String_ToLower(&GenFileName);
        String_BuildPath(&OutputDebugFile, IntermediateBaseDirectory, GenFileName);

        FileHandle f = {0};
        bool bSuccess = Filesystem_Open(OutputDebugFile, FileMode_Write, &f);

        if (bSuccess)
        {
            // write the cmd line of this program to a file in the intermediate directory for comparison between subsequent runs
            Filesystem_Write(&f, RiftCmdLine.Length, RiftCmdLine.Data, NULL);
            Filesystem_WriteLine(&f, StrLit("\n"), NULL);

            for each (v, ExpandedVariablesDB)
            {
                StringLocal(Line, 4096);
                String_Append(&Line, v.Name);
                String_AppendSpace(&Line);
                String_Append(&Line, v.Value);
                String_AppendChar(&Line, '\n');
                Filesystem_WriteLine(&f, Line, NULL);
            }
        }

        Filesystem_Close(&f);
    }

    if (Array_Num(Messages) > 0)
    {
        for each (m, Messages)
        {
            LOG("%S", m);
        }

        LOG_LINE_BREAK();
    }
    
    if (bFoundBuildFile)
    {
        String Mode = GetCmdOptionValue(CmdOptionsDB, StrLit("mode"));

        if (!String_IsValid(Mode))
            LOG("Build Configuration: (default)");
        else
            LOG("Build Configuration: (%S)", Mode);

        u32 WhitespaceIndex = 0;
        bool bHasSpace = String_IndexOfFirstWhitespace(Extension_Og, &WhitespaceIndex);

        if (bIsAssemblyExe || !bHasSpace)
        {
            LOG("    Assembly:            %S", AssemblyNameWithExt);
        }
        else
        {
            String NextExt = StrShiftF(Extension_Og, WhitespaceIndex+1);
            LOG("    Assembly:            %S and %S%S", AssemblyNameWithExt, AssemblyName, NextExt);
        }

        if (Type.Length > 0) LOG("    Type:                %S", Type);
        LOG("    Version:             %S", Version);
        LOG("    Compiler:            %S -> \"%S\"", CompilerProgram, CompilerPath);

        bool bLogged = false;
        if (CompilerFlags.Length > 0)      { LogBuildVariable(VariablesDB, StrLit("CompilerFlags"),      StrLit("    Compiler Flags:      ")); bLogged = true; }
        if (IncludeFlags.Length > 0)       { LogBuildVariable(VariablesDB, StrLit("Includes"),           StrLit("    Includes:            ")); bLogged = true; }
        if (LinkerFlags.Length > 0)        { LogBuildVariable(VariablesDB, StrLit("LinkerFlags"),        StrLit("    Linker Flags:        ")); bLogged = true; }
        if (Libraries.Length > 0)          { LogBuildVariable(VariablesDB, StrLit("Libraries"),          StrLit("    Libraries:           ")); bLogged = true; }
        if (LibraryDirectories.Length > 0) { LogBuildVariable(VariablesDB, StrLit("LibraryDirectories"), StrLit("    Library Directories: ")); bLogged = true; }
        if (Defines.Length > 0)            { LogBuildVariable(VariablesDB, StrLit("Defines"),            StrLit("    Defines:             ")); bLogged = true; }
        if (UnDefines.Length > 0)          { LogBuildVariable(VariablesDB, StrLit("UnDefines"),          StrLit("    UnDefines:           ")); bLogged = true; }
        if (LinkerDefines.Length > 0)      { LogBuildVariable(VariablesDB, StrLit("LinkerDefines"),      StrLit("    Linker Defines:      ")); bLogged = true; }

        if (bLogged)
        {
            LOG_LINE_BREAK();
        }
    }

    const String ExpandedCompilerFlags = GetExpandedVariableValue(ExpandedVariablesDB, StrLit("CompilerFlags"));
    const String ExpandedLinkerFlags   = GetExpandedVariableValue(ExpandedVariablesDB, StrLit("LinkerFlags"));

    StringLocal(ExpandedIncludeFlags, 4096);
    StringLocal(ExpandedLibraries, 1024);
    StringLocal(ExpandedLibraryDirectories, 1024);
    StringLocal(ExpandedDefineFlags, 1024);
    StringLocal(ExpandedUnDefineFlags, 1024);
    StringLocal(ExpandedLinkerDefineFlags, 1024);

    StringLocal(FlagPrefix, 4);
    String_Append(&FlagPrefix, CompilerFlagPrefixSymbol);
    String_Append(&FlagPrefix, StrLit("I"));

    PrefixVariables(&ExpandedIncludeFlags, IncludeFlags, FlagPrefix);

    FlagPrefix.Data[1] = 'l';
    if (String_IsEqual(CompilerProgram, StrLit("cl"), false) ||
        String_IsEqual(CompilerProgram, StrLit("msvc"), false)) // todo: something better
    {
        SuffixVariables(&ExpandedLibraries, Libraries, StrLit(".lib"));
    }
    else
    {
        PrefixVariables(&ExpandedLibraries, Libraries, FlagPrefix);
    }

    FlagPrefix.Data[1] = 'L';
    if (String_IsEqual(CompilerProgram, StrLit("cl"), false) ||
        String_IsEqual(CompilerProgram, StrLit("msvc"), false)) // todo: something better
    {
        PrefixVariables(&ExpandedLibraryDirectories, LibraryDirectories, StrLit("/LIBPATH:"));
    }
    else
    {
        PrefixVariables(&ExpandedLibraryDirectories, LibraryDirectories, FlagPrefix);
    }

    FlagPrefix.Data[1] = 'D';
    PrefixVariables(&ExpandedDefineFlags, Defines, FlagPrefix);
    PrefixVariables(&ExpandedLinkerDefineFlags, LinkerDefines, FlagPrefix);

    FlagPrefix.Data[1] = 'U';
    PrefixVariables(&ExpandedUnDefineFlags, UnDefines, FlagPrefix);

    LogString_WordWrapped(S("Expanded Compiler Flags: "), ExpandedCompilerFlags, true);
    LogString_WordWrapped(S("Expanded Include  Flags: "), ExpandedIncludeFlags, true);
    LogString_WordWrapped(S("Expanded Linker   Flags: "), ExpandedLinkerFlags, true);
    LogString_WordWrapped(S("Expanded Library  Flags: "), ExpandedLibraries, true);
    LogString_WordWrapped(S("Expanded Library  Paths: "), ExpandedLibraryDirectories, true);
    LogString_WordWrapped(S("Expanded Define   Flags: "), ExpandedDefineFlags, true);
    LogString_WordWrapped(S("Expanded UnDefine Flags: "), ExpandedUnDefineFlags, true);
    LogString_WordWrapped(S("Expanded Linker Defines: "), ExpandedLinkerDefineFlags, true);

    // TODO: figure this one out
    if (CountData.NumSources > 0)
    //if (Array_Num(SourceFilesFiltered) > 0)
    {
        u32 WhitespaceIndex = 0;
        bool bHasSpace = String_IndexOfFirstWhitespace(Extension_Og, &WhitespaceIndex);

        #ifndef HOOD
        if (bIsAssemblyExe || !bHasSpace)
        {
            LOG("Building %S\n", AssemblyNameWithExt);
        }
        else
        {
            String NextExt = StrShiftF(Extension_Og, WhitespaceIndex+1);
            LOG("Building %S and %S%S\n", AssemblyNameWithExt, AssemblyName, NextExt);
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
    // note: only works on windows atm
    StringLocal(IconResFilePath, MAX_PATH_LENGTH);
    StringLocal(VersionResFilePath, MAX_PATH_LENGTH);

    #if PLATFORM_WINDOWS
    String RCProgram = StrLit("llvm-rc");
    bool bHasRcProgram = Platform_FindProgram(RCProgram);

    if (Icon.Length > 0)
    {
        if (bHasRcProgram)
        {
            String IconName = Icon;

            u32 LastSlashIndex = 0;
            if (String_IndexOfLastPathSlash(Icon, &LastSlashIndex))
            {
                IconName = StrShiftF(Icon, LastSlashIndex+1);
                String_Copy(&IconFilePath, Icon);
            }
            else
            {
                struct Data
                {
                    TArray(FileVariable) ExpandedVarsArray;
                    String* IconFilePath;
                };

                struct Data d = {ExpandedVariablesDB, &IconFilePath};

                Filesystem_IterateDirectory_Ex(WorkingPath, IconFileDirectoryIterator, true, &d);
            }

            String_IndexOfLastPathSlash(IconFilePath, &LastSlashIndex);

            StringLocal(RcFilePath, MAX_PATH_LENGTH);
            String BasePath = StrSlice(IconFilePath.Data, LastSlashIndex);
            String RcFile = StrLit("icon.rc");
            String ResFile = StrLit("icon.res");
            String_BuildPath(&RcFilePath, WorkingPath, BasePath, RcFile);

            FileHandle f = {0};
            if (!Filesystem_Open(RcFilePath, FileMode_Write, &f))
            {
                return 1;
            }

            StringLocal(IconString, 256);
            String_Format(&IconString, StrLit("id ICON \"%S\""), 256, IconName);
            if (!Filesystem_Write(&f, IconString.Length, IconString.Data, NULL))
            {
                LOG_ERROR("Failed to write icon data to \"%S\"", RcFilePath);
                Filesystem_Close(&f);
                return 1;
            }

            Filesystem_Close(&f);

            StringLocal(ResPath, MAX_PATH_LENGTH);
            String_BuildPath(&ResPath, BasePath, ResFile);
            String_Append(&IconResFilePath, StrLit("\""));
            String_Append(&IconResFilePath, ResPath);
            String_Append(&IconResFilePath, StrLit("\""));

            StringLocal(CmdLine, 1024);
            String_Append(&CmdLine, RCProgram);
            String_Append(&CmdLine, StrLit(" \""));
            String_Append(&CmdLine, RcFilePath);
            String_AppendChar(&CmdLine, '"');

            LOG("Building icon \"%S\"", IconFilePath);
            LOG("    %S\n", CmdLine);

            PlatformHandle h = Platform_RunCommand(CmdLine, WorkingPath);
            u32 ExitCode = Platform_WaitForProcessAndGetExitCode(h);
            if (ExitCode != 0)
            {
                LOG_WARNING("Failed to build icon \"%S\" for %S%S. Skipping icon build...", IconFilePath, AssemblyName, Extension);
                String_Empty(&IconResFilePath);
            }
        }
        else
        {
            LOG_WARNING(
                "Unable to build icon. \"llvm-rc\" tool does not exist."
                " Download the LLVM toolchain and add a new environment path that points to \"llvm-rc\"."
                " Skipping icon build...");
        }
    }

    /*
    if (Resource.Length > 0)
    {
        String ResourceProgram = StrLit("llvm-rc");

        if (Platform_FindProgram(ResourceProgram, StrLit(".exe")))
        {
            u32 LastSlashIndex = 0;
            if (String_IndexOfLastPathSlash(Resource, &LastSlashIndex))
            {
                String_Copy(&GResourceFilePath, Resource);
            }
            else
            {
                Filesystem_IterateDirectory(WorkingPath, ResourceFileDirectoryIterator, true);
            }

            String_IndexOfLastPathSlash(GResourceFilePath, &LastSlashIndex);

            StringLocal(CmdLine, 1024);
            String_BuildSeparator(&CmdLine, ' ', ResourceProgram, GResourceFilePath);
            LOG("Building resource \"%S\"", GResourceFilePath);
            LOG("    %S\n", CmdLine);
            PlatformHandle h = Platform_RunCommand(CmdLine, WorkingPath);
            u32 ExitCode = Platform_WaitForProcessAndGetExitCode(h);
            if (ExitCode != 0)
            {
                LOG("Failed to build resource \"%S\" for %S.%S. Aborting build...", GResourceFilePath, AssemblyName, Extension);
                return 1;
            }
        }
        else
        {
            LOG_WARNING("Unable to build resource... \"llvm-rc\" tool does not exist. Download the LLVM toolchain and add a new environment path that points to \"llvm-rc\". Skipping resource build...");
        }
    }
    */

    // only build the version resource if we have TitleName, CompanyName, Description, Version, Copyright, or CompanyName
    // and no custom resource file was specified
    if (//Resource.Length == 0 && 
        (TitleName.Length > 0 || CompanyName.Length > 0 || Description.Length > 0 ||
        (!bFallbackVersion && Version.Length > 0) || CompanyName.Length > 0 || Copyright.Length > 0))
    {
        if (bHasRcProgram)
        {
            StringLocal(VersionRCPath, MAX_PATH_LENGTH);
            const String VersionRCName = StrLit("_version.rc");
            const String VersionResName = StrLit("_version.res");
            String_Append(&VersionRCPath, IntermediateBaseDirectory);
            String_Append(&VersionRCPath, BuildFileName);
            String_Append(&VersionRCPath, VersionRCName);

            FileHandle VersionRCFile = {0};
            Filesystem_Open(VersionRCPath, FileMode_Write, &VersionRCFile);

            StringLocal(AssemblyWithExt, 256);
            String_Append(&AssemblyWithExt, AssemblyName);
            //String_Append(&AssemblyWithExt, StrLit("."));
            String_Append(&AssemblyWithExt, Extension);

            String_Append(&VersionResFilePath, StrLit("\""));
            String_Append(&VersionResFilePath, IntermediateBaseDirectory);
            String_Append(&VersionResFilePath, BuildFileName);
            String_Append(&VersionResFilePath, VersionResName);
            String_Append(&VersionResFilePath, StrLit("\""));

            String FileType = StrLit("UNKNOWN");

            if (String_IsEqual(Extension, StrLit(".exe"), false))
                FileType = StrLit("APP");
            else if (String_IsEqual(Extension, StrLit(".dll"), false))
                FileType = StrLit("DLL");
            else if (String_IsEqual(Extension, StrLit(".lib"), false))
                FileType = StrLit("STATIC_LIB");

            // TODO: when building both a static/shared lib, we do not generate the correct FILETYPE. fix it boy

            StringLocal(FileData, 2048);
            String_Format(&FileData, StrLit("#include <winresrc.h>\n\n"

                                            "VS_VERSION_INFO  VERSIONINFO\n"
                                            "FILEVERSION      %S     // this can only have 4 parts\n"
                                            "PRODUCTVERSION   %S     // same here\n"
                                            "FILEFLAGS        VS_FF_PRERELEASE\n"
                                            "FILEOS           VOS__WINDOWS32\n"
                                            "FILETYPE         VFT_%S\n" // VFT_APP or VF_DLL or VFT_STATIC_LIB
                                            "FILESUBTYPE      VFT2_UNKNOWN\n\n"

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
                                            FileData.Capacity,
                                            VersionCommas, VersionCommas, FileType, CompanyName, Description, Version, Copyright, AssemblyWithExt, TitleName, Version);

            Filesystem_Write(&VersionRCFile, FileData.Length, FileData.Data, NULL);
            Filesystem_Close(&VersionRCFile);

            StringLocal(CmdLine, 1024);
            String_Append(&CmdLine, RCProgram);
            String_Append(&CmdLine, StrLit(" \""));
            String_Append(&CmdLine, VersionRCPath);
            String_AppendChar(&CmdLine, '"');

            LOG("Compiling resource file \"%S\"", VersionRCPath);
            if (bVerboseLog)
                LOG("    %S\n", CmdLine);
            PlatformHandle h = Platform_RunCommand(CmdLine, WorkingPath);
            u32 ExitCode = Platform_WaitForProcessAndGetExitCode(h);
            if (ExitCode != 0)
            {
                LOG_WARNING("Failed to build resource file \"%S\" for %S%S. Skipping...", VersionRCPath, AssemblyName, Extension);
                String_Empty(&VersionResFilePath);
            }
        }
        else
        {
            LOG_WARNING(
                "Unable to build version resource. \"llvm-rc\" tool does not exist."
                " Download the LLVM toolchain and add a new environment path that points to \"llvm-rc\"."
                " Skipping resource build...");
        }
    }
    #endif


    u32 MaxLogicalCores = Platform_GetNumLogicalProcessors();
    u8 MaxCompilersAtOnce = (u8)MaxLogicalCores; // bound by max logical processors on the user's machine
    //LOG_INFO("Max logical cores: %u", MaxLogicalCores);
    if (String_IsValid(MaxConcurrentCompilations))
    {
        u8 Num = 0;
        String_ToU8(MaxConcurrentCompilations, &Num);
        MaxCompilersAtOnce = Min(Num, (u8)MaxLogicalCores);
    }


    if (!String_IsEqual(BuildDirectory, StrLit("."), false))
    {
        StringLocal(FullBuildDirectory, MAX_PATH_LENGTH);
        String_BuildPath(&FullBuildDirectory, WorkingPath, BuildDirectory);
        if (!Filesystem_DoesDirectoryExist(FullBuildDirectory))
        {
            Filesystem_OpenDirectory(FullBuildDirectory);
        }
    }

    StringLocal(IntSrcDir, MAX_PATH_LENGTH);
    String_BuildPath(&IntSrcDir, IntermediateBaseDirectory, SourceDirectory);
    if (!Filesystem_DoesDirectoryExist(IntermediateBaseDirectory))
    {
        Filesystem_OpenDirectory(IntermediateBaseDirectory);
    }

    BuildParams p = {0};
    p.Arena                            = Arena;
    p.CompilerProgram                  = bExplicitProgramPath ? CompilerPath : CompilerProgram;
    p.CompilerPath                     = CompilerPath;
    p.Assembly                         = AssemblyName;
    p.AssemblyWithExt                  = AssemblyNameWithExt;
    p.Extension                        = Extension;
    p.Extension_Og                     = Extension_Og;
    p.WhitelistFiles                   = WhitelistArray;
    p.WhitelistDirectories             = WhitelistDirArray;
    p.BlacklistFiles                   = BlacklistArray;
    p.BlacklistDirectories             = BlacklistDirArray;
    //p.SourceFiles                      = SourceFilesFiltered;
    //p.SourceFiles_Unfiltered           = GSourceFiles;
    p.Processes                        = &Processes;
    p.RootDirectory                    = WorkingPath;
    p.SourceDirectory                  = SourceDirectory;
    p.BuildDirectory                   = BuildDirectory;
    p.IntermediateDirectory            = RelativeIntermediateDirectory;
    p.IntermediateBaseDirectory        = IntermediateBaseDirectory;
    p.MaxCompilersAtOnce               = MaxCompilersAtOnce;
    p.MaxErrors                        = MaxErrorsAllowed;
    p.bShouldWaitPerCompileProcess     = bShouldWaitPerCompileProcess;
    p.CompilerFlags                    = ExpandedCompilerFlags;
    p.LinkerFlags                      = ExpandedLinkerFlags;
    p.IncludeFlags                     = ExpandedIncludeFlags;
    p.DefineFlags                      = ExpandedDefineFlags;
    p.LinkerDefineFlags                = ExpandedLinkerDefineFlags;
    p.Libraries                        = ExpandedLibraries;
    p.LibraryDirectories               = ExpandedLibraryDirectories;
    p.IconResFilePath                  = IconResFilePath;
    p.VersionResFilePath               = VersionResFilePath;
    p.bIsAssemblyExe                   = bIsAssemblyExe;
    p.bVerbose                         = bVerboseLog;
    p.NumSources                       = CountData.NumSources;
    p.NumHeaders                       = CountData.NumHeaders;
    p.NumRcSources                     = CountData.NumRcSources;

    bool bSuccess = false;
    u32 NumCompiled = 0;

    Clock CompileClock;

    // switch between different compiler backends
    if (String_IsEqual(CompilerProgram, StrLit("cl"), false) ||
        String_IsEqual(CompilerProgram, StrLit("msvc"), false))
    {
        Clock_Start(&CompileClock);
        bSuccess = MSVC_CompileV2(&p, &NumCompiled);
    }
    else
    {
        Clock_Start(&CompileClock);
        bSuccess = C_CompileV2(&p, &NumCompiled);
    }

    Clock_Tick(&CompileClock);

    if (!bSuccess)
    {
        #ifdef DEVELOPER
        Platform_Sleep(5000);
        #endif

        return 1;
    }

    if (NumCompiled == 0)
    {
        if (bRunPostBuildOnlyWhenWorkWasDone)
        {
            goto End;
        }

        goto PostBuild;
    }

    Clock LinkClock;

    if (String_IsEqual(CompilerProgram, StrLit("cl"), false) ||
        String_IsEqual(CompilerProgram, StrLit("msvc"), false))
    {
        Clock_Start(&LinkClock);
        bSuccess = MSVC_LinkV2(&p);
    }
    else // if unrecognized, treat as clang/gcc style compiler
    {
        Clock_Start(&LinkClock);
        bSuccess = C_LinkV2(&p);
    }

    Clock_Tick(&LinkClock);

    if (!bSuccess)
    {
        #ifdef DEVELOPER
        Platform_Sleep(5000);
        #endif

        return 1;
    }

    Clock_Tick(&BuildRuntime);

    StringLocal(TimeString, 32);

    Clock_GetElapsedTime_ToString(&CompileClock, true, &TimeString);
    LOG("\nCompile     time: %S", TimeString);

    Clock_GetElapsedTime_ToString(&LinkClock, true, &TimeString);
    LOG("Link        time: %S", TimeString);

    if (bFoundBuildFile)
    {
        Clock_GetElapsedTime_ToString(&BuildFileParseClock, true, &TimeString);
        LOG("Build parse time: %S", TimeString);
    }

    Clock_GetElapsedTime_ToString(&BuildRuntime, true, &TimeString);
    LOG("Total build time: %S", TimeString);

    StringLocal(BuildPath, MAX_PATH_LENGTH);
    String_BuildPath(&BuildPath, WorkingPath, BuildDirectory);
    String_AppendPathSeparator_Checked(&BuildPath);

    StringLocal(OutputPath, MAX_PATH_LENGTH);
    String_AppendChar(&OutputPath, '"');
    String_Append(&OutputPath, BuildPath);
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
        String_Append    (&OutputPath2, BuildPath);
        String_Append    (&OutputPath2, AssemblyName);
        String_Append    (&OutputPath2, NextExt);
        String_AppendChar(&OutputPath2, '"');

        #ifndef HOOD
        LOG_SUCCESS("Build complete: %S\n                           %S", OutputPath, OutputPath2);
        #else
        LOG_SUCCESS("lessss goooo: %S\n                         %S", OutputPath, OutputPath2);
        #endif
    }

    #if !PLATFORM_WINDOWS
    LOG_LINE_BREAK();
    #endif

    // run post build commands (if specified)
PostBuild:
    for each (Var, ExpandedVariablesDB)
    {
        if (String_StartsWith(Var.Name, StrLit("PostBuild"), false))
        {
            NumPostBuildCmds++;
        }
    }

    if (NumPostBuildCmds > 0)
    {
        #if PLATFORM_WINDOWS
        LOG_LINE_BREAK();
        #endif

        #ifndef HOOD
        LOG("Running post build commands...");
        #else
        LOG("cool mang, gonna run some post build cmds...");
        #endif

        for each (Var, ExpandedVariablesDB)
        {
            if (String_StartsWith(Var.Name, StrLit("PostBuild"), false))
            {
                u32 ExitCode = 0;
                bool bResult = Internal_ExecuteBuildCmd(WorkingPath, Var.Name, Var.Value, &ExitCode);
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
    // TODO: run assmebly on when work was done
    if (bIsAssemblyExe && DoesBuildVarExist(VariablesDB, StrLit("RunAssembly")))
    {
        for each (v, ExpandedVariablesDB)
        {
            if (!String_IsEqual(v.Name, StrLit("RunAssembly"), false))
                continue;

            u32 PipeIndex = 0;
            bool bFound = String_IndexOfChar(v.Value, '|', &PipeIndex);

            const String Args = bFound ? StrSlice(v.Value.Data, PipeIndex) : v.Value;
            const String CustomPath = bFound ? String_EatSpaces(StrShiftF(v.Value, PipeIndex+1)) : String_Null();

            StringLocal(CmdLine, 8192);

            #if PLATFORM_WINDOWS
            String_Append(&CmdLine, StrLit("cmd.exe /c \""));
            #endif

            StringLocal(BuildDir, MAX_PATH_LENGTH);
            String_BuildPath(&BuildDir, WorkingPath, BuildDirectory);
            String_AppendPathSeparator_Checked(&BuildDir);

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
                String_Copy(&ExecutableWorkingPath, BuildDir);
            }

            Filesystem_ConvertRelativeToAbsolutePath(&ExecutableWorkingPath);

            String_Append(&CmdLine, StrLit("cd \""));
            String_Append(&CmdLine, ExecutableWorkingPath);
            String_Append(&CmdLine, StrLit("\" && "));

            StringLocal(ExePath, MAX_PATH_LENGTH);
            String_Append(&ExePath, BuildDir);
            String_Append(&ExePath, AssemblyNameWithExt);

            String_AppendChar(&CmdLine, '"');
            String_Append(&CmdLine, ExePath);
            String_AppendChar(&CmdLine, '"');

            String_AppendSpace(&CmdLine);
            String_Append(&CmdLine, Args);

            String_EatSpacesInlineFromEnd(&CmdLine);

            #if PLATFORM_WINDOWS
            String_AppendChar(&CmdLine, '"');

            LOG_LINE_BREAK();
            #endif

            if (Filesystem_DoesFileExist(ExePath))
            {
                LOG("Launching %S ...", AssemblyNameWithExt);
                LOG(" -> Working Directory: %S", ExecutableWorkingPath);

                if (Args.Length > 0)
                {
                    LOG(" -> Parameters: %S", Args);
                }

                LOG_LINE_BREAK();

                //LOG("CMD: %S", CmdLine);

                Platform_WaitForHandle(Platform_RunCommand(CmdLine, ExecutableWorkingPath), -1);
            }
        }
    }

    if (String_IsValid(CameFromBuildFile) && NumCompiled > 0)
    {
        // special exit code to let the parent build know this child build finished successfully (and that it did some work)
        return 2;
    }

    return 0;
}

u32 RunApplication(const StringArray Arguments)
{
    Logging_ToggleLogTimeStamp(false);
    Logging_ToggleLogCategory(false);

    #ifndef HOOD
        #ifdef DEVELOPER
        LOG("\nRift Build System Alpha v%S (%S) [DEBUG]\n", StrLit(RIFTBUILD_VERSION_STRING), StrLit(PLATFORM_STRING));
        #else
        LOG("\nRift Build System Alpha v%S (%S)\n", StrLit(RIFTBUILD_VERSION_STRING), StrLit(PLATFORM_STRING));
        #endif
    #else
    LOG("\nRift Build System Alpha v%S (%S) - (HOOD EDITION)\n", StrLit(RIFTBUILD_VERSION_STRING), StrLit(PLATFORM_STRING));
    LOG("\nwasssup yo. les get build'n...");
    #endif

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

        for (u8 i = 0; i < Arguments.Num; i++)
        {
            if (i == BuildFileIndex)
                continue;

            if (String_StartsWith(Arguments.List[i], StrLit("override:"), false) ||
                String_StartsWith(Arguments.List[i], StrLit("list:"), false))
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
                WorkingDirectory = Arguments.List[RootPathIndex];
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

                if (!String_EndsWith(BuildFilePath, StrLit(".build"), false))
                    String_Append(&BuildFilePath, StrLit(".build"));

                if (!String_EndsWith(BuildFileName, StrLit(".build"), false))
                    String_Append(&BuildFileName, StrLit(".build"));

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
            LOG_ERROR("%S is too shallow of a directory. Create a new directory from here and then run riftbuild again from the new directory", RootCopy);

            return 1;
        }
    }

    LinearAllocator ProgramArena = {0};
    LinearAllocator_Create(Kilobytes(400), NULL, &ProgramArena);

    const u64 MemAmount_InternalOptions = _ArrayCalculateMemRequirement(32, sizeof(InternalVariable)); // 1024 bytes
    InternalVariablesDB  = Array_CreateStatic(InternalVariable, 32, LinearAllocator_Allocate(&ProgramArena, MemAmount_InternalOptions));

    // store internal options. like platform, native os .lib's, etc..
    #if PLATFORM_WINDOWS
    AddInternalVariable(StrLit("_Platform"), StrLit("Windows"));
    #elif PLATFORM_MAC
    AddInternalVariable(StrLit("_Platform"), StrLit("Apple MacOS Unix"));
    #elif PLATFORM_LINUX
    AddInternalVariable(StrLit("_Platform"), StrLit("Linux Unix"));
    #else
    AddInternalVariable(StrLit("_Platform"), StrLit("Unix"));
    #endif

    String Win32Libs = StrLit("kernel32 user32 opengl32 shell32 gdi32 comdlg32 comctl32 ws2_32 winmm netapi32 ole32 advapi32 "
                                "wldap32 crypt32 rpcrt4 shlwapi dbghelp bcrypt version imm32 cfgmgr32 setupapi oleaut32 "
                                "uuid odbc32 odbccp32 delayimp pathcch");

    String LinuxLibs = StrLit("m");

    AddInternalVariable(StrLit("_Win32Libs"), Win32Libs);
    AddInternalVariable(StrLit("_LinuxLibs"), LinuxLibs);

    #if PLATFORM_WINDOWS
    AddInternalVariable(StrLit("_NativeLibs"), Win32Libs);
    #elif PLATFORM_LINUX || PLATFORM_UNIX
    AddInternalVariable(StrLit("_NativeLibs"), LinuxLibs);
    #endif

    AddInternalVariable(StrLit("_Arch"), StrLit(CPU_ARCHITECTURE_STRING));
    
    #if PLATFORM_64_BIT
    AddInternalVariable(StrLit("_Bit"), S("64"));
    #else
    AddInternalVariable(StrLit("_Bit"), S("32"));
    #endif

    bool CpuArchs[32] = {0};

    #if __SSE4_2__
    CpuArchs[0] = CpuArchs[1] = CpuArchs[2] = CpuArchs[3] = CpuArchs[4] = CpuArchs[5] = 1;
    #elif __SSE4_1__
    CpuArchs[0] = CpuArchs[1] = CpuArchs[2] = CpuArchs[3] = CpuArchs[4] = 1;
    #elif __SSE3__
    CpuArchs[0] = CpuArchs[1] = CpuArchs[2] = 1;
    #elif __SSE2__
    CpuArchs[0] = CpuArchs[1] = 1;
    #elif __SSE__
    CpuArchs[0] = 1;
    #endif

    #if __AVX512__ || __AVX512CD__ || __AVX512ER__ || __AVX512F__ || __AVX512PF__
    CpuArchs[6] = CpuArchs[7] = CpuArchs[8] = 1;
    #elif __AVX2__
    CpuArchs[6] = CpuArchs[7] = 1;
    #elif __AVX__
    CpuArchs[6] = 1;
    #endif

    // todo: we need to query the system and automatically add these 
    if (CpuArchs[0]) AddInternalVariable(S("_SSE"),    S("1"));
    if (CpuArchs[1]) AddInternalVariable(S("_SSE2"),   S("1"));
    if (CpuArchs[2]) AddInternalVariable(S("_SSE3"),   S("1"));
    if (CpuArchs[3]) AddInternalVariable(S("_SSE4"),   S("1"));
    if (CpuArchs[4]) AddInternalVariable(S("_SSE4.1"), S("1"));
    if (CpuArchs[5]) AddInternalVariable(S("_SSE4.2"), S("1"));
    if (CpuArchs[6]) AddInternalVariable(S("_AVX"),    S("1"));
    if (CpuArchs[7]) AddInternalVariable(S("_AVX2"),   S("1"));
    if (CpuArchs[8]) AddInternalVariable(S("_AVX512"), S("1"));

    String AccountName = String_Reserve(&ProgramArena, 256);
    Platform_GetAccountName(&AccountName);
    AddInternalVariable(StrLit("_Account"), AccountName);

    String UserName = String_Reserve(&ProgramArena, 256);
    Platform_GetUserName(&UserName);
    AddInternalVariable(StrLit("_User"), UserName);

    String UserDirectory = String_Reserve(&ProgramArena, MAX_PATH_LENGTH);
    Platform_GetUserDirectory(&UserDirectory);
    String_EatPathSeparatorsInlineFromEnd(&UserDirectory);
    AddInternalVariable(StrLit("_UserDirectory"), UserDirectory);

    String_EatPathSeparatorsInlineFromEnd(&WorkingDirectory);

    BuildFileDirectoryIteratorData Data = {0};
    Data.bNoBuildFileSpecifiedInCmd = bNoBuildFileSpecifiedInCmd;
    Data.BuildFileIndex = BuildFileIndex;
    Data.RootPathIndex = RootPathIndex;
    Data.Name = &BuildFileName;
    Data.Path = &BuildFilePath;
    Data.Arguments = Arguments;

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
    }

    const bool bSingleThreadMode = StringArray_Contains(Arguments, S("-singlethread"), false);

    u32 ExitCode = BuildTarget(&ProgramArena, &BuildFileHandle, WorkingDirectory, Arguments, StrLit(""), BuildFileIndex, RootPathIndex, bSingleThreadMode);

    return ExitCode;
}
