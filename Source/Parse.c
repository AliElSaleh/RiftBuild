#include "Backend.h"

#include "Platform/Platform.h"
#include "Platform/Filesystem.h"
#include "String/StringUtils.h"
#include "Structures/Array.h"
#include "Uuid.h"
#include "Log.h"

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
                    bool bIsAssemblyExe)
{
    if (ReturnCode)
        *ReturnCode = 0;

    StringLocal(Line, 512);

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
    bool bInMultiLineComment = false;

    StringLocal(SwitchValue, 64);
    StringLocal(GotoValue, 64);

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

        String Trimmed = Line;

        if (Trimmed.Length < 1 ||
            Trimmed.Data[0] == '{' ||
            Trimmed.Data[0] == '[' ||
            Trimmed.Data[0] == '\0')
        {
            continue;
        }

        // the line also cant start with any of these symbols (reserved for special commands)
        if (Trimmed.Data[0] == '%' ||
            Trimmed.Data[0] == '$' ||
            Trimmed.Data[0] == '@')
        {
            continue;
        }

        // multiline comment
        if (Trimmed.Data[0] == '#' && Trimmed.Data[1] == '#')
        {
            bInMultiLineComment = !bInMultiLineComment;
            continue;
        }

        // single line comment
        if (Trimmed.Data[0] == '#')
        {
            continue;
        }

        if (bInMultiLineComment)
        {
            continue;
        }

        // skip blank lines and comments
        Trimmed = String_EatSpaces(Trimmed);

        u32 LengthAfterTrim = Trimmed.Length;

        // handle multi-variable lines (on a single line)
        u32 SemiColonIndex = 0;
        bool bStartsWithIf = String_StartsWith(Trimmed, StrLit("if"), false);
        if (!bStartsWithIf && String_IndexOfChar(Trimmed, ';', &SemiColonIndex))
        {
            Trimmed.Length = SemiColonIndex;
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

                    String CmdValue = GetCmdOptionValue(CmdOptionsDB, SwitchValue);

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

                    String CmdValue = GetCmdOptionValue(CmdOptionsDB, SwitchValue);

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
                LOG_ERROR("Missing closing \" for %S on line %hu in %S", Trimmed, LineNumber, BuildFilePath);
                return false;
            }

            String RestOfTheLine = String_EatSpaces(StrSlice(Trimmed.Data+LastQuoteIndex+1, Trimmed.Length-LastQuoteIndex-1));

            StringLocal(FormattedMsg, 256);

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
                        String Val = GetCmdOptionValue(CmdOptionsDB, Var);
                        String_Append(&FormattedMsg, Val);
                        continue;
                    }

                    String_AppendChar(&FormattedMsg, C);
                }
            }

            if (FormattedMsg.Length > 0)
            {
                Array_Add(Messages, String_Create(Arena, FormattedMsg));
            }

            continue;
        }

        if (String_IsEqual(VarName, StrLit("if"), false) && bFoundSpace) // make sure this isnt a lone 'if'
        {
            u32 Index = 0;
            String_IndexOfFirstWhitespace(VarValue, &Index);

            bool bIsMultiLineIf = String_IndexOfChar(VarValue, '{', NULL);
            bInsideIf = bIsMultiLineIf;
            bInsideElse = false;

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
                    if (!o.bEqualsToSomething || 
                        (o.bEqualsToSomething && o.Value.Length > 0)) // make sure we have some value if we specified an '=' sign
                    {
                        ConditionValuePtr = o.Value;
                        bConditionMet = true;
                        break;
                    }
                }
            }

            if (!bConditionMet)
            {
                for each (v, InternalVariablesDB)
                {
                    if (String_IsEqual(v.Name, Condition, false))
                    {
                        ConditionValuePtr = v.Value;
                        bConditionMet = true;
                        break;
                    }
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
            // TODO: starts_with and ends_with, contains, has char
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
                    var.Name = String_Create(Arena, VarName);
                    var.Value.Data = LinearAllocator_Allocate(Arena, 8192); // allocate one really long line, because we dont know how many lines there will be
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
            var.Name = String_Create(Arena, VarName);
            var.Value = String_Create(Arena, String_EatSpacesFromEnd(VarValue));

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
                StringLocal(IncludePath, MAX_PATH_LENGTH);
                Filesystem_GetFilePath(H, &IncludePath);

                String_IndexOfLastPathSlash(IncludePath, &LastSlash);
                const String Path = StrSlice(IncludePath.Data, LastSlash);
                String_BuildPath(&IncludeFilePath, Path, VarValue);
            }
            else
            {
                String_BuildPath(&IncludeFilePath, WorkingDirectory, VarValue);
            }

            StringLocal(ExpandedPath, 512);
            if (!ExpandBuildVariable(VariablesDB, CmdOptionsDB, &ExpandedPath, StrLit("Include"), IncludeFilePath, StrLit("Include"), false, bIsAssemblyExe))
            {
                return false;
            }

            String_ConvertSlashToPlatformSlash(&ExpandedPath);
            Filesystem_ConvertRelativeToAbsolutePath(&ExpandedPath);

            if (!Filesystem_DoesFileExist(ExpandedPath))
            {
                #ifndef HOOD
                LOG_ERROR("Failed to open include file \"%S\" in %S", ExpandedPath, BuildFilePath);
                #else
                LOG_ERROR("bruh sort your shit out man, cant find this file bro \"%S\" in %S", ExpandedPath, BuildFilePath);
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
                StringLocal(IncludePath, MAX_PATH_LENGTH);
                Filesystem_GetFilePath(&IncludeFileHandle, &IncludePath);

                if (Includes)
                {
                    bool bError = false;
                    for each_str_list (*Includes)
                    {
                        if (String_IsEqual(IncludePath, It.String, false))
                        {
                            LOG_ERROR("\"%S\" is including itself\n", IncludePath);
                            bError = true;
                            break;
                        }
                    }

                    if (bError)
                    {
                        LOG("  Include hierarchy for %S", BuildFilePath);
                        LOG_INLINE("     ");
                        u8 i = 0;
                        for each_str_list (*Includes)
                        {
                            LOG("%S", It.String);

                            for (u8 Level = 0; Level < i; Level++)
                                LOG_INLINE("   ");
                            
                            LOG_INLINE("     |- ");

                            i++;
                        }

                        LOG("%S <--", IncludeFilePath);

                        return false;
                    }
                }

                // detect circular includes
                StringList Entry;
                Entry.String = IncludePath;
                Entry.Next = NULL;

                StringList** Next = &Includes;
                while (*Next)
                {
                    Next = &(*Next)->Next;
                }

                *Next = &Entry;

                Array_Add(IncludeFiles, IncludeFileHandle);

                if (!ParseBuildFile(Arena, &IncludeFileHandle, BuildFilePath, WorkingDirectory, VariablesDB, ExpandedVariablesDB, CmdOptionsDB, Messages, IncludeFiles, ReturnCode, true, Includes, bIsAssemblyExe))
                {
                    return false;
                }

                *Next = NULL;
            }
        }

        if (SemiColonIndex > 0)
        {
            Trimmed.Length = LengthAfterTrim;
            Line = StrShiftF(Trimmed, SemiColonIndex+1);

            goto LoopStart;
        }
    }

    return true;
}

bool ExpandBuildVariable(TArray(FileVariable) VariablesDB, TArray(CmdOption) CmdOptionsDB,
                                  String* Dest, const String Key, const String Value, const String Root, bool bLowerStrings,
                                  bool bIsAssemblyExe)
{
    if (!String_IsValid(Value))
    {
        return true;
    }

    bool bLinux = false;
    #if PLATFORM_UNIX
    bLinux = bIsAssemblyExe;
    #endif

    bool bLowerAll = bLowerStrings || (String_IsEqual(Key, StrLit("Assembly"), false) && bLinux); // hack but whatever. todo: revisit this

    u32 Offset = 1;
    for (u32 i = 0; i < Value.Length; i+=Offset)
    {
        Offset = 1;

        String StrVal = StrSlice(Value.Data+i, Value.Length-i);
        char C = Value.Data[i];

        if (C == '#') // a comment. disgard everything and exit
        {
            goto End;
        }

        String Slice = String_Null();
        bool bWantsToLower = false;
        bool bWantsToUpper = false;
        //bool bIsEnclosed = false;
        if (C == '%' || C == '$' || C == '@')
        {
            u32 Index = 0;

            StrVal = StrShiftF(StrVal, 1);
            
            bWantsToLower = String_EatCharInline_Single(&StrVal, '-');
            if (bWantsToLower) Offset++;
            bWantsToUpper = String_EatCharInline_Single(&StrVal, '^');
            if (bWantsToUpper) Offset++;

            if (String_EatCharInline(&StrVal, '('))
            {
                Offset++;

                if (String_IndexOfChar(StrVal, ')', &Index))
                {
                    Offset++;
                    //bIsEnclosed = true;
                }
            }

            if (Index == 0)
            {
                String_IndexOfFirstWhitespace(StrVal, &Index);
            }

            // find this variable
            if (Index > 0)
            {
                Slice = StrSlice(StrVal.Data, Index);
                Offset += Index;
            }
            else
            {
                Slice = StrVal;
                Offset += StrVal.Length;
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
            const bool bWantsPaste = String_EatCharInline(&Slice, '%');

            bool bFoundCmd = false;
            bool bEqualsToSomething = false;
            for each (o, CmdOptionsDB)
            {
                if (String_IsEqual(o.Name, Slice, false))
                {
                    bFoundCmd = true;
                    VarValue = o.Value;
                    bEqualsToSomething = o.bEqualsToSomething;
                    break;
                }
            }

            for each (v, InternalVariablesDB)
            {
                if (String_IsEqual(v.Name, Slice, false))
                {
                    bFoundCmd = true;
                    VarValue = v.Value;
                    bEqualsToSomething = true;
                    break;
                }
            }

            // run through the cmd var assert list
            // TODO: something better
            {
                for each (Var, VariablesDB)
                {
                    if (String_IsEqual(Var.Name, StrLit("AssertCmdVarExists"), false))
                    {
                        TEMP_SCRATCH(Assert)
                        {
                            StringArray CmdVarsArray = String_ParseIntoArray(Scratch_Assert.Allocator, Var.Value, ' ', 0, 128);

                            for each_str (S, CmdVarsArray)
                            {
                                const String Trimmed = String_EatSpaces(*S);

                                bool bFound = DoesCmdVarExist(CmdOptionsDB, Trimmed);

                                if (!bFound)
                                {
                                    #ifndef HOOD
                                    LOG_ERROR("Build assertion failure. Command line argument \"%S\" or \"%S=VALUE\" was not given. This is needed for the build to work properly. Aborting build...", Trimmed, Trimmed);
                                    #else
                                    LOG_ERROR("yo da cmd line var \"%S\" don exist cuh. dat shit not there nigga", Trimmed);
                                    #endif

                                    return false;
                                }
                            }
                        }
                    }
                }
            }

            if (String_IsValid(VarValue))
            {
                // if the first letter is capitalized, then also make the first letter of the value capitalized. revert back when done
                bool bWasValueLower = IsAlphabetLower(VarValue.Data[0]);
                bool bIsVarUpper = IsAlphabetUpper(Slice.Data[0]);
                if (bIsVarUpper)
                    VarValue.Data[0] = ToUpper(VarValue.Data[0]);

                String DestEnd = StrShiftF(*Dest, Dest->Length);
                u32 DestLengthBefore = Dest->Length;

                if (!ExpandBuildVariable(VariablesDB, CmdOptionsDB, Dest, Slice, VarValue, Root, false, bIsAssemblyExe))
                {
                    return false;
                }

                DestEnd.Length = Dest->Length - DestLengthBefore;
                if (bWantsToLower) String_ToLower(&DestEnd);
                if (bWantsToUpper) String_ToUpper(&DestEnd);

                if (bIsVarUpper && bWasValueLower)
                    VarValue.Data[0] = ToLower(VarValue.Data[0]);
            }
            else
            {
                if (!bEqualsToSomething)
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
                        if (bWantsPaste)
                        {
                            if (bFoundCmd)
                            {
                                String DestEnd = StrShiftF(*Dest, Dest->Length);
                                String_Append(Dest, Slice);
                                DestEnd.Length = Slice.Length;

                                if (bWantsToLower) String_ToLower(&DestEnd);
                                if (bWantsToUpper) String_ToUpper(&DestEnd);
                            }
                        }
                        else
                        {
                            bool bIsNative = Slice.Data[0] == '_';
                            if (!bIsNative)
                            {
                                //LOG("%S", Slice);

                                String_AppendChar(Dest, bFoundCmd ? '1' : '0');
                            }
                        }
                    }
                }
            }
        }
        else if (C == '$')
        {
            bool bFound = DoesBuildVarExist(VariablesDB, Slice);
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

                    String DestEnd = StrShiftF(*Dest, Dest->Length);
                    u32 DestLengthBefore = Dest->Length;

                    if (!ExpandBuildVariable(VariablesDB, CmdOptionsDB, Dest, Slice, Var.Value, Root, bLowerStrings, bIsAssemblyExe))
                    {
                        return false;
                    }

                    DestEnd.Length = Dest->Length - DestLengthBefore;
                    if (bWantsToLower) String_ToLower(&DestEnd);
                    if (bWantsToUpper) String_ToUpper(&DestEnd);

                    NumEntries++;
                }
            }
        }
        else if (C == '@')
        {
            // find this variable
            StringLocal(VarValue, 4096);
            if (!Platform_GetEnvironmentVariableValue(Slice, &VarValue))
            {
                LOG_ERROR("Could not retrieve environment variable for %S", Slice);
                return false;
            }

            String DestEnd = StrShiftF(*Dest, Dest->Length);
            u32 DestLengthBefore = Dest->Length;

            if (!ExpandBuildVariable(VariablesDB, CmdOptionsDB, Dest, Slice, VarValue, Root, false, bIsAssemblyExe))
            {
                return false;
            }

            DestEnd.Length = Dest->Length - DestLengthBefore;
            if (bWantsToLower) String_ToLower(&DestEnd);
            if (bWantsToUpper) String_ToUpper(&DestEnd);
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

            if (C == '/' || C == '\\')
            {
                const String KeysToCareAbout[] = 
                {
                    StrLit("SourceDirectory"),
                    StrLit("BuildDirectory"),
                    StrLit("IntermediateDirectory"),
                    StrLit("LibraryDirectories"),
                    StrLit("Includes"),
                    StrLit("Icon"),
                    StrLit("Compiler"),
                    StrLit("IncludedSourceDirectories"),
                    StrLit("ExcludedSourceDirectories"),
                    StrLit("ExternalSourceDirectories"),
                };

                for (u8 j = 0; j < SArray_Capacity(KeysToCareAbout); j++)
                {
                    if (String_IsEqual(Key, KeysToCareAbout[j], false))
                    {
                        #if PLATFORM_WINDOWS
                        C = '\\';
                        #else
                        C = '/';
                        #endif
                        
                        break;
                    }
                }
            }

            String_AppendChar(Dest, bLowerAll ? ToLower(C) : C);
        }
    }

End:
    String_EatSpacesInlineFromEnd(Dest);

    return true;
}
