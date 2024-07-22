// Copyright (c) 2024 Ali El Saleh

#include "Backend.h"

#include "Platform/Platform.h"
#include "Platform/Filesystem.h"
#include "String/StringUtils.h"
#include "Structures/Array.h"
#include "Uuid.h"
#include "Log.h"

/*
##
# something like this would be nice
    if /usr/bin/gnome-terminal
    if /usr/bin/konsole

    if Source/Resources/Info.plist Defsiofnef j
##
*/

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
                    bool bIsAssemblyExe)
{
    if (ReturnCode)
        *ReturnCode = 0;

    StringLocal(Line, 512);

    bool bInsideIf = false;
    bool bIfFailed = false;
    bool bInsideElse = false;
    bool bInsideSquareBrackets = false;
    bool bGoto = false;
    bool bInMultiLineComment = false;
    bool bInMultiLineErrorMessage = false;

    StringLocal(NamespaceKey, 64);
    StringLocal(GotoValue, 64);
    StringLocal(ErrorMessage_Name, 256);
    StringLocal(ErrorMessage, 4096);

    EComparisonType Comparison = Cmp_None;

    u16 LineNumber = 0;
    while (Filesystem_ReadLine(H, &Line))
    {
        LineNumber++;

    LoopStart:

        if (bInMultiLineErrorMessage)
        {
            // prevent leading/trailing spaces causing confusion if we only have the '}' in the line. its better than doing Line.Data[0]
            String Trimmed = String_EatSpacesFromEnd(String_EatSpaces(Line));
            if (Trimmed.Data[0] == '}')
            {
                String_EatNewLinesInlineFromEnd(&ErrorMessage);

                // TODO: extract into func?
                FileVariable var;
                var.Name = String_Create(Arena, ErrorMessage_Name);
                var.Value = String_Create(Arena, ErrorMessage);
                var.bHasSpecial = false;
                Array_Add(VariablesDB, var);

                String_Empty(&ErrorMessage);

                bInMultiLineErrorMessage = false;
                continue;
            }
            
            String_Append(&ErrorMessage, Line);
            String_Append(&ErrorMessage, S("\n"));

            continue;
        }

        String Trimmed = String_EatSpaces(Line);

        if (Trimmed.Length == 0)
        {
            continue;
        }

        // the line also cant start with any of these symbols (reserved for special commands)
        // funnily enough, these also act as single line comments
        if (!bInsideSquareBrackets &&
            (Trimmed.Data[0] == '%' ||
             Trimmed.Data[0] == '$' ||
             Trimmed.Data[0] == '!' ||
             Trimmed.Data[0] == '@'))
        {
            continue;
        }

        if (Trimmed.Data[0] == '{' ||
            Trimmed.Data[0] == '[' ||
            Trimmed.Data[0] == '\0')
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

        u32 LengthAfterTrim = Trimmed.Length;

        // handle multi-variable lines (on a single line)
        u32 SemiColonIndex = 0;
        bool bStartsWithIf = String_StartsWith(Trimmed, S("if"), false);
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
                    if (String_IsEqual(Else, S("else"), false))
                    {
                        bSeenElse = true;
                        bInsideElse = true;
                        if (bIfFailed)
                            continue;
                    }

                    String_IndexOfLastWhitespace(Else, &Space);
                    String ElseIf = String_EatSpacesFromEnd(StrSlice(Else.Data, Space));
                    if (String_IsEqual(ElseIf, S("else if"), false))
                    {
                        bInsideElse = true;
                        bSeenElse = true;
                        if (bIfFailed)
                        {
                            // extract just the if statement
                            String_IndexOfFirstWhitespace(ElseOg, &Space);

                            bIfFailed = false;
                            bInsideElse = false;
    
                            // do a indirect copy otherwise it will crash on OpenBSD due to overlapping memory
                            StringLocal(LineCopy, 512);
                            String_Copy(&LineCopy, StrShiftF(ElseOg, Space+1));

                            String_Copy(&Line, LineCopy);
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
        }

        if (Trimmed.Data[0] == '}')
        {
            bInsideIf = false;
            bIfFailed = false;
            String_Empty(&NamespaceKey); // todo: this will obviously break for nested { }, so we need an indexing system, maybe just rewrite the parser.... oh god
            continue;
        }

        // @parse name/value
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

        const bool bHasOverwrite = String_EatCharInlineFromEnd(&VarName, '`');
        const bool bHasSpecial = String_EatCharInlineFromEnd(&VarName, '!');

        // TODO: ignore ! when parsing a preset: var

        if (String_IsEqual(VarName, S("_stop"), false))
        {
            //LOG_INFO("Stopping build file parsing...");
            break;
        }

        if (String_IsEqual(VarName, S("_abort"), false))
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
            if (bIfFailed)
            {
                if (Trimmed.Data[0] == ']')
                {
                    bInsideSquareBrackets = false;
                    bIfFailed = false;
                }

                continue;
            }

            String* LastValue = &VariablesDB[Array_Num(VariablesDB)-1].Value;
            bool bLastIsSpecial = VariablesDB[Array_Num(VariablesDB)-1].bHasSpecial;

            if (Trimmed.Data[0] == ']')
            {
                bInsideSquareBrackets = false;
                String_EatSpacesInlineFromEnd(LastValue);
                continue;
            }

            VarValue = Trimmed;
            String_Append(LastValue, String_EatSpacesFromEnd(VarValue));

            if (bLastIsSpecial)
            {
                String_AppendNewline(LastValue);
            }
            else
            {
                String_AppendSpace(LastValue);
            }

            continue;
        }

        if (String_IsEqual(VarName, S("goto"), false))
        {
            if (VarValue.Length > 0)
            {
                bGoto = true;
                String_Copy(&GotoValue, VarValue);
                continue;
            }
        }

        if (String_EndsWith(VarName, S(".errormessage"), false))
        {
            String_Copy(&ErrorMessage_Name, VarName);

            if (VarValue.Length > 0 && VarValue.Data[0] == '{')
            {
                bInMultiLineErrorMessage = true;
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

            LinearAllocator Scratch = *Arena;
            StringArray MsgArgsList = String_ParseIntoArray(&Scratch, RestOfTheLine, ' ', 0, 64);

            u8 ArgIndex = 0;
            String MsgString = StrSlice(Trimmed.Data+1, LastQuoteIndex-1);
            for (u32 i = 0; i < MsgString.Length; i++)
            {
                char C = MsgString.Data[i];
                if (C == '%')
                {
                    const String Arg = StringArray_GetStringFromIndex(MsgArgsList, ArgIndex);
                    ArgIndex++;

                    //todo :expand to 0 if not exist but expand to nothing if it has an =
                    String Var = String_EatChar(Arg, '%');
                    String Val = GetCmdOptionValue(CmdOptionsDB, Var);
                    String_Append(&FormattedMsg, Val);
                    continue;
                }

                String_AppendChar(&FormattedMsg, C);
            }

            if (FormattedMsg.Length > 0)
            {
                String New = String_Create(Arena, FormattedMsg);
                Array_Add(Messages, New);
            }

            continue;
        }

        if (String_IsEqual(VarName, S("if"), false) && bFoundSpace) // make sure this isnt a lone 'if'
        {
            u32 Index = 0;
            String_IndexOfFirstWhitespace(VarValue, &Index);

            bool bIsMultiLineIf = String_IndexOfChar(VarValue, '{', NULL);
            //bool bIsMultiLineVar = VarValue.Data[VarValue.Length - 1];
            bool bIsMultiLineVar = String_IsLast(VarValue, '[');
            bInsideIf = bIsMultiLineIf;
            bInsideElse = false;

            String Condition;
            if (Index > 0)
                Condition = StrSlice(VarValue.Data, Index);
            else
                Condition = VarValue;

            bool bSearchEnv = String_EatCharInline(&Condition, '@');

            bool bIsNot = Condition.Data[0] == '!';
            String_EatCharInline(&Condition, '!');
            bool bCaseSensitive = String_EatCharInline(&Condition, '^');

            bool bConditionMet = false;
            String ConditionValuePtr = String_Null();

            StringLocal(EnvValue, 1024);

            if (bSearchEnv)
            {
                // find this variable
                StringLocal(Temp, 256);
                String_Copy(&Temp, Condition);
                if (Platform_GetEnvironmentVariableValue(Temp, &EnvValue))
                {
                    ConditionValuePtr = EnvValue;
                    bConditionMet = true;
                }
            }

            // check the condition string against the internal build vars passed in from the command line
            // override VarValue for single line if's, for multiline if's, loop back to the top and process each line until '}' is found
            if (!bConditionMet)
            {
                for each (CmdOption, o, CmdOptionsDB)
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
            }

            if (!bConditionMet)
            {
                for each (InternalVariable, v, InternalVariablesDB)
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
                for each (FileVariable, o, VariablesDB) // intentional that we're not using expanded DB, this should only be used for simple things anyway
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

            if (!String_IsEqual(ComparisonOperator, S("!="), false)) // ignore !=
            {
                if (String_EatCharInline(&ComparisonOperator, '!'))
                {
                    bIsNot = true;
                }
            }

            Comparison = Cmp_None;

            if      (String_IsEqual(ComparisonOperator, S("=="), false))           Comparison = Cmp_Equal;
            else if (String_IsEqual(ComparisonOperator, S("!="), false))           Comparison = Cmp_NotEqual;
            else if (String_IsEqual(ComparisonOperator, S(">="), false))           Comparison = Cmp_GreaterThanOrEqual;
            else if (String_IsEqual(ComparisonOperator, S("<="), false))           Comparison = Cmp_LessThanOrEqual;
            else if (String_IsEqual(ComparisonOperator, S(">"), false))            Comparison = Cmp_GreaterThan;
            else if (String_IsEqual(ComparisonOperator, S("<"), false))            Comparison = Cmp_LessThan;
            else if (String_IsEqual(ComparisonOperator, S("starts_with"), false))  Comparison = Cmp_StartsWith;
            else if (String_IsEqual(ComparisonOperator, S("ends_with"), false))    Comparison = Cmp_EndsWith;
            else if (String_IsEqual(ComparisonOperator, S("contains"), false))     Comparison = Cmp_Contains;

            //String TestValue = String_EatSpaces(StrSlice(VarValue.Data+Index+1+SecondWhitespaceIndex, VarValue.Length-Index-1-SecondWhitespaceIndex));
            String TestValue = String_EatSpaces(StrShiftF(VarValue, Index+1+SecondWhitespaceIndex));
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
                case Cmp_None: break;

                case Cmp_Equal:
                {
                    LinearAllocator Scratch = *Arena;
                    StringArray Values = String_ParseIntoArray(&Scratch, TestValue, '|', 0, 128);
                    for each_str (v, Values)
                    {
                        bConditionMet = String_IsEqual(ConditionValuePtr, *v, bCaseSensitive);
                        if (bConditionMet)
                        {
                            break;
                        }

                        // if the condition value has more than one value separated by a space
                        StringArray Values2 = String_ParseIntoArray(&Scratch, ConditionValuePtr, ' ', 0, 128);
                        for each_str (v2, Values2)
                        {
                            bConditionMet = String_IsEqual(*v, *v2, bCaseSensitive);
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
                break;

                case Cmp_NotEqual:
                {
                    LinearAllocator Scratch = *Arena;
                    StringArray Values = String_ParseIntoArray(&Scratch, TestValue, '|', 0, 128);
                    for each_str (v, Values)
                    {
                        bConditionMet = !String_IsEqual(ConditionValuePtr, *v, bCaseSensitive);
                        if (bConditionMet)
                        {
                            break;
                        }

                        // if the condition value has more than one value separated by a space
                        StringArray Values2 = String_ParseIntoArray(&Scratch, ConditionValuePtr, ' ', 0, 128);
                        for each_str (v2, Values2)
                        {
                            bConditionMet = !String_IsEqual(*v, *v2, bCaseSensitive);
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

                case Cmp_StartsWith:
                {
                    LinearAllocator Scratch = *Arena;
                    StringArray Values = String_ParseIntoArray(&Scratch, TestValue, '|', 0, 128);
                    for each_str (v, Values)
                    {
                        bConditionMet = String_StartsWith(ConditionValuePtr, *v, bCaseSensitive);
                        if (bConditionMet)
                        {
                            break;
                        }

                        // if the condition value has more than one value separated by a space
                        StringArray Values2 = String_ParseIntoArray(&Scratch, ConditionValuePtr, ' ', 0, 128);
                        for each_str (v2, Values2)
                        {
                            bConditionMet = String_StartsWith(*v2, *v, bCaseSensitive);
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
                break;

                case Cmp_EndsWith:
                {
                    LinearAllocator Scratch = *Arena;
                    StringArray Values = String_ParseIntoArray(&Scratch, TestValue, '|', 0, 128);
                    for each_str (v, Values)
                    {
                        bConditionMet = String_EndsWith(ConditionValuePtr, *v, bCaseSensitive);
                        if (bConditionMet)
                        {
                            break;
                        }

                        // if the condition value has more than one value separated by a space
                        StringArray Values2 = String_ParseIntoArray(&Scratch, ConditionValuePtr, ' ', 0, 128);
                        for each_str (v2, Values2)
                        {
                            bConditionMet = String_EndsWith(*v2, *v, bCaseSensitive);
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
                break;

                case Cmp_Contains:
                {
                    LinearAllocator Scratch = *Arena;
                    StringArray Values = String_ParseIntoArray(&Scratch, TestValue, '|', 0, 128);
                    for each_str (v, Values)
                    {
                        bConditionMet = String_Contains(ConditionValuePtr, *v, bCaseSensitive);
                        if (bConditionMet)
                        {
                            break;
                        }

                        // if the condition value has more than one value separated by a space
                        StringArray Values2 = String_ParseIntoArray(&Scratch, ConditionValuePtr, ' ', 0, 128);
                        for each_str (v2, Values2)
                        {
                            bConditionMet = String_Contains(*v2, *v, bCaseSensitive);
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
                break;
            }

            if (bIsNot)
            {
                bConditionMet = !bConditionMet;
            }

            String RestOfTheLine = StrShiftF(VarValue, Index);
            String_EatSpacesInlineFromEnd(&RestOfTheLine);

            StringLocal(LineCopy, 512);
            String_Copy(&LineCopy, RestOfTheLine);

            // else statement detection
            u32 LengthCap = 0;
            bool bHasElse = false;
            {
                LinearAllocator Scratch = *Arena;
                StringArray Strings = String_ParseIntoArray(&Scratch, LineCopy, ' ', 0, 128);
                for each_str (S, Strings)
                {
                    if (String_IsEqual(*S, S("else"), false))
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
                    if (LineCopy.Length > 0)
                    {
                        if (bIsMultiLineIf)
                        {
                            continue;
                        }

                        if (bHasElse)
                        {
                            // the following crashes on OpenBSD memcpy, due to the pointers being overlapped
                            //String_Copy(&Line, String_EatSpaces(StrSlice(RestOfTheLine.Data, LengthCap)));

                            String_Copy(&Line, String_EatSpaces(StrSlice(LineCopy.Data, LengthCap)));
                        }
                        else
                        {
                            // the following crashes on OpenBSD memcpy, due to the pointers being overlapped
                            //String_Copy(&Line, RestOfTheLine);

                            String_Copy(&Line, LineCopy);
                        }

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
                bInsideSquareBrackets = bIsMultiLineVar;
                if (bIsMultiLineIf || bIsMultiLineVar)
                {
                    bIfFailed = true;
                }

                if (LineCopy.Length > 0)
                {
                    if (bHasElse)
                    {
                        String_Copy(&Line, String_EatSpaces(StrShiftF(LineCopy, LengthCap+5))); // else is 4 chars + 1 white space
                        goto LoopStart;
                    }
                }

                continue;
            }
        }

        const String Keywords[] =
        {
            S("include"),
            S("switch"),
            S("endswitch"),
            S("if"),
            S("case"),
            S("goto"),
            S("_abort"),
            S("_stop"),
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
            if (String_EndsWith(VarValue, S("{"), false))
            {
                String_Copy(&NamespaceKey, VarName);
                continue;
            }

            StringLocal(NewVarName, 64);
            if (NamespaceKey.Length > 0)
            {
                String_Append(&NewVarName, NamespaceKey);
                String_Append(&NewVarName, S("::"));
                String_Append(&NewVarName, VarName);

                VarName = NewVarName;
            }

            bool bWantsOverride = false;

            if (bHasOverwrite)
            {
                String* Value = GetVariableValue_Ref(VariablesDB, VarName);
                if (Value && Value->Length > 0)
                {
                    bWantsOverride = true;
                    String_Empty(Value);
                }
            }

            if (VarValue.Length > 0)
            {
                if (VarValue.Data[0] == '[')
                {
                    bInsideSquareBrackets = true;
                    
                    if (!bWantsOverride)
                    {
                        FileVariable var;
                        var.Name = String_Create(Arena, VarName);
                        var.Value.Data = LinearAllocator_Allocate(Arena, 8192); // allocate one really long line, because we dont know how many lines there will be
                        var.Value.Length = 0;
                        var.Value.Capacity = 8191;
                        var.bHasSpecial = bHasSpecial;

                        Array_Add(VariablesDB, var);
                    }

                    continue;
                }
            }

            if (bWantsOverride)
            {
                // we are kind of leaking the old value, but idk. i think we need to have a fixed sized when we allocate from the arena
                // TODO: something better
                *GetVariableValue_Ref(VariablesDB, VarName) = String_Create(Arena, String_EatSpacesFromEnd(VarValue));
            }
            else
            {
                // check if we already added this value for this build variable
                bool bDuplicateValueFound = false;
                for each (FileVariable, Var, VariablesDB)
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
                var.Name  = String_Create(Arena, VarName);
                var.Value = String_Create(Arena, String_EatSpacesFromEnd(VarValue));
                var.bHasSpecial = bHasSpecial;

                Array_Add(VariablesDB, var);
            }
        }

        if (String_IsEqual(VarName, S("Include"), false))
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
            if (!ExpandBuildVariable(*Arena, VariablesDB, CmdOptionsDB, &ExpandedPath, S("Include"), IncludeFilePath, S("Include"), WorkingDirectory, false, bIsAssemblyExe))
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
            Filesystem_GetFileSize(IncludeFileHandle, &Size);

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
                Filesystem_GetFilePath(IncludeFileHandle, &IncludePath);

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

                if (!ParseBuildFile(Arena, IncludeFileHandle, BuildFilePath, WorkingDirectory,
                                    VariablesDB, ExpandedVariablesDB, CmdOptionsDB, Messages,
                                    IncludeFiles, ReturnCode, true, Includes, bIsAssemblyExe))
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

bool ExpandBuildVariable(LinearAllocator Scratch, TArray(FileVariable) VariablesDB, TArray(CmdOption) CmdOptionsDB,
                        String* Dest, const String Key, const String Value, const String Root, const String WorkingDirectory,
                        bool bLowerStrings, bool bIsAssemblyExe)
{
    if (!String_IsValid(Value))
    {
        return true;
    }

    bool bLinux = false;
    #if PLATFORM_UNIX
    bLinux = bIsAssemblyExe;
    #endif

    bool bLowerAll = bLowerStrings || (String_IsEqual(Key, S("Assembly"), false) && bLinux); // hack but whatever. todo: revisit this

    bool bInsideQuote = false; // what happens when we expand a variable inside a quote and recursively call this func?
    u32 Offset = 1;
    for (u32 i = 0; i < Value.Length; i+=Offset)
    {
        Offset = 1;

        String StrVal = StrSlice(Value.Data+i, Value.Length-i);
        char C = Value.Data[i];

        if (bInsideQuote  && C == '"') bInsideQuote = false;
        if (!bInsideQuote && C == '"') bInsideQuote = true;

        if (!bInsideQuote && C == '#') // a comment. disgard everything and exit
        {
            goto End;
        }

        String Slice = String_Null();
        bool bWantsToLower = false;
        bool bWantsToUpper = false;

        if (String_EndsWith(Key, S(".errormessage"), false) ||
            String_EndsWith(Key, S(".Cmd"), false)) // todo: rethink
        {
            if (C == '!')
            {
                String_AppendChar(Dest, C);
                continue;
            }
        }

        if (C == '%' || C == '$' || C == '@' || C == '!')
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

            const bool bHasNot = Slice.Length > 1 ? String_EatCharInline(&Slice, '!') : false;
            const bool bWantsPaste = Slice.Length > 1 ? String_EatCharInline(&Slice, '%') : false;

            bool bFoundCmd = false;
            bool bEqualsToSomething = false;
            for each (CmdOption, o, CmdOptionsDB)
            {
                if (String_IsEqual(o.Name, Slice, false))
                {
                    bFoundCmd = true;
                    VarValue = o.Value;
                    bEqualsToSomething = o.bEqualsToSomething;
                    break;
                }
            }

            for each (InternalVariable, v, InternalVariablesDB)
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
                for each (FileVariable, Var, VariablesDB)
                {
                    if (String_IsEqual(Var.Name, S("AssertCmdVarExists"), false))
                    {
                        StringArray CmdVarsArray = String_ParseIntoArray(&Scratch, Var.Value, ' ', 0, 128);

                        for each_str (S, CmdVarsArray)
                        {
                            const String Trimmed = String_EatSpaces(*S);

                            bool bFound = false;
                            for each (CmdOption, o, CmdOptionsDB)
                            {
                                if (String_IsEqual(o.Name, Trimmed, false))
                                {
                                    bFound = true;
                                    if (o.bEqualsToSomething && o.Value.Length == 0)
                                    {
                                        bFound = false;
                                    }
                                    
                                    break;
                                }
                            }

                            if (!bFound)
                            {
                                #ifndef HOOD
                                LOG_ERROR("Build assertion failure. Command line argument \"%S\" or \"%S=VALUE\" was not given."
                                "\n        This is needed for the build to work properly. Aborting build...", Trimmed, Trimmed);
                                #else
                                LOG_ERROR("yo da cmd line var \"%S\" don exist cuh. dat shit not there nigga", Trimmed);
                                #endif

                                LogCustomErrorMessage(VariablesDB, S("Cmd"), Trimmed, true);

                                return false;
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

                if (!ExpandBuildVariable(Scratch,VariablesDB, CmdOptionsDB, Dest, Slice, VarValue, Root, WorkingDirectory, false, bIsAssemblyExe))
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
                    if (String_IsEqual(Root, S("Depends"), false))
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
            for each (FileVariable, Var, VariablesDB)
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

                    if (!ExpandBuildVariable(Scratch, VariablesDB, CmdOptionsDB, Dest, Slice, Var.Value, Root, WorkingDirectory, bLowerStrings, bIsAssemblyExe))
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
                LOG_ERROR("Could not retrieve environment variable named %S\n", Slice);

                if (LogCustomErrorMessage(VariablesDB, S("Env"), Slice, false))
                {
                    LOG_LINE_BREAK();
                }

                LogRegularEnvVarTutorialSteps();

                return false;
            }

            String DestEnd = StrShiftF(*Dest, Dest->Length);
            u32 DestLengthBefore = Dest->Length;

            if (!ExpandBuildVariable(Scratch, VariablesDB, CmdOptionsDB, Dest, Slice, VarValue, Root, WorkingDirectory, false, bIsAssemblyExe))
            {
                return false;
            }

            DestEnd.Length = Dest->Length - DestLengthBefore;
            if (bWantsToLower) String_ToLower(&DestEnd);
            if (bWantsToUpper) String_ToUpper(&DestEnd);
        }
        else if (C == '!' && Slice.Length > 0) // run custom shell commands and append the output of the command to Dest
        {
            StringLocal(CmdLine, 8192);

            #if PLATFORM_WINDOWS
            String_Append(&CmdLine, S("cmd.exe /c \""));
            String_Append(&CmdLine, Slice);
            String_AppendChar(&CmdLine, '"');
            #else
            String_Append(&CmdLine, Slice);
            #endif

            //LOG("CMD: %S", Slice);

            PlatformPipe StdOutHandle = {0};
            PlatformHandle ShellCmd = Platform_RunCommand_Ex(CmdLine, WorkingDirectory, &StdOutHandle);
            if (!ShellCmd)
            {
                return false;
            }

            Platform_WaitForHandle(ShellCmd, -1);

            StringLocal(StdOutData, 8192);
            u64 BytesRead = 0;
            if (!Filesystem_ReadPipe(StdOutHandle, StdOutData.Capacity, StdOutData.Data, &BytesRead))
            {
                LOG_ERROR("Failed to read from standard output pipe for command -> \"%S\"", Slice);
                return false;
            }

            StdOutData.Length = Min((u32)BytesRead, StdOutData.Capacity);
            String_EatNewLinesInlineFromEnd(&StdOutData);

            String DestEnd = StrShiftF(*Dest, Dest->Length);
            u32 DestLengthBefore = Dest->Length;

            String_Append(Dest, StdOutData);
            DestEnd.Length = Dest->Length - DestLengthBefore;

            if (bWantsToLower) String_ToLower(&DestEnd);
            if (bWantsToUpper) String_ToUpper(&DestEnd);

            Platform_CloseHandle(StdOutHandle[0]);
            Platform_CloseHandle(StdOutHandle[1]);
        }
        else
        {
            bool bCheckChar = true;

            if (String_EndsWith(Key, S(".errormessage"), false))
            {
                bCheckChar = false;
            }

            if (bCheckChar)
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
                        S("SourceDirectory"),
                        S("BuildDirectory"),
                        S("IntermediateDirectory"),
                        S("LibraryDirectories"),
                        S("Includes"),
                        S("Icon"),
                        S("Compiler"),
                        S("IncludedSourceDirectories"),
                        S("ExcludedSourceDirectories"),
                        S("ExternalSourceDirectories"),
                        S("IncludedSourceFiles"),
                        S("ExcludedSourceFiles"),
                    };

                    bool bKeyIsPathBased = false;
                    for (u8 j = 0; j < SArray_Capacity(KeysToCareAbout); j++)
                    {
                        if (String_IsEqual(Key, KeysToCareAbout[j], false))
                        {
                            #if PLATFORM_WINDOWS
                            C = '\\';
                            #else
                            C = '/';
                            #endif
                            
                            bKeyIsPathBased = true;

                            break;
                        }
                    }

                    // only check for duplicate path separator for certain keys
                    if (bKeyIsPathBased)
                    {
                        if (C == '/' || C == '\\')
                        {
                            if (Dest->Length > 0)
                            {
                                char LastChar = Dest->Data[Dest->Length-1];
                                bool bHasPathSep = LastChar == '/' || LastChar == '\\';
                                if (bHasPathSep)
                                    continue;
                            }
                        }
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
