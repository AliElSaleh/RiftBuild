#include "EntryPoint.h"

#include "Platform/Filesystem.h"
#include "String/StringUtils.h"
#include "Structures/Array.h"

#include "Backend.h"

u64 GEngineMemoryAmount = Mebibytes(1); // we'll see if we can get away with this for now, might need to increase to 2mb or 4mb on release
u64 GEngineScratchAmount = Kibibytes(32);

static TArray(FileVariable) VariablesDB = NULL;
static TArray(FileVariable) ExpandedVariablesDB = NULL;
static TArray(CmdOption) CmdOptionsDB = NULL;
static TArray(SourceFileData) GSourceFiles = NULL;
static TArray(SourceFileData) GHeaderFiles = NULL;
static TArray(String) GBuildFiles = NULL;

static TArray(FileHandle) IncludeFiles = NULL;
static TArray(String) Messages = NULL;

static LinearAllocator GVariablesAllocator = {0};
static LinearAllocator GIncludePathAllocator = {0};
static LinearAllocator GMessagesAllocator = {0};
static LinearAllocator GExpandedVariablesAllocator = {0};
static LinearAllocator GSourceFilePathAllocator = {0};
static LinearAllocator GBuildFilePathAllocator = {0};

static String GBuildFileName = {0};
static String GCameFromBuildFile = {0};
static String GRootPath = {0};
static String GBuildFilePath = {0};
static String GIconFilePath = {0};
static String GResourceFilePath = {0};
static String GIntermediateBaseDirectory = {0};
static u8 NumBuildFilesFound = 0;
static bool bFoundBuildFile = false;
static bool bNoBuildFileSpecifiedInCmd = false;
static bool bIsAssemblyExe = false;

static ECompiler GCompiler = Compiler_Clang;

internal bool IsBuildFileExt(const String Extension)
{
    return  String_IsEqual(Extension, StrLit(".build"), false) ||
            String_IsEqual(Extension, StrLit(".rbuild"), false) ||
            String_IsEqual(Extension, StrLit(".rb"), false) ||
            String_IsEqual(Extension, StrLit(".riftbuild"), false);
}

internal bool IsBuildFile(const String FilePath)
{
    return  String_EndsWith(FilePath, StrLit(".build"), false) ||
            String_EndsWith(FilePath, StrLit(".rbuild"), false) ||
            String_EndsWith(FilePath, StrLit(".rb"), false) ||
            String_EndsWith(FilePath, StrLit(".riftbuild"), false);
}

bool DoesCmdVarExist(const String Name)
{
    for each (Var, CmdOptionsDB)
    {
        if (String_IsEqual(Var.Name, Name, false))
        {
            return true;
        }
    }

    return false;
}

String GetCmdOptionValue(const String Name)
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

bool DoesBuildVarExist(const String Name)
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

StringList GetVariableValueList(LinearAllocator* Arena, const String Name)
{
    StringList list = StringList_Null();

    for each (Var, VariablesDB)
    {
        if (String_IsEqual(Var.Name, Name, false))
        {
            StringList* This = &list;
            while (This->Next && This->Next != StringList_Null().Next)
            {
                This = This->Next;
            }

            This->String = Var.Value;
            This->Next = LinearAllocator_Allocate(Arena, sizeof(StringList));
            This->Next->String = String_Null();
            This->Next->Next = StringList_Null().Next;
        }
    }

    return list;
}

/*
internal String GetVariableValue(const String Name)
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
*/

String GetExpandedVariableValue(const String Name)
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

internal bool ExpandBuildVariable(String* Dest, const String Key, const String Value, const String Root, bool bLowerStrings)
{
    if (!String_IsValid(Value))
    {
        return true;
    }

    bool bLinux = false;
    #if PLATFORM_LINUX
    bLinux = bIsAssemblyExe;
    #endif

    bool bLowerAll = bLowerStrings || (String_IsEqual(Key, StrLit("Assembly"), false) && bLinux);

    u32 Offset = 1;
    for (u32 i = 0; i < Value.Length; i+=Offset)
    {
        Offset = 1;

        String Str = StrSlice(Value.Data+i, Value.Length-i);
        char C = Value.Data[i];

        if (C == '#') // a comment. disgard everything and exit
        {
            return true;
        }

        String Slice = String_Null();
        bool bIsEnclosed = false;
        if (C == '%' || C == '$' || C == '@')
        {
            u32 Index = 0;

            String_EatCharInline(&Str, C);
            if (String_EatCharInline(&Str, '('))
            {
                Offset++;

                if (String_IndexOfChar(Str, ')', &Index))
                {
                    Offset++;
                    bIsEnclosed = true;
                }
            }

            if (Index == 0)
            {
                String_IndexOfFirstWhitespace(Str, &Index);
            }

            // find this variable
            if (Index > 0)
            {
                Slice = StrSlice(Str.Data, Index);
                Offset += Index;
            }
            else
            {
                Slice = Str;
                Offset += Str.Length;
            }

            bool bIgnore = Slice.Data[0] == '\\';
            if (bIgnore)
            {
                String_AppendChar(Dest, C);
                String_Append(Dest, StrShiftF(Slice, 1));
                continue;   
            }
        }

        if (C == '%')
        {
            String VarValue = String_Null();

            const bool bHasNot = String_EatCharInline(&Slice, '!');

            bool bFoundCmd = false;
            for each (o, CmdOptionsDB)
            {
                if (String_IsEqual(o.Name, Slice, false))
                {
                    bFoundCmd = true;
                    VarValue = o.Value;
                    break;
                }
            }

            if (String_IsValid(VarValue))
            {
                // if the first letter is capitalized, then also make the first letter of the value capitalized. revert back when done
                bool bWasValueLower = IsAlphabetLower(VarValue.Data[0]);
                bool bIsVarUpper = IsAlphabetUpper(Slice.Data[0]);
                if (bIsVarUpper)
                    VarValue.Data[0] = ToUpper(VarValue.Data[0]);

                if (!ExpandBuildVariable(Dest, Slice, VarValue, Root, false))
                {
                    return false;
                }

                if (bIsVarUpper && bWasValueLower)
                    VarValue.Data[0] = ToLower(VarValue.Data[0]);
            }
            else
            {
                if (bHasNot)
                    bFoundCmd = !bFoundCmd;

                // the output of a found empty % cmd depends on the context...
                // if we're inside certain keywords (like "Depends") then expand to nothing if we didnt find a value
                bool bExpandToNothing = false;
                if (String_IsEqual(Root, StrLit("Depends"), false))
                {
                    bExpandToNothing = true;
                }

                if (bExpandToNothing)
                {
                    if (bFoundCmd) // but if it was mentioned, just paste the name in
                        String_Append(Dest, Slice);
                }
                else
                {
                    String_AppendChar(Dest, bFoundCmd ? '1' : '0');
                }
            }
        }
        else if (C == '$')
        {
            bool bFound = DoesBuildVarExist(Slice);
            if (!bFound)
            {
                LOG_WARNING("Unrecognized build variable \"%S\". Expanded to nothing...", Slice);
                continue;
            }

            if (String_IsEqual(Slice, Key, false))
            {
                LOG_ERROR("Circular expansion: %S is referencing itself", Key);
                return false;
            }

            if (String_IsEqual(Slice, Root, false))
            {
                LOG_ERROR("Circular expansion: %S is indirectly referencing itself from %S", Root, Key);
                return false;
            }

            u16 NumEntries = 0;
            for each (Var, VariablesDB)
            {
                if (String_IsEqual(Var.Name, Slice, false))
                {
                    if (NumEntries > 0)
                    {
                        if (Dest->Length > 0)
                        {
                            String_EatSpacesInlineFromEnd(Dest);
                            String_AppendSpace(Dest);
                        }
                    }

                    if (!ExpandBuildVariable(Dest, Slice, Var.Value, Root, bLowerStrings))
                    {
                        return false;
                    }

                    NumEntries++;
                }
            }
        }
        else if (C == '@' && bIsEnclosed) // we must be enclosed by ( )
        {
            // find this variable
            StringLocal(VarValue, 4096);
            if (!Platform_GetEnvironmentVariableValue(Slice, &VarValue))
            {
                LOG_ERROR("Could not retrieve environment variable for %S", Slice);
                return false;
            }

            if (!ExpandBuildVariable(Dest, Slice, VarValue, Root, false))
            {
                return false;
            }
        }
        else
        {
            // fix duplicate spaces when expanding
            if (Dest->Length > 0)
            {
                if (IsWhitespace(Dest->Data[Dest->Length-1]) && IsWhitespace(C))
                {
                    continue;
                }
            }
            else
            {
                // the first char should never be whitespace
                if (IsWhitespace(C))
                {
                    continue;
                }
            }

            // when expanding, turn tab spaces into regular spaces
            if (C == '\t')
            {
                C = ' ';
            }

            String_AppendChar(Dest, bLowerAll ? ToLower(C) : C);
        }
    }

    String_EatSpacesInlineFromEnd(Dest);

    return true;
}

internal void PrefixVariables(String* Dest, String VariableValue, const String Prefix)
{
    bool bInsideQuote = false;
    bool bSawSpace = false;

    if (VariableValue.Length > 0)
    {
        String_Append(Dest, Prefix);

        // TODO: only if we're using a C compiler
        #if PLATFORM_LINUX
        if (String_StartsWith(VariableValue, StrLit("lib"), false) &&
            String_IsEqual(Prefix, StrLit("-l"), true))
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

                    // TODO: only if we're using a C compiler
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

/*
internal void PrefixAndSuffixVariables(String* Dest, String VariableValue, const String Prefix, const String Suffix)
{
    bool bInsideQuote = false;
    bool bSawSpace = false;

    if (VariableValue.Length > 0)
    {
        String_Append(Dest, Prefix);
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
                    String_Append(Dest, Prefix);
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
*/

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
internal bool IconFileDirectoryIterator(const String FullPath, const String RelativePath, const String FileName, u64 FileSize, bool bIsDirectory)
{
    if (FileSize > 0)
    {
        if (String_IsEqual(FileName, GetExpandedVariableValue(StrLit("Icon")), false))
        {
            String_Copy(&GIconFilePath, RelativePath);
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

internal bool IsSource(const String Ext)
{
    if (GCompiler == Compiler_MSVC) // todo: try to do the same with clang
    {
        if (String_IsEqual(Ext, StrLit(".asm"), false))
        {
            return true;
        }
    }

    return C_IsSource(Ext);
}

internal bool IsHeader(const String Ext)
{
    return C_IsHeader(Ext);
}

internal bool SourceFileDirectoryIterator(const String FullPath, const String RelativePath, const String FileName, u64 FileSize, bool bIsDirectory)
{
    if (FileSize > 0)
    {
        if (String_StartsWith(RelativePath, StrLit("Intermediate"), false) ||
            String_StartsWith(RelativePath, StrLit("Build"), false))
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

internal bool BuildFileDirectoryIterator(const String FullPath, const String RelativePath, const String FileName, u64 FileSize, bool bIsDirectory)
{
    if (FileSize > 0)
    {
        if (IsBuildFile(FileName))
        {
            if (String_StartsWith(FileName, StrLit("__"), false))
            {
                if (bNoBuildFileSpecifiedInCmd)
                {
                    return true;
                }
            }

            if (bNoBuildFileSpecifiedInCmd ||
                !String_IsValid(GBuildFileName) ||
                String_IsEqual(FileName, GBuildFileName, false))
            {
                bFoundBuildFile = true;

                if (!String_IsValid(GBuildFileName))
                {
                    String_Copy(&GBuildFileName, FileName);
                }

                String_Copy(&GBuildFilePath, RelativePath);

                Array_Add(GBuildFiles, String_Duplicate(&GBuildFilePathAllocator, RelativePath));
                NumBuildFilesFound++;

                if (!bNoBuildFileSpecifiedInCmd)
                    return false;
            }
        }
    }

    return true;
}

internal bool Internal_ParseBuildFile(FileHandle* H, u32* ReturnCode, bool bIsIncludeFile)
{
    if (ReturnCode)
        *ReturnCode = 0;

    StringLocal(Line, 4096);

    bool bIsSwitch = false;
    bool bCaseMatch = false;
    bool bFindEndSwitch = false;
    bool bEndSwitchOnNextCase = false;
    bool bSkipUntilNextCase = false;
    bool bInsideIf = false;
    bool bIfFailed = false;
    bool bInsideElse = false;
    bool bInsideSquareBrackets = false;
    bool bGoto = false;

    StringLocal(SwitchValue, 256);
    StringLocal(GotoValue, 128);

    EComparisonType Comparison = Cmp_None;

    u16 LineNumber = 0;
    while (Filesystem_ReadLine(H, &Line))
    {
        LineNumber++;

    LoopStart:
        if (Line.Length == 0)
        {
            continue;
        }

        // skip blank lines and comments
        const String Trimmed = String_EatSpaces(Line);

        if (Trimmed.Length < 1 ||
            Trimmed.Data[0] == '#' ||
            Trimmed.Data[0] == '{' ||
            Trimmed.Data[0] == '[' ||
            Trimmed.Data[0] == '\0')
        {
            continue;
        }

        if (bGoto)
        {
            u32 Colon = 0;
            String_IndexOfChar(Trimmed, ':', &Colon);
            if (!String_IsEqual(StrSlice(Trimmed.Data, Colon), GotoValue, false))
            {
                continue;
            }

            bGoto = false;
            continue;
        }

        if (bInsideIf)
        {
            bool bSeenElse = false;

            u32 Space = 0;
            if (String_IndexOfFirstWhitespace(Trimmed, &Space))
            {
                String Else = StrShiftF(Trimmed, Space+1);
                String ElseOg = Else;
                u32 OpenCurly = 0;
                if (String_IndexOfChar(Else, '{', &OpenCurly))
                {
                    Else = StrSlice(Else.Data, OpenCurly);
                    Else = String_EatSpacesFromEnd(Else);
                    if (String_IsEqual(Else, StrLit("else"), false))
                    {
                        bSeenElse = true;
                        bInsideElse = true;
                        if (bIfFailed)
                            continue;
                    }

                    String_IndexOfLastWhitespace(Else, &Space);
                    String ElseIf = String_EatSpacesFromEnd(StrSlice(Else.Data, Space));
                    if (String_IsEqual(ElseIf, StrLit("else if"), false))
                    {
                        bInsideElse = true;
                        bSeenElse = true;
                        if (bIfFailed)
                        {
                            // extract just the if statement
                            String_IndexOfFirstWhitespace(ElseOg, &Space);

                            bIfFailed = false;
                            bInsideElse = false;
                            String_Copy(&Line, StrShiftF(ElseOg, Space+1));
                            goto LoopStart;
                        }
                    }
                }
            }

            if (bIfFailed)
            {
                if (Trimmed.Data[0] != '}' && !bInsideElse)
                {
                    continue;
                }
            }
            else
            {
                if (bSeenElse)
                    continue;

                if (Trimmed.Data[0] != '}' && bInsideElse)
                {
                    continue;
                }
            }

            if (Trimmed.Data[0] == '}')
            {
                bInsideIf = false;
                continue;
            }
        }

        if (bIsSwitch)
        {
            if (String_IsEqual(Trimmed, StrLit("endswitch"), false))
            {
                bIsSwitch = false;
                bFindEndSwitch = false;
                String_Empty(&SwitchValue);
                continue;
            }

            if (bSkipUntilNextCase)
            {
                bool bFoundNextCase = false;

                TEMP_SCRATCH(Case)
                {
                    StringArray Cases = String_ParseIntoArray(Scratch_Case.Allocator, Trimmed, ' ', 0, 128);

                    String CmdValue = GetCmdOptionValue(SwitchValue);

                    if (!String_IsValid(CmdValue))
                    {
                        for each (o, VariablesDB) // intentional that we're not using expanded DB, this should only be used for simple things anyway
                        {
                            bool bMatch = String_IsEqual(o.Name, SwitchValue, false);
                            if (bMatch)
                            {
                                CmdValue = o.Value;
                                break;
                            }
                        }
                    }

                    for each_str (m, Cases)
                    {
                        if (!String_IsValid(*m))
                            continue;

                        if (String_IsEqual(*m, CmdValue, false) ||
                            (String_IsEqual(*m, StrLit("Default"), false) && !String_IsValid(CmdValue)))
                        {
                            bSkipUntilNextCase = false;
                            bFoundNextCase = true;
                            break;
                        }
                    }
                }

                if (!bFoundNextCase)
                {
                    continue;
                }
            }
        }

        if (bIsSwitch && (!bCaseMatch || bEndSwitchOnNextCase) && !bFindEndSwitch)
        {
            bool bSawCaseKeyword = false;
            bool bFoundCase = false;

            TEMP_SCRATCH(Case)
            {
                StringArray Modes = String_ParseIntoArray(Scratch_Case.Allocator, Trimmed, ' ', 0, 128);

                for each_str (m, Modes)
                {
                    if (!String_IsValid(*m))
                        continue;

                    if (String_IsEqual(*m, StrLit("case"), false))
                    {
                        bSawCaseKeyword = true;
                        continue;
                    }

                    String CmdValue = GetCmdOptionValue(SwitchValue);

                    if (!String_IsValid(CmdValue))
                    {
                        for each (o, VariablesDB) // intentional that we're not using expanded DB, this should only be used for simple things anyway
                        {
                            bool bMatch = String_IsEqual(o.Name, SwitchValue, false);
                            if (bMatch)
                            {
                                CmdValue = o.Value;
                                break;
                            }
                        }
                    }

                    if (String_IsEqual(*m, CmdValue, false) ||
                        (String_IsEqual(*m, StrLit("Default"), false) && !String_IsValid(CmdValue)))
                    {
                        bFoundCase = true;
                        bCaseMatch = true;
                        break;
                    }
                }
            }

            if (bEndSwitchOnNextCase)
            {
                if (bSawCaseKeyword)
                {
                    bFindEndSwitch = true;
                    bSkipUntilNextCase = false;
                }
            }
            else
            {
                if (!bFoundCase)
                {
                    bSkipUntilNextCase = true;
                }

                continue;
            }
        }

        u32 SpaceIndex = 0;
        bool bFoundSpace = String_IndexOfFirstWhitespace(Trimmed, &SpaceIndex);

        String VarName, VarValue;

        if (bFoundSpace)
        {
            const String Name = String_EatSpacesFromEnd(StrSlice(Trimmed.Data, SpaceIndex));
            const String Value = String_EatSpacesFromEnd(String_EatSpaces(StrSlice(Trimmed.Data+(SpaceIndex+1), Trimmed.Length-(SpaceIndex+1))));

            VarName = Name;
            VarValue = Value;
        }
        else
        {
            VarName = Trimmed;
            VarValue = String_Null();
        }

        if (String_IsEqual(VarName, StrLit("_stop"), false))
        {
            LOG_INFO("Stopping build file parsing...");
            break;
        }

        if (String_IsEqual(VarName, StrLit("_abort"), false))
        {
            u32 ExitCode = 0;
            u32 FirstSpace = 0;
            String_IndexOfFirstWhitespace(VarValue, &FirstSpace);

            const String a = FirstSpace == 0 ? VarValue : StrSlice(VarValue.Data, FirstSpace);
            if (String_ToU32(a, &ExitCode))
            {
                const String RestOfLine = String_EatSpaces(StrShiftF(VarValue, FirstSpace));
                if (FirstSpace > 0 && RestOfLine.Length > 0)
                {
                    LOG("Exiting with code %i | %S", ExitCode, RestOfLine);
                }
                else
                {
                    LOG("Exiting with code %i", ExitCode);
                }

                return ExitCode;
            }

            if (VarValue.Length > 0)
            {
                LOG("Exiting with message: %S", VarValue);
            }
            else
            {
                LOG("Exiting...");
            }

            *ReturnCode = ExitCode;

            return ExitCode == 0;
        }

        if (bInsideSquareBrackets)
        {
            String* LastValue = &VariablesDB[Array_Num(VariablesDB)-1].Value;

            if (Trimmed.Data[0] == ']')
            {
                bInsideSquareBrackets = false;
                String_EatSpacesInlineFromEnd(LastValue);
                continue;
            }

            VarValue = Trimmed;
            String_Append(LastValue, String_EatSpacesFromEnd(VarValue));
            String_AppendSpace(LastValue);

            continue;
        }

        if (String_IsEqual(VarName, StrLit("switch"), false) && bFoundSpace) // make sure this isnt a lone 'switch'
        {
            bIsSwitch = true;
            bCaseMatch = false;
            bEndSwitchOnNextCase = false;
            bFindEndSwitch = false;
            bSkipUntilNextCase = false;

            String_Copy(&SwitchValue, VarValue);
        }

        if (bIsSwitch)
        {
            if (bFindEndSwitch)
            {
                continue;
            }

            if (bCaseMatch)
            {
                bEndSwitchOnNextCase = true;
            }
            else
            {
                continue;
            }
        }

        if (String_IsEqual(VarName, StrLit("goto"), false))
        {
            if (VarValue.Length > 0)
            {
                bGoto = true;
                String_Copy(&GotoValue, VarValue);
                continue;
            }
        }

        if (Trimmed.Data[0] == '"')
        {
            u32 LastQuoteIndex = 0;
            String_IndexOfLastChar(Trimmed, '"', &LastQuoteIndex);
            if (LastQuoteIndex == 0)
            {
                LOG_ERROR("Missing closing \" for %S on line %hu in %S", Trimmed, LineNumber, Str(GBuildFileName));
                return false;
            }

            String RestOfTheLine = String_EatSpaces(StrSlice(Trimmed.Data+LastQuoteIndex+1, Trimmed.Length-LastQuoteIndex-1));

            StringLocal(FormattedMsg, 2048);

            TEMP_SCRATCH(MsgList)
            {
                StringArray MsgArgsList = String_ParseIntoArray(Scratch_MsgList.Allocator, RestOfTheLine, ' ', 0, 64);

                u8 ArgIndex = 0;
                String MsgString = StrSlice(Trimmed.Data+1, LastQuoteIndex-1);
                for (u32 i = 0; i < MsgString.Length; i++)
                {
                    char C = MsgString.Data[i];
                    if (C == '%')
                    {
                        const String Arg = StringArray_GetStringFromIndex(MsgArgsList, ArgIndex);
                        ArgIndex++;

                        String Var = String_EatChar(Arg, '%');
                        String Val = GetCmdOptionValue(Var);
                        String_Append(&FormattedMsg, Val);
                        continue;
                    }

                    String_AppendChar(&FormattedMsg, C);
                }
            }

            if (FormattedMsg.Length > 0)
            {
                Array_Add(Messages, String_Create(&GMessagesAllocator, FormattedMsg));
            }

            continue;
        }

        if (String_IsEqual(VarName, StrLit("if"), false) && bFoundSpace) // make sure this isnt a lone 'if'
        {
            u32 Index = 0;
            String_IndexOfFirstWhitespace(VarValue, &Index);

            bool bIsMultiLineIf = String_IndexOfChar(VarValue, '{', NULL);
            bInsideIf = bIsMultiLineIf;

            String Condition;
            if (Index > 0)
                Condition = StrSlice(VarValue.Data, Index);
            else
                Condition = VarValue;

            bool bIsNot = Condition.Data[0] == '!';
            String_EatCharInline(&Condition, '!');

            bool bConditionMet = false;

            // check the condition string against the internal build vars passed in from the command line
            // override VarValue for single line if's, for multiline if's, loop back to the top and process each line until '}' is found
            String ConditionValuePtr = String_Null();
            for each (o, CmdOptionsDB)
            {
                bool bMatch = String_IsEqual(o.Name, Condition, false);
                if (bMatch)
                {
                    ConditionValuePtr = o.Value;
                    bConditionMet = true;
                    break;
                }
            }

            if (!bConditionMet)
            {
                for each (o, VariablesDB) // intentional that we're not using expanded DB, this should only be used for simple things anyway
                {
                    bool bMatch = String_IsEqual(o.Name, Condition, false);
                    if (bMatch)
                    {
                        ConditionValuePtr = o.Value;
                        bConditionMet = true;
                        break;
                    }
                }
            }

            String ComparisonOperator = String_EatSpaces(StrSlice(VarValue.Data+Index, VarValue.Length-Index));
            u32 SecondWhitespaceIndex = 0;
            String_IndexOfFirstWhitespace(ComparisonOperator, &SecondWhitespaceIndex);
            ComparisonOperator = StrSlice(ComparisonOperator.Data, SecondWhitespaceIndex);

            if (String_IsEqual(ComparisonOperator, StrLit("=="), false))
            {
                Comparison = Cmp_Equal;
            }
            else if (String_IsEqual(ComparisonOperator, StrLit("!="), false))
            {
                Comparison = Cmp_NotEqual;
            }
            else if (String_IsEqual(ComparisonOperator, StrLit(">="), false))
            {
                Comparison = Cmp_GreaterThanOrEqual;
            }
            else if (String_IsEqual(ComparisonOperator, StrLit("<="), false))
            {
                Comparison = Cmp_LessThanOrEqual;
            }
            else if (String_IsEqual(ComparisonOperator, StrLit(">"), false))
            {
                Comparison = Cmp_GreaterThan;
            }
            else if (String_IsEqual(ComparisonOperator, StrLit("<"), false))
            {
                Comparison = Cmp_LessThan;
            }
            else
            {
                Comparison = Cmp_None;
            }

            String TestValue = String_EatSpaces(StrSlice(VarValue.Data+Index+1+SecondWhitespaceIndex, VarValue.Length-Index-1-SecondWhitespaceIndex));
            u32 ThirdWhitespaceIndex = 0;
            String_IndexOfFirstWhitespace(TestValue, &ThirdWhitespaceIndex);
            TestValue = StrSlice(TestValue.Data, ThirdWhitespaceIndex);

            if (Comparison != Cmp_None && ThirdWhitespaceIndex)
            {
                Index = (u32)((TestValue.Data+ThirdWhitespaceIndex) - VarValue.Data);
            }

            i64 LeftInt = 0, RightInt = 0;
            switch (Comparison)
            {
                default:
                case Cmp_None:
                break;

                case Cmp_Equal:
                {
                    TEMP_SCRATCH(Cond)
                    {
                        StringArray Values = String_ParseIntoArray(Scratch_Cond.Allocator, TestValue, '|', 0, 128);
                        for each_str (v, Values)
                        {
                            bConditionMet = String_IsEqual(ConditionValuePtr, *v, false);
                            if (bConditionMet)
                            {
                                break;
                            }

                            StringArray Values2 = String_ParseIntoArray(Scratch_Cond.Allocator, ConditionValuePtr, ' ', 0, 128);
                            for each_str (v2, Values2)
                            {
                                bConditionMet = String_IsEqual(*v, *v2, false);
                                if (bConditionMet)
                                {
                                    break;
                                }
                            }

                            if (bConditionMet)
                            {
                                break;
                            }
                        }
                    }

                    //bConditionMet = String_IsEqual(ConditionValuePtr, TestValue, false);
                }
                break;

                case Cmp_NotEqual:
                {
                    TEMP_SCRATCH(Cond)
                    {
                        StringArray Values = String_ParseIntoArray(Scratch_Cond.Allocator, TestValue, '|', 0, 128);
                        for each_str (v, Values)
                        {
                            bConditionMet = !String_IsEqual(ConditionValuePtr, *v, false);
                            if (bConditionMet)
                            {
                                break;
                            }

                            StringArray Values2 = String_ParseIntoArray(Scratch_Cond.Allocator, ConditionValuePtr, ' ', 0, 128);
                            for each_str (v2, Values2)
                            {
                                bConditionMet = !String_IsEqual(*v, *v2, false);
                                if (bConditionMet)
                                {
                                    break;
                                }
                            }

                            if (bConditionMet)
                            {
                                break;
                            }
                        }
                    }

                    //bConditionMet = !String_IsEqual(ConditionValuePtr, TestValue, false);
                }
                break;

                case Cmp_GreaterThanOrEqual:
                {
                    if (!String_ToI64(ConditionValuePtr, &LeftInt) ||
                        !String_ToI64(TestValue, &RightInt))
                    {
                        bConditionMet = false;
                        break;
                    }

                    bConditionMet = LeftInt >= RightInt;
                }
                break;

                case Cmp_LessThanOrEqual:
                {
                    if (!String_ToI64(ConditionValuePtr, &LeftInt) ||
                        !String_ToI64(TestValue, &RightInt))
                    {
                        bConditionMet = false;
                        break;
                    }

                    bConditionMet = LeftInt <= RightInt;
                }
                break;

                case Cmp_GreaterThan:
                {
                    if (!String_ToI64(ConditionValuePtr, &LeftInt) ||
                        !String_ToI64(TestValue, &RightInt))
                    {
                        bConditionMet = false;
                        break;
                    }

                    bConditionMet = LeftInt > RightInt;
                }
                break;

                case Cmp_LessThan:
                {
                    if (!String_ToI64(ConditionValuePtr, &LeftInt) ||
                        !String_ToI64(TestValue, &RightInt))
                    {
                        bConditionMet = false;
                        break;
                    }

                    bConditionMet = LeftInt < RightInt;
                }
                break;
            }

            if (bIsNot)
            {
                bConditionMet = !bConditionMet;
            }

            String RestOfTheLine = StrShiftF(VarValue, Index);
            String_EatSpacesInlineFromEnd(&RestOfTheLine);

            // else statement detection
            u32 LengthCap = 0;
            bool bHasElse = false;
            TEMP_SCRATCH(Else)
            {
                StringArray Strings = String_ParseIntoArray(Scratch_Else.Allocator, RestOfTheLine, ' ', 0, 128);
                for each_str (S, Strings)
                {
                    if (String_IsEqual(*S, StrLit("else"), false))
                    {
                        bHasElse = true;
                        break;
                    }

                    LengthCap += S->Length;
                    LengthCap += 1;
                }
            }

            if (bConditionMet)
            {
                bIfFailed = false;
                bool bError = false;

                if (Index > 0)
                {
                    if (RestOfTheLine.Length > 0)
                    {
                        if (bIsMultiLineIf)
                        {
                            continue;
                        }

                        if (bHasElse)
                            String_Copy(&Line, String_EatSpaces(StrSlice(RestOfTheLine.Data, LengthCap)));
                        else
                            String_Copy(&Line, String_EatSpaces(RestOfTheLine));

                        goto LoopStart;
                    }
                    else
                    {
                        bError = true;
                    }
                }
                else
                {
                    bError = true;
                }

                if (bError)
                {
                    LOG_ERROR("Missing '{' for if statement \"%S\" on line %hu", Line, LineNumber);
                    return false;
                }
            }
            else
            {
                if (bIsMultiLineIf)
                {
                    bIfFailed = true;
                }

                if (RestOfTheLine.Length > 0)
                {
                    if (bHasElse)
                    {
                        String_Copy(&Line, String_EatSpaces(StrShiftF(RestOfTheLine, LengthCap+5))); // else is 4 chars + 1 white space
                        goto LoopStart;
                    }
                }

                continue;
            }
        }

        const String Keywords[] =
        {
            StrLit("include"),
            StrLit("switch"),
            StrLit("endswitch"),
            StrLit("if"),
            StrLit("case"),
            StrLit("goto"),
            StrLit("_abort"),
            StrLit("_stop"),
        };

        bool bVarNameIsKeyword = false;
        for (u8 i = 0; i < (sizeof Keywords / sizeof(String)); i++)
        {
            if (String_IsEqual(VarName, Keywords[i], false))
            {
                bVarNameIsKeyword = true;
                break;
            }
        }

        if (!bVarNameIsKeyword)
        {
            if (VarValue.Length > 0)
            {
                if (VarValue.Data[0] == '[')
                {
                    FileVariable var;
                    var.Name = String_Create(&GVariablesAllocator, VarName);
                    var.Value.Data = LinearAllocator_Allocate(&GVariablesAllocator, 8192); // allocate one really long line, because we dont know how many lines there will be
                    var.Value.Length = 0;
                    var.Value.Capacity = 8191;

                    Array_Add(VariablesDB, var);

                    bInsideSquareBrackets = true;
                    continue;
                }
            }

            // check if we already added this value for this build variable
            bool bDuplicateValueFound = false;
            for each (Var, VariablesDB)
            {
                if (String_IsEqual(Var.Name, VarName, false))
                {
                    if (String_IsEqual(Var.Value, VarValue, false))
                    {
                        bDuplicateValueFound = true;
                        break;
                    }
                }
            }

            if (bDuplicateValueFound)
            {
                LOG_WARNING("Duplicate build variable \"%S %S\". Skipping...", VarName, VarValue);
                continue;
            }

            FileVariable var;
            var.Name = String_Create(&GVariablesAllocator, VarName);
            var.Value = String_Create(&GVariablesAllocator, String_EatSpacesFromEnd(VarValue));

            Array_Add(VariablesDB, var);
        }

        if (String_IsEqual(VarName, StrLit("Include"), false))
        {
            StringLocal(IncludeFilePath, MAX_PATH_LENGTH);
            
            // if we're inside a build variables include file then
            // any include we declare should be relative to us
            if (bIsIncludeFile)
            {
                u32 LastSlash = 0;
                String_IndexOfLastPathSlash(StrView(H->Path), &LastSlash);

                const String Path = StrSlice(H->Path.Data, LastSlash);
                String_BuildPath(&IncludeFilePath, Path, VarValue);
            }
            else
            {
                String_BuildPath(&IncludeFilePath, GRootPath, VarValue);
            }

            StringLocal(ExpandedPath, 512);
            if (!ExpandBuildVariable(&ExpandedPath, StrLit("Include"), IncludeFilePath, StrLit("Include"), false))
            {
                return false;
            }

            String_ConvertSlashToPlatformSlash(&ExpandedPath);

            if (!Filesystem_DoesFileExist(ExpandedPath))
            {
                #ifndef HOOD
                LOG_ERROR("Failed to open include file \"%S\" in %S", ExpandedPath, GBuildFilePath);
                #else
                LOG_ERROR("bruh sort your shit out man, cant find this file bro \"%S\" in %S", Path, GBuildFilePath);
                #endif
                return false;
            }

            FileHandle IncludeFileHandle = {0};
            if (!Filesystem_Open(ExpandedPath, FileMode_Read, &IncludeFileHandle))
            {
                #ifndef HOOD
                LOG_ERROR("Failed to open include file \"%S\" for reading", ExpandedPath);
                #else
                LOG_ERROR("huhh?!!!!! cant read the include file for some reason bro, \"%S\", think you gotta check it out on your end cuh", ExpandedPath);
                #endif
                return false;
            }

            u64 Size = 0;
            Filesystem_GetFileSize(&IncludeFileHandle, &Size);

            if (Size == 0)
            {
                #ifndef HOOD
                LOG_WARNING("Include file \"%S\" has a size of 0. Skipping...", ExpandedPath);
                #else
                LOG_WARNING("ay bro heads up, gonna skip dis one, dis shit is empty nigga \"%S\"", ExpandedPath);
                #endif
            }
            else
            {
                Array_Add(IncludeFiles, IncludeFileHandle);

                if (!Internal_ParseBuildFile(&IncludeFileHandle, ReturnCode, true))
                {
                    return false;
                }
            }
        }
    }

    return true;
}

internal void ListVariables(const String Name)
{
    const String Exclusions[] =
    {
        StrLit("AssertProgramExists"),
        StrLit("AssertBuildVarExists"),
        StrLit("AssertCmdVarExists"),
        StrLit("AssertEnvVarExists"),
        StrLit("AssertPlatform"),
        StrLit("PreBuildCmd"),
        StrLit("PostBuildCmd"),
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
        StrLit("Icon"),
        StrLit("Compiler"),
    };

    TEMP_SCRATCH(_)
    for each (v, ExpandedVariablesDB)
    {
        if (Name.Length == 0 || String_IsEqual(v.Name, Name, false))
        {
            LOG_INLINE_WARNING("%S\n", v.Name);

            bool bOneLine = false;
            for (u32 j = 0; j < SArray_Capacity(Exclusions); j++)
            {
                if (String_IsEqual(v.Name, Exclusions[j], false))
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
                StringArray Values = String_ParseIntoArray(Scratch__.Allocator, v.Value, ' ', 0, 4096);
                for each_str (It, Values)
                {
                    LOG("    %S", *It);
                }
            }
        
            LOG_LINE_BREAK();
        }
    }
}

u32 RunApplication(const StringArray Arguments)
{
    Logging_ToggleLogTimeStamp(false);
    Logging_ToggleLogCategory(false);

    #ifndef HOOD
    LOG("Rift Build System Alpha v%u.%u.%u (%S)", MAJOR_VERSION, MINOR_VERSION, PATCH_VERSION, StrLit(PLATFORM_STRING));
    #else
    LOG("Rift Build System Alpha v%u.%u.%u (%S) - (HOOD EDITION)", MAJOR_VERSION, MINOR_VERSION, PATCH_VERSION, StrLit(PLATFORM_STRING));
    LOG("\nwasssup yo. les get build'n...");
    #endif

    StringLocal(RiftCmdLine, 2048);
    for (u8 i = 0; i < Arguments.Num; i++)
    {
        String_Append(&RiftCmdLine, Arguments.List[i]);
        String_AppendSpace(&RiftCmdLine);
    }
    String_EatSpacesInlineFromEnd(&RiftCmdLine);

    Clock ProgramRuntime;
    Clock CompileClock;
    Clock LinkClock = {0};
    Clock BuildFileParseClock;
    Clock_Start(&ProgramRuntime);

    StringLocal(BuildFileName, 128);
    GBuildFileName = BuildFileName;

    StringLocal(BuildFilePath, MAX_PATH_LENGTH);
    GBuildFilePath = BuildFilePath;

    StringLocal(IconFilePath, MAX_PATH_LENGTH);
    GIconFilePath = IconFilePath;

    StringLocal(ResourceFilePath, MAX_PATH_LENGTH);
    GResourceFilePath = ResourceFilePath;

    StringLocal(WorkingDirectory, MAX_PATH_LENGTH);
    Platform_GetWorkingDirectory(&WorkingDirectory);

    bool bBuildPathGivenInCmdLine = false;

    GRootPath = WorkingDirectory;

    CmdOptionsDB = Array_Reserve(CmdOption, 4);

    if (Arguments.Num == 0)
    {
        bNoBuildFileSpecifiedInCmd = true;
    }
    else
    {
        i8 BuildFileIndex = -1;
        for (u8 i = 0; i < Arguments.Num; i++)
        {
            if (IsBuildFile(Arguments.List[i]))
            {
                BuildFileIndex = (i8)i;
                break;
            }
        }

        i8 RootPathIndex = -1;
        for (u8 i = 0; i < Arguments.Num; i++)
        {
            if (i == BuildFileIndex)
                continue;
        
            if (String_IndexOfChar(Arguments.List[i], '\\', NULL) ||
                String_IndexOfChar(Arguments.List[i], '/', NULL))
            {
                RootPathIndex = (i8)i;
                break;
            }
        }

        i8 CameFromBuildFileIndex = -1;
        for (u8 i = 0; i < Arguments.Num; i++)
        {
            const String Param = Arguments.List[i];

            u32 Index = 0;
            if (String_IndexOfLastChar(Param, '_', &Index))
            {
                String Underscore = StrSlice(Param.Data+Index, Param.Length-Index);

                String_IndexOfLastChar(Underscore, '.', &Index);
                String Dot = StrShiftF(Underscore, Index);

                if (IsBuildFileExt(Dot))
                {
                    CameFromBuildFileIndex = (i8)i;
                    break;
                }
            }
        }

        // store custom command line options to be referenced inside of a .build file
        for (u8 i = 0; i < Arguments.Num; i++)
        {
            const String Param = Arguments.List[i];
            if (Param.Length >= 2)
            {
                if (i != BuildFileIndex &&
                    i != RootPathIndex)
                {
                    const String P = String_EatChar(Param, '-');

                    u32 EqualIndex = 0;
                    bool bFoundEqual = String_IndexOfChar(P, '=', &EqualIndex);

                    CmdOption c;
                    if (bFoundEqual)
                    {
                        c.Name = StrSlice(P.Data, EqualIndex);
                        String_EatSpacesInlineFromEnd(&c.Name);
                        c.Value = StrSlice(P.Data+EqualIndex+1, P.Length - (EqualIndex + 1));
                        String_EatSpacesInline(&c.Value);
                        if (c.Value.Length == 0)
                            continue;
                    }
                    else
                    {
                        c.Name = P;
                        String_EatSpacesInlineFromEnd(&c.Name);
                        c.Value = String_Null();
                    }

                    Array_Add(CmdOptionsDB, c);
                }
            }
        }

        if (BuildFileIndex == -1)
            bNoBuildFileSpecifiedInCmd = true;

        if (RootPathIndex >= 0)
        {
            String UserPath = Arguments.List[RootPathIndex];
            #if PLATFORM_WINDOWS
            bool bDriveSymbol = String_IndexOfChar(UserPath, ':', NULL);
            #else
            bool bDriveSymbol = UserPath.Data[0] == '/';
            #endif

            bool bRelative = !bDriveSymbol;

            if (bRelative)
            {
                String_BuildPath(&GRootPath, UserPath);
            }
            else
            {
                GRootPath = Arguments.List[RootPathIndex];
            }
        }

        if (BuildFileIndex >= 0)
        {
            u32 LastSlash = 0;
            if (String_IndexOfLastPathSlash(Arguments.List[BuildFileIndex], &LastSlash))
            {
                String Name = StrShiftF(Arguments.List[BuildFileIndex], LastSlash+1);
                String_Copy(&GBuildFileName, Name);
                String_Copy(&GBuildFilePath, Arguments.List[BuildFileIndex]);
                bBuildPathGivenInCmdLine = true;
            }
            else
            {
                String_Copy(&GBuildFileName, Arguments.List[BuildFileIndex]);
            }
        }

        if (CameFromBuildFileIndex >= 0)
            GCameFromBuildFile = String_EatChar(Arguments.List[CameFromBuildFileIndex], '_');
    }

    String_EatPathSeparatorsInlineFromEnd(&GRootPath);
    Filesystem_ConvertRelativeToAbsolutePath(&GRootPath);
    String_ConvertSlashToPlatformSlash(&GRootPath);

    if (GRootPath.Length < 1 || GRootPath.Length > MAX_PATH_LENGTH)
    {
        #ifndef HOOD
        LOG_ERROR("Invalid root path: %S", GRootPath);
        #else
        LOG_ERROR("wtf is this my nigga, dis path make no sense, cant work wit it: %S", GRootPath);
        #endif

        return 1;
    }

    if (!Filesystem_DoesDirectoryExist(GRootPath))
    {
        #ifndef HOOD
        LOG_ERROR("Given root directory \"%S\" does not exist", GRootPath);
        #else
        LOG_ERROR("nah cuh, dis path aint nowhere to be seen: %S", GRootPath);
        #endif

        return 1;
    }

    // prevent riftbuild from running in a root drive directory like C:/ (or / on linux). it's non-sensical anyway, it has no business running in those places
    {
        StringLocal(RootCopy, MAX_PATH_LENGTH);
        String_Copy(&RootCopy, GRootPath);
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

    // store internal options. like platform, native os .lib's, etc..
    CmdOption Platform;
    Platform.Name = StrLit("_Platform");
    Platform.Value = StrLit(PLATFORM_STRING);

    Array_Add(CmdOptionsDB, Platform);

    // add platform libs
    {
        CmdOption WinLibs = {0};
        WinLibs.Name = StrLit("_Win32Libs");
        WinLibs.Value = StrLit("kernel32 user32 shell32 gdi32 comdlg32 comctl32 ws2_32 winmm netapi32 ole32 advapi32 wldap32 crypt32 rpcrt4 shlwapi dbghelp bcrypt version imm32 cfgmgr32 setupapi oleaut32 uuid odbc32 odbccp32 delayimp pathcch");

        Array_Add(CmdOptionsDB, WinLibs);

        CmdOption LinuxLibs = {0};
        LinuxLibs.Name = StrLit("_LinuxLibs");
        LinuxLibs.Value = StrLit("m");

        Array_Add(CmdOptionsDB, LinuxLibs);

        CmdOption PlatformLibs = {0};
        PlatformLibs.Name = StrLit("_NativeLibs");
        PlatformLibs.Value = String_Null();

        #if PLATFORM_WINDOWS
        PlatformLibs.Value = WinLibs.Value;
        #elif PLATFORM_LINUX || PLATFORM_UNIX
        PlatformLibs.Value = LinuxLibs.Value;
        #endif

        Array_Add(CmdOptionsDB, PlatformLibs);
    }

    #ifndef HOOD
    LOG("Working Directory: %S\n", GRootPath);
    #else
    LOG("dis da work'n directory bro: %S", GRootPath);
    #endif

    SystemTime TimeNow = Platform_GetSystemLocalTime();
    StringLocal(TimeStamp, 64);
    String_Format(&TimeStamp, StrLit("%hu-%.2hu-%.2hu %.2hu:%.2hu:%.2hu"), 64, TimeNow.Day, TimeNow.Month, TimeNow.Year, TimeNow.Hour, TimeNow.Minute, TimeNow.Second);
    LOG("Timestamp: %S\n", TimeStamp);

    String_EatPathSeparatorsInlineFromEnd(&GRootPath);

    GBuildFiles = Array_Reserve(String, 32);
    LinearAllocator_Create(Kibibytes(64), NULL, &GBuildFilePathAllocator);

    if (GBuildFilePath.Length == 0) // only search if we did not get an explicit build file path from the user
    {
        Filesystem_IterateDirectory(GRootPath, BuildFileDirectoryIterator, !bNoBuildFileSpecifiedInCmd);
    }
    else
    {
        bFoundBuildFile = Filesystem_DoesFileExist(GBuildFilePath);
        if (!bFoundBuildFile)
        {
            LOG_ERROR("Failed to find %S", GBuildFilePath);
            return 1;
        }
    }

    if (!bFoundBuildFile)
    {
        if (bNoBuildFileSpecifiedInCmd)
        {
        }
        else
        {
            LOG_ERROR("Failed to find %S in %S", Str(GBuildFileName), GRootPath);
            return 1;
        }
    }

    if (bFoundBuildFile)
    {
        if (NumBuildFilesFound > 1)
        {
            #ifndef HOOD
            LOG_ERROR("Multiple build files found. Please specify a build file\n");
            LOG("Here is the list of all the build files found within %S", GRootPath);
            for (u8 i = 0; i < NumBuildFilesFound; i++)
                LOG("    [%hhu] %S", i, GBuildFiles[i]);
            #else
            LOG_ERROR("yooo thes too many buil files here dawg. gotta be more specific for me\n");
            LOG("got a list for ya here, found em from %S", GRootPath);
            for (u8 i = 0; i < NumBuildFilesFound; i++)
                LOG("    [%hhu] %S", i, GBuildFiles[i]);
            #endif

            return 1;
        }

        #ifndef HOOD
        LOG("Using build file: %S\n", GBuildFilePath);
        #else
        LOG("alright sweet, using this build file btw: %S\n", GBuildFilePath);
        #endif
    }

    String_AppendPathSeparator(&GRootPath);

    FileHandle BuildFileHandle = {0};

    if (bFoundBuildFile)
    {
        StringLocal(BuildFilePathFull, MAX_PATH_LENGTH);
        if (bBuildPathGivenInCmdLine)
            String_Copy(&BuildFilePathFull, GBuildFilePath);
        else
            String_BuildPath(&BuildFilePathFull, GRootPath, GBuildFilePath);

        if (!Filesystem_Open(BuildFilePathFull, FileMode_Read, &BuildFileHandle))
        {
            #ifndef HOOD
            LOG_ERROR("Failed to open build file \"%S\" for reading", BuildFilePathFull);
            #else
            LOG_ERROR("wtf, cant read this shit man, think the path to the build file is wrong or smthg homie. this is what i got: %S", BuildFilePath);
            #endif
            return 1;
        }

        u32 LastSlash = 0;
        String_IndexOfLastPathSlash(StrView(BuildFileHandle.Path), &LastSlash);

        u32 LastDot = 0;
        const String Name = StrShiftF(StrView(BuildFileHandle.Path), LastSlash+1);
        String_IndexOfLastChar(Name, '.', &LastDot);

        CmdOption FileName;
        FileName.Name = StrLit("_FileName");
        FileName.Value = StrSlice(Name.Data, LastDot);

        Array_Add(CmdOptionsDB, FileName);

        CmdOption FileNameExt;
        FileNameExt.Name = StrLit("_FileNameExt");
        FileNameExt.Value = Name;

        Array_Add(CmdOptionsDB, FileNameExt);
    }

    bool bIsClean   = StringArray_Contains(Arguments, StrLit("clean"), false);
    bool bIsRebuild = StringArray_Contains(Arguments, StrLit("rebuild"), false);
    //bool bGenCompileCommandsJSON = StringArray_Contains(Arguments, StrLit("gen_compile_commands"), false);

    VariablesDB         = Array_Reserve(FileVariable, 32);
    ExpandedVariablesDB = Array_Reserve(FileVariable, 32);

    IncludeFiles = Array_Reserve(FileHandle, 8);
    Messages     = Array_Reserve(String, 8);

    LinearAllocator_Create(Kilobytes(64), NULL, &GVariablesAllocator);
    LinearAllocator_Create(Kilobytes(64), NULL, &GExpandedVariablesAllocator);
    LinearAllocator_Create(Kilobytes(4), NULL, &GIncludePathAllocator);
    LinearAllocator_Create(Kilobytes(2), NULL, &GMessagesAllocator);

    bool bShouldWaitPerCompileProcess = false;

    if (bFoundBuildFile)
    {
        Clock_Start(&BuildFileParseClock);

        if (!Internal_ParseBuildFile(&BuildFileHandle, NULL, false))
        {
            return 1;
        }

        // first expand Type and Extension. so on linux we can tell if its an assembly exe and not a library
        for each (v, VariablesDB)
        {
            if (String_IsEqual(v.Name, StrLit("Extension"), false) ||
                String_IsEqual(v.Name, StrLit("Type"), false))
            {
                StringLocal(ExpandedVar, 4096);

                TEMP_SCRATCH(Exp)
                {
                    StringList List = GetVariableValueList(Scratch_Exp.Allocator, v.Name);
                    for each_str_list (List)
                    {
                        if (!ExpandBuildVariable(&ExpandedVar, v.Name, It->String, v.Name, false))
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

                FileVariable Expanded;
                Expanded.Name = v.Name;
                Expanded.Value = String_Create(&GExpandedVariablesAllocator, String_EatChar(ExpandedVar, '.'));
                Array_Add(ExpandedVariablesDB, Expanded);
            }
        }

        const String Ext = GetExpandedVariableValue(StrLit("Extension"));
        const String Type = GetExpandedVariableValue(StrLit("Type"));

        bIsAssemblyExe = Type.Length == 0 && Ext.Length == 0;

        if (!bIsAssemblyExe)
        {
            bIsAssemblyExe = String_IsEqual(Type, StrLit("app"), false);
        }

        if (!bIsAssemblyExe && Type.Length == 0)
        {
            bIsAssemblyExe = Ext.Length == 0 || String_IsEqual(Ext, StrLit("out"), false) || String_IsEqual(Ext, StrLit("exe"), false);
        }

        // expand all build variables
        for each (v, VariablesDB)
        {
            // already expanded
            if (String_IsEqual(v.Name, StrLit("Extension"), false) ||
                String_IsEqual(v.Name, StrLit("Type"), false))
            {
                continue;
            }

            StringLocal(ExpandedVar, 4096);

            const String Exclusions[] =
            {
                StrLit("AssertProgramExists"),
                StrLit("AssertBuildVarExists"),
                StrLit("AssertCmdVarExists"),
                StrLit("AssertEnvVarExists"),
                StrLit("AssertPlatform"),
                StrLit("PreBuildCmd"),
                StrLit("PostBuildCmd"),
                StrLit("Depends"),
                StrLit("RunAssembly"),
            };

            // do not join the above variables into one long string basically, is what this is for
            bool bIsExcludedFromMultiVarDeclarations = false;
            for (u8 i = 0; i < (sizeof Exclusions / sizeof(String)); i++)
            {
                if (String_IsEqual(Exclusions[i], v.Name, false))
                {
                    bIsExcludedFromMultiVarDeclarations = true;
                    break;
                }
            }

            if (!bIsExcludedFromMultiVarDeclarations)
            {
                bool bAlreadyExpanded = GetExpandedVariableValue(v.Name).Length > 0;
                if (bAlreadyExpanded)
                    continue;

                TEMP_SCRATCH(Exp)
                {
                    StringList List = GetVariableValueList(Scratch_Exp.Allocator, v.Name);
                    for each_str_list (List)
                    {
                        if (!ExpandBuildVariable(&ExpandedVar, v.Name, It->String, v.Name, false))
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
                if (!ExpandBuildVariable(&ExpandedVar, v.Name, v.Value, v.Name, false))
                {
                    return 1;
                }
            }

            String_EatSpacesInlineFromEnd(&ExpandedVar);

            FileVariable Expanded;
            Expanded.Name = v.Name;
            Expanded.Value = String_Create(&GExpandedVariablesAllocator, ExpandedVar);

            Array_Add(ExpandedVariablesDB, Expanded);
        }

        Clock_Tick(&BuildFileParseClock);
    }

    // build file variable listing feature. list:all or list:varname

    if (bFoundBuildFile) // can only list variables if we're using a build file
    {
        for (u8 i = 0; i < Arguments.Num; i++)
        {
            const String Arg = Arguments.List[i];

            if (String_StartsWith(Arg, StrLit("list:"), false))
            {
                u32 Colon = 0;
                if (String_IndexOfChar(Arg, ':', &Colon))
                {
                    const String VarToList = StrShiftF(Arg, Colon+1);

                    if (VarToList.Length == 0)
                    {
                        LOG_ERROR("Failed to list build variable. No variable name was given after :");
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

                                ListVariables(String_Null());
                            }
                            else
                            {
                                if (!DoesBuildVarExist(*var))
                                {
                                    LOG_ERROR("Failed to list \"%S\". It does not exist in \"%S\" (within the context of the given build parameters)", *var, GBuildFilePath);
                                    return 1;
                                }

                                ListVariables(*var);
                            }
                        }
                    }

                    return 0;
                }
            }
        }
    }

    String AssemblyName                         = GetExpandedVariableValue(StrLit("Assembly"));
    String Extension                            = GetExpandedVariableValue(StrLit("Extension"));
    String Type                                 = GetExpandedVariableValue(StrLit("Type"));
    String CompilerProgram                      = GetExpandedVariableValue(StrLit("Compiler"));
    String CompilerFlagPrefixSymbol             = StrLit("-");
    const String CompilerFlags                  = GetExpandedVariableValue(StrLit("CompilerFlags"));
    String IncludeFlags                         = GetExpandedVariableValue(StrLit("IncludeFlags"));
    const String Libraries                      = GetExpandedVariableValue(StrLit("Libraries"));
    String LibraryDirectories                   = GetExpandedVariableValue(StrLit("LibraryDirectories"));
    String LinkerFlags                          = GetExpandedVariableValue(StrLit("LinkerFlags"));
    const String Defines                        = GetExpandedVariableValue(StrLit("Defines"));
    const String UnDefines                      = GetExpandedVariableValue(StrLit("UnDefines"));
    const String LinkerDefines                  = GetExpandedVariableValue(StrLit("LinkerDefines"));
    const String AssertPlatforms                = GetExpandedVariableValue(StrLit("AssertPlatform"));
    const String AssertPrograms                 = GetExpandedVariableValue(StrLit("AssertProgramExists"));
    const String AssertEnvVars                  = GetExpandedVariableValue(StrLit("AssertEnvVarExists"));
    const String AssertBuildVars                = GetExpandedVariableValue(StrLit("AssertBuildVarExists"));
    String IncludedSourceFiles                  = GetExpandedVariableValue(StrLit("IncludedSourceFiles"));
    String ExcludedSourceFiles                  = GetExpandedVariableValue(StrLit("ExcludedSourceFiles"));
    const String IncludedSourceDir              = GetExpandedVariableValue(StrLit("IncludedSourceDirectories"));
    const String ExcludedSourceDir              = GetExpandedVariableValue(StrLit("ExcludedSourceDirectories"));
    String OutputToken                          = GetExpandedVariableValue(StrLit("OutputToken"));
    const String MaxConcurrentCompilations      = GetExpandedVariableValue(StrLit("MaxConcurrentCompilations"));
    const String OutsideSourceDirectories       = GetExpandedVariableValue(StrLit("OutsideSourceDirectories"));
    const String MultiThread                    = GetExpandedVariableValue(StrLit("MultiThread"));
    String Icon                                 = GetExpandedVariableValue(StrLit("Icon"));
    const String PostBuildSetting               = GetExpandedVariableValue(StrLit("RunPostBuildOnChange"));
    const String MaxCompilerErrors              = GetExpandedVariableValue(StrLit("MaxCompilerErrors"));

    #if PLATFORM_WINDOWS
    const String TitleName                      = GetExpandedVariableValue(StrLit("TitleName"));
    const String Description                    = GetExpandedVariableValue(StrLit("Description"));
    const String CompanyName                    = GetExpandedVariableValue(StrLit("CompanyName"));
    const String Copyright                      = GetExpandedVariableValue(StrLit("Copyright"));
    #endif
    String Version                              = GetExpandedVariableValue(StrLit("Version"));

    const bool bNoRebuildOnDependencyChange     = String_ToBool(GetExpandedVariableValue(StrLit("NoRebuildOnDependencyChange")));

    if (String_IsValid(MultiThread))
    {
        bool bMultiThread = String_ToBool(MultiThread);
        bShouldWaitPerCompileProcess = !bMultiThread;
    }

    bool bRunPostBuildOnlyWhenWorkWasDone = false;
    if (String_IsValid(PostBuildSetting))
    {
        bRunPostBuildOnlyWhenWorkWasDone = String_ToBool(PostBuildSetting);
    }

    u8 MaxErrorsAllowed = 0; // infinite
    if (String_IsValid(MaxCompilerErrors))
    {
        String_ToU8(MaxCompilerErrors, &MaxErrorsAllowed);
    }

    String_ConvertSlashToPlatformSlash(&LibraryDirectories);
    String_ConvertSlashToPlatformSlash(&IncludeFlags);
    //String_ConvertSlashToPlatformSlash(&LinkerFlags); // keep commented out, theres a reason for it
    String_ConvertSlashToPlatformSlash(&Icon);
    String_ConvertSlashToPlatformSlash(&IncludedSourceFiles);
    String_ConvertSlashToPlatformSlash(&ExcludedSourceFiles);

    if (String_IsValid(Type))
    {
        if (String_IsEqual(Type, StrLit("lib"), false))
        {
            #if PLATFORM_WINDOWS
                Extension = StrLit("dll lib");
            #elif PLATFORM_LINUX || PLATFORM_UNIX
                Extension = StrLit("so a");
            #elif PLATFORM_APPLE
                Extension = StrLit("dylib a");
            #endif
        }
        else if (String_IsEqual(Type, StrLit("static_lib"), false))
        {
            #if PLATFORM_WINDOWS
                Extension = StrLit("lib");
            #elif PLATFORM_LINUX || PLATFORM_UNIX
                Extension = StrLit("a");
            #elif PLATFORM_APPLE
                Extension = StrLit("a");
            #endif
        }
        else if (String_IsEqual(Type, StrLit("dynamic_lib"), false))
        {
            #if PLATFORM_WINDOWS
                Extension = StrLit("dll");
            #elif PLATFORM_LINUX || PLATFORM_UNIX
                Extension = StrLit("so");
            #elif PLATFORM_APPLE
                Extension = StrLit("dylib");
            #endif
        }
        else if (String_IsEqual(Type, StrLit("app"), false) ||
                String_IsEqual(Type, StrLit("exe"), false))
        {
            #if PLATFORM_WINDOWS
                Extension = StrLit("exe");
            #elif PLATFORM_LINUX || PLATFORM_UNIX
                Extension = StrLit("");
            #elif PLATFORM_APPLE
                Extension = StrLit("");
            #endif
        }
    }

    // Extension could have multiple options listed
    // for example: to allow for a .dll and a static lib to be generated. So the first one is always the real extension
    String Extension_Og = Extension;
    {
        u32 Index = 0;
        String_IndexOfFirstWhitespace(Extension, &Index);
        if (Index > 0)
            Extension.Length = Index;
    }

    bool bNoCompilerProgramExplicityGiven = false;

    #if PLATFORM_WINDOWS
    bool bFallbackVersion = false;
    #endif

    // failsafes
    {
        if (!String_IsValid(AssemblyName))
        {
            AssemblyName = StrLit("Untitled");
        }

        if (!String_IsValid(Extension))
        {
            #if PLATFORM_WINDOWS
                Extension = StrLit("exe");
            #elif PLATFORM_LINUX || PLATFORM_UNIX
                Extension = StrLit("");
            #elif PLATFORM_APPLE
                Extension = StrLit("");
            #endif
        }

        if (!String_IsValid(CompilerProgram))
        {
            bNoCompilerProgramExplicityGiven = true;
            CompilerProgram = StrLit("clang");
        }

        if (!String_IsValid(OutputToken))
        {
            OutputToken = StrLit("-o");
        }

        if (!String_IsValid(Version))
        {
            Version = StrLit("1.0.0");

            #if PLATFORM_WINDOWS
            bFallbackVersion = true;
            #endif
        }
    }

    StringLocal(VersionCommas, 64);
    String_Copy(&VersionCommas, Version);
    String_ReplaceCharInline(&VersionCommas, '.', ',');

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
        bool bCompilerProgramFound = false;
        #if PLATFORM_WINDOWS
        bCompilerProgramFound = Platform_FindProgram_Ex(CompilerProgram, StrLit(".exe"), &CompilerPath);
        #else
        bCompilerProgramFound = Platform_FindProgram_Ex(CompilerProgram, String_Null(), &CompilerPath);
        #endif

        // failsafe 1
        if (!bCompilerProgramFound)
        {
            if (bNoCompilerProgramExplicityGiven)
            {
                CompilerProgram = StrLit("gcc");

                #if PLATFORM_WINDOWS
                bCompilerProgramFound = Platform_FindProgram_Ex(CompilerProgram, StrLit(".exe"), &CompilerPath);
                #else
                bCompilerProgramFound = Platform_FindProgram_Ex(CompilerProgram, String_Null(), &CompilerPath);
                #endif

                if (!bCompilerProgramFound)
                {
                    CompilerProgram = StrLit("x86_64-w64-mingw32-gcc");
                    if (!bCompilerProgramFound)
                    {
                        #if PLATFORM_WINDOWS
                        bCompilerProgramFound = Platform_FindProgram_Ex(CompilerProgram, StrLit(".exe"), &CompilerPath);
                        #else
                        bCompilerProgramFound = Platform_FindProgram_Ex(CompilerProgram, String_Null(), &CompilerPath);
                        #endif
                    }
                }
            }
        }

        // failsafe 2
        if (!bCompilerProgramFound)
        {
            if (bNoCompilerProgramExplicityGiven)
            {
                CompilerProgram = StrLit("clang++");

                #if PLATFORM_WINDOWS
                bCompilerProgramFound = Platform_FindProgram_Ex(CompilerProgram, StrLit(".exe"), &CompilerPath);
                #else
                bCompilerProgramFound = Platform_FindProgram_Ex(CompilerProgram, String_Null(), &CompilerPath);
                #endif
            }
        }

        // failsafe 3
        if (!bCompilerProgramFound)
        {
            if (bNoCompilerProgramExplicityGiven)
            {
                CompilerProgram = StrLit("g++");

                #if PLATFORM_WINDOWS
                bCompilerProgramFound = Platform_FindProgram_Ex(CompilerProgram, StrLit(".exe"), &CompilerPath);
                #else
                bCompilerProgramFound = Platform_FindProgram_Ex(CompilerProgram, String_Null(), &CompilerPath);
                #endif
            }
        }

        #if PLATFORM_WINDOWS
        // failsafe 4
        if (!bCompilerProgramFound)
        {
            if (bNoCompilerProgramExplicityGiven)
            {
                CompilerProgram = StrLit("cl");
                bCompilerProgramFound = Platform_FindProgram_Ex(CompilerProgram, StrLit(".exe"), &CompilerPath);
            }
        }
        #endif

        if (!bCompilerProgramFound)
        {
            if (bNoCompilerProgramExplicityGiven)
            {
                #if PLATFORM_WINDOWS
                LOG_ERROR(
                    "You don't seem to have either \"clang\", \"gcc\" nor \"cl (msvc)\" installed on your machine."
                    " Install one of these compilers before using RiftBuild, as we require a working"
                    " compiler program to function properly. Aborting build...");
                #else
                LOG_ERROR(
                    "You don't seem to have either \"clang\" nor \"gcc\" installed on your machine."
                    " Install one of these compilers before using RiftBuild, as we require a working"
                    " compiler program to function properly. Aborting build...");
                #endif
                return 1;
            }

            #ifndef HOOD
            LOG_ERROR(
                "Compiler program \"%S\" does not exist. Make sure that you have the Visual Studio build tools installed"
                " and that you run riftbuild from a different terminal application named"
                " \"x64 (or x86) Native Tools Command Prompt for VS\"."
                " This can be found through the windows search. Aborting build...", CompilerProgram);
            #else
            LOG_ERROR(
                "yo dat compiler program \"%S\" don exist cuh."
                " need to be installed and set in da path ma nigga", CompilerProgram);
            #endif

            return 1;
        }
    }

    if (String_IsEqual(CompilerProgram, StrLit("cl"), false) ||
        String_IsEqual(CompilerProgram, StrLit("msvc"), false) || // todo: detect msvc and chang "Compiler" value to "cl"
        String_IsEqual(CompilerProgram, StrLit("clang"), false) ||
        String_IsEqual(CompilerProgram, StrLit("clang++"), false) ||
        String_IsEqual(CompilerProgram, StrLit("gcc"), false) ||
        String_IsEqual(CompilerProgram, StrLit("x86_64-w64-mingw32-gcc"), false) ||
        String_IsEqual(CompilerProgram, StrLit("g++"), false))
    {
        if (String_IsEqual(CompilerProgram, StrLit("cl"), false) ||
            String_IsEqual(CompilerProgram, StrLit("msvc"), false))
        {
            CompilerFlagPrefixSymbol = StrLit("/");
            GCompiler = Compiler_MSVC;
        }
    }
    else
    {
        GCompiler = Compiler_Clang;
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
        StringArray ProgramsArray  = String_ParseIntoArray(Scratch_Assert.Allocator, AssertPrograms, ' ', 0, 128);
        StringArray EnvVarsArray   = String_ParseIntoArray(Scratch_Assert.Allocator, AssertEnvVars, ' ', 0, 128);
        StringArray BuildVarsArray = String_ParseIntoArray(Scratch_Assert.Allocator, AssertBuildVars, ' ', 0, 128);
        StringArray PlatformsArray = String_ParseIntoArray(Scratch_Assert.Allocator, AssertPlatforms, ' ', 0, 128);

        for each_str (S, ProgramsArray)
        {
            String Trimmed = String_EatSpaces(*S);

            #if PLATFORM_WINDOWS
            bool bHasDot = String_IndexOfLastChar(Trimmed, '.', NULL);
            bool bFound = Platform_FindProgram(Trimmed, bHasDot ? StrLit("") : StrLit(".exe"));
            #else
            bool bFound = Platform_FindProgram(Trimmed, StrLit(""));
            #endif

            if (!bFound)
            {
                #ifndef HOOD
                LOG_ERROR("Build assertion failure. Program \"%S\" does not exist. Make sure that \"%S\" is installed and that its directory has been set in the PATH environment variable. Aborting build...", Trimmed, Trimmed);
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
                LOG_ERROR("Build assertion failure. Environment variable \"%S\" does not exist. Aborting build...", Trimmed);
                #else
                LOG_ERROR("yo da environment var \"%S\" don exist cuh. need to be setup n' shit ma nigga", CompilerProgram);
                #endif
                return 1;
            }
        }

        for each_str (S, BuildVarsArray)
        {
            String Trimmed = String_EatSpaces(*S);

            bool bFound = DoesBuildVarExist(Trimmed);

            if (!bFound)
            {
                #ifndef HOOD
                LOG_ERROR("Build assertion failure. Build variable \"%S\" does not exist. Aborting build...", Trimmed);
                #else
                LOG_ERROR("yo da build var \"%S\" don exist cuh. dat shit not there nigga", CompilerProgram);
                #endif
                return 1;
            }
        }

        StringLocal(PlatformsLogString, 128);
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

        for each_str (S, PlatformsArray)
        {
            String Trimmed = String_EatSpaces(*S);

            bool bMatch = String_IsEqual(Trimmed, StrLit(PLATFORM_STRING), false);
            if (bMatch)
                break;

            StringArray AdditionalPlatforms = String_ParseIntoArray(Scratch_Assert.Allocator, StrLit(PLATFORM_STRING), ' ', 0, 128);
            for each_str (p, AdditionalPlatforms)
            {
                bMatch = String_IsEqual(Trimmed, *p, false);
                if (bMatch)
                    break;
            }

            if (bMatch)
                break;

            if (!bMatch)
            {
                #ifndef HOOD
                LOG_ERROR("Build assertion failure. %S can only be used on %S. Aborting build...", GBuildFileName, PlatformsLogString);
                #else
                LOG_ERROR("yo u cant build on dis platform nigga", CompilerProgram);
                #endif
                return 1;
            }
        }
    }

    /// TODO: linux
    #if PLATFORM_WINDOWS
    if (bIsAssemblyExe)
    {
        if (Platform_IsProgramRunning(AssemblyName))
        {
            LOG_ERROR("Assembly \"%S\" is currently running. Close all instances of \"%S.exe\" to continue with the build process. Aborting build...", AssemblyName, AssemblyName);
            return 1;
        }
    }
    #endif

    #if PLATFORM_WINDOWS
    const bool bHasRiftBuildInPath = Platform_FindProgram(StrLit("RiftBuild"), StrLit(".exe")); // todo: simplify api
    #else
    const bool bHasRiftBuildInPath = Platform_FindProgram(StrLit("riftbuild"), StrLit(""));
    #endif

    bool bRanAnyDependencies = false;
    // run build depenencies
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
            if (String_IsValid(GCameFromBuildFile))
            {
                u32 Dot = 0;
                String_IndexOfLastChar(GCameFromBuildFile, '.', &Dot);
                if (String_IsEqual(BuildFile, StrSlice(GCameFromBuildFile.Data, Dot), false))
                {
                    LOG_ERROR("Circular build dependency. We came from \"%S\" but \"%S\" is trying to build \"%S\", which is circular and doesn't make sense", GCameFromBuildFile, GBuildFileName, GCameFromBuildFile);
                    return 1;
                }
            }

            StringLocal(WorkingPath, MAX_PATH_LENGTH);

            if (SpaceIndex > 0)
            {
                String CustomPath = String_Null();

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

                // is custom path relative?
                #if PLATFORM_WINDOWS
                bool bDriveSymbol = String_IndexOfChar(CustomPath, ':', NULL);
                #else
                bool bDriveSymbol = CustomPath.Data[0] == '/';
                #endif

                bool bRelative = !bDriveSymbol;

                if (bRelative)
                {
                    String_BuildPath(&WorkingPath, GRootPath, CustomPath);
                    Filesystem_ConvertRelativeToAbsolutePath(&WorkingPath);
                }
                else
                {
                    String_Copy(&WorkingPath, CustomPath);
                }
            }

            if (!bHasRiftBuildInPath)
            {
                #if PLATFORM_WINDOWS
                String Program = StrLit("RiftBuild.exe");
                #else
                String Program = StrLit("riftbuild");
                #endif

                LOG_ERROR("\"%S\" is not in the PATH environment variable. Cannot continue from here. Aborting build...", Program);
                return 1;
            }

            String_EatPathSeparatorsInlineFromEnd(&WorkingPath);

            // todo: factor into a function
            StringLocal(CmdLine, 8192);

            #if PLATFORM_WINDOWS
            String_Append(&CmdLine, StrLit("cmd.exe /c RiftBuild "));
            #else
            String_Append(&CmdLine, StrLit("riftbuild "));
            #endif

            String_AppendChar(&CmdLine, '"');
            String_Append(&CmdLine, WorkingPath);
            String_AppendChar(&CmdLine, '"');
            String_AppendSpace(&CmdLine);
            String_Append(&CmdLine, BuildFile);
            String_Append(&CmdLine, StrLit(".build"));

            if (SpecifiedParams.Length > 0)
            {
                String_AppendSpace(&CmdLine);
                String_Append(&CmdLine, SpecifiedParams);
            }

            if (bIsRebuild)
            {
                String_Append(&CmdLine, StrLit(" rebuild"));
            }
            else if (bIsClean)
            {
                String_Append(&CmdLine, StrLit(" clean"));
            }

            // pass in the build file we came from (if we have one)
            if (bFoundBuildFile)
            {
                String_AppendSpace(&CmdLine);
                String_AppendChar(&CmdLine, '_');
                String_Append(&CmdLine, GBuildFileName);
            }

            LOG("Depend -> %S.build", BuildFile);
            bRanAnyDependencies = true;

            PlatformHandle Handle = Platform_RunCommand(CmdLine, WorkingPath);
            const u32 ExitCode = Platform_WaitForProcessAndGetExitCode(Handle);
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
        }
    }

    if (bRanAnyDependencies)
    {
        LOG_LINE_BREAK();
        LOG("[All build dependencies complete. Continuing with %S]\n", GBuildFileName);
    }

    TArray(String) PreBuildCmds = Array_Reserve(String, 4);
    TArray(String) PostBuildCmds = Array_Reserve(String, 4);

    // run pre build commands (if specified)
    for each (Var, ExpandedVariablesDB)
    {
        if (String_IsEqual(Var.Name, StrLit("PreBuildCmd"), false))
        {
            Array_Add(PreBuildCmds, Var.Value);
        }
    }

    if (Array_Num(PreBuildCmds) > 0)
    {
        #ifndef HOOD
        LOG("Running pre build commands...");
        #else
        LOG("cool mang, gonna run some pre build cmds...");
        #endif

        // run pre build commands (if specified)
        for each (Cmd, PreBuildCmds)
        {
            if (!String_IsValid(Cmd))
                continue;
            
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

            PlatformHandle Handle = Platform_RunCommand(CmdLine, GRootPath);
            if (!Platform_IsValidHandle(Handle))
            {
                return 1;
            }

            const u32 ExitCode = Platform_WaitForProcessAndGetExitCode(Handle);
            if (ExitCode != 0)
            {
                #ifndef HOOD
                LOG_ERROR("Pre-build command exited with a failure result: %u", ExitCode);
                #else
                LOG_ERROR("brah wtf, gon have to stop you there nigga. da command we jus run fuck'n failed on me nigga");
                #endif
                return 1;
            }
        }

        LOG_LINE_BREAK();
    }

    String SourceDirectory          = String_EatPathSeparatorsFromEnd(GetExpandedVariableValue(StrLit("SourceDirectory")));
    String BuildDirectory           = String_EatPathSeparatorsFromEnd(GetExpandedVariableValue(StrLit("BuildDirectory")));
    String IntermediateDirectory    = String_EatPathSeparatorsFromEnd(GetExpandedVariableValue(StrLit("IntermediateDirectory")));

    String_ConvertSlashToPlatformSlash(&SourceDirectory);

    if (!String_IsValid(IntermediateDirectory))
    {
        IntermediateDirectory = StrLit("Intermediate");
    }

    String_ConvertSlashToPlatformSlash(&IntermediateDirectory);

    StringLocal(IntermediateBaseDirectory, MAX_PATH_LENGTH);
    String_BuildPath(&IntermediateBaseDirectory, GRootPath, IntermediateDirectory);
    String_EatPathSeparatorsInlineFromEnd(&IntermediateBaseDirectory);
    String_AppendPathSeparator(&IntermediateBaseDirectory);
    String_ConvertSlashToPlatformSlash(&IntermediateBaseDirectory);
    GIntermediateBaseDirectory = IntermediateBaseDirectory;

    if (!String_IsValid(BuildDirectory))
    {
        BuildDirectory = StrLit("Build");
    }

    String_ConvertSlashToPlatformSlash(&BuildDirectory);

    LinearAllocator_Create(Kibibytes(512), NULL, &GSourceFilePathAllocator);
    GSourceFiles = Array_Reserve(SourceFileData, 32);
    GHeaderFiles = Array_Reserve(SourceFileData, 32);
    TArray(SourceFileData*) SourceFilesFiltered = Array_Reserve(SourceFileData*, 32);

    StringLocal(SourceDir, MAX_PATH_LENGTH);
    String_BuildPath(&SourceDir, GRootPath, SourceDirectory);

    Filesystem_IterateDirectory(SourceDir, SourceFileDirectoryIterator, true);

    // also include outside source directories if specified
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

    TEMP_SCRATCH(Filter)
    {
        StringArray WhitelistArray = String_ParseIntoArray(Scratch_Filter.Allocator, IncludedSourceFiles, ' ', 0, 128);
        StringArray BlacklistArray = String_ParseIntoArray(Scratch_Filter.Allocator, ExcludedSourceFiles, ' ', 0, 128);

        StringArray WhitelistDirArray = String_ParseIntoArray(Scratch_Filter.Allocator, IncludedSourceDir, ' ', 0, 128);
        StringArray BlacklistDirArray = String_ParseIntoArray(Scratch_Filter.Allocator, ExcludedSourceDir, ' ', 0, 128);

        for each (SFile, GSourceFiles)
        {
            String TrimmedFileName = String_Null();
            String TrimmedDirName = String_Null();
            u32 SlashIndex = 0;
            if (String_IndexOfLastPathSlash(SFile.RelativePath, &SlashIndex))
            {
                u32 Len = SFile.RelativePath.Length - SlashIndex;
                TrimmedFileName = StrCompC(SFile.RelativePath.Data + (SlashIndex+1), Len-1, Len);
                TrimmedDirName = StrSlice(SFile.RelativePath.Data, SlashIndex);
            }
            else
            {
                TrimmedFileName = SFile.RelativePath;
            }

            bool bIsBlacklisted = false;
            for each_str (File, BlacklistArray)
            {
                u32 Index = 0;
                if (String_IndexOfLastChar(*File, '*', &Index))
                {
                    String Left = StrSlice(File->Data, Index);
                    String Right = StrShiftF(*File, Index+1);
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

                if (String_IsEqual(*File, TrimmedFileName, true))
                {
                    bIsBlacklisted = true;
                    break;
                }

                if (String_IsEqual(*File, SFile.RelativePath, true))
                {
                    bIsBlacklisted = true;
                    break;
                }
            }

            StringLocal(DirPath, MAX_PATH_LENGTH);
            String_BuildPath(&DirPath, GRootPath, SourceDirectory, TrimmedDirName);

            for each_str (Dir, BlacklistDirArray)
            {
                if (String_IsEqual(*Dir, StrLit("*"), false))
                {
                    bIsBlacklisted = true;
                    break;
                }

                StringLocal(TestPath, MAX_PATH_LENGTH);
                String_BuildPath(&TestPath, GRootPath, SourceDirectory, *Dir);
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
                            if (Filesystem_ArePathsCommon(DirPath, SFile.FullPath))
                            {
                                bIsBlacklisted = true;
                                break;
                            }
                        }
                    }
                }

                if (Filesystem_ArePathsCommon(TestPath, SFile.FullPath))
                {
                    bIsBlacklisted = true;
                    break;
                }

                // also look in the outside directories
                if (String_IsValid(OutsideSourceDirectories))
                {
                    StringArray Dirs = String_ParseIntoArray(Scratch_Filter.Allocator, OutsideSourceDirectories, ' ', 0, 128);
                    for each_str (DirO, Dirs)
                    {
                        StringLocal(DirCopy, MAX_PATH_LENGTH);
                        String_Copy(&DirCopy, *DirO);
                        String_EatPathSeparatorsInlineFromEnd(&DirCopy);
                        String_ConvertSlashToPlatformSlash(&DirCopy);

                        if (Filesystem_ArePathsCommon(DirCopy, SFile.FullPath))
                        {
                            bIsBlacklisted = true;
                            break;
                        }
                    }
                }
            }

            if (bIsBlacklisted)
                continue;

            bool bIsAllowed = WhitelistArray.Num == 0 && WhitelistDirArray.Num == 0;

            if (WhitelistArray.Num > 0)
            {
                for each_str (File, WhitelistArray)
                {
                    u32 Index = 0;
                    if (String_IndexOfLastChar(*File, '*', &Index))
                    {
                        String Left = StrSlice(File->Data, Index);
                        String Right = StrShiftF(*File, Index+1);
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

                    if (String_IsEqual(*File, TrimmedFileName, true))
                    {
                        bIsAllowed = true;
                        break;
                    }

                    if (String_IsEqual(*File, SFile.RelativePath, true))
                    {
                        bIsAllowed = true;
                        break;
                    }
                }
            }

            if (WhitelistDirArray.Num > 0)
            {
                for each_str (Dir, WhitelistDirArray)
                {
                    StringLocal(TestPath, MAX_PATH_LENGTH);
                    String_BuildPath(&TestPath, GRootPath, SourceDirectory, *Dir);
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
                                if (Filesystem_ArePathsCommon(DirPath, SFile.FullPath))
                                {
                                    bIsAllowed = true;
                                    break;
                                }
                            }
                        }
                    }

                    if (Filesystem_ArePathsCommon(TestPath, SFile.FullPath))
                    {
                        bIsAllowed = true;
                        break;
                    }

                    // also look in the outside directories
                    if (String_IsValid(OutsideSourceDirectories))
                    {
                        StringArray Dirs = String_ParseIntoArray(Scratch_Filter.Allocator, OutsideSourceDirectories, ' ', 0, 128);
                        for each_str (DirO, Dirs)
                        {
                            StringLocal(DirCopy, MAX_PATH_LENGTH);
                            String_Copy(&DirCopy, *DirO);
                            String_EatPathSeparatorsInlineFromEnd(&DirCopy);
                            String_ConvertSlashToPlatformSlash(&DirCopy);

                            if (Filesystem_ArePathsCommon(DirCopy, SFile.FullPath))
                            {
                                bIsAllowed = true;
                                break;
                            }
                        }
                    }
                }
            }

            if (bIsAllowed)
            {
                Array_Add(SourceFilesFiltered, SFile_);
            }
        }
    }

    // use the first source file as the assembly name (if none provided or if "untitled" was set)
    if (Array_Num(GSourceFiles) == 1)
    {
        if (!String_IsValid(AssemblyName) ||
            String_IsEqual(AssemblyName, StrLit("Untitled"), false))
        {
            String TrimmedFileName = String_Null();

            SourceFileData SFile = GSourceFiles[0];

            // extract the name only, remove the path prefixes
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

            // todo: left chop function
            u32 DotIndex = 0;
            if (String_IndexOfLastChar(TrimmedFileName, '.', &DotIndex))
            {
                TrimmedFileName.Length -= TrimmedFileName.Length-DotIndex;
            }

            AssemblyName = TrimmedFileName;
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

    // force a rebuild if the .build file has been modified
    if (!bIsRebuild && !bIsClean && bFoundBuildFile)
    {
        // build the full source directory path
        StringLocal(AssemblyPath, MAX_PATH_LENGTH);
        String_BuildPath(&AssemblyPath, GRootPath, BuildDirectory, AssemblyName);
        if (Extension.Length > 0)
        {
            String_Append(&AssemblyPath, StrLit("."));
            String_Append(&AssemblyPath, Extension);
        }

        if (!Filesystem_DoesFileExist(AssemblyPath))
        {
            LOG("Assembly file \"%S\" does not exist. Forcing rebuild...\n", AssemblyPath);
            bIsRebuild = true;
        }

        if (!bIsRebuild)
        {
            u64 AssemblyFileTime = Filesystem_GetLastWriteTime(AssemblyPath);
            u64 BuildFileTime = Filesystem_GetLastWriteTimeH(&BuildFileHandle);

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
    if (!bIsRebuild && bFoundBuildFile && Array_Num(IncludeFiles) > 0)
    {
        StringLocal(AssemblyPath, MAX_PATH_LENGTH);
        String_BuildPath(&AssemblyPath, GRootPath, BuildDirectory, AssemblyName);
        if (Extension.Length > 0)
        {
            String_Append(&AssemblyPath, StrLit("."));
            String_Append(&AssemblyPath, Extension);
        }

        u64 AssemblyFileTime = Filesystem_GetLastWriteTime(AssemblyPath);

        if (AssemblyFileTime > 0)
        {
            for each (Include, IncludeFiles)
            {
                u64 IncludeFileTime = Filesystem_GetLastWriteTimeH(&Include);

                if (IncludeFileTime >= AssemblyFileTime)
                {
                    bIsRebuild = true;

                    #ifndef HOOD
                    LOG("Build variables file \"%S\" has been modified since last build. Forcing rebuild...", StrView(Include.Path));
                    #else
                    LOG("dawwwg, dis build vars file \"%S\" has been modified since last build. gon force a rebuild...", StrView(Include.Path));
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
    if (!bIsRebuild)
    {
        StringLocal(FullBuildDirectory, MAX_PATH_LENGTH);
        String_BuildPath(&FullBuildDirectory, GRootPath, BuildDirectory);

        if (!Filesystem_DoesDirectoryExist(FullBuildDirectory) ||
            !Filesystem_DoesDirectoryExist(IntermediateBaseDirectory))
        {
            bIsRebuild = true;
        }
    }

    // force a rebuild if the cmd line given to this program was different than the previous run
    /// TODO: fix this, idk what to do
    if (!bIsRebuild && !String_IsValid(GCameFromBuildFile))
    {
        StringLocal(OutputCmdLineFile, MAX_PATH_LENGTH);
        StringLocal(FileName, 128);
        String_Append(&FileName, GBuildFileName);
        String_Append(&FileName, StrLit("_cmdline.txt"));
        String_BuildPath(&OutputCmdLineFile, IntermediateBaseDirectory, FileName);
        bool bFileExists = Filesystem_DoesFileExist(OutputCmdLineFile);
        if ((!bFileExists && RiftCmdLine.Length > 0) ||
            bFileExists)
        {
            FileHandle h = {0};
            Filesystem_Open(OutputCmdLineFile, FileMode_Read, &h);
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
    if (!bIsRebuild)
    {
        StringLocal(AssemblyPath, MAX_PATH_LENGTH);
        String_BuildPath(&AssemblyPath, GRootPath, BuildDirectory, AssemblyName);
        if (Extension.Length > 0)
        {
            String_Append(&AssemblyPath, StrLit("."));
            String_Append(&AssemblyPath, Extension);
        }

        u64 AssemblyFileTime = Filesystem_GetLastWriteTime(AssemblyPath);

        if (AssemblyFileTime > 0)
        {
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
        String_BuildPath(&BuildDirectoryPath, GRootPath, BuildDirectory);
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
        String_BuildPath(&IntermediateDirectoryPath, IntermediateBaseDirectory, SourceDirectory);

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
            Filesystem_Close(&BuildFileHandle);

            return 0;
        }
    }

    // write the cmd line of this program to a file in the intermediate directory for comparison between subsequent runs
    if (Array_Num(GSourceFiles) > 0) // only if we have something...
    {
        StringLocal(OutputCmdLineFile, MAX_PATH_LENGTH);
        StringLocal(FileName, 128);
        String_Append(&FileName, GBuildFileName);
        String_Append(&FileName, StrLit("_cmdline.txt"));
        String_BuildPath(&OutputCmdLineFile, IntermediateBaseDirectory, FileName);

        FileHandle h = {0};
        Filesystem_Open(OutputCmdLineFile, FileMode_Write, &h);
        Filesystem_Write(&h, RiftCmdLine.Length, RiftCmdLine.Data, NULL);
        Filesystem_Close(&h);
    }

    if (bFoundBuildFile)
    {
        StringLocal(OutputDebugFile, MAX_PATH_LENGTH);
        StringLocal(GenFileName, 256);
        String_Append(&GenFileName, Str(GBuildFileName));
        String_Append(&GenFileName, StrLit("_generated.txt"));
        String_BuildPath(&OutputDebugFile, IntermediateBaseDirectory, GenFileName);

        FileHandle f = {0};
        bool bSuccess = Filesystem_Open(OutputDebugFile, FileMode_Write, &f);

        if (bSuccess)
        {
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

    // log the custom build messages
    if (Array_Num(Messages) > 0)
    {
        for each (msg, Messages)
        {
            LOG("%S", msg);
        }

        LOG_LINE_BREAK();
    }

    if (bFoundBuildFile)
    {
        String Mode = GetCmdOptionValue(StrLit("mode"));

        if (!String_IsValid(Mode))
            LOG("Build Configuration: (default)");
        else
            LOG("Build Configuration: (%S)", Mode);

            if (Extension.Length > 0)
                LOG("    Assembly:            %S.%S", AssemblyName, Extension);
            else
                LOG("    Assembly:            %S", AssemblyName);

            LOG("    Compiler:            %S -> \"%S\"", CompilerProgram, CompilerPath);

        TEMP_SCRATCH(Log)
        {
            if (CompilerFlags.Length > 0)
            {
                StringList List = GetVariableValueList(Scratch_Log.Allocator, StrLit("CompilerFlags"));
                LOG_INLINE("    Compiler Flags:     ");
                for each_str_list (List)
                {
                    if (String_IsValid(It->String))
                    {
                        LOG_INLINE(" %S", It->String);
                    }
                }
                LOG_LINE_BREAK();
            }
            if (IncludeFlags.Length > 0)
            {
                StringList List = GetVariableValueList(Scratch_Log.Allocator, StrLit("IncludeFlags"));
                LOG_INLINE("    Include Flags:      ");
                for each_str_list (List)
                {
                    if (String_IsValid(It->String))
                    {
                        LOG_INLINE(" %S", It->String);
                    }
                }
                LOG_LINE_BREAK();
            }
            if (LinkerFlags.Length > 0)
            {
                StringList List = GetVariableValueList(Scratch_Log.Allocator, StrLit("LinkerFlags"));
                LOG_INLINE("    Linker Flags:       ");
                for each_str_list (List)
                {
                    if (String_IsValid(It->String))
                    {
                        LOG_INLINE(" %S", It->String);
                    }
                }
                LOG_LINE_BREAK();
            }
            if (Libraries.Length > 0)
            {
                StringList List = GetVariableValueList(Scratch_Log.Allocator, StrLit("Libraries"));
                LOG_INLINE("    Libraries:          ");
                for each_str_list (List)
                {
                    if (String_IsValid(It->String))
                    {
                        LOG_INLINE(" %S", It->String);
                    }
                }
                LOG_LINE_BREAK();
            }
            if (LibraryDirectories.Length > 0)
            {
                StringList List = GetVariableValueList(Scratch_Log.Allocator, StrLit("LibraryDirectories"));
                LOG_INLINE("    Library Directories:");
                for each_str_list (List)
                {
                    if (String_IsValid(It->String))
                    {
                        LOG_INLINE(" %S", It->String);
                    }
                }
                LOG_LINE_BREAK();
            }
            if (Defines.Length > 0)
            {
                StringList List = GetVariableValueList(Scratch_Log.Allocator, StrLit("Defines"));
                LOG_INLINE("    Defines:            ");
                for each_str_list (List)
                {
                    if (String_IsValid(It->String))
                    {
                        LOG_INLINE(" %S", It->String);
                    }
                }
                LOG_LINE_BREAK();
            }
            if (UnDefines.Length > 0)
            {
                StringList List = GetVariableValueList(Scratch_Log.Allocator, StrLit("UnDefines"));
                LOG_INLINE("    UnDefines:          ");
                for each_str_list (List)
                {
                    if (String_IsValid(It->String))
                    {
                        LOG_INLINE(" %S", It->String);
                    }
                }
                LOG_LINE_BREAK();
            }
            if (LinkerDefines.Length > 0)
            {
                StringList List = GetVariableValueList(Scratch_Log.Allocator, StrLit("LinkerDefines"));
                LOG_INLINE("    Linker Defines:    ");
                for each_str_list (List)
                {
                    if (String_IsValid(It->String))
                    {
                        LOG_INLINE(" %S", It->String);
                    }
                }
                LOG_LINE_BREAK();
            }
        }

        LOG_LINE_BREAK();
    }

    const String ExpandedCompilerFlags = GetExpandedVariableValue(StrLit("CompilerFlags"));
    const String ExpandedLinkerFlags   = GetExpandedVariableValue(StrLit("LinkerFlags"));

    String ExpandedOutputToken = GetExpandedVariableValue(StrLit("OutputToken"));
    if (!String_IsValid(ExpandedOutputToken))
    {
        ExpandedOutputToken = OutputToken;
    }

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

    if (ExpandedCompilerFlags.Length > 0)
        LOG("Expanded Compiler Flags: %S", ExpandedCompilerFlags);
    if (ExpandedIncludeFlags.Length > 0)
        LOG("Expanded Include  Flags: %S", ExpandedIncludeFlags);
    if (LinkerFlags.Length > 0)
        LOG("Expanded Linker   Flags: %S", ExpandedLinkerFlags);
    if (ExpandedLibraries.Length > 0)
        LOG("Expanded Library  Flags: %S", ExpandedLibraries);
    if (ExpandedLibraryDirectories.Length > 0)
        LOG("Expanded Library  Paths: %S", ExpandedLibraryDirectories);
    if (ExpandedDefineFlags.Length > 0)
        LOG("Expanded Define   Flags: %S", ExpandedDefineFlags);
    if (ExpandedUnDefineFlags.Length > 0)
        LOG("Expanded UnDefine Flags: %S", ExpandedUnDefineFlags);
    if (ExpandedLinkerDefineFlags.Length > 0)
        LOG("Expanded Linker Defines: %S", ExpandedLinkerDefineFlags);

    if (ExpandedCompilerFlags.Length > 0 ||
        ExpandedIncludeFlags.Length > 0 ||
        ExpandedLinkerFlags.Length > 0 ||
        ExpandedLibraries.Length > 0 ||
        ExpandedLibraryDirectories.Length > 0 ||
        ExpandedDefineFlags.Length > 0 ||
        ExpandedUnDefineFlags.Length > 0 ||
        ExpandedLinkerDefineFlags.Length > 0)
    {
        LOG_LINE_BREAK();
    }

    if (Array_Num(SourceFilesFiltered) > 0)
    {
        if (Extension.Length == 0)
        {
            #ifndef HOOD
            LOG("Building %S\n", AssemblyName);
            #else
            LOG("build'n %S\n", AssemblyName);
            #endif
        }
        else
        {
            #ifndef HOOD
            LOG("Building %S.%S\n", AssemblyName, Extension);
            #else
            LOG("build'n %S.%S\n", AssemblyName, Extension);
            #endif
        }
    }

    // compile executable icon just before we link (if specified)
    // note: only works on windows atm
    StringLocal(IconResFilePath, MAX_PATH_LENGTH);
    StringLocal(VersionResFilePath, MAX_PATH_LENGTH);

    /// TODO: check if rc program exists first, if it doesn't no big deal, log a warning and carry on, dont abort the build

    #if PLATFORM_WINDOWS
    if (Icon.Length > 0)
    {
        String IconProgram = StrLit("llvm-rc");

        if (Platform_FindProgram(IconProgram, StrLit(".exe")))
        {
            String IconName = Icon;

            u32 LastSlashIndex = 0;
            if (String_IndexOfLastPathSlash(Icon, &LastSlashIndex))
            {
                IconName = StrShiftF(Icon, LastSlashIndex+1);
                String_Copy(&GIconFilePath, Icon);
            }
            else
            {
                Filesystem_IterateDirectory(GRootPath, IconFileDirectoryIterator, true);
            }

            String_IndexOfLastPathSlash(GIconFilePath, &LastSlashIndex);

            StringLocal(RcFilePath, MAX_PATH_LENGTH);
            String BasePath = StrSlice(GIconFilePath.Data, LastSlashIndex);
            String RcFile = StrLit("icon.rc");
            String ResFile = StrLit("icon.res");
            String_BuildPath(&RcFilePath, BasePath, RcFile);

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

            StringLocal(CmdLine, 1024);
            StringLocal(ResPath, MAX_PATH_LENGTH);
            String_BuildPath(&ResPath, BasePath, ResFile);
            String_Append(&IconResFilePath, StrLit("\""));
            String_Append(&IconResFilePath, ResPath);
            String_Append(&IconResFilePath, StrLit("\""));
            String_BuildSeparator(&CmdLine, ' ', IconProgram, RcFilePath);
            LOG("Building icon \"%S\"", GIconFilePath);
            LOG("    %S\n", CmdLine);
            PlatformHandle h = Platform_RunCommand(CmdLine, GRootPath);
            u32 ExitCode = Platform_WaitForProcessAndGetExitCode(h);
            if (ExitCode != 0)
            {
                LOG("Failed to build icon \"%S\" for %S.%S. Aborting build...", GIconFilePath, AssemblyName, Extension);
                return 1;
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
                Filesystem_IterateDirectory(GRootPath, ResourceFileDirectoryIterator, true);
            }

            String_IndexOfLastPathSlash(GResourceFilePath, &LastSlashIndex);

            StringLocal(CmdLine, 1024);
            String_BuildSeparator(&CmdLine, ' ', ResourceProgram, GResourceFilePath);
            LOG("Building resource \"%S\"", GResourceFilePath);
            LOG("    %S\n", CmdLine);
            PlatformHandle h = Platform_RunCommand(CmdLine, GRootPath);
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
        StringLocal(VersionRCPath, MAX_PATH_LENGTH);
        const String VersionRCName = StrLit("_version.rc");
        const String VersionResName = StrLit("_version.res");
        String_Append(&VersionRCPath, IntermediateBaseDirectory);
        String_Append(&VersionRCPath, GBuildFileName);
        String_Append(&VersionRCPath, VersionRCName);

        FileHandle VersionRCFile = {0};
        Filesystem_Open(VersionRCPath, FileMode_Write, &VersionRCFile);

        StringLocal(AssemblyWithExt, 256);
        String_Append(&AssemblyWithExt, AssemblyName);
        String_Append(&AssemblyWithExt, StrLit("."));
        String_Append(&AssemblyWithExt, Extension);

        String_Append(&VersionResFilePath, StrLit("\""));
        String_Append(&VersionResFilePath, IntermediateBaseDirectory);
        String_Append(&VersionResFilePath, GBuildFileName);
        String_Append(&VersionResFilePath, VersionResName);
        String_Append(&VersionResFilePath, StrLit("\""));

        String FileType = StrLit("UNKNOWN");

        if (String_IsEqual(Extension, StrLit("exe"), false))
            FileType = StrLit("APP");
        else if (String_IsEqual(Extension, StrLit("dll"), false))
            FileType = StrLit("DLL");
        else if (String_IsEqual(Extension, StrLit("lib"), false))
            FileType = StrLit("STATIC_LIB");

        StringLocal(FileData, 8192);
        String_Format(&FileData, StrLit("#include <winresrc.h>\n\n"

                                        "VS_VERSION_INFO  VERSIONINFO\n"
                                        "FILEVERSION      %S\n"
                                        "PRODUCTVERSION   %S\n"
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
        const String RCProgram = StrLit("llvm-rc ");
        String_Append(&CmdLine, RCProgram);
        String_AppendChar(&CmdLine, '"');
        String_Append(&CmdLine, VersionRCPath);
        String_AppendChar(&CmdLine, '"');

        LOG("Compiling resource file \"%S\"", VersionRCPath);
        LOG("    %S\n", CmdLine);
        PlatformHandle h = Platform_RunCommand(CmdLine, GRootPath);
        u32 ExitCode = Platform_WaitForProcessAndGetExitCode(h);
        if (ExitCode != 0)
        {
            LOG("Failed to build resource file \"%S\" for %S.%S. Aborting build...", VersionRCPath, AssemblyName, Extension);
            return 1;
        }
    }
    #endif


    // Compile .c files to .o and put them in the Intermediate directory
    // Use compiler flags, defines and include flags only
    // clang [File.c] [CompilerFlags] -c -o [Intermediate/SubDir/.../File.c.o] [Defines] [IncludeFlags]

    u32 MaxLogicalCores = Platform_GetNumLogicalProcessors();
    u8 MaxCompilersAtOnce = (u8)MaxLogicalCores; // bound by max logical processors on the user's machine
    if (String_IsValid(MaxConcurrentCompilations))
    {
        u8 Num = 0;
        String_ToU8(MaxConcurrentCompilations, &Num);
        MaxCompilersAtOnce = Min(Num, (u8)MaxLogicalCores);
    }

    if (!String_IsEqual(BuildDirectory, StrLit("."), false))
    {
        if (!Filesystem_DoesDirectoryExist(BuildDirectory))
        {
            Filesystem_OpenDirectory(BuildDirectory);
        }
    }

    StringLocal(IntSrcDir, MAX_PATH_LENGTH);
    String_BuildPath(&IntSrcDir, IntermediateBaseDirectory, SourceDirectory);
    if (!Filesystem_DoesDirectoryExist(IntSrcDir))
    {
        Filesystem_OpenDirectory(IntSrcDir);
    }

    TArray(PlatformHandle) Processes = Array_Reserve(PlatformHandle, MaxCompilersAtOnce == 0 ? 32 : MaxCompilersAtOnce);

    BuildParams p = {0};
    p.CompilerProgram                  = bExplicitProgramPath ? CompilerPath : CompilerProgram;
    p.CompilerPath                     = CompilerPath;
    p.Assembly                         = AssemblyName;
    p.Extension                        = Extension;
    p.Extension_Og                     = Extension_Og;
    p.SourceFiles                      = SourceFilesFiltered;
    p.SourceFiles_Unfiltered           = GSourceFiles;
    p.Processes                        = &Processes;
    p.RootDirectory                    = GRootPath;
    p.SourceDirectory                  = SourceDirectory;
    p.BuildDirectory                   = BuildDirectory;
    p.IntermediateDirectory            = IntermediateDirectory;
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

    bool bSuccess = false;
    u32 NumCompiled = 0;

    // switch between different compiler backends
    if (String_IsEqual(CompilerProgram, StrLit("cl"), false) ||
        String_IsEqual(CompilerProgram, StrLit("msvc"), false))
    {
        Clock_Start(&CompileClock);
        bSuccess = MSVC_Compile(&p, &NumCompiled);
    }
    else
    {
        Clock_Start(&CompileClock);
        bSuccess = C_Compile(&p, &NumCompiled);
    }

    Clock_Tick(&CompileClock);

    if (!bSuccess)
    {
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

    if (String_IsEqual(CompilerProgram, StrLit("cl"), false) ||
        String_IsEqual(CompilerProgram, StrLit("msvc"), false))
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

    Clock_Tick(&ProgramRuntime);

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

    Clock_GetElapsedTime_ToString(&ProgramRuntime, true, &TimeString);
    LOG("Total build time: %S", TimeString);

    StringLocal(BuildPath, MAX_PATH_LENGTH);
    String_BuildPath(&BuildPath, GRootPath, BuildDirectory);
    String_AppendPathSeparator_Checked(&BuildPath);

    StringLocal(OutputPath, MAX_PATH_LENGTH);
    String_AppendChar(&OutputPath, '"');
    String_Append(&OutputPath, BuildPath);
    String_Append(&OutputPath, AssemblyName);
    if (Extension.Length > 0)
    {
        String_Append(&OutputPath, StrLit("."));
        String_Append(&OutputPath, Extension);
    }
    String_AppendChar(&OutputPath, '"');

    LOG_LINE_BREAK();

    #ifndef HOOD
    LOG_SUCCESS("Build complete: %S", OutputPath);
    #else
    LOG_SUCCESS("lessss goooo da build is complete homie: %S", OutputPath);
    #endif

PostBuild:
    // run post build commands (if specified)
    for each (Var, ExpandedVariablesDB)
    {
        if (String_IsEqual(Var.Name, StrLit("PostBuildCmd"), false))
        {
            Array_Add(PostBuildCmds, Var.Value);
        }
    }

    if (Array_Num(PostBuildCmds) > 0)
    {
        #ifndef HOOD
        LOG("\nRunning post build commands...");
        #else
        LOG("\ncool mang, gonna run some post build cmds...");
        #endif

        u32 Index = 0;
        for each_i (Index, Cmd, PostBuildCmds)
        {
            StringLocal(CmdLine, 8192);

            #if PLATFORM_WINDOWS
            String_Append(&CmdLine, StrLit("cmd.exe /c \""));
            String_Append(&CmdLine, Cmd);
            String_AppendChar(&CmdLine, '"');
            #else
            String_Append(&CmdLine, Cmd);
            #endif

            PlatformHandle Handle = Platform_RunCommand(CmdLine, GRootPath);

            #ifndef HOOD
            LOG("CMD: %S", Cmd);
            #else
            LOG("da cmd: %S", Cmd);
            #endif

            if (!Platform_IsValidHandle(Handle))
            {
                #ifdef HOOD
                LOG_ERROR("Somth'n fuckd up bro");
                #endif
                return 1;
            }

            #if PLATFORM_WINDOWS
            Platform_WaitForHandle(Handle, -1);
            #endif

            if (Index != Array_Num(PostBuildCmds)-1) // dont need to report the error if we're last, the build has finished anyway
            {
                const u32 ExitCode = Platform_GetExitCodeForProcess(Handle);
                if (ExitCode != 0)
                {
                    #ifndef HOOD
                    LOG_ERROR("Post-build command exited with a failure result: %u", ExitCode);
                    #else
                    LOG_ERROR("brah wtf, dis post-build command fuck'n failed on me nigga");
                    #endif
                    return 1;
                }
            }
        }
    }

End:
    // run the assembly (if an executable)
    if (bIsAssemblyExe && DoesBuildVarExist(StrLit("RunAssembly")))
    {
        TEMP_SCRATCH(_)
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
                String_BuildPath(&BuildDir, GRootPath, BuildDirectory);
                String_AppendPathSeparator_Checked(&BuildDir);

                String_Append(&CmdLine, StrLit("cd \""));
                String_Append(&CmdLine, BuildDir);
                String_Append(&CmdLine, StrLit("\" && "));

                #if !PLATFORM_WINDOWS
                String_Append(&CmdLine, StrLit("./"));
                #endif

                String_Append(&CmdLine, AssemblyName);

                if (Extension.Length > 0)
                {
                    String_AppendChar(&CmdLine, '.');
                    String_Append(&CmdLine, Extension);
                }

                String_AppendSpace(&CmdLine);
                String_Append(&CmdLine, Args);

                String_EatSpacesInlineFromEnd(&CmdLine);

                String_AppendChar(&CmdLine, '"');

                if (Extension.Length > 0)
                {
                    if (Args.Length > 0)
                        LOG("\nLaunching %S.%S with args -> (%S)", AssemblyName, Extension, Args);
                    else
                        LOG("\nLaunching %S.%S ...", AssemblyName, Extension);
                }
                else
                {
                    if (Args.Length > 0)
                        LOG("\nLaunching %S with args -> (%S)", AssemblyName, Args);
                    else
                        LOG("\nLaunching %S ...", AssemblyName);
                }

                //LOG("CMD: %S", CmdLine);

                Platform_WaitForHandle(Platform_RunCommand(CmdLine, CustomPath.Length > 0 ? CustomPath : BuildPath), -1);
            }
        }
    }

    if (String_IsValid(GCameFromBuildFile) && Array_Num(Processes) > 0)
    {
        return 2; // special exit code to let the parent riftbuild process know that this is a child process that finished successfully (and that it did some work)
    }

    return 0;
}
