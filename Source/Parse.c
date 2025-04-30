// Copyright (c) Artisan Softworks
// Licensed under the BSD 3-Clause License. See the LICENSE file for details.

#ifndef UNITY_BUILD
#include "Backend.h"

#include "Core/Allocators.h"
#include "Core/Platform.h"
#include "Core/StringUtils.h"
#include "Core/Array.h"
#include "Core/Globals.h"
#include "Core/Log.h"
#endif

// todo: when appending values, use linked list?
// support this, replace the special flag '!' with named special instead. like this PreLink.Delete(silent ignore_exit_code)
// rename "IncludedSourceFiles" to "SourceFiles", same with the dir version

#define MAX_KEY_LENGTH 64
#define MAX_VALUE_LENGTH 8192
#define MAX_META_KEY_LENGTH 16
#define LINE_BUFFER_SIZE (MAX_KEY_LENGTH+MAX_VALUE_LENGTH+MAX_META_KEY_LENGTH)

static void Internal_AddVariable(LinearAllocator* Arena,
                                   TArray(FileVariable) VariablesDB,
                                   const String Name,
                                   const String Value,
                                   const String SpecialData,
                                   bool bHasSpecial)
{
    // always reserve a fixed limited size so we can override if needed
    FileVariable var;
    var.Name        = String_ReserveAndCopy(Arena, MAX_KEY_LENGTH, Name);
    var.Value       = String_ReserveAndCopy(Arena, MAX_VALUE_LENGTH, Value);
    var.SpecialData = String_IsValid(SpecialData) ? String_ReserveAndCopy(Arena, MAX_META_KEY_LENGTH, SpecialData) : String_Null();
    var.bHasSpecial = bHasSpecial;

    Array_Add(VariablesDB, var);
}

ENUM(ETokenType)
{
    Token_None = 0,

    // single-char tokens
    Token_LParen,
    Token_RParen,
    Token_LCurly,
    Token_RCurly,
    Token_LSquare,
    Token_RSquare,
    Token_FSlash,
    Token_BSlash,
    //Token_Star,
    Token_Caret,
    Token_Mod,
    Token_Pipe,
    Token_Colon,
    Token_Semicolon,
    Token_Not,
    Token_At,
    Token_Dollar,
    Token_GreaterThan,
    Token_LessThan,

    // two-char tokens
    Token_GreaterOrEqual,
    Token_LessOrEqual,
    Token_NotEqual,
    Token_EqualEqual,
    Token_Equal,

    // Literals
    Token_Text,

    // Reserved Keywords
    Token_Artifact,
    Token_Target,
    Token_Include,
    Token_If,
    Token_Else,
    Token_And,
    Token_Or,
    Token_Goto,
    Token_Contains,
    Token_StartsWith,
    Token_EndsWith,
    Token_Stop,
    Token_Abort,
    Token_Help,
    Token_ErrorMessage,

    Token_Newline,

    Token_Max
};

STRUCT(Token)
{
    usize      Line;
    String     Lexeme;
    ETokenType Type;
    u8         Padding[7];
};

static Token Token_Null(void)
{
    return (Token)
    {
        .Line = 0,
        .Lexeme = S(""),
        .Type = Token_None
    };
}

static String TokenTypeEnumStringTable_NoPrefix[Token_Max] =
{
    SC("None"),

    // single-char tokens
    SC("LParen"),
    SC("RParen"),
    SC("LCurly"),
    SC("RCurly"),
    SC("LSquare"),
    SC("RSquare"),
    SC("FSlash"),
    SC("BSlash"),
    //SC("Star"),
    SC("Caret"),
    SC("Mod"),
    SC("Pipe"),
    SC("Colon"),
    SC("Semicolon"),
    SC("Not"),
    SC("At"),
    SC("Dollar"),
    SC("GreaterThan"),
    SC("LessThan"),

    // two-char tokens
    SC("GreaterOrEqual"),
    SC("LessOrEqual"),
    SC("NotEqual"),
    SC("EqualEqual"),
    SC("Equal"),

    // Literals
    SC("Text"),

    // Reserved Keywords
    SC("Artifact"),
    SC("Target"),
    SC("Include"),
    SC("If"),
    SC("Else"),
    SC("And"),
    SC("Or"),
    SC("Goto"),
    SC("Contains"),
    SC("StartsWith"),
    SC("EndsWith"),
    SC("Stop"),
    SC("Abort"),
    SC("Help"),
    SC("ErrorMessage"),

    SC("Newline"),
};

static String TokenTypeEnumStringTable[Token_Max] =
{
    SC("Token_None"),

    // single-char tokens
    SC("Token_LParen"),
    SC("Token_RParen"),
    SC("Token_LCurly"),
    SC("Token_RCurly"),
    SC("Token_LSquare"),
    SC("Token_RSquare"),
    SC("Token_FSlash"),
    SC("Token_BSlash"),
    //SC("Token_Star"),
    SC("Token_Caret"),
    SC("Token_Mod"),
    SC("Token_Pipe"),
    SC("Token_Colon"),
    SC("Token_Semicolon"),
    SC("Token_Not"),
    SC("Token_At"),
    SC("Token_Dollar"),
    SC("Token_GreaterThan"),
    SC("Token_LessThan"),

    // two-char tokens
    SC("Token_GreaterOrEqual"),
    SC("Token_LessOrEqual"),
    SC("Token_NotEqual"),
    SC("Token_EqualEqual"),
    SC("Token_Equal"),

    // Literals
    SC("Token_Text"),

    // Reserved Keywords
    SC("Token_Artifact"),
    SC("Token_Target"),
    SC("Token_Include"),
    SC("Token_If"),
    SC("Token_Else"),
    SC("Token_And"),
    SC("Token_Or"),
    SC("Token_Goto"),
    SC("Token_Contains"),
    SC("Token_StartsWith"),
    SC("Token_EndsWith"),
    SC("Token_Stop"),
    SC("Token_Abort"),
    SC("Token_Help"),
    SC("Token_ErrorMessage"),

    SC("Token_Newline"),
};

ENUM_TYPED(ENodeType, u32)
{
    Node_None,
    Node_Block,
    Node_If,
    Node_Help,
    Node_Include,
    Node_Assert,
    Node_KeyValue,
};

STRUCT(NodeList)
{
    struct Node* Node;
    NodeList* Next;
};

STRUCT(Node)
{
    ENodeType   Type;
    ETokenType  ComparisonOp;
    bool        bIsSpecial;
    u8          Padding[2];

    String*     Key;
    StringList* Value;

    String      Condition;

    Node*       Left;
    Node*       Right;

    NodeList*   List;
};

static Node Node_Null = { .Type = Node_None, .Left = &Node_Null, .Right = &Node_Null };

NO_DISCARD static NodeList* NodeList_Create(LinearAllocator* Arena, Node* Node, NodeList* Next)
{
    NodeList* List = LinearAllocator_Allocate(Arena, sizeof(struct NodeList));
    List->Node     = Node;
    List->Next     = Next;
    return List;
}

NO_DISCARD static NodeList* NodeList_CreateNull(LinearAllocator* Arena)
{
    NodeList* List = LinearAllocator_Allocate(Arena, sizeof(struct NodeList));
    List->Node     = NULL;
    List->Next     = NULL;
    return List;
}

NO_DISCARD static Node* Node_Create(LinearAllocator* Arena, ENodeType Type)
{
    Node* Node = LinearAllocator_Allocate(Arena, sizeof(struct Node));
    Node->Type = Type;
    return Node;
}

NO_DISCARD static Node* Node_Create_KeyValue(LinearAllocator* Arena, String* Key, StringList* Value, bool bIsSpecial)
{
    Node* Node       = LinearAllocator_Allocate(Arena, sizeof(struct Node));
    Node->Type       = Node_KeyValue;
    Node->Key        = Key;
    Node->Value      = Value;
    Node->bIsSpecial = bIsSpecial;
    return Node;
}

STRUCT(KeywordTableEntry)
{
    String     Name;
    ETokenType Type;
    u8         Padding[7];
};

static KeywordTableEntry ReservedKeywordsTable[14] =
{
    { .Type = Token_If,          .Name = SC("if")          },
    { .Type = Token_Else,        .Name = SC("else")        },
    { .Type = Token_Include,     .Name = SC("include")     },
    { .Type = Token_And,         .Name = SC("and")         },
    { .Type = Token_Or,          .Name = SC("or")          },
    { .Type = Token_Goto,        .Name = SC("goto")        },
    { .Type = Token_Artifact,    .Name = SC("artifact")    },
    { .Type = Token_Target ,     .Name = SC("target")      },
    { .Type = Token_Contains ,   .Name = SC("contains")    },
    { .Type = Token_StartsWith,  .Name = SC("starts_with") },
    { .Type = Token_EndsWith,    .Name = SC("ends_with")   },
    { .Type = Token_Stop,        .Name = SC(".stop")       },
    { .Type = Token_Abort,       .Name = SC(".abort")      },
    { .Type = Token_Help,        .Name = SC(".help")       },
};

static KeywordTableEntry ReservedEndingKeywordsTable[1] =
{
    { .Type = Token_ErrorMessage, .Name = SC(".ErrorMessage")},
};

static String ReservedKeys[] =
{
    SC("Compiler"),
};

static String ReservedKeys_CanMakeScopeBlock[] =
{
    SC("Linker"),
    SC("Assert"),
    SC("Bundle"),
    SC("PreDepend"),
    SC("PreBuild"),
    SC("PostBuild"),
    SC("PreCompile"),
    SC("PostCompile"),
    SC("PreLink"),
    SC("PostLink"),
};

static ETokenType DisallowedInIfElseBlock[1] =
{
    Token_Help,
};

UNUSED static bool Parser_IsTokenDisallowedInIfElseBlock(ETokenType Type)
{
    bool bSuccess = false;

    for (u8 i = 0; i < SArray_Capacity(DisallowedInIfElseBlock); i++)
    {
        if (Type == DisallowedInIfElseBlock[i])
        {
            bSuccess = true;
            break;
        }
    }

    return bSuccess;
}

static String ETokenType_ToString(ETokenType Type)
{
    String Result = String_Null();

    if (Type < Token_Max)
    {
        Result = TokenTypeEnumStringTable[Type];
    }

    return Result;
}

static String ETokenTypeNoPrefix_ToString(ETokenType Type)
{
    String Result = String_Null();

    if (Type < Token_Max)
    {
        Result = TokenTypeEnumStringTable_NoPrefix[Type];
    }

    return Result;
}

static void Lexer_Advance(u32* Current, u32 Length)
{
    if (*Current < Length)
    {
        *Current += 1;
    }
}

static uchar Lexer_Peek(String Text, u32 Current)
{
    uchar Char = 0;
    if (Current < Text.Length)
    {
        Char = Text.Data[Current];
    }

    return Char;
}

static bool Lexer_Match(u32* Current, String Text, uchar Expected)
{
    bool bResult = false;

    if (*Current < Text.Length)
    {
        if (Text.Data[*Current] == Expected)
        {
            *Current += 1;
            bResult = true;
        }
    }

    return bResult;
}

static void Parser_Advance(u32* Current, u32 Length)
{
    if (*Current < Length)
    {
        *Current += 1;
    }
}

static Token Parser_Peek(TArray(Token) Tokens, u32 Current)
{
    Token Tok = Token_Null();
    if (Current < Array_Num(Tokens))
    {
        Tok = Tokens[Current];
    }

    return Tok;
}

static Token Parser_LookBack(TArray(Token) Tokens, u32 Current)
{
    Token Tok = Token_Null();
    if (Current > 0)
    {
        Tok = Tokens[Current-1];
    }

    return Tok;
}

static bool Parser_Match(TArray(Token) Tokens, u32* Current, ETokenType Expected)
{
    bool bResult = false;

    if (*Current < Array_Num(Tokens))
    {
        if (Tokens[*Current].Type == Expected)
        {
            *Current += 1;
            bResult = true;
        }
    }

    return bResult;
}

static void Lexer_AddToken(TArray(Token) Tokens, u32 Current, u32 Start, u32 Line, String Text, ETokenType Type)
{
    u32 Diff = ClampMin(Current - Start, 1);

    Token NewToken  = {0};
    NewToken.Type   = Type;
    NewToken.Lexeme = Text.Length == 0 ? Text : StrSub(Text, Start, Diff);
    NewToken.Line   = Line;

    Array_Add(Tokens, NewToken);
}

static bool IsValidTextToken(uchar Char, bool bAllowWhitespace)
{
    bool bSymbols = Char == '%' || Char == '$' || Char == '@' || Char == '!' || Char == '(' || Char == ')' || Char == '|' || Char == '{' || Char == '}';
    bool bWhitespaceValid = (bAllowWhitespace || !bAllowWhitespace && !IsWhitespace(Char));
    bool bValid = bWhitespaceValid && !bSymbols;

    return bValid;
}

static ETokenType PeekLastTokenType(TArray(Token) Tokens)
{
    ETokenType Result = Token_None;

    if (Array_Num(Tokens) > 0)
    {
        Result = Array_Last(Tokens).Type;
    }

    return Result;
}

ENUM(ELexerState)
{
    LexerState_Key,
    LexerState_Value,
    LexerState_If,
    LexerState_IfAfterIdent,
};

#define SLL_Push(List, Entry) \
            *(List) = Entry; \
            List = &(*List)->Next

static Node* Parse_Special_Help(LinearAllocator* Arena, u32* Current, u32 NumTokens, TArray(Token) Tokens)
{
    Node* Root = Node_Create(Arena, Node_Help);

    //u32 Start = *Current;

    //LOG("[KEY]          %S", Tokens[Start].Lexeme);

    Parser_Advance(Current, NumTokens);

    //LOG_INLINE("[VALUE]       ");

    StringList* ValueList = LinearAllocator_Allocate(Arena, sizeof(StringList));
    StringList** Next = &ValueList;

    if (Parser_Match(Tokens, Current, Token_LCurly))
    {
        while (Parser_Peek(Tokens, *Current).Type != Token_RCurly)
        {
            String Lexeme = Parser_Peek(Tokens, *Current).Lexeme;

            /*
            StringList* Entry = LinearAllocator_Allocate(Arena, sizeof(StringList));
            Entry->String = Lexeme;
            Entry->Next = NULL;

            *Next = Entry;
            Next = &(*Next)->Next;

            SLL_Push(*Next, Entry);
            */

            SLL_Push(Next, StringList_Create(Arena, Lexeme, NULL));

            Parser_Advance(Current, NumTokens);
        }

        Parser_Advance(Current, NumTokens);

        //LOG_LINE_BREAK();
    }
    else
    {
        while (!(Parser_Peek(Tokens, *Current).Type == Token_Newline   ||
                Parser_Peek(Tokens, *Current).Type == Token_Semicolon))
        {
            //LOG_INLINE("%S ", Parser_Peek(Tokens, *Current).Lexeme);

            String Lexeme = Parser_Peek(Tokens, *Current).Lexeme;

            /*
            StringList* Entry = LinearAllocator_Allocate(Arena, sizeof(StringList));
            Entry->String = Lexeme;
            Entry->Next = NULL;

            *Next = Entry;
            Next = &(*Next)->Next;
            */

            SLL_Push(Next, StringList_Create(Arena, Lexeme, NULL));

            Parser_Advance(Current, NumTokens);
        }

        //LOG_LINE_BREAK();
    }

    Root->Value = ValueList;

    return Root;
}

static Node* Parse_If(LinearAllocator* Arena,
                    const String WorkingDirectory,
                    TArray(FileVariable) VariablesDB,
                    TArray(FileVariable) ExpandedVariablesDB,
                    TArray(CmdOption) CmdOptionsDB,
                    TArray(Token) Tokens,
                    u32 Offset,
                    u32* NumTokensParsed,
                    bool bCameFromInline);


static Node* Parse_Block(LinearAllocator* Arena,
                    const String WorkingDirectory,
                    TArray(FileVariable) VariablesDB,
                    TArray(FileVariable) ExpandedVariablesDB,
                    TArray(CmdOption) CmdOptionsDB,
                    TArray(Token) Tokens,
                    u32 Offset,
                    u32* NumTokensParsed,
                    bool bInIf
                    );

static Node* Parse_Include(LinearAllocator* Arena,
                           TArray(Token) Tokens,
                           u32 NumTokens,
                           u32 Offset,
                           u32* NumTokensParsed)
{
    Node* Root = Node_Create(Arena, Node_Include);

    u32 Current = Offset;

    Parser_Advance(&Current, NumTokens);

    StringList* ValueList = LinearAllocator_Allocate(Arena, sizeof(StringList));
    StringList** Next = &ValueList;

    bool bFoundTokens = false;
    while (Parser_Peek(Tokens, Current).Type == Token_Text    ||
           Parser_Peek(Tokens, Current).Type == Token_At      ||
           Parser_Peek(Tokens, Current).Type == Token_Mod     ||
           Parser_Peek(Tokens, Current).Type == Token_LParen  ||
           Parser_Peek(Tokens, Current).Type == Token_RParen  ||
           Parser_Peek(Tokens, Current).Type == Token_FSlash  ||
           Parser_Peek(Tokens, Current).Type == Token_BSlash  ||
           Parser_Peek(Tokens, Current).Type == Token_Dollar)
    {
        bFoundTokens = true;

        String Lexeme = Parser_Peek(Tokens, Current).Lexeme;
        SLL_Push(Next, StringList_Create(Arena, Lexeme, NULL));

        Parser_Advance(&Current, NumTokens);
    }

    Root->Value = ValueList;

    if (!bFoundTokens)
    {
        LOG_ERROR("[Parser] [Line %u]: '%S' was unexpected after 'include'. Expected a file path or expression.", Tokens[Offset].Line, Parser_Peek(Tokens, Current).Lexeme);
        return &Node_Null;
    }

    if (NumTokensParsed)
    {
        *NumTokensParsed = Current - Offset;
    }

    return Root;
}


static Node* Parse_If(LinearAllocator* Arena,
                    const String WorkingDirectory,
                    TArray(FileVariable) VariablesDB,
                    TArray(FileVariable) ExpandedVariablesDB,
                    TArray(CmdOption) CmdOptionsDB,
                    TArray(Token) Tokens,
                    u32 Offset,
                    u32* NumTokensParsed,
                    bool bCameFromInline)
{
    Node* Root = Node_Create(Arena, Node_If);

    u32 Current = Offset;
    u32 Start = 0;
    u32 NumTokens = (u32)Array_Num(Tokens);
    bool bInlineIf = false;
    ETokenType LastTokenType = Token_If;
    while (Current < NumTokens)
    {
        Start = Current;
        const Token t = Tokens[Current];

        if (Root->Left && !Root->Right) // we have an 'if' but no 'else' yet...
        {
            // stop if we see something other than 'else'
            if (t.Type != Token_Newline &&
                t.Type != Token_Semicolon &&
                t.Type != Token_LCurly &&
                t.Type != Token_RCurly &&
                t.Type != Token_Else
                )
            {
                break;
            }
        }

        if (t.Type == Token_Newline)
        {
            if (bInlineIf)
            {
                bInlineIf = false;
                break;
            }

            if (bCameFromInline)
            {
                break;
            }
        }
        else if (t.Type == Token_Semicolon)
        {
        }
        else if (t.Type == Token_RCurly)
        {
            break;
        }
        else if (t.Type == Token_LCurly)
        {
            u32 NumParsed = 0;
            Node* BlockNode = Parse_Block(Arena, WorkingDirectory, VariablesDB, ExpandedVariablesDB, CmdOptionsDB, Tokens, Current+1, &NumParsed, false);
            if (BlockNode == &Node_Null)
            {
                return &Node_Null;
            }

            Current += NumParsed+1;

            if (!Parser_Match(Tokens, &Current, Token_RCurly))
            {
                LOG_ERROR("[Parser] [Line %u]: '}' is missing for 'if' block.", Parser_Peek(Tokens, Current-1).Line);
                return &Node_Null;
            }

            if (LastTokenType == Token_If)
            {
                Root->Left = BlockNode;
            }
            else
            {
                Root->Right = BlockNode;
            }

            //if (bCameFromInline)
            //{
                //break;
            //}
        }
        else if (t.Type == Token_Else)
        {
            LastTokenType = Token_Else;

            Parser_Advance(&Current, NumTokens);

            if (bInlineIf)
            {
                // todo: make sure ther eis no lcurly
                //bool bHasCurly = Parser_Match(Tokens, &Current, Token_LCurly);

                u32 NumParsed = 0;
                Node* BlockNode = Parse_Block(Arena, WorkingDirectory, VariablesDB, ExpandedVariablesDB, CmdOptionsDB, Tokens, Current, &NumParsed, true);
                if (BlockNode == &Node_Null)
                {
                    return &Node_Null;
                }

                Current += NumParsed;

                Root->Right = BlockNode;

                if (Parser_Match(Tokens, &Current, Token_Newline) ||
                    Parser_Match(Tokens, &Current, Token_Semicolon))
                {
                    break;
                }
            }
            else
            {
            }

            bInlineIf = false;

        }
        else if (t.Type == Token_If)
        {
            break;
        }
        // evaluate if conditions
        else if (t.Type == Token_Text  ||
                 t.Type == Token_Not   ||
                 t.Type == Token_At    ||
                 t.Type == Token_Mod   ||
                 t.Type == Token_Dollar)
        {
            if (bInlineIf)
            {
                bInlineIf = false;
                break;
            }

            Token NextToken = t;
            while (NextToken.Type == Token_Text   ||
                   NextToken.Type == Token_Not    ||
                   NextToken.Type == Token_At     ||
                   NextToken.Type == Token_Mod    ||
                   NextToken.Type == Token_Dollar ||
                   NextToken.Type == Token_Pipe)
            {
                u8 Prefixes = 0;

                while (Parser_Match(Tokens, &Current, Token_Not)    ||
                       Parser_Match(Tokens, &Current, Token_Dollar) ||
                       Parser_Match(Tokens, &Current, Token_Mod)    ||
                       Parser_Match(Tokens, &Current, Token_At))
                {
                    ETokenType Peek = Parser_Peek(Tokens, Current-1).Type;
                    
                    if      (Peek == Token_Not)    { Prefixes |= BIT(1); }
                    else if (Peek == Token_Dollar) { Prefixes |= BIT(2); }
                    else if (Peek == Token_Mod)    { Prefixes |= BIT(3); }
                    else if (Peek == Token_At)     { Prefixes |= BIT(4); }
                    else    {}
                }

                //const String Condition = Parser_Peek(Tokens, Current).Lexeme;
                //LOG("%S", Condition);

                Root->Key = &Tokens[Current].Lexeme;

                Parser_Advance(&Current, NumTokens);

                Token Comparison = Parser_Peek(Tokens, Current);

                if (Comparison.Type == Token_If)
                {
                    u32 NumParsed = 0;
                    Node* IfNode = Parse_If(Arena, WorkingDirectory, VariablesDB, ExpandedVariablesDB, CmdOptionsDB, Tokens, Current+1, &NumParsed, true);
                    if (IfNode == &Node_Null)
                    {
                        return &Node_Null;
                    }

                    Current += NumParsed+1;

                    Root->Left = IfNode;

                    bInlineIf = true;
                    break;
                }
                else
                {
                    //String TestValue = String_Null();
                    if (Comparison.Type == Token_EqualEqual     || Comparison.Type == Token_NotEqual    ||
                        Comparison.Type == Token_GreaterOrEqual || Comparison.Type == Token_LessOrEqual ||
                        Comparison.Type == Token_GreaterThan    || Comparison.Type == Token_LessThan    ||
                        Comparison.Type == Token_StartsWith     || Comparison.Type == Token_EndsWith    ||
                        Comparison.Type == Token_Contains)
                    {
                        Parser_Advance(&Current, NumTokens);

                        Token TestToken = Parser_Peek(Tokens, Current);
                        if (TestToken.Type == Token_Text)
                        {
                            while (1)
                            {
                                // do stuff...

                                Parser_Advance(&Current, NumTokens);
                                if (Parser_Peek(Tokens, Current).Type == Token_Pipe)
                                {
                                    Parser_Advance(&Current, NumTokens);
                                    TestToken = Parser_Peek(Tokens, Current);

                                    if (TestToken.Type != Token_Text)
                                    {
                                        LOG_ERROR("[Parser] [Line %u]: '%S' was unexpected after '|'. Please delete.", TestToken.Line, TestToken.Lexeme);
                                        return &Node_Null;
                                    }
                                }
                                else
                                {
                                    break;
                                }
                            }
                        }
                        else
                        {
                            LOG_ERROR("[Parser] [Line %u]: '%S' was unexpected after '%S'. Please delete.", TestToken.Line, TestToken.Lexeme, Comparison.Lexeme);
                            return &Node_Null;
                        }
                    }



                    /*
                    String ConditionValuePtr = String_Null();
                    StringLocal(EnvValue, 1024);

                    bool bNot                = Prefixes & BIT(1);
                    bool bSearchUserVar      = Prefixes & BIT(2);
                    bool bSearchCmdVar       = Prefixes & BIT(3);
                    bool bSearchEnv          = Prefixes & BIT(4);
                    bool bPrefixedWithSymbol = bSearchUserVar || bSearchCmdVar || bSearchEnv;

                    (void)bNot;

                    bool bConditionMet = false;
                    bool bIsPath = String_ContainsPathSeparators(Condition);
                    if (bIsPath)
                    {
                        bool bIsDirectory = String_IsLast(Condition, '/') || String_IsLast(Condition, '\\');

                        StringLocal(Temp, MAX_PATH_LENGTH);
                        if (Filesystem_IsPathRelative(Condition))
                        {
                            String_BuildPath(&Temp, WorkingDirectory, Condition);
                        }
                        else
                        {
                            String_Copy(&Temp, Condition);
                        }

                        if (bIsDirectory)
                        {
                            bConditionMet = Filesystem_DoesDirectoryExist(Temp);
                        }
                        else
                        {
                            bConditionMet = Filesystem_DoesFileExist(Temp);
                        }
                    }
                    else
                    {
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

                        if (!bConditionMet && (bSearchCmdVar || !bPrefixedWithSymbol))
                        {
                            // check the condition string against the internal build vars passed in from the command line
                            // override VarValue for single line if's, for multiline if's, loop back to the top and process each line until '}' is found
                            for each (CmdOption, o, CmdOptionsDB)
                            {
                                bool bMatch = String_IsEqual(o.Name, Condition, false);
                                if (bMatch)
                                {
                                    if (!o.bEqualsToSomething || o.Value.Length > 0) // make sure we have some value if we specified an '=' sign
                                    {
                                        ConditionValuePtr = o.Value;
                                        bConditionMet = true;
                                        break;
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
                        }

                        if (!bConditionMet && (bSearchUserVar || !bPrefixedWithSymbol))
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
                    }

                    (void)ConditionValuePtr;
                    */

                    // TODO: token assert
                    if (Parser_Peek(Tokens, Current).Type == Token_Text ||
                        Parser_Peek(Tokens, Current).Type == Token_Help ||
                        Parser_Peek(Tokens, Current).Type == Token_Include ||
                        Parser_Peek(Tokens, Current).Type == Token_Stop ||
                        Parser_Peek(Tokens, Current).Type == Token_Abort ||
                        Parser_Peek(Tokens, Current).Type == Token_Goto)
                    {
                        u32 NumParsed = 0;
                        Node* BlockNode = Parse_Block(Arena, WorkingDirectory, VariablesDB, ExpandedVariablesDB, CmdOptionsDB, Tokens, Current, &NumParsed, true);
                        if (BlockNode == &Node_Null)
                        {
                            return &Node_Null;
                        }

                        Current += NumParsed;

                        Root->Left = BlockNode;

                        bInlineIf = true;

                        break;
                    }

                    if (Parser_Peek(Tokens, Current).Type == Token_Pipe)
                    {
                        Parser_Advance(&Current, NumTokens);
                    }
                }

                NextToken = Parser_Peek(Tokens, Current);
            }
        }
        else
        {
            if (t.Lexeme.Length > 0)
            {
                LOG_ERROR("[Parser] [Line %u]: '%S' was unexpected after 'if'", t.Line, t.Lexeme);
            }
            else
            {
                LOG_ERROR("[Parser] [Line %u]: Missing expression after 'if'", t.Line);
            }

            return &Node_Null;
        }

        // only advance when nothing happened
        if (Start == Current)
        {
            Parser_Advance(&Current, NumTokens);
        }
    }

    if (NumTokensParsed)
    {
        *NumTokensParsed = Current - Offset;
    }

    return Root;
}

static Node* Parse_Block(LinearAllocator* Arena,
                    const String WorkingDirectory,
                    TArray(FileVariable) VariablesDB,
                    TArray(FileVariable) ExpandedVariablesDB,
                    TArray(CmdOption) CmdOptionsDB,
                    TArray(Token) Tokens,
                    u32 Offset,
                    u32* NumTokensParsed,
                    bool bInIf
                    )
{
    Node* Root = Node_Create(Arena, Node_Block);
    NodeList** NextNode = &Root->List;

    u32 Current = Offset;
    u32 Start = 0;
    u32 NumTokens = (u32)Array_Num(Tokens);
    Token LastRootToken = Token_Null();
    bool bPreviouslyEvaluatedIfStatement = false;
    bool bSkipRootTokenUpdate = false;
    while (Current < NumTokens)
    {
        Start = Current;
        Token t = Tokens[Current];
        Token* tPtr = &Tokens[Current];

        bool bJustEvaluatedIfStatement = false;

        // @todo: make sure to handle all tokens possible

        if (t.Type == Token_Semicolon)
        {

        }
        else if (t.Type == Token_Newline)
        {
            if (bInIf)
            {
                break;
            }
        }
        else if (t.Type == Token_LCurly)
        {
            ETokenType PrevTokenType = LastRootToken.Type;

            if (PrevTokenType != Token_Text)
            {
                LOG_ERROR("[Parser] [Line %u]: Anonymous blocks are not allowed. Missing <key> before '{'.", t.Line);
                return &Node_Null;
            }

            u32 NumParsed = 0;
            NodeList* List = NodeList_CreateNull(Arena);
            Node* BlockNode = Parse_Block(Arena, WorkingDirectory, VariablesDB, ExpandedVariablesDB, CmdOptionsDB, Tokens, Current+1, &NumParsed, bPreviouslyEvaluatedIfStatement);
            List->Node = BlockNode;
            if (BlockNode == &Node_Null)
            {
                return &Node_Null;
            }

            Current += NumParsed+1;

            if (!Parser_Match(Tokens, &Current, Token_RCurly))
            {
                LOG_ERROR("[Parser] [Line %u]: '}' is missing for '%S' block.", Parser_Peek(Tokens, Current-1).Line, ETokenTypeNoPrefix_ToString(PrevTokenType));
                return &Node_Null;
            }

            SLL_Push(NextNode, List);
        }
        else if (t.Type == Token_RCurly)
        {
            break;
        }
        else if (t.Type == Token_Help)
        {
            NodeList* List = NodeList_CreateNull(Arena);
            Node* HelpNode = Parse_Special_Help(Arena, &Current, NumTokens, Tokens);
            List->Node = HelpNode;
            if (HelpNode == &Node_Null)
            {
                return &Node_Null;
            }

            SLL_Push(NextNode, List);
        }
        else if (t.Type == Token_ErrorMessage)
        {
            LOG("TODO: handle .ErrorMessage");

            // this is an error
            if (String_IsEqual(t.Lexeme, S(".ErrorMessage"), false))
            {
                LOG_ERROR("[Parser] [Line %u]: '%S' must be paired with a key.", t.Line, t.Lexeme);
                
                String Spaces = S("     ");
                LOG_INLINE_WARNING("\n%SExample of valid syntax\n\n", Spaces);
                LOG("%S      Arg.something.ErrorMessage this is some error message", Spaces);
                LOG_INLINE_WARNING("%S   or\n", Spaces);
                LOG("%S      Arg.something.ErrorMessage {\n%S         this is some error message\n%S      }", Spaces, Spaces, Spaces);
                return &Node_Null;
            }

        }
        else if (t.Type == Token_Text)
        {
            ETokenType Prev = Parser_LookBack(Tokens, Current).Type;
            if (Prev == Token_None ||
                Prev == Token_Max ||
                Prev == Token_Else ||
                Prev == Token_Newline ||
                Prev == Token_Semicolon ||
                Prev == Token_LCurly ||
                bPreviouslyEvaluatedIfStatement ||
                bInIf) // TODO: rename
            {
                // this means we are the key
                bool bIsSpecial = false;
                if (Parser_Match(Tokens, &Current, Token_Text))
                {
                    if (Parser_Match(Tokens, &Current, Token_Not))
                    {
                        bIsSpecial = true;
                    }
                }

                if (!IsAlphabet(t.Lexeme.Data[0]))
                {
                    LOG_ERROR("[Parser] [Line %u]: Key '%S' can only start with an alphabet character. Please remove '%c'", t.Line, t.Lexeme, t.Lexeme.Data[0]);
                    return &Node_Null;
                }

                StringList* ValueList = LinearAllocator_Allocate(Arena, sizeof(StringList));
                StringList** NextValue = &ValueList;

                bool bFoundTokens = false;
                // we are the value to that key, blast through to the end of line or semicolon (whichever is first)
                while (!(Parser_Peek(Tokens, Current).Type == Token_Newline   ||
                         Parser_Peek(Tokens, Current).Type == Token_Semicolon ||
                         Parser_Peek(Tokens, Current).Type == Token_LCurly    ||
                         Parser_Peek(Tokens, Current).Type == Token_RCurly    ||
                         Parser_Peek(Tokens, Current).Type == Token_Else))
                {
                    bFoundTokens = true;

                    String Lexeme = Parser_Peek(Tokens, Current).Lexeme;

                    SLL_Push(NextValue, StringList_Create(Arena, Lexeme, NULL));

                    Parser_Advance(&Current, NumTokens);
                }

                NodeList* List = NodeList_CreateNull(Arena);
                Node* KV_Node = Node_Create_KeyValue(Arena, &tPtr->Lexeme, ValueList, bIsSpecial);
                List->Node = KV_Node;
                SLL_Push(NextNode, List);

                if (!bFoundTokens)
                {
                    LastRootToken = t;
                }
                else
                {
                    LastRootToken = Token_Null();
                }
                
                bSkipRootTokenUpdate = true;
            }
        }
        else if (t.Type == Token_Include)
        {
            u32 NumParsed = 0;
            NodeList* List = NodeList_CreateNull(Arena);
            Node* IncludeNode = Parse_Include(Arena, Tokens, NumTokens, Current, &NumParsed);
            List->Node = IncludeNode;
            if (IncludeNode == &Node_Null)
            {
                return &Node_Null;
            }

            Current += NumParsed;

            SLL_Push(NextNode, List);
        }
        /*
            if <<prefix?>condition|...> <comparison_operator?> <test?|...>
                                        <key> <value>
                                    | <key> <value> else <key> <value>
                                    | { <key> <value> <stmt-end> ... }
                                    | { <key> <value> <stmt-end> ... } else { <key> <value> <stmt-end> ... }
        */
        else if (t.Type == Token_Else)
        {
            bSkipRootTokenUpdate = true;

            if (!bInIf)
            {
                LOG_ERROR("[Parser] [Line %u]: Illegal 'else' without matching 'if'", t.Line);
                return &Node_Null;
            }

            break;
        }
        else if (t.Type == Token_If)
        {
            bSkipRootTokenUpdate = true;

            u32 NumParsed = 0;
            NodeList* List = NodeList_CreateNull(Arena);
            Node* IfNode = Parse_If(Arena, WorkingDirectory, VariablesDB, ExpandedVariablesDB, CmdOptionsDB, Tokens, Current+1, &NumParsed, false);
            List->Node = IfNode;
            if (IfNode == &Node_Null)
            {
                return &Node_Null;
            }

            Current += NumParsed+1;

            SLL_Push(NextNode, List);

            bJustEvaluatedIfStatement = true;
        }
        else
        {
            LOG_ERROR("[Parser] [Line %u]: Keys can not start with '%S'. Please delete.", t.Line, t.Lexeme);
            return &Node_Null;
        }

        // only advance when nothing happened
        if (Start == Current)
        {
            Parser_Advance(&Current, NumTokens);
        }

        if (!(t.Type == Token_LCurly || t.Type == Token_Newline))
        {
            bPreviouslyEvaluatedIfStatement = bJustEvaluatedIfStatement;
        }

        if (!bSkipRootTokenUpdate)
        {
            if (t.Type != Token_Newline && t.Type != Token_Semicolon)
            {
                LastRootToken = t;
            }
        }

        bSkipRootTokenUpdate = false;
    }

    if (NumTokensParsed)
    {
        *NumTokensParsed = Current - Offset;
    }

    return Root;
}

static Node* Parse_Root(LinearAllocator* Arena,
                    const String WorkingDirectory,
                    TArray(FileVariable) VariablesDB,
                    TArray(FileVariable) ExpandedVariablesDB,
                    TArray(CmdOption) CmdOptionsDB,
                    TArray(Token) Tokens)
{
    return Parse_Block(Arena, WorkingDirectory, VariablesDB, ExpandedVariablesDB, CmdOptionsDB, Tokens, 0, NULL, false);
}

static void Print_BlockNode(NodeList* Root, u32 Level);
static void Print_IfNode(Node* Root, u32 Level);

static void Print_HelpNode(Node* Root, u32 Level)
{
    if (!Root)
    {
        return;
    }

    StringLocal(Spaces, 256);
    for (u32 i = 0; i < Level; i++)
    {
        String_AppendChar(&Spaces, ' ');
    }

    LOG("%SHELP", Spaces);

    if (Root->Value)
    {
        for each_str_list (*Root->Value)
        {
            if (!String_IsValid(It.String))
            {
                continue;
            }

            LOG_INLINE("%S", It.String);
        }
        LOG_LINE_BREAK();
    }

    LOG("%SEND HELP", Spaces);
}

static void Print_IncludeNode(Node* Root, u32 Level)
{
    if (!Root)
    {
        return;
    }

    StringLocal(Spaces, 256);
    for (u32 i = 0; i < Level; i++)
    {
        String_AppendChar(&Spaces, ' ');
    }

    LOG_INLINE("%SINCLUDE ", Spaces);
    
    if (Root->Value)
    {
        for each_str_list (*Root->Value)
        {
            if (!String_IsValid(It.String))
            {
                continue;
            }

            LOG_INLINE("%S", It.String);
        }
        LOG_LINE_BREAK();
    }
}

static void Print_BlockNode(NodeList* List, u32 Level)
{
    if (!List)
    {
        return;
    }

    if (!List->Node)
    {
        return;
    }

    StringLocal(Spaces, 256);
    for (u32 i = 0; i < Level; i++)
    {
        String_AppendChar(&Spaces, ' ');
    }

    LOG("%SBLOCK", Spaces);

    NodeList** Next = &List;
    while (*Next)
    {
        Node* Root = (*Next)->Node;

        if (Root && Root != &Node_Null)
        {
            if (Root->Type == Node_Block)
            {
                Print_BlockNode(Root->List, Level+1);
            }
            else if (Root->Type == Node_If)
            {
                Print_IfNode(Root, Level+1);
            }
            else if (Root->Type == Node_Help)
            {
                Print_HelpNode(Root, Level+1);
            }
            else if (Root->Type == Node_Include)
            {
                Print_IncludeNode(Root, Level+1);
            }
            else if (Root->Type == Node_KeyValue)
            {
                if (Root->bIsSpecial)
                {
                    LOG("%S [SPECIAL MOD]    ", Spaces);
                }

                if (Root->Key)
                {
                    LOG("\n%S [KEY]      %S", Spaces, *Root->Key);
                }

                if (Root->Value)
                {
                    LOG_INLINE("%S [VALUE]    ", Spaces);
                    for each_str_list (*Root->Value)
                    {
                        LOG_INLINE("%S ", It.String);
                    }
                    LOG_LINE_BREAK();
                }
            }
        }

        Next = &(*Next)->Next;
    }


    LOG("%SEND BLOCK", Spaces);
}

static void Print_IfNode(Node* Root, u32 Level)
{
    if (!Root)
    {
        return;
    }

    if (Root == &Node_Null)
    {
        return;
    }

    StringLocal(Spaces, 256);
    for (u32 i = 0; i < Level; i++)
    {
        String_AppendChar(&Spaces, ' ');
    }

    LOG("%SIF %S", Spaces, *Root->Key);

    Node* Left = Root->Left;
    Node* Right = Root->Right;

    if (Left)
    {
        LOG("%S PATH A", Spaces);
        if (Left->Type == Node_Block)
        {
            Print_BlockNode(Left->List, Level+2);
        }

        if (Left->Type == Node_If)
        {
            Print_IfNode(Left, Level+2);
        }
    }

    if (Right)
    {
        LOG("%S PATH B", Spaces);
        if (Right->Type == Node_Block)
        {
            Print_BlockNode(Right->List, Level+2);
        }

        if (Right->Type == Node_If)
        {
            Print_IfNode(Right, Level+2);
        }
    }

    LOG("%SEND IF", Spaces);
}

bool ParseBuildFileV2(LinearAllocator* Arena,
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
    {
        *ReturnCode = 0;
    }

    StringLocal(Text, Kibibytes(48));
    
    usize Length = 0;
    if (Filesystem_ReadEntireFile(H, Text.Data, &Length))
    {
        Text.Length = (u32)Min(Length, Kibibytes(48));

        // tokenize the text
        u32 Start = 0;
        u32 Current = 0;
        u32 Line = 1;
        bool bAllowWhitespace = false;
        ArrayLocal_Arena(Token, Tokens, 1024, Arena);
        while (Current < Text.Length)
        {
            Start = Current;
            
            const uchar Char = Text.Data[Current];
            Lexer_Advance(&Current, Text.Length);

            if (Char == '\n')
            {
                Line += 1;
            }

            ETokenType LastTokenType = PeekLastTokenType(Tokens);

            if      (Char == '(') { Lexer_AddToken(Tokens, Current, Start, Line, Text, Token_LParen);       }
            else if (Char == ')') { Lexer_AddToken(Tokens, Current, Start, Line, Text, Token_RParen);       }
            else if (Char == '[') { Lexer_AddToken(Tokens, Current, Start, Line, Text, Token_LSquare);      }
            else if (Char == ']') { Lexer_AddToken(Tokens, Current, Start, Line, Text, Token_RSquare);      }
            else if (Char == '/') { Lexer_AddToken(Tokens, Current, Start, Line, Text, Token_FSlash);      }
            else if (Char == '\\') { Lexer_AddToken(Tokens, Current, Start, Line, Text, Token_BSlash);      }
            //else if (Char == '*') { Lexer_AddToken(Tokens, Current, Start, Line, Text, Token_Star);         }
            else if (Char == '^') { Lexer_AddToken(Tokens, Current, Start, Line, Text, Token_Caret);        }
            else if (Char == ';') { Lexer_AddToken(Tokens, Current, Start, Line, Text, Token_Semicolon);    }
            else if (Char == '$') { Lexer_AddToken(Tokens, Current, Start, Line, Text, Token_Dollar);       }
            else if (Char == '@') { Lexer_AddToken(Tokens, Current, Start, Line, Text, Token_At);           }
            else if (Char == '|') { Lexer_AddToken(Tokens, Current, Start, Line, Text, Token_Pipe);         }
            else if (Char == '%') { Lexer_AddToken(Tokens, Current, Start, Line, Text, Token_Mod);          }
            else if (Char == '{')
            {
                if (LastTokenType == Token_Help || LastTokenType == Token_ErrorMessage)
                {
                    bAllowWhitespace = true;
                }

                Lexer_AddToken(Tokens, Current, Start, Line, Text, Token_LCurly);
            }
            else if (Char == '}')
            {
                if (bAllowWhitespace)
                {
                    bAllowWhitespace = false;
                }

                Lexer_AddToken(Tokens, Current, Start, Line, Text, Token_RCurly);
            }
            else if (Char == '#')
            {
                bool bIsMultiLine = Lexer_Match(&Current, Text, '#');
                if (bIsMultiLine)
                {
                    while (1)
                    {
                        uchar PeekChar = Lexer_Peek(Text, Current);
                        if (PeekChar == '#')
                        {
                            (void)Lexer_Advance(&Current, Text.Length);
                            if (Lexer_Match(&Current, Text, '#'))
                            {
                                break;
                            }
                        }

                        if (PeekChar == 0)
                        {
                            break;
                        }

                        if (PeekChar == '\n')
                        {
                            Line += 1;
                        }

                        (void)Lexer_Advance(&Current, Text.Length);
                    }
                }
                else
                {
                    while (!IsNewline(Lexer_Peek(Text, Current)))
                    {
                        (void)Lexer_Advance(&Current, Text.Length);
                    }
                }
            }
            else if (Char == ':')
            {
                Lexer_AddToken(Tokens, Current, Start, Line, Text, Token_Colon);
            }
            else if (Char == '!')
            {
                ETokenType Type = Lexer_Match(&Current, Text, '=') ? Token_NotEqual : Token_Not;
                Lexer_AddToken(Tokens, Current, Start, Line, Text, Type);
            }
            else if (Char == '=')
            {
                ETokenType Type = Lexer_Match(&Current, Text, '=') ? Token_EqualEqual : Token_Equal;
                Lexer_AddToken(Tokens, Current, Start, Line, Text, Type);
            }
            else if (Char == '<')
            {
                ETokenType Type = Lexer_Match(&Current, Text, '=') ? Token_LessOrEqual : Token_LessThan;
                Lexer_AddToken(Tokens, Current, Start, Line, Text, Type);
            }
            else if (Char == '>')
            {
                ETokenType Type = Lexer_Match(&Current, Text, '=') ? Token_GreaterOrEqual : Token_GreaterThan;
                Lexer_AddToken(Tokens, Current, Start, Line, Text, Type);
            }
            else if (Char == '\'' || Char == '"')
            {
                const uchar Letter = Char;
                uchar PeekLetter = Lexer_Peek(Text, Current);

                while (PeekLetter != 0 && PeekLetter != Letter && PeekLetter != '\n')
                {
                    (void)Lexer_Advance(&Current, Text.Length);
                    PeekLetter = Lexer_Peek(Text, Current);

                    // handle quote strings within this string
                    /*
                    PeekLetter = Lexer_Peek(Text, Current);
                    if (PeekLetter == '\\')
                    {
                        (void)Lexer_Advance(&Current, Text.Length);
                        (void)Lexer_Match(&Current, Text, Letter);
                        PeekLetter = Lexer_Peek(Text, Current);
                    }
                    */
                }

                // consume ending quote
                (void)Lexer_Advance(&Current, Text.Length);

                bool bError = PeekLetter == 0 || PeekLetter == '\n';

                if (bError)
                {
                    LOG_ERROR("[Line %u]: Missing closing quote '%c'", Line, Char);
                }
                else
                {
                    Lexer_AddToken(Tokens, Current, Start, Line, Text, Token_Text);
                }
            }
            else if ((!bAllowWhitespace && IsWhitespace(Char)) || IsNewline(Char) || Char == '\0')
            {
                if (Char == '\n')
                {
                    if (LastTokenType != Token_Newline)// && // prevent duplicates as we dont really care
                        //LastTokenType != Token_LCurly &&
                        //LastTokenType != Token_LSquare)
                    {
                        Lexer_AddToken(Tokens, Current, Start, Line, String_Null(), Token_Newline);
                    }
                }
            }
            else //if (IsValidTextToken(Char, bInsideHelp))
            {
                uchar Peek = Lexer_Peek(Text, Current);
                while (IsValidTextToken(Peek, bAllowWhitespace))
                {
                    if (Peek == '\n')
                    {
                        Line += 1;
                    }

                    (void)Lexer_Advance(&Current, Text.Length);
                    Peek = Lexer_Peek(Text, Current);
                }

                u32 Diff = ClampMin(Current - Start, 1);
                String Lexeme = StrSub(Text, Start, Diff);

                ETokenType FinalType = Token_Text;

                for (u8 j = 0; j < SArray_Capacity(ReservedKeywordsTable); j++)
                {
                    if (String_IsEqual(Lexeme, ReservedKeywordsTable[j].Name, false))
                    {
                        FinalType = ReservedKeywordsTable[j].Type;
                        break;
                    }
                }

                for (u8 j = 0; j < SArray_Capacity(ReservedEndingKeywordsTable); j++)
                {
                    if (String_EndsWith(Lexeme, ReservedEndingKeywordsTable[j].Name, false))
                    {
                        FinalType = ReservedEndingKeywordsTable[j].Type;
                        break;
                    }
                }

                Lexer_AddToken(Tokens, Current, Start, Line, Text, FinalType);
            }
            /*
            else
            {
                LOG_ERROR("[Lexer] [Line %u]: Unexpected character '%c'", Line, Char);
                return false;
            }
            */
        }

        for each (Token, t, Tokens)
        {
            if (t.Lexeme.Length > 0)
            {
                LOG("[%u] %S -> %S", t.Line, ETokenType_ToString(t.Type), t.Lexeme);
            }
            else
            {
                LOG("[%u] %S", t.Line, ETokenType_ToString(t.Type));
            }
        }
        LOG("Num Tokens: %u", Array_Num(Tokens));

        Node* AST = Parse_Root(Arena, WorkingDirectory, VariablesDB, ExpandedVariablesDB, CmdOptionsDB, Tokens);
        if (!AST || AST == &Node_Null)
        {
            return false;
        }
        
        Print_BlockNode(AST->List, 0);
    }

    return true;
}

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
    {
        *ReturnCode = 0;
    }

    StringLocal(Line, LINE_BUFFER_SIZE);

    bool bInsideIf = false;
    bool bIfFailed = false;
    bool bInsideElse = false;
    bool bInsideSquareBrackets = false;
    bool bGoto = false;
    bool bInMultiLineComment = false;
    bool bInMultiLineErrorMessage = false;
    bool bInMultiLineHelpMessage = false;

    StringLocal(NamespaceKey, 64);
    StringLocal(GotoValue, 64);
    StringLocal(ErrorMessage_Name, 256);
    StringLocal(ErrorMessage, 4096);
    StringLocal(HelpMessage, 4096);

    EComparisonType Comparison = Cmp_None;

    u16 LineNumber = 0;
    while (Filesystem_ReadLine(H, &Line))
    {
        LineNumber++;

    LoopStart:

        ENSURE(Line.Capacity == LINE_BUFFER_SIZE); // sanity check to make sure no-one is modifying the line buffer

        // TODO: allow whitespace in single line error messages
        if (bInMultiLineErrorMessage)
        {
            // prevent leading/trailing spaces causing confusion if we only have the '}' in the line. its better than doing Line.Data[0]
            String Trimmed = String_EatSpacesFromEnd(String_EatSpaces(Line));
            if (Trimmed.Data[0] == '}')
            {
                (void)String_EatNewLinesInlineFromEnd(&ErrorMessage);

                Internal_AddVariable(Arena, VariablesDB, ErrorMessage_Name, ErrorMessage, String_Null(), false);

                String_Empty(&ErrorMessage);

                bInMultiLineErrorMessage = false;
                continue;
            }
            
            String_Append(&ErrorMessage, Line);
            String_Append(&ErrorMessage, S("\n"));

            continue;
        }

        if (bInMultiLineHelpMessage)
        {
            // prevent leading/trailing spaces causing confusion if we only have the '}' in the line. its better than doing Line.Data[0]
            String Trimmed = String_EatSpacesFromEnd(String_EatSpaces(Line));
            if (Trimmed.Data[0] == '}')
            {
                (void)String_EatNewLinesInlineFromEnd(&HelpMessage);

                Internal_AddVariable(Arena, VariablesDB, S(".help"), HelpMessage, String_Null(), false);

                String_Empty(&HelpMessage);

                bInMultiLineHelpMessage = false;
                continue;
            }
            
            String_Append(&HelpMessage, Line);
            String_Append(&HelpMessage, S("\n"));

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
            bool bHasColon = String_IndexOfChar(Trimmed, ':', &Colon);
            if (!bHasColon)
            {
                continue;
            }

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
                        {
                            continue;
                        }
                    }

                    (void)String_IndexOfLastWhitespace(Else, &Space);
                    String ElseIf = String_EatSpacesFromEnd(StrSlice(Else.Data, Space));
                    if (String_IsEqual(ElseIf, S("else if"), false))
                    {
                        bInsideElse = true;
                        bSeenElse = true;
                        if (bIfFailed)
                        {
                            // extract just the if statement
                            (void)String_IndexOfFirstWhitespace(ElseOg, &Space);

                            bIfFailed = false;
                            bInsideElse = false;
    
                            // do a indirect copy otherwise it will crash on OpenBSD due to overlapping memory
                            StringLocal(LineCopy, LINE_BUFFER_SIZE);
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
                {
                    continue;
                }

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

        // validate
        if (!bInsideSquareBrackets)
        {
            if (VarName.Length > 64)
            {
                LOG_ERROR("Variable name \"%S\" is too long. (%u chars)\n        Max length is 64 characters", VarName, VarName.Length);
                return false;
            }

            if (VarValue.Length > 2048)
            {
                LOG_ERROR("Variable value \"%S\" is too long. (%u chars)\n       Max length is 2048 characters", VarValue, VarValue.Length);
                return false;
            }
        }

        bool bHasSpecial = false;
        u32 ExclamationMarkIndex = 0;
        String SpecialData = String_Null();
        if (String_IndexOfChar(VarName, '!', &ExclamationMarkIndex))
        {
            bHasSpecial = true;
            SpecialData = StrShiftF(VarName, ExclamationMarkIndex+1);
            VarName = StrSlice(VarName.Data, ExclamationMarkIndex);
        }

        const bool bHasOverwrite = String_EatCharInlineFromEnd(&VarName, '`'); // todo: move this to the start?

        // TODO: ignore ! when parsing a preset: var

        if (String_IsEqual(VarName, S("_stop"), false) ||
            String_IsEqual(VarName, S(".stop"), false))
        {
            break;
        }

        if (String_IsEqual(VarName, S("_abort"), false) ||
            String_IsEqual(VarName, S(".abort"), false))
        {
            u32 ExitCode = 0;
            u32 FirstSpace = 0;
            (void)String_IndexOfFirstWhitespace(VarValue, &FirstSpace);

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

                if (ReturnCode) { *ReturnCode = ExitCode; }

                return ExitCode == 0;
            }

            if (VarValue.Length > 0)
            {
                LOG("Exiting with message: %S", VarValue);
            }
            else
            {
                LOG("Exiting...");
            }

            if (ReturnCode) { *ReturnCode = ExitCode; }

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
                (void)String_EatSpacesInlineFromEnd(LastValue);
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

        if (String_IsEqual(VarName, S(".help"), false))
        {
            if (VarValue.Length > 0 && VarValue.Data[0] == '{')
            {
                bInMultiLineHelpMessage = true;
                continue;
            }
        }

        if (Trimmed.Data[0] == '"')
        {
            u32 LastQuoteIndex = 0;
            if (!String_IndexOfLastChar(Trimmed, '"', &LastQuoteIndex))
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
                u8 C = MsgString.Data[i];
                if (C == '%')
                {
                    const String Arg = StringArray_GetStringFromIndex(MsgArgsList, ArgIndex);
                    ArgIndex++;

                    // TODO: $
                    String Var = String_EatChar(Arg, '%');
                    String Val = GetCmdOptionValue(CmdOptionsDB, Var);
                    String_Append(&FormattedMsg, Val.Length == 0 ? S("0") : Val);
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
            (void)String_IndexOfFirstWhitespace(VarValue, &Index);

            if (String_IsFirst(VarValue, '"'))
            {
                u32 LastQuote = 0;
                if (String_IndexOfChar(StrShiftF(VarValue, 1), '"', &LastQuote))
                {
                    (void)String_IndexOfFirstWhitespace(StrShiftF(VarValue, LastQuote), &Index);

                    Index += LastQuote;
                }
            }

            bool bIsMultiLineIf = String_IndexOfChar(VarValue, '{', NULL);
            bool bIsMultiLineVar = String_IsLast(VarValue, '[');
            bInsideIf = bIsMultiLineIf;
            bInsideElse = false;

            String Condition;
            if (Index > 0)
            {
                Condition = StrSlice(VarValue.Data, Index);
            }
            else
            {
                Condition = VarValue;
            }

            (void)String_EatCharInline(&Condition, '"');
            (void)String_EatCharInlineFromEnd(&Condition, '"');

            bool bConditionMet = false;
            String ConditionValuePtr = String_Null();

            StringLocal(EnvValue, 1024);

            bool bIsNot = false;
            bool bCaseSensitive = false;

            {
                LinearAllocator Scratch = *Arena;
                StringArray ConditionArray = String_ParseIntoArray(&Scratch, Condition, '|', 0, 32);
                for each_str (SubCondition, ConditionArray)
                {
                    Condition = *SubCondition;

                    bool bSearchUserVar      = String_EatCharInline(&Condition, '$');
                    bool bSearchCmdVar       = String_EatCharInline(&Condition, '%');
                    bool bSearchEnv          = String_EatCharInline(&Condition, '@');
                         bIsNot              = String_EatCharInline(&Condition, '!');
                         bCaseSensitive      = String_EatCharInline(&Condition, '^');
                    bool bPrefixedWithSymbol = bSearchUserVar || bSearchCmdVar || bSearchEnv;

                    bool bIsPath = String_ContainsPathSeparators(Condition);
                    if (bIsPath)
                    {
                        bool bIsDirectory = String_IsLast(Condition, '/') || String_IsLast(Condition, '\\');

                        StringLocal(Temp, MAX_PATH_LENGTH);
                        if (Filesystem_IsPathRelative(Condition))
                        {
                            String_BuildPath(&Temp, WorkingDirectory, Condition);
                        }
                        else
                        {
                            String_Copy(&Temp, Condition);
                        }

                        if (bIsDirectory)
                        {
                            bConditionMet = Filesystem_DoesDirectoryExist(Temp);
                        }
                        else
                        {
                            bConditionMet = Filesystem_DoesFileExist(Temp);
                        }
                    }
                    else
                    {
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

                        if (!bConditionMet && (bSearchCmdVar || !bPrefixedWithSymbol))
                        {
                            // check the condition string against the internal build vars passed in from the command line
                            // override VarValue for single line if's, for multiline if's, loop back to the top and process each line until '}' is found
                            for each (CmdOption, o, CmdOptionsDB)
                            {
                                bool bMatch = String_IsEqual(o.Name, Condition, false);
                                if (bMatch)
                                {
                                    if (!o.bEqualsToSomething || o.Value.Length > 0) // make sure we have some value if we specified an '=' sign
                                    {
                                        ConditionValuePtr = o.Value;
                                        bConditionMet = true;
                                        break;
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
                        }

                        if (!bConditionMet && (bSearchUserVar || !bPrefixedWithSymbol))
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
                    }

                    if (bConditionMet)
                    {
                        break;
                    }
                }
            }

            String ComparisonOperator = String_EatSpaces(StrSlice(VarValue.Data+Index, VarValue.Length-Index));
            u32 SecondWhitespaceIndex = 0;
            (void)String_IndexOfFirstWhitespace(ComparisonOperator, &SecondWhitespaceIndex);
            ComparisonOperator = StrSlice(ComparisonOperator.Data, SecondWhitespaceIndex);

            if (!String_IsEqual(ComparisonOperator, S("!="), false)) // ignore !=
            {
                if (String_EatCharInline(&ComparisonOperator, '!'))
                {
                    bIsNot = true;
                }
            }

            if      (String_IsEqual(ComparisonOperator, S("=="), false))          { Comparison = Cmp_Equal; }
            else if (String_IsEqual(ComparisonOperator, S("!="), false))          { Comparison = Cmp_NotEqual; }
            else if (String_IsEqual(ComparisonOperator, S(">="), false))          { Comparison = Cmp_GreaterThanOrEqual; }
            else if (String_IsEqual(ComparisonOperator, S("<="), false))          { Comparison = Cmp_LessThanOrEqual; }
            else if (String_IsEqual(ComparisonOperator, S(">"), false))           { Comparison = Cmp_GreaterThan; }
            else if (String_IsEqual(ComparisonOperator, S("<"), false))           { Comparison = Cmp_LessThan; }
            else if (String_IsEqual(ComparisonOperator, S("starts_with"), false)) { Comparison = Cmp_StartsWith; }
            else if (String_IsEqual(ComparisonOperator, S("ends_with"), false))   { Comparison = Cmp_EndsWith; }
            else if (String_IsEqual(ComparisonOperator, S("contains"), false))    { Comparison = Cmp_Contains; }
            else                                                                  { Comparison = Cmp_None; }

            String TestValue = String_EatSpaces(StrShiftF(VarValue, Index+1+SecondWhitespaceIndex));

            u32 ThirdWhitespaceIndex = 0;
            (void)String_IndexOfFirstWhitespace(TestValue, &ThirdWhitespaceIndex);

            if (String_IsFirst(TestValue, '"'))
            {
                u32 LastQuote = 0;
                if (String_IndexOfChar(StrShiftF(TestValue, 1), '"', &LastQuote))
                {
                    (void)String_IndexOfFirstWhitespace(StrShiftF(TestValue, LastQuote), &ThirdWhitespaceIndex);

                    ThirdWhitespaceIndex += LastQuote;
                }
            }

            TestValue = StrSlice(TestValue.Data, ThirdWhitespaceIndex);

            (void)String_EatCharInline(&TestValue, '"');
            (void)String_EatCharInlineFromEnd(&TestValue, '"');

            if (Comparison != Cmp_None && ThirdWhitespaceIndex)
            {
                Index = (u32)((TestValue.Data+ThirdWhitespaceIndex) - VarValue.Data);
            }

            i64 LeftInt = 0, RightInt = 0;
            switch (Comparison)
            {
                case Cmp_None:
                break;

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

                default:
                break;
            }

            if (bIsNot)
            {
                bConditionMet = !bConditionMet;
            }

            String RestOfTheLine = StrShiftF(VarValue, Index);
            (void)String_EatSpacesInline(&RestOfTheLine);
            (void)String_EatSpacesInlineFromEnd(&RestOfTheLine);

            StringLocal(LineCopy, LINE_BUFFER_SIZE);
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
                        // TODO: temp. remove/clean up syntax
                        if (String_StartsWith(LineCopy, S(".Help"), false))
                        {
                            bIsMultiLineIf = false;
                        }

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

        const String Keywords[8] =
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
                        Internal_AddVariable(Arena, VariablesDB, VarName, S(""), SpecialData, bHasSpecial);
                    }

                    continue;
                }
            }

            if (bWantsOverride)
            {
                String* Ref = GetVariableValue_Ref(VariablesDB, VarName);
                String_Copy(Ref, VarValue);
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

                Internal_AddVariable(Arena, VariablesDB, VarName, VarValue, SpecialData, bHasSpecial);
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
                if (Filesystem_GetFilePath(H, &IncludePath))
                {
                    bool bHasSlash = String_IndexOfLastPathSlash(IncludePath, &LastSlash);
                    const String Path = bHasSlash ? StrSlice(IncludePath.Data, LastSlash) : IncludePath;
                    String_BuildPath(&IncludeFilePath, Path, VarValue);
                }
            }
            else
            {
                String_BuildPath(&IncludeFilePath, WorkingDirectory, VarValue);
            }

            StringLocal(ExpandedPath, MAX_PATH_LENGTH);
            if (!ExpandBuildVariable(*Arena, VariablesDB, CmdOptionsDB, &ExpandedPath, S("Include"), IncludeFilePath, S("Include"), WorkingDirectory, false, bIsAssemblyExe))
            {
                return false;
            }

            String_ConvertSlashToPlatformSlash(&ExpandedPath);
            (void)Filesystem_ConvertRelativeToAbsolutePath(&ExpandedPath);

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

            usize Size = 0;
            bool bResult = Filesystem_GetFileSize(IncludeFileHandle, &Size);

            if (!bResult || Size == 0)
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
                if (Filesystem_GetFilePath(IncludeFileHandle, &IncludePath))
                {
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
                                {
                                    LOG_INLINE("   ");
                                }
                                
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
        }

        if (SemiColonIndex > 0)
        {
            Trimmed.Length = LengthAfterTrim;

            StringLocal(LineCopy, LINE_BUFFER_SIZE);
            String_Copy(&LineCopy, StrShiftF(Trimmed, SemiColonIndex+1));
            String_Copy(&Line, LineCopy);

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
        u8 C = Value.Data[i];

        if (bInsideQuote  && C == '"') { bInsideQuote = false; }
        if (!bInsideQuote && C == '"') { bInsideQuote = true; }

        if (!bInsideQuote && C == '#') // a comment. disgard everything and exit
        {
            goto End;
        }

        String Slice = String_Null();
        bool bWantsToLower = false;
        bool bWantsToUpper = false;

        if (String_EndsWith(Key, S(".errormessage"), false) ||
            String_StartsWith(Key, S(".help"), false) ||
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
            if (bWantsToLower) { Offset++; }
            bWantsToUpper = String_EatCharInline_Single(&StrVal, '^');
            if (bWantsToUpper) { Offset++; }

            if (String_EatCharInline(&StrVal, '('))
            {
                Offset++;

                if (String_IndexOfChar(StrVal, ')', &Index))
                {
                    Offset++;
                }
            }

            if (Index == 0)
            {
                (void)String_IndexOfFirstWhitespace(StrVal, &Index);
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

            if (!bFoundCmd)
            {
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
            }

            // run through the cmd var assert list
            // TODO: something better
            {
                for each (FileVariable, Var, VariablesDB)
                {
                    if (String_IsEqual(Var.Name, S("Assert.Arg"), false))
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
                                LOG_INLINE_ERROR("[ASSERTION FAILURE] Command line argument \"%S\" or \"%S=VALUE\" was not given."
                                               "\n                    This is needed for the build to work properly. Aborting build...\n", Trimmed, Trimmed);
                                #else
                                LOG_ERROR("yo da cmd line var \"%S\" don exist cuh. dat shit not there nigga", Trimmed);
                                #endif

                                LogCustomErrorMessage(VariablesDB, S("Arg"), Trimmed, true);

                                return false;
                            }
                        }
                    }
                    else if (String_IsEqual(Var.Name, S("Assert.Arg.Any"), false))
                    {
                        StringArray ArgArray = String_ParseIntoArray(&Scratch, Var.Value, ' ', 0, 128);

                        bool bFound = false;
                        for each_str (Arg, ArgArray)
                        {
                            const String Trimmed = String_EatSpaces(*Arg);

                            for each (CmdOption, o, CmdOptionsDB)
                            {
                                if (String_IsEqual(o.Name, Trimmed, false))
                                {
                                    bFound = true;

                                    // TODO: this might confuse people if they specify 'arg' but not 'arg=VALUE'?
                                    if (o.bEqualsToSomething && o.Value.Length == 0)
                                    {
                                        bFound = false;
                                    }

                                    if (bFound)
                                    {
                                        break;
                                    }
                                }
                            }
                        }

                        if (!bFound)
                        {
                            LOG_INLINE_ERROR("[ASSERTION FAILURE] Any one of these arguments must be specified: %S\n", Var.Value);
                            return false;
                        }
                    }
                    else if (String_IsEqual(Var.Name, S("Assert.Arg.OnlyOne"), false)) // TODO: make dynamic
                    {
                        StringArray ArgArray = String_ParseIntoArray(&Scratch, Var.Value, ' ', 0, 128);

                        bool bFound = false;
                        for each_str (Arg, ArgArray)
                        {
                            const String Trimmed = String_EatSpaces(*Arg);

                            for each (CmdOption, o, CmdOptionsDB)
                            {
                                if (String_IsEqual(o.Name, Trimmed, false))
                                {
                                    if (bFound)
                                    {
                                        bFound = false;
                                        goto MultipleArgsFound;
                                    }

                                    bFound = true;
                                    if (o.bEqualsToSomething && o.Value.Length == 0)
                                    {
                                        bFound = false;
                                    }
                                }
                            }
                        }
                        
                        MultipleArgsFound:
                        if (!bFound)
                        {
                            LOG_INLINE_ERROR("[ASSERTION FAILURE] Only one of these arguments can be specified: %S\n", Var.Value);
                            return false;
                        }
                    }
                    else
                    {
                        // no action is required
                    }
                }
            }

            if (String_IsValid(VarValue))
            {
                // if the first letter is capitalized, then also make the first letter of the value capitalized. revert back when done
                bool bIsVarUpper = IsAlphabetUpper(Slice.Data[0]);
                if (bIsVarUpper)
                {
                    VarValue.Data[0] = ToUpper(VarValue.Data[0]);
                }

                String DestEnd = StrShiftF(*Dest, Dest->Length);
                u32 DestLengthBefore = Dest->Length;

                if (!ExpandBuildVariable(Scratch,VariablesDB, CmdOptionsDB, Dest, Slice, VarValue, Root, WorkingDirectory, false, bIsAssemblyExe))
                {
                    return false;
                }

                DestEnd.Length = Dest->Length - DestLengthBefore;
                if (bWantsToLower) { String_ToLower(&DestEnd); }
                if (bWantsToUpper) { String_ToUpper(&DestEnd); }
            }
            else
            {
                if (!bEqualsToSomething)
                {
                    if (bHasNot)
                    {
                        bFoundCmd = !bFoundCmd;
                    }

                    // the output of a found empty % cmd depends on the context...
                    // if we're inside certain keywords (like "Depends") then expand to nothing if we didnt find a value
                    bool bExpandToNothing = false;
                    if (String_IsEqual(Root, S("Depends"), false))
                    {
                        bExpandToNothing = true;
                    }

                    if (bExpandToNothing)
                    {
                        // but if it was mentioned, just paste the name in
                        if (bFoundCmd)
                        {
                            String_Append(Dest, Slice);
                        }
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

                                if (bWantsToLower) { String_ToLower(&DestEnd); }
                                if (bWantsToUpper) { String_ToUpper(&DestEnd); }
                            }
                        }
                        else
                        {
                            bool bIsNative = Slice.Data[0] == '_';
                            if (!bIsNative)
                            {
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
                            (void)String_EatSpacesInlineFromEnd(Dest);
                            String_AppendSpace(Dest);
                        }
                    }

                    String TempDest = String_Reserve(&Scratch, Dest->Capacity);
                    if (!ExpandBuildVariable(Scratch, VariablesDB, CmdOptionsDB, &TempDest, Slice, Var.Value, Root, WorkingDirectory, bLowerStrings, bIsAssemblyExe))
                    {
                        return false;
                    }

                    if (bWantsToLower) { String_ToLower(&TempDest); }
                    if (bWantsToUpper) { String_ToUpper(&TempDest); }
                    
                    String_Append(Dest, TempDest);

                    if (Var.Value.Length > 0)
                    {
                        NumEntries++;
                    }
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
            if (bWantsToLower) { String_ToLower(&DestEnd); }
            if (bWantsToUpper) { String_ToUpper(&DestEnd); }
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

            PlatformPipe StdOutHandle = {0};
            PlatformHandle ShellCmd = Platform_RunCommand_Ex(CmdLine, WorkingDirectory, &StdOutHandle);
            if (!Platform_IsValidHandle(ShellCmd)) { return false; }
            Platform_WaitForHandle(ShellCmd, -1);

            StringLocal(StdOutData, 8192);
            usize BytesRead = 0;
            if (!Filesystem_ReadPipe(StdOutHandle, StdOutData.Capacity, StdOutData.Data, &BytesRead))
            {
                LOG_ERROR("Failed to read from standard output pipe for command -> \"%S\"", Slice);
                return false;
            }

            StdOutData.Length = Min((u32)BytesRead, StdOutData.Capacity);
            (void)String_EatNewLinesInlineFromEnd(&StdOutData);

            String DestEnd = StrShiftF(*Dest, Dest->Length);
            u32 DestLengthBefore = Dest->Length;

            String_Append(Dest, StdOutData);
            DestEnd.Length = Dest->Length - DestLengthBefore;

            if (bWantsToLower) { String_ToLower(&DestEnd); }
            if (bWantsToUpper) { String_ToUpper(&DestEnd); }

            Platform_CloseHandle(StdOutHandle[0]);
            Platform_CloseHandle(StdOutHandle[1]);
        }
        else
        {
            bool bCheckChar = true;

            if (String_EndsWith(Key, S(".errormessage"), false) ||
                String_StartsWith(Key, S(".help"), false))
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
                    const String KeysToCareAbout[14] = 
                    {
                        S("SourceDirectory"),
                        S("BuildDirectory"),
                        S("IntermediateDirectory"),
                        S("LibraryDirectories"),
                        S("Includes"),
                        S("Icon"),
                        S("Compiler"),
                        S("PCH"),
                        S("PCH.h"),
                        S("IncludedSourceDirectories"),
                        S("ExcludedSourceDirectories"),
                        S("ExternalSourceDirectories"),
                        S("IncludedSourceFiles"),
                        S("ExcludedSourceFiles"),
                    };

                    bool bKeyIsPathBased = false;
                    for (u8 j = 0; j < SArray_Capacity(KeysToCareAbout); j++)
                    {
                        if (String_IsEqual(Key, KeysToCareAbout[j], false) ||
                            String_EndsWith(Key, S(".Copy"), false) ||
                            String_EndsWith(Key, S(".Move"), false) ||
                            String_EndsWith(Key, S(".Delete"), false) ||
                            String_EndsWith(Key, S(".Rename"), false) ||
                            String_EndsWith(Key, S(".NewDir"), false) ||
                            String_EndsWith(Key, S(".NewDirectory"), false) ||
                            String_EndsWith(Key, S(".NewFile"), false))
                        {
                            C = PATH_SEPARATOR;
                            
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
                                u8 LastChar = Dest->Data[Dest->Length-1];
                                bool bHasPathSep = LastChar == '/' || LastChar == '\\';
                                if (bHasPathSep)
                                {
                                    continue;
                                }
                            }
                        }
                    }
                }
            }

            String_AppendChar(Dest, bLowerAll ? ToLower(C) : C);
        }
    }

End:
    (void)String_EatSpacesInlineFromEnd(Dest);

    return true;
}
