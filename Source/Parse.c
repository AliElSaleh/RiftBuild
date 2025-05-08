// Copyright (c) Artisan Softworks
// Licensed under the BSD 3-Clause License. See the LICENSE file for details.

#ifndef UNITY_BUILD
#include "Backend.h"

#include "Core/Allocators.h"
#include "Core/Platform.h"
#include "Core/StringUtils.h"
#include "Core/Array.h"
#include "Core/Clock.h"
#include "Core/Globals.h"
#include "Core/Log.h"
#endif

// todos
// when appending values, use linked list?
// provide examples with every parser error message
// rename "IncludedSourceFiles" to "SourceFiles", same with the dir version
// .rpath key
// two arenas, one for permanently storing the key-value and another temp one for parsing that can be discarded






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


#define MAX_KEY_LENGTH 64
#define MAX_VALUE_LENGTH 8192
#define MAX_META_KEY_LENGTH 64
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

ENUM_TYPED(ETokenType, u32)
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
    Token_Quote,
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
    Token_Target,
    Token_Include,
    Token_If,
    Token_Else,
    Token_Or,
    Token_Contains,
    Token_StartsWith,
    Token_EndsWith,
    Token_Stop,
    Token_Abort,
    Token_Help,
    Token_ErrorMessage,

    Token_Whitespace,
    Token_Newline,

    Token_Max
};

STRUCT(Token)
{
    String     Lexeme;
    u32        Line;
    ETokenType Type;
};

read_only static Token Token_Null = { .Line = 0, .Lexeme = SC(""), .Type = Token_None };

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
    SC("Quote"),
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
    SC("Target"),
    SC("Include"),
    SC("If"),
    SC("Else"),
    SC("Or"),
    SC("Contains"),
    SC("StartsWith"),
    SC("EndsWith"),
    SC("Stop"),
    SC("Abort"),
    SC("Help"),
    SC("ErrorMessage"),

    SC("Whitespace"),
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
    SC("Token_Quote"),
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
    SC("Token_Target"),
    SC("Token_Include"),
    SC("Token_If"),
    SC("Token_Else"),
    SC("Token_Or"),
    SC("Token_Contains"),
    SC("Token_StartsWith"),
    SC("Token_EndsWith"),
    SC("Token_Stop"),
    SC("Token_Abort"),
    SC("Token_Help"),
    SC("Token_ErrorMessage"),

    SC("Token_Whitespace"),
    SC("Token_Newline"),
};

STRUCT(Lexer)
{
    TArray(Token) Tokens;
    String Text;
    u32 Current;
    u32 Start;
    u16 Line;
    u8 Padding[2];
    ETokenType Type;
};

ENUM_TYPED(ENodeType, u32)
{
    Node_None,
    Node_Block,
    Node_If,
    Node_Help,
    Node_ErrorMessage,
    Node_Include,
    Node_Assert,
    Node_KeyValue,
    Node_LogMessage,
};

STRUCT(NodeList)
{
    struct Node* Node;
    NodeList* Next;
};

STRUCT(IfConditionData)
{
    String Condition;
    String TestValue;
    ETokenType ComparisonOp;
    u8 Prefix;
    u8 Padding[3];
};

STRUCT(IfConditionList)
{
    IfConditionData Data;
    struct IfConditionList* Next;
};

NO_DISCARD RETURN_NON_NULL static IfConditionList* IfConditionList_Create(LinearAllocator* Arena, IfConditionData Value)
{
    IfConditionList* List = LinearAllocator_Allocate(Arena, sizeof(struct IfConditionList));
    List->Data     = Value;
    List->Next     = NULL;
    return List;
}

STRUCT(Node)
{
    ENodeType   Type;
    ETokenType  ComparisonOp;
    bool        bIsSpecial;
    bool        bIsReservedKey;
    u8          Padding[6];

    String      Key;
    StringList* Value;
    StringList* Parameters;
    IfConditionList* ConditionList;

    Node*       Left;
    Node*       Right;

    NodeList*   List;
};

read_only static Node Node_Null = { .Type = Node_None, .Left = &Node_Null, .Right = &Node_Null };

STRUCT(Parser)
{
    u32 Current;
    u32 NumTokens;
    TArray(Token) Tokens;
    u8          Padding[8];
};

NO_DISCARD RETURN_NON_NULL static NodeList* NodeList_Create(LinearAllocator* Arena, Node* Node, NodeList* Next)
{
    NodeList* List = LinearAllocator_Allocate(Arena, sizeof(struct NodeList));
    List->Node     = Node;
    List->Next     = Next;
    return List;
}

NO_DISCARD RETURN_NON_NULL static NodeList* NodeList_CreateNull(LinearAllocator* Arena)
{
    NodeList* List = LinearAllocator_Allocate(Arena, sizeof(struct NodeList));
    List->Node     = NULL;
    List->Next     = NULL;
    return List;
}

NO_DISCARD RETURN_NON_NULL static Node* Node_Create(LinearAllocator* Arena, ENodeType Type)
{
    Node* Node = LinearAllocator_Allocate(Arena, sizeof(struct Node));
    Node->Type = Type;
    return Node;
}

NO_DISCARD RETURN_NON_NULL static Node* Node_Create_KeyValue(LinearAllocator* Arena, String Key, StringList* Value, bool bIsSpecial)
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
    u8         Padding[4];
};

static KeywordTableEntry ReservedKeywordsTable[14] =
{
    { .Type = Token_If,          .Name = SC("if")          },
    { .Type = Token_Else,        .Name = SC("else")        },
    { .Type = Token_Include,     .Name = SC("include")     },
    { .Type = Token_Or,          .Name = SC("or")          },
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

STRUCT(DeferredKVData)
{
    Token* Key;
    StringList* Params;
    Node* FilterNode;
    Node* LastIfNode;
    bool bIsSpecial;
    u8 Padding[7];
};

read_only static DeferredKVData DeferredKVData_Null = 
{
    .Key = &Token_Null,
    .Params = NULL,
    .FilterNode = &Node_Null,
    .LastIfNode = &Node_Null,
    .bIsSpecial = false,
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

static void Lexer_Advance(Lexer* L)
{
    if (L->Current < L->Text.Length)
    {
        L->Current += 1;
    }
}

static uchar Lexer_Peek(Lexer* L)
{
    uchar Char = 0;
    if (L->Current < L->Text.Length)
    {
        Char = L->Text.Data[L->Current];
    }

    return Char;
}


static bool Lexer_Match(Lexer* L, uchar Expected)
{
    bool bResult = false;

    if (L->Current < L->Text.Length)
    {
        if (L->Text.Data[L->Current] == Expected)
        {
            L->Current += 1;
            bResult = true;
        }
    }

    return bResult;
}

static void Lexer_AddToken(Lexer* L, ETokenType Type)
{
    u32 Diff = ClampMin(L->Current - L->Start, 1);

    Token NewToken  = {0};
    NewToken.Type   = Type;
    NewToken.Lexeme = L->Text.Length == 0 ? L->Text : StrSub(L->Text, L->Start, Diff);
    NewToken.Line   = L->Line;

    Array_Add(L->Tokens, NewToken);
}

static bool IsValidTextToken(uchar Char, bool bAllowWhitespace) CONST_FN;
static bool IsValidTextToken(uchar Char, bool bAllowWhitespace)
{
    bool bSymbols = Char == '%' || Char == '$'  || Char == '@'  || Char == '!'  ||
                    Char == '(' || Char == ')'  || Char == ':'  || Char == '{'  ||
                    Char == '}' || Char == '['  || Char == ']'  || Char == '\'' ||
                    Char == '"' || Char == '|'  || Char == '^'  || Char == ';'; // ||
                    //Char == '/' || Char == '\\';

    bool bWhitespaceValid = bAllowWhitespace || (!bAllowWhitespace && !IsWhitespace(Char));
    bool bValid = bWhitespaceValid && !bSymbols && Char != 0;

    return bValid;
}

static ETokenType Lexer_PeekLastTokenType(Lexer* L)
{
    ETokenType Result = Token_None;

    if (Array_Num(L->Tokens) > 0)
    {
        Result = Array_Last(L->Tokens).Type;
    }

    return Result;
}

//
// PARSER --------------------------------------------------------
//

static void Parser_Advance(Parser* P)
{
    if (P->Current < P->NumTokens)
    {
        P->Current += 1;
    }
}

static Token Parser_Peek(Parser* P)
{
    Token Tok = Token_Null;
    if (P->Current < P->NumTokens)
    {
        Tok = P->Tokens[P->Current];
    }

    return Tok;
}

static Token Parser_LookBack(Parser* P)
{
    Token Tok = Token_Null;
    if (P->Current > 0)
    {
        Tok = P->Tokens[P->Current-1];
    }

    return Tok;
}

static bool Parser_Match(Parser* P, ETokenType Expected)
{
    bool bResult = false;

    if (P->Current < P->NumTokens)
    {
        if (P->Tokens[P->Current].Type == Expected)
        {
            P->Current += 1;
            bResult = true;
        }
    }

    return bResult;
}

static void Parser_SkipWhitespace(Parser* P)
{
    while (Parser_Match(P, Token_Whitespace))
    {
    }
}

NO_DISCARD RETURN_NON_NULL static Node* Parse_Special_LogMessage(LinearAllocator* Arena, Parser* P)
{
    Node* Root = Node_Create(Arena, Node_LogMessage);

    Parser_Advance(P);

    StringList* ValueList = NULL;
    StringList** Next = &ValueList;

    while (Parser_Peek(P).Type != Token_Quote)
    {
        Token Peek = Parser_Peek(P);
        String Lexeme = Peek.Lexeme;
        if (Peek.Type == Token_Newline)
        {
            Lexeme = S("\n");
        }
        
        SLinkedList_Push(Next, StringList_Create(Arena, String_EatCharFromEnd(Lexeme, '\\'), NULL));
        Parser_Advance(P);

        if (Parser_Peek(P).Type == Token_Quote)
        {
            if (String_IsLast(Parser_LookBack(P).Lexeme, '\\'))
            {
                SLinkedList_Push(Next, StringList_Create(Arena, Parser_Peek(P).Lexeme, NULL));
                Parser_Advance(P);
            }
        }
    }

    Parser_Advance(P);

    Root->Value = ValueList;

    return Root;
}

NO_DISCARD RETURN_NON_NULL static Node* Parse_Special_ErrorMessage(LinearAllocator* Arena, Parser* P)
{
    Node* Root = Node_Create(Arena, Node_ErrorMessage);
    Root->Key  = Parser_Peek(P).Lexeme;

    Parser_Advance(P);
    Parser_SkipWhitespace(P);

    (void)Parser_Match(P, Token_Newline);

    StringList* ValueList = NULL;
    StringList** Next = &ValueList;

    if (Parser_Match(P, Token_LCurly) || 
        Parser_Match(P, Token_LSquare))
    {
        while (Parser_Peek(P).Type != Token_RCurly &&
               Parser_Peek(P).Type != Token_RSquare)
        {
            Token Peek = Parser_Peek(P);
            String Lexeme = Peek.Lexeme;
            if (Peek.Type == Token_Newline)
            {
                Lexeme = S("\n");
            }

            SLinkedList_Push(Next, StringList_Create(Arena, String_EatCharFromEnd(Lexeme, '\\'), NULL));
            Parser_Advance(P);

            if (Parser_Peek(P).Type == Token_RSquare ||
                Parser_Peek(P).Type == Token_RCurly)
            {
                if (String_IsLast(Parser_LookBack(P).Lexeme, '\\'))
                {
                    SLinkedList_Push(Next, StringList_Create(Arena, Parser_Peek(P).Lexeme, NULL));
                    Parser_Advance(P);
                }
            }
        }

        Parser_Advance(P);
    }
    else
    {
        while (!(Parser_Peek(P).Type == Token_Newline   ||
                Parser_Peek(P).Type == Token_Semicolon))
        {
            String Lexeme = Parser_Peek(P).Lexeme;
            SLinkedList_Push(Next, StringList_Create(Arena, Lexeme, NULL));

            Parser_Advance(P);
        }
    }

    Root->Value = ValueList;

    return Root;
}

NO_DISCARD RETURN_NON_NULL static Node* Parse_Special_Help(LinearAllocator* Arena, Parser* P)
{
    Node* Root = Node_Create(Arena, Node_Help);
    Root->Key = S(".Help");

    Parser_Advance(P);
    Parser_SkipWhitespace(P);

    (void)Parser_Match(P, Token_Newline);

    StringList* ValueList = NULL;
    StringList** Next = &ValueList;

    if (Parser_Match(P, Token_LCurly) || 
        Parser_Match(P, Token_LSquare))
    {
        while (Parser_Peek(P).Type != Token_RCurly &&
               Parser_Peek(P).Type != Token_RSquare)
        {
            Token Peek = Parser_Peek(P);
            String Lexeme = Peek.Lexeme;
            if (Peek.Type == Token_Newline)
            {
                Lexeme = S("\n");
            }

            SLinkedList_Push(Next, StringList_Create(Arena, String_EatCharFromEnd(Lexeme, '\\'), NULL));
            Parser_Advance(P);

            if (Parser_Peek(P).Type == Token_RSquare ||
                Parser_Peek(P).Type == Token_RCurly)
            {
                if (String_IsLast(Parser_LookBack(P).Lexeme, '\\'))
                {
                    SLinkedList_Push(Next, StringList_Create(Arena, Parser_Peek(P).Lexeme, NULL));
                    Parser_Advance(P);
                }
            }
        }

        Parser_Advance(P);
    }
    else
    {
        while (!(Parser_Peek(P).Type == Token_Newline   ||
                Parser_Peek(P).Type == Token_Semicolon))
        {
            String Lexeme = Parser_Peek(P).Lexeme;
            SLinkedList_Push(Next, StringList_Create(Arena, Lexeme, NULL));

            Parser_Advance(P);
        }
    }

    Root->Value = ValueList;

    return Root;
}

NO_DISCARD RETURN_NON_NULL static Node* Parse_If(LinearAllocator* Arena,
                    Parser *P,
                    u32 Offset,
                    bool bCameFromInline);


NO_DISCARD RETURN_NON_NULL static Node* Parse_Block(LinearAllocator* Arena,
                    Parser *P,
                    u32 Offset,
                    bool bInIf
                    );

NO_DISCARD RETURN_NON_NULL static Node* Parse_Include(LinearAllocator* Arena, Parser *P)
{
    Node* Root = Node_Create(Arena, Node_Include);

    Parser_Advance(P);
    Parser_SkipWhitespace(P);

    StringList* ValueList = NULL;
    StringList** Next = &ValueList;

    bool bFoundTokens = false;
    while (Parser_Peek(P).Type == Token_Text    ||
           Parser_Peek(P).Type == Token_At      ||
           Parser_Peek(P).Type == Token_Mod     ||
           Parser_Peek(P).Type == Token_LParen  ||
           Parser_Peek(P).Type == Token_RParen  ||
           Parser_Peek(P).Type == Token_Dollar)
    {
        bFoundTokens = true;

        String Lexeme = Parser_Peek(P).Lexeme;
        SLinkedList_Push(Next, StringList_Create(Arena, Lexeme, NULL));

        Parser_Advance(P);
    }

    Root->Value = ValueList;

    if (!bFoundTokens)
    {
        LOG_ERROR("[Parser] [Line %u]: '%S' was unexpected after 'include'. Expected a file path or expression.", Parser_Peek(P).Line, Parser_Peek(P).Lexeme);
        return &Node_Null;
    }

    return Root;
}


NO_DISCARD RETURN_NON_NULL static Node* Parse_If(LinearAllocator* Arena,
                    Parser* P,
                    u32 Offset,
                    bool bCameFromInline)
{
    Node* Root = Node_Create(Arena, Node_If);

    IfConditionList* ConditionList = NULL;
    IfConditionList** NextCondition = &ConditionList;

    /*
    StringList* ValueList = NULL;
    StringList** NextValue = &ValueList;

    IfPrefixList* PrefixList = NULL;
    IfPrefixList** NextPrefix = &PrefixList;
    */

    P->Current += Offset;

    u32 Start = 0;
    u32 NumTokens = P->NumTokens;
    bool bInlineIf = false;
    ETokenType LastTokenType = Token_If;
    while (P->Current < NumTokens)
    {
        Start = P->Current;
        const Token t = P->Tokens[P->Current];

        if (Root->Left && !Root->Right) // we have an 'if' but no 'else' yet...
        {
            // stop if we see something other than 'else'
            if (t.Type != Token_Newline &&
                t.Type != Token_Semicolon &&
                t.Type != Token_Whitespace &&
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
        else if (t.Type == Token_Whitespace)
        {
        }
        else if (t.Type == Token_RCurly)
        {
            break;
        }
        else if (t.Type == Token_LCurly)
        {
            Node* BlockNode = Parse_Block(Arena, P, 1, false);
            if (BlockNode == &Node_Null)
            {
                return &Node_Null;
            }

            if (!Parser_Match(P, Token_RCurly))
            {
                LOG_ERROR("[Parser] [Line %u]: '}' is missing for 'if' block.", Parser_LookBack(P).Line);
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
        }
        else if (t.Type == Token_Else)
        {
            LastTokenType = Token_Else;

            Parser_Advance(P);
            Parser_SkipWhitespace(P);

            if (bInlineIf)
            {
                bool bHasCurly = Parser_Match(P, Token_LCurly);
                if (bHasCurly)
                {
                    LOG_ERROR("[Parser] [Line %u]: '{' are not allowed for inline if statements.", Parser_LookBack(P).Line);
                    return &Node_Null;
                }

                Node* BlockNode = Parse_Block(Arena, P, 0, true);
                if (BlockNode == &Node_Null)
                {
                    return &Node_Null;
                }

                Root->Right = BlockNode;

                if (Parser_Match(P, Token_Newline) ||
                    Parser_Match(P, Token_Semicolon))
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
        else if (t.Type is Token_Text         or
                 t.Type is Token_ErrorMessage or
                 t.Type is Token_Not          or
                 t.Type is Token_At           or
                 t.Type is Token_Mod          or
                 t.Type is Token_Dollar)
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
                   NextToken.Type == Token_Dollar)
            {
                IfConditionData Condition = {0};

                u8 Prefixes = 0;

                while (Parser_Match(P, Token_Not)    ||
                       Parser_Match(P, Token_Dollar) ||
                       Parser_Match(P, Token_Mod)    ||
                       Parser_Match(P, Token_At))
                {
                    ETokenType Peek = Parser_LookBack(P).Type;
                    
                    if      (Peek == Token_Not)    { Prefixes |= BIT(1); }
                    else if (Peek == Token_Dollar) { Prefixes |= BIT(2); }
                    else if (Peek == Token_Mod)    { Prefixes |= BIT(3); }
                    else if (Peek == Token_At)     { Prefixes |= BIT(4); }
                    else    {}
                }

                if (Parser_Peek(P).Type != Token_Text)
                {
                    LOG_ERROR("[Parser] [Line %u]: '%S' are not allowed within 'if' statements. Please delete.", Parser_Peek(P).Line, Parser_Peek(P).Lexeme);
                    return &Node_Null;
                }

                String Lexeme = Parser_Peek(P).Lexeme;

                Condition.Prefix = Prefixes;
                Condition.Condition = Lexeme;

                Parser_Advance(P);
                Parser_SkipWhitespace(P);

                Token Comparison = Parser_Peek(P);

                if (Comparison.Type != Token_If &&
                    Comparison.Type != Token_Text &&
                    Comparison.Type != Token_ErrorMessage &&
                    Comparison.Type != Token_Help &&
                    Comparison.Type != Token_Include &&
                    Comparison.Type != Token_Stop &&
                    Comparison.Type != Token_Abort &&
                    Comparison.Type != Token_LCurly &&
                    Comparison.Type != Token_Pipe &&
                    Comparison.Type != Token_Or &&
                    Comparison.Type != Token_Newline &&

                    !(Comparison.Type == Token_EqualEqual   || Comparison.Type == Token_NotEqual    ||
                    Comparison.Type == Token_GreaterOrEqual || Comparison.Type == Token_LessOrEqual ||
                    Comparison.Type == Token_GreaterThan    || Comparison.Type == Token_LessThan    ||
                    Comparison.Type == Token_StartsWith     || Comparison.Type == Token_EndsWith    ||
                    Comparison.Type == Token_Contains))
                {
                    LOG_ERROR("[Parser] [Line %u]: '%S' was unexpected after '%S'. Expected either another 'if', comparison operator, Key Value, or new block after '%S'. Please delete.", Comparison.Line,Comparison.Lexeme, Parser_LookBack(P).Lexeme, Parser_LookBack(P).Lexeme);
                    return &Node_Null;
                }

                if (Comparison.Type == Token_If)
                {
                    SLinkedList_Push(NextCondition, IfConditionList_Create(Arena, Condition));

                    Node* IfNode = Parse_If(Arena, P, 1, true);
                    if (IfNode == &Node_Null)
                    {
                        return &Node_Null;
                    }

                    Root->Left = IfNode;

                    bInlineIf = true;
                    break;
                }
                else
                {
                    if (Comparison.Type == Token_EqualEqual     || Comparison.Type == Token_NotEqual    ||
                        Comparison.Type == Token_GreaterOrEqual || Comparison.Type == Token_LessOrEqual ||
                        Comparison.Type == Token_GreaterThan    || Comparison.Type == Token_LessThan    ||
                        Comparison.Type == Token_StartsWith     || Comparison.Type == Token_EndsWith    ||
                        Comparison.Type == Token_Contains)
                    {
                        Condition.ComparisonOp = Comparison.Type;

                        Parser_Advance(P);
                        Parser_SkipWhitespace(P);

                        Token TestToken = Parser_Peek(P);
                        if (TestToken.Type == Token_Text)
                        {
                            Condition.TestValue = TestToken.Lexeme;
                            Parser_Advance(P);
                        }
                        else
                        {
                            LOG_ERROR("[Parser] [Line %u]: '%S' was unexpected after '%S'. Please delete.", TestToken.Line, TestToken.Lexeme, Comparison.Lexeme);
                            return &Node_Null;
                        }
                    }

                    SLinkedList_Push(NextCondition, IfConditionList_Create(Arena, Condition));

                    Parser_SkipWhitespace(P);

                    if (Parser_Peek(P).Type == Token_Text ||
                        Parser_Peek(P).Type == Token_ErrorMessage ||
                        Parser_Peek(P).Type == Token_Help ||
                        Parser_Peek(P).Type == Token_Include ||
                        Parser_Peek(P).Type == Token_Stop ||
                        Parser_Peek(P).Type == Token_Abort)
                    {
                        Node* BlockNode = Parse_Block(Arena, P, 0, true);
                        if (BlockNode == &Node_Null)
                        {
                            return &Node_Null;
                        }

                        Root->Left = BlockNode;

                        bInlineIf = true;

                        break;
                    }

                    while (Parser_Match(P, Token_Pipe) ||
                           Parser_Match(P, Token_Or));
                }

                NextToken = Parser_Peek(P);
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
        if (Start == P->Current)
        {
            Parser_Advance(P);
        }
    }

    Root->ConditionList = ConditionList;

    // TODO: update print node
    //Root->Value = ValueList;
    //Root->PrefixList = PrefixList;

    return Root;
}

NO_DISCARD RETURN_NON_NULL static Node* Parse_Block(LinearAllocator* Arena,
                    Parser* P,
                    u32 Offset,
                    bool bInIf
                    )
{
    Node* Root = Node_Create(Arena, Node_Block);
    NodeList** NextNode = &Root->List;

    P->Current += Offset;

    u32 Start = 0;
    u32 NumTokens = P->NumTokens;
    Token LastRootToken = Token_Null;
    bool bPreviouslyEvaluatedIfStatement = false;
    bool bSkipRootTokenUpdate = false;

    DeferredKVData Deferred = DeferredKVData_Null;

    while (P->Current < NumTokens)
    {
        Start = P->Current;
        Token t = P->Tokens[P->Current];
        Token* tPtr = &P->Tokens[P->Current];

        bool bJustEvaluatedIfStatement = false;

        // @todo: make sure to handle all tokens possible

        if (t.Type == Token_Semicolon)
        {

        }
        else if (t.Type == Token_Whitespace)
        {
        }
        else if (t.Type == Token_Stop)
        {
        }
        else if (t.Type == Token_Abort)
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

            NodeList* List = NodeList_CreateNull(Arena);
            Node* BlockNode = Parse_Block(Arena, P, 1, bPreviouslyEvaluatedIfStatement);
            if (BlockNode == &Node_Null)
            {
                return &Node_Null;
            }

            if (!Parser_Match(P, Token_RCurly))
            {
                LOG_ERROR("[Parser] [Line %u]: '}' is missing for '%S' block.", Parser_LookBack(P).Line, ETokenTypeNoPrefix_ToString(PrevTokenType));
                return &Node_Null;
            }

            if (Deferred.Key != &Token_Null)
            {
                BlockNode->Key = Deferred.Key->Lexeme;

                if (Deferred.FilterNode != &Node_Null)
                {
                    Deferred.LastIfNode->Left = BlockNode;
                    List->Node = Deferred.FilterNode;
                }
                else
                {
                    List->Node = BlockNode;
                }
            }
            else
            {
                List->Node = BlockNode;
            }

            SLinkedList_Push(NextNode, List);

            Deferred = DeferredKVData_Null;
        }
        else if (t.Type == Token_RCurly)
        {
            break;
        }
        else if (t.Type == Token_LSquare)
        {
            if (Deferred.Key != &Token_Null)
            {
                Parser_Advance(P);

                // skip newlines
                while (Parser_Match(P, Token_Newline)) {}

                NodeList* List = NodeList_CreateNull(Arena);

                StringList* ValueList = NULL;
                StringList** NextValue = &ValueList;

                while (Parser_Peek(P).Type != Token_RSquare)
                {
                    String Lexeme = Parser_Peek(P).Lexeme;
                    if (Parser_Peek(P).Type == Token_Newline)
                    {
                        SLinkedList_Push(NextValue, StringList_Create(Arena, S(" "), NULL));
                    }
                    else
                    {
                        SLinkedList_Push(NextValue, StringList_Create(Arena, Lexeme, NULL));
                    }

                    Parser_Advance(P);
                    Parser_SkipWhitespace(P);
                }

                Parser_Advance(P); // go past ']'

                Node* KV_Node = Node_Create_KeyValue(Arena, Deferred.Key->Lexeme, ValueList, Deferred.bIsSpecial);
                KV_Node->Parameters = Deferred.Params;

                if (Deferred.FilterNode != &Node_Null)
                {
                    Deferred.LastIfNode->Left = KV_Node;
                    List->Node = Deferred.FilterNode;
                }
                else
                {
                    List->Node = KV_Node;
                }

                SLinkedList_Push(NextNode, List);

                Deferred = DeferredKVData_Null;
            }
            else
            {
                LOG_ERROR("[Parser] [Line %u]: Unexpected token '['. Please delete.", t.Line);
                return &Node_Null;
            }
        }
        // the positioning of this if statement is important, do not move this!!
        else if (Deferred.Key != &Token_Null)
        {
            // this means this key has no value
            NodeList* List = NodeList_CreateNull(Arena);
            Node* KV_Node = Node_Create_KeyValue(Arena, Deferred.Key->Lexeme, NULL, Deferred.bIsSpecial);
            KV_Node->Parameters = Deferred.Params;

            if (Deferred.FilterNode != &Node_Null)
            {
                Deferred.LastIfNode->Left = KV_Node;
                List->Node = Deferred.FilterNode;
            }
            else
            {
                List->Node = KV_Node;
            }

            SLinkedList_Push(NextNode, List);

            Deferred = DeferredKVData_Null;

            P->Current -= 1;
            continue;
        }
        else if (t.Type == Token_Help)
        {
            NodeList* List = NodeList_CreateNull(Arena);
            Node* HelpNode = Parse_Special_Help(Arena, P);
            List->Node = HelpNode;
            if (HelpNode == &Node_Null)
            {
                return &Node_Null;
            }

            SLinkedList_Push(NextNode, List);
        }
        else if (t.Type == Token_Quote)
        {
            NodeList* List = NodeList_CreateNull(Arena);
            Node* LogMsgNode = Parse_Special_LogMessage(Arena, P);
            List->Node = LogMsgNode;
            if (LogMsgNode == &Node_Null)
            {
                return &Node_Null;
            }

            SLinkedList_Push(NextNode, List);
        }
        else if (t.Type == Token_ErrorMessage)
        {
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

            NodeList* List = NodeList_CreateNull(Arena);
            Node* ErrorMsgNode = Parse_Special_ErrorMessage(Arena, P);
            List->Node = ErrorMsgNode;
            if (ErrorMsgNode == &Node_Null)
            {
                return &Node_Null;
            }

            SLinkedList_Push(NextNode, List);
        }
        else if (t.Type == Token_Text)
        {
            Parser_SkipWhitespace(P);

            // this means we are the key
            bool bIsSpecial = false;
            if (Parser_Match(P, Token_Text))
            {
                if (Parser_Match(P, Token_Not))
                {
                    bIsSpecial = true;
                }
            }

            if (!IsAlphabet(t.Lexeme.Data[0]))
            {
                LOG_ERROR("[Parser] [Line %u]: Key '%S' can only start with an alphabet character. Please remove '%c'", t.Line, t.Lexeme, t.Lexeme.Data[0]);
                return &Node_Null;
            }

            StringList* ParamList = NULL;
            if (Parser_Match(P, Token_LParen))
            {
                StringList** Next = &ParamList;
                while (Parser_Peek(P).Type == Token_Text ||
                       Parser_Peek(P).Type == Token_Colon)
                {
                    SLinkedList_Push(Next, StringList_Create(Arena, Parser_Peek(P).Lexeme, NULL));
                    Parser_Advance(P);
                    Parser_SkipWhitespace(P);
                }

                if (!Parser_Match(P, Token_RParen))
                {
                    LOG_ERROR("[Parser] [Line %u]: '%S' was unexpected within parameter list. Missing enclosing ')'", Parser_Peek(P).Line, Parser_Peek(P).Lexeme);
                    return &Node_Null;
                }
            }

            // we are filtering this key. aka if statement
            Node* FilterNode = &Node_Null;
            Node* LastIfNode = &Node_Null;
            if (Parser_Match(P, Token_Colon))
            {
                while (Parser_Peek(P).Type == Token_Text   ||
                       Parser_Peek(P).Type == Token_Not    ||
                       Parser_Peek(P).Type == Token_At     ||
                       Parser_Peek(P).Type == Token_Mod    ||
                       Parser_Peek(P).Type == Token_Dollar)
                {
                    IfConditionList* ConditionList = NULL;
                    IfConditionList** NextCondition = &ConditionList;

                    Node* IfNode = Node_Create(Arena, Node_If);
                    while (1)
                    {
                        u8 Prefixes = 0;
                        while (Parser_Match(P, Token_Not)    ||
                               Parser_Match(P, Token_Dollar) ||
                               Parser_Match(P, Token_Mod)    ||
                               Parser_Match(P, Token_At))
                        {
                            ETokenType Peek = Parser_LookBack(P).Type;
                            
                            if      (Peek == Token_Not)    { Prefixes |= BIT(1); }
                            else if (Peek == Token_Dollar) { Prefixes |= BIT(2); }
                            else if (Peek == Token_Mod)    { Prefixes |= BIT(3); }
                            else if (Peek == Token_At)     { Prefixes |= BIT(4); }
                            else    {}
                        }

                        if (!Parser_Match(P, Token_Text))
                        {
                            LOG_ERROR("[Parser] [Line %u]: Text was expected after '%S'. Please delete '%S'", Parser_Peek(P).Line, Parser_LookBack(P).Lexeme, Parser_Peek(P).Lexeme);
                            return &Node_Null;
                        }

                        String Lexeme = Parser_LookBack(P).Lexeme;

                        IfConditionData Condition = {0};
                        Condition.Condition = Lexeme;
                        Condition.Prefix    = Prefixes;

                        // TODO: comparison support?

                        SLinkedList_Push(NextCondition, IfConditionList_Create(Arena, Condition));

                        if (!Parser_Match(P, Token_Pipe))
                        {
                            break;
                        }
                    }

                    IfNode->ConditionList = ConditionList;

                    if (FilterNode == &Node_Null)
                    {
                        FilterNode = IfNode;
                    }

                    if (LastIfNode != &Node_Null)
                    {
                        LastIfNode->Left = IfNode;
                    }

                    LastIfNode = IfNode;

                    // another if node
                    if (Parser_Match(P, Token_Colon))
                    {

                    }
                    else
                    {
                        break;
                    }
                }
            }

            Parser_SkipWhitespace(P);

            bool bFoundTokens = false;

            // this means we are multiline
            if (Parser_Peek(P).Type == Token_Newline ||
                Parser_Peek(P).Type == Token_LSquare ||
                Parser_Peek(P).Type == Token_LCurly)
            {
                Deferred.Key        = tPtr;
                Deferred.Params     = ParamList;
                Deferred.FilterNode = FilterNode;
                Deferred.LastIfNode = LastIfNode;
                Deferred.bIsSpecial = bIsSpecial;
            }
            else
            {
                NodeList* List = NodeList_CreateNull(Arena);

                // now we are the value to that key
                StringList* ValueList = NULL;
                StringList** NextValue = &ValueList;

                while (!(Parser_Peek(P).Type == Token_Newline  ||
                        Parser_Peek(P).Type == Token_Semicolon ||
                        Parser_Peek(P).Type == Token_LCurly    ||
                        Parser_Peek(P).Type == Token_RCurly    ||
                        Parser_Peek(P).Type == Token_None      ||
                        Parser_Peek(P).Type == Token_Else))
                {
                    bFoundTokens = true;

                    String Lexeme = Parser_Peek(P).Lexeme;
                    SLinkedList_Push(NextValue, StringList_Create(Arena, Lexeme, NULL));

                    Parser_Advance(P);
                }

                Node* KV_Node = Node_Create_KeyValue(Arena, tPtr->Lexeme, ValueList, bIsSpecial);
                KV_Node->Parameters = ParamList;

                if (FilterNode != &Node_Null)
                {
                    LastIfNode->Left = KV_Node;
                    List->Node = FilterNode;
                }
                else
                {
                    List->Node = KV_Node;
                }

                SLinkedList_Push(NextNode, List);
            }

            if (!bFoundTokens)
            {
                LastRootToken = t;
            }
            else
            {
                LastRootToken = Token_Null;
            }
            
            bSkipRootTokenUpdate = true;
        }
        else if (t.Type == Token_Include)
        {
            NodeList* List = NodeList_CreateNull(Arena);
            Node* IncludeNode = Parse_Include(Arena, P);
            List->Node = IncludeNode;
            if (IncludeNode == &Node_Null)
            {
                return &Node_Null;
            }

            SLinkedList_Push(NextNode, List);
        }
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

            NodeList* List = NodeList_CreateNull(Arena);
            Node* IfNode = Parse_If(Arena, P, 1, false);
            List->Node = IfNode;
            if (IfNode == &Node_Null)
            {
                return &Node_Null;
            }

            SLinkedList_Push(NextNode, List);

            bJustEvaluatedIfStatement = true;
        }
        else
        {
            LOG_ERROR("[Parser] [Line %u]: Keys can not start with '%S'. Please delete.", t.Line, t.Lexeme);
            return &Node_Null;
        }

        // only advance when nothing happened
        if (Start == P->Current)
        {
            Parser_Advance(P);
        }

        if (!(t.Type == Token_LCurly || t.Type == Token_Newline))
        {
            bPreviouslyEvaluatedIfStatement = bJustEvaluatedIfStatement;
        }

        if (!bSkipRootTokenUpdate)
        {
            if (t.Type != Token_Newline && t.Type != Token_Semicolon && t.Type != Token_Whitespace)
            {
                LastRootToken = t;
            }
        }

        bSkipRootTokenUpdate = false;
    }

    return Root;
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

static void Print_LogNode(Node* Root, u32 Level)
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

    LOG("%SLOG MESSAGE", Spaces);

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

    LOG("%SEND LOG MESSAGE", Spaces);
}

static void Print_ErrorNode(Node* Root, u32 Level)
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

    LOG("%SERROR MESSAGE", Spaces);

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

    LOG("%SEND ERROR MESSAGE", Spaces);
}

static void Print_KVNode(Node* Root, u32 Level)
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

    //if (Root->Key)
    {
        LOG("\n%S [KEY]      %S", Spaces, Root->Key);

        if (Root->bIsSpecial)
        {
            LOG("%S [SPECIAL MOD]    ", Spaces);
        }
    }

    if (Root->Value)
    {
        LOG_INLINE("%S [VALUE]    ", Spaces);
        for each_str_list (*Root->Value)
        {
            LOG_INLINE("%S", It.String);
        }
        LOG_LINE_BREAK();
    }

    if (Root->Parameters)
    {
        LOG_INLINE("%S [PARAMS]   ", Spaces);
        for each_str_list (*Root->Parameters)
        {
            LOG_INLINE("%S ", It.String);
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
                //if (Root->Key)
                {
                    LOG(" %S[%S]", Spaces, Root->Key);
                }

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
            else if (Root->Type == Node_ErrorMessage)
            {
                Print_ErrorNode(Root, Level+1);
            }
            else if (Root->Type == Node_LogMessage)
            {
                Print_LogNode(Root, Level+1);
            }
            else if (Root->Type == Node_KeyValue)
            {
                Print_KVNode(Root, Level);
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

    LOG_INLINE("%SIF ", Spaces);

    if (Root->ConditionList)
    {
        IfConditionList* Next = Root->ConditionList;
        while (Next)
        {
            IfConditionData c = Next->Data;

            if (c.ComparisonOp != Token_None)
            {
                LOG_INLINE("%S %S %S | ", c.Condition, ETokenType_ToString(c.ComparisonOp), c.TestValue);
            }
            else
            {
                LOG_INLINE("%S ", c.Condition);
            }

            Next = Next->Next;
        }
        /*
        for each_str_list (*Root->ConditionList)
        {
            LOG_INLINE("%S ", It.String);
        }
        */
    }

    LOG_LINE_BREAK();

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

        if (Left->Type == Node_KeyValue)
        {
            Print_KVNode(Left, Level+1);
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

        if (Right->Type == Node_KeyValue)
        {
            Print_KVNode(Right, Level+1);
        }
    }

    LOG("%SEND IF\n", Spaces);
}

NO_DISCARD RETURN_NON_NULL static Node* Internal_ParseBuildFile(LinearAllocator* Arena, const FileHandle H)
{
    Node* Result = &Node_Null;

    Clock c;
    Clock_Start(&c);

    //StringLocal(Text, Kibibytes(48));
    // TODO: debate if we should take a pointer to things in this text or make new strings instead
    String Text = String_Reserve(Arena, Kibibytes(48));
    
    usize Length = 0;
    if (Filesystem_ReadEntireFile(H, Text.Data, &Length))
    {
        Text.Length = (u32)Min(Length, Kibibytes(48));

        bool bAllowWhitespace = false;
        bool bInsideWhitespaceAllowedBlock = false;

        // tokenize the text
        Lexer l = {0};
        l.Text = Text;
        l.Line = 1;
        ArrayLocal_Arena(Token, Tokens, 2048, Arena);
        l.Tokens = Tokens;
        while (l.Current < l.Text.Length)
        {
            l.Start = l.Current;
            
            const uchar Char = l.Text.Data[l.Current];
            const uchar PrevChar = l.Text.Data[ClampMin((i32)l.Current-1, 0)];
            Lexer_Advance(&l);

            if (Char == '\n')
            {
                l.Line += 1;
            }

            ETokenType LastTokenType = Lexer_PeekLastTokenType(&l);

            if (LastTokenType == Token_Help || LastTokenType == Token_ErrorMessage)
            {
                bInsideWhitespaceAllowedBlock = true;
            }

            if      (Char == '(') { Lexer_AddToken(&l, Token_LParen);       }
            else if (Char == ')') { Lexer_AddToken(&l, Token_RParen);       }
            else if (Char == '^') { Lexer_AddToken(&l, Token_Caret);        }
            else if (Char == ';') { Lexer_AddToken(&l, Token_Semicolon);    }
            else if (Char == '$') { Lexer_AddToken(&l, Token_Dollar);       }
            else if (Char == '@') { Lexer_AddToken(&l, Token_At);           }
            else if (Char == '|') { Lexer_AddToken(&l, Token_Pipe);         }
            else if (Char == '%') { Lexer_AddToken(&l, Token_Mod);          }
            else if (Char == '\'')
            {
                if (!bInsideWhitespaceAllowedBlock)
                {
                    if (PrevChar != '\\')
                    {
                        bAllowWhitespace = !bAllowWhitespace;
                    }
                }

                Lexer_AddToken(&l, Token_Quote);
            }
            else if (Char == '"')
            {
                if (!bInsideWhitespaceAllowedBlock)
                {
                    if (PrevChar != '\\')
                    {
                        bAllowWhitespace = !bAllowWhitespace;
                    }
                }

                Lexer_AddToken(&l, Token_Quote);
            }
            else if (Char == '{')
            {
                if (bInsideWhitespaceAllowedBlock)
                {
                    bAllowWhitespace = true;
                }

                Lexer_AddToken(&l, Token_LCurly);
            }
            else if (Char == '}')
            {
                if (bInsideWhitespaceAllowedBlock && PrevChar != '\\')
                {
                    bInsideWhitespaceAllowedBlock = false;
                    bAllowWhitespace = false;
                }

                Lexer_AddToken(&l, Token_RCurly);
            }
            else if (Char == '[')
            { 
                if (bInsideWhitespaceAllowedBlock)
                {
                    bAllowWhitespace = true;
                }

                Lexer_AddToken(&l, Token_LSquare);
            }
            else if (Char == ']')
            {
                if (bInsideWhitespaceAllowedBlock && PrevChar != '\\')
                {
                    bInsideWhitespaceAllowedBlock = false;
                    bAllowWhitespace = false;
                }

                Lexer_AddToken(&l, Token_RSquare);
            }
            // TODO: escape #
            else if (Char == '#')
            {
                bool bIsMultiLine = Lexer_Match(&l, '#');
                if (bIsMultiLine)
                {
                    while (1)
                    {
                        uchar PeekChar = Lexer_Peek(&l);
                        if (PeekChar == '#')
                        {
                            (void)Lexer_Advance(&l);
                            if (Lexer_Match(&l, '#'))
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
                            l.Line += 1;
                        }

                        (void)Lexer_Advance(&l);
                    }
                }
                else
                {
                    while (!IsNewline(Lexer_Peek(&l)))
                    {
                        (void)Lexer_Advance(&l);
                    }
                }
            }
            else if (Char == ':')
            {
                Lexer_AddToken(&l, Token_Colon);
            }
            else if (Char == '!')
            {
                ETokenType Type = Lexer_Match(&l, '=') ? Token_NotEqual : Token_Not;
                Lexer_AddToken(&l, Type);
            }
            else if (Char == '=')
            {
                ETokenType Type = Lexer_Match(&l, '=') ? Token_EqualEqual : Token_Equal;
                Lexer_AddToken(&l, Type);
            }
            else if (Char == '<')
            {
                ETokenType Type = Lexer_Match(&l, '=') ? Token_LessOrEqual : Token_LessThan;
                Lexer_AddToken(&l, Type);
            }
            else if (Char == '>')
            {
                ETokenType Type = Lexer_Match(&l, '=') ? Token_GreaterOrEqual : Token_GreaterThan;
                Lexer_AddToken(&l, Type);
            }
            else if ((!bAllowWhitespace && IsWhitespace(Char)) || IsNewline(Char) || Char == '\0')
            {
                if (Char == '\n')
                {
                    if (bAllowWhitespace || LastTokenType != Token_Newline)
                    {
                        Lexer_AddToken(&l, Token_Newline);
                    }
                }
                else
                {
                    if ((Char == ' ' || Char == '\t') && LastTokenType != Token_Newline)
                    {
                        while (Lexer_Peek(&l) == ' ' ||
                               Lexer_Peek(&l) == '\t')
                        {
                            (void)Lexer_Advance(&l);
                        }

                        Lexer_AddToken(&l, Token_Whitespace);
                    }
                }
            }
            else
            {
                uchar Peek = Lexer_Peek(&l);
                while (IsValidTextToken(Peek, bAllowWhitespace))
                {
                    if (Peek == '\n')
                    {
                        l.Line += 1;
                    }

                    (void)Lexer_Advance(&l);
                    Peek = Lexer_Peek(&l);
                }

                u32 Diff = ClampMin(l.Current - l.Start, 1);
                String Lexeme = StrSub(l.Text, l.Start, Diff);

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

                Lexer_AddToken(&l, FinalType);
            }
        }
        Clock_Tick(&c);
        Clock_PrintElapsedTime(&c, true);

        for each (Token, t, Tokens)
        {
            if (t.Lexeme.Length > 0 && t.Type != Token_Newline)
            {
                LOG("[%u] %S -> %S.", t.Line, ETokenType_ToString(t.Type), t.Lexeme);
            }
            else
            {
                LOG("[%u] %S", t.Line, ETokenType_ToString(t.Type));
            }
        }
        LOG("Num Tokens: %u", Array_Num(Tokens));

        // Parse the tokens into a tree
        {
            Clock_Start(&c);

            Parser p = {0};
            p.Tokens = Tokens;
            p.NumTokens = (u32)Array_Num(Tokens);
            Result = Parse_Block(Arena, &p, 0, false);

            Clock_Tick(&c);
            Clock_PrintElapsedTime(&c, true);

            Print_BlockNode(Result->List, 0);
        }
    }

    return Result;
}

STRUCT(ParsingContext)
{
    TArray(FileVariable) VariablesDB;
    TArray(CmdOption) CmdOptionsDB;
    TArray(String) Messages;
    String WorkingDirectory;
    StringList* ParentKeys;
};

NO_DISCARD static NodeList* Analyze_IfNode(LinearAllocator* Arena, Node* Root, ParsingContext Context, bool bInIf);
NO_DISCARD static NodeList* Analyze_List(LinearAllocator* Arena, Node* Block, ParsingContext Context, bool bInIf);

NO_DISCARD static NodeList* Analyze_IncludeNode(LinearAllocator* Arena, Node* Root, ParsingContext Context)
{
    bool bSuccess = false;

    NodeList* IndeterminateList = NULL;
    NodeList** IndeterminateNext = &IndeterminateList;

    StringLocal(Expanded, MAX_VALUE_LENGTH);

    if (Root->Value)
    {
        LOG_INLINE("INCLUDE: ");

        StringLocal(Val, MAX_VALUE_LENGTH);
        for each_string_in_list (*Root->Value)
        {
            String_Append(&Val, It.String);
        }

        if (!ExpandBuildVariableV2(*Arena, Context.VariablesDB, Context.CmdOptionsDB, &Expanded, Root->Key, Val, Root->Key, Context.WorkingDirectory, false, false))
        {
            LOG_INLINE_WARNING("<indeterminate>\n");
            bSuccess = false;
        }
        else
        {
            LOG("%S", Expanded);
            bSuccess = true;
        }

        LOG_LINE_BREAK();
    }

    if (bSuccess)
    {
        // parse the include file

        FileHandle f = {0};
        if (!Filesystem_Open(Expanded, FileMode_Read, &f))
        {
            #ifndef HOOD
            LOG_ERROR("Failed to open include file \"%S\" for reading", Expanded);
            #else
            LOG_ERROR("huhh?!!!!! cant read the include file for some reason bro, \"%S\", think you gotta check it out on your end cuh", Expanded);
            #endif

            bSuccess = false;
        }

        if (bSuccess)
        {
            usize Size = 0;
            bool bResult = Filesystem_GetFileSize(f, &Size);

            if (!bResult || Size == 0)
            {
                #ifndef HOOD
                LOG_WARNING("Include file \"%S\" has a size of 0. Skipping...", Expanded);
                #else
                LOG_WARNING("ay bro heads up, gonna skip dis one, dis shit is empty nigga \"%S\"", Expanded);
                #endif

                bSuccess = false;
            }
        }
        
        if (bSuccess)
        {
            StringLocal(IncludePath, MAX_PATH_LENGTH);
            if (Filesystem_GetFilePath(f, &IncludePath))
            {
                /*
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
                SLinkedList_Push(Next, &Entry);

                if (!ParseBuildFile(Arena, IncludeFileHandle, BuildFilePath, WorkingDirectory,
                                    VariablesDB, CmdOptionsDB, Messages,
                                    IncludeFiles, ReturnCode, true, Includes, bIsAssemblyExe))
                {
                    return false;
                }

                *Next = NULL;
                */

                Node* AST = Internal_ParseBuildFile(Arena, f);

                NodeList* List = Analyze_List(Arena, AST, Context, false);
                if (List)
                {
                    SLinkedList_Push(IndeterminateNext, List);
                }
            }
        }
    }

    return IndeterminateList;
}

NO_DISCARD static bool Analyze_KVNode(LinearAllocator* Arena, Node* Root, ParsingContext Context)
{
    bool bSuccess = true;

    StringLocal(FinalKey, MAX_KEY_LENGTH);
    if (Context.ParentKeys)
    {
        StringList** NextKey = &Context.ParentKeys;
        while (*NextKey)
        {
            String_AppendF(&FinalKey, S("%S."), (*NextKey)->String);

            NextKey = &(*NextKey)->Next;
        }

        String_Append(&FinalKey, Root->Key);
    }
    else
    {
        FinalKey = Root->Key;
    }

    LOG("KEY:   %S", FinalKey);

    StringLocal(Expanded, MAX_VALUE_LENGTH);
    StringLocal(Params, MAX_META_KEY_LENGTH);

    if (Root->Value)
    {
        LOG_INLINE("VALUE: ");

        StringLocal(Val, MAX_VALUE_LENGTH);
        for each_string_in_list (*Root->Value)
        {
            String_Append(&Val, It.String);
        }

        /* todo: make this a macro template
        LinearAllocator Scratch = {0};
        i8 ScratchMemory[Kibibytes(16)] = {0};
        LinearAllocator_Create(Kibibytes(16), ScratchMemory, &Scratch);
        */

        if (!ExpandBuildVariableV2(*Arena, Context.VariablesDB, Context.CmdOptionsDB, &Expanded, FinalKey, Val, FinalKey, Context.WorkingDirectory, false, false))
        {
            LOG_INLINE_WARNING("<indeterminate>\n");
            bSuccess = false;
        }
        else
        {
            LOG("%S", Expanded);
            bSuccess = true;
        }
    }

    if (Root->Parameters)
    {
        LOG_INLINE("PARAMS: ");
        for each_str_list (*Root->Parameters)
        {
            String_AppendF(&Params, S("%S "), It.String);
        }
        LOG("%S", Params);
    }

    LOG_LINE_BREAK();

    if (bSuccess)
    {
        Internal_AddVariable(Arena, Context.VariablesDB, FinalKey, Expanded, Params, Root->bIsSpecial);
    }

    return bSuccess;
}


NO_DISCARD static NodeList* Analyze_IfNode(LinearAllocator* Arena, Node* Root, ParsingContext Context, bool bInIf)
{
    NodeList* IndeterminateList = NULL;
    NodeList** IndeterminateNext = &IndeterminateList;

    bool bConditionMet = false;
    bool bFoundVar = false;

    // evaluate the conditions
    if (ALWAYS(Root->ConditionList != NULL))
    {
        IfConditionList* Next = Root->ConditionList;
        while (Next)
        {
            IfConditionData c = Next->Data;

            bool bPrefixedWithSymbol   = c.Prefix > 0;
            bool bNot                  = c.Prefix & BIT(1);
            bool bSearchFileVar        = c.Prefix & BIT(2);
            bool bSearchInternalVar    = c.Prefix & BIT(3);
            bool bSearchEnvironmentVar = c.Prefix & BIT(4);
            bool bCaseSensitive        = false; // TODO

            StringLocal(EnvVar, MAX_PATH_LENGTH);

            String VarValue = String_Null();
            const String Condition = c.Condition;

            bool bIsPath = String_ContainsPathSeparators(Condition);
            if (bIsPath)
            {
                bool bIsDirectory = String_IsLast(Condition, '/') || String_IsLast(Condition, '\\');

                StringLocal(Temp, MAX_PATH_LENGTH);
                if (Filesystem_IsPathRelative(Condition))
                {
                    String_BuildPath(&Temp, Context.WorkingDirectory, Condition);
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

                bFoundVar = true;
            }
            else
            {
                if (bSearchEnvironmentVar)
                {
                    if (Platform_GetEnvironmentVariableValue(Condition, &EnvVar))
                    {
                        bConditionMet = c.ComparisonOp == Token_None;
                        VarValue = EnvVar;
                        bFoundVar = true;
                    }

                }

                if (!bConditionMet && (bSearchInternalVar || !bPrefixedWithSymbol))
                {
                    // check the condition string against the internal build vars passed in from the command line
                    for each (CmdOption, o, Context.CmdOptionsDB)
                    {
                        bool bMatch = String_IsEqual(o.Name, Condition, false);
                        if (bMatch)
                        {
                            // make sure we have some value if we specified an '=' sign
                            if (!o.bEqualsToSomething || o.Value.Length > 0)
                            {
                                VarValue = o.Value;
                                bConditionMet = c.ComparisonOp == Token_None;
                                bFoundVar = true;
                                break;
                            }
                        }
                    }

                    if (!bConditionMet)
                    {
                        // check the condition string against the internal build vars native to this program
                        for each (InternalVariable, v, InternalVariablesDB)
                        {
                            if (String_IsEqual(v.Name, Condition, false))
                            {
                                VarValue = v.Value;
                                bConditionMet = c.ComparisonOp == Token_None;
                                bFoundVar = true;
                                break;
                            }
                        }
                    }
                }

                if (!bConditionMet && (bSearchFileVar || !bPrefixedWithSymbol))
                {
                    // check the condition string against the currently expanded build file vars
                    for each (FileVariable, v, Context.VariablesDB)
                    {
                        bool bMatch = String_IsEqual(v.Name, Condition, false);
                        if (bMatch)
                        {
                            VarValue = v.Value;
                            bConditionMet = c.ComparisonOp == Token_None;
                            bFoundVar = true;
                            break;
                        }
                    }
                }
            }

            if (c.ComparisonOp != Token_None)
            {
                LinearAllocator Scratch = *Arena;

                i64 LeftInt = 0, RightInt = 0;
                switch (c.ComparisonOp)
                {
                    default:
                    break;

                    case Cmp_Equal:
                    {
                        bConditionMet = String_IsEqual(VarValue, c.TestValue, bCaseSensitive);
                        if (bConditionMet)
                        {
                            break;
                        }

                        // if the var value has more than one value separated by a space
                        StringArray Values2 = String_ParseIntoArray(&Scratch, VarValue, ' ', 0, 128);
                        for each_str (v2, Values2)
                        {
                            bConditionMet = String_IsEqual(c.TestValue, *v2, bCaseSensitive);
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
                    break;

                    case Cmp_NotEqual:
                    {
                        bConditionMet = !String_IsEqual(VarValue, c.TestValue, bCaseSensitive);
                        if (bConditionMet)
                        {
                            break;
                        }

                        // if the var value has more than one value separated by a space
                        StringArray Values2 = String_ParseIntoArray(&Scratch, VarValue, ' ', 0, 128);
                        for each_str (v2, Values2)
                        {
                            bConditionMet = !String_IsEqual(c.TestValue, *v2, bCaseSensitive);
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
                    break;

                    case Cmp_GreaterThanOrEqual:
                    {
                        if (!String_ToI64(VarValue, &LeftInt) ||
                            !String_ToI64(c.TestValue, &RightInt))
                        {
                            bConditionMet = false;
                            break;
                        }

                        bConditionMet = LeftInt >= RightInt;
                    }
                    break;

                    case Cmp_LessThanOrEqual:
                    {
                        if (!String_ToI64(VarValue, &LeftInt) ||
                            !String_ToI64(c.TestValue, &RightInt))
                        {
                            bConditionMet = false;
                            break;
                        }

                        bConditionMet = LeftInt <= RightInt;
                    }
                    break;

                    case Cmp_GreaterThan:
                    {
                        if (!String_ToI64(VarValue, &LeftInt) ||
                            !String_ToI64(c.TestValue, &RightInt))
                        {
                            bConditionMet = false;
                            break;
                        }

                        bConditionMet = LeftInt > RightInt;
                    }
                    break;

                    case Cmp_LessThan:
                    {
                        if (!String_ToI64(VarValue, &LeftInt) ||
                            !String_ToI64(c.TestValue, &RightInt))
                        {
                            bConditionMet = false;
                            break;
                        }

                        bConditionMet = LeftInt < RightInt;
                    }
                    break;

                    case Cmp_StartsWith:
                    {
                        bConditionMet = String_StartsWith(VarValue, c.TestValue, bCaseSensitive);
                        if (bConditionMet)
                        {
                            break;
                        }

                        // if the var value has more than one value separated by a space
                        StringArray Values2 = String_ParseIntoArray(&Scratch, VarValue, ' ', 0, 128);
                        for each_str (v2, Values2)
                        {
                            bConditionMet = String_StartsWith(*v2, c.TestValue, bCaseSensitive);
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
                    break;

                    case Cmp_EndsWith:
                    {
                        bConditionMet = String_EndsWith(VarValue, c.TestValue, bCaseSensitive);
                        if (bConditionMet)
                        {
                            break;
                        }

                        // if the var value has more than one value separated by a space
                        StringArray Values2 = String_ParseIntoArray(&Scratch, VarValue, ' ', 0, 128);
                        for each_str (v2, Values2)
                        {
                            bConditionMet = String_EndsWith(*v2, c.TestValue, bCaseSensitive);
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
                    break;

                    case Cmp_Contains:
                    {
                        bConditionMet = String_Contains(VarValue, c.TestValue, bCaseSensitive);
                        if (bConditionMet)
                        {
                            break;
                        }

                        // if the var value has more than one value separated by a space
                        StringArray Values2 = String_ParseIntoArray(&Scratch, VarValue, ' ', 0, 128);
                        for each_str (v2, Values2)
                        {
                            bConditionMet = String_Contains(*v2, c.TestValue, bCaseSensitive);
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
                    break;
                }
            }

            if (bNot)
            {
                bConditionMet = !bConditionMet;
            }

            if (bConditionMet)
            {
                bFoundVar = true;
                break;
            }

            Next = Next->Next;
        }
    }

    if (bFoundVar)
    {
        Node* Left = Root->Left;
        Node* Right = Root->Right;

        // main if block
        if (Left && bConditionMet)
        {
            if (Left->Type == Node_Block)
            {
                NodeList* BlockTree = Analyze_List(Arena, Left, Context, bInIf);
                if (BlockTree)
                {
                    SLinkedList_Push(IndeterminateNext, BlockTree);
                }
            }
            else if (Left->Type == Node_If)
            {
                NodeList* IfTree = Analyze_IfNode(Arena, Left, Context, bInIf);
                if (IfTree)
                {
                    SLinkedList_Push(IndeterminateNext, IfTree);
                }
            }
            else if (Left->Type == Node_KeyValue)
            {
                bool bSuccess = Analyze_KVNode(Arena, Left, Context);
                if (!bSuccess)
                {
                    SLinkedList_Push(IndeterminateNext, NodeList_Create(Arena, Left, NULL));
                }
            }
        }

        // else block
        if (Right && !bConditionMet)
        {
            if (Right->Type == Node_Block)
            {
                NodeList* BlockTree = Analyze_List(Arena, Right, Context, bInIf);
                if (BlockTree)
                {
                    SLinkedList_Push(IndeterminateNext, BlockTree);
                }
            }
            else if (Right->Type == Node_If)
            {
                NodeList* ElseTree = Analyze_IfNode(Arena, Right, Context, bInIf);
                if (ElseTree)
                {
                    SLinkedList_Push(IndeterminateNext, ElseTree);
                }
            }
            else if (Right->Type == Node_KeyValue)
            {
                bool bSuccess = Analyze_KVNode(Arena, Right, Context);
                if (!bSuccess)
                {
                    SLinkedList_Push(IndeterminateNext, NodeList_Create(Arena, Right, NULL));
                }
            }
        }
    }
    else
    {
        SLinkedList_Push(IndeterminateNext, NodeList_Create(Arena, Root, NULL));
    }

    return IndeterminateList;
}

NO_DISCARD static NodeList* Analyze_List(LinearAllocator* Arena, Node* Block, ParsingContext Context, bool bInIf)
{
    NodeList* IndeterminateList = NULL;
    NodeList** IndeterminateNext = &IndeterminateList;

    // this is a namespace basically
    // SomeKey {
    //     AnotherKey some value
    // }
    // 
    // which will become one key: SomeKey.AnotherKey some value
    if (Block->Key.Length > 0)
    {
        StringList** NextKey = &Context.ParentKeys;
        while (*NextKey)
        {
            NextKey = &(*NextKey)->Next;
        }
        *NextKey = StringList_Create(Arena, Block->Key, NULL);
    }

    NodeList** Next = &Block->List;
    while (*Next)
    {
        Node* Root = (*Next)->Node;

        bool bValid = Root && Root != &Node_Null;
        if (bValid)
        {
            if (Root->Type == Node_Block)
            {
                NodeList* BlockTree = Analyze_List(Arena, Root, Context, bInIf);
                if (BlockTree)
                {
                    SLinkedList_Push(IndeterminateNext, BlockTree);
                }
            }
            else if (Root->Type == Node_If)
            {
                NodeList* IfTree = Analyze_IfNode(Arena, Root, Context, true);
                if (IfTree)
                {
                    LOG_INLINE_WARNING("IF <indeterminate>\n");
                    SLinkedList_Push(IndeterminateNext, IfTree);
                }
            }
            else if (Root->Type == Node_Help)
            {
                if (bInIf) // TODO: ensure it is at root block
                {
                    LOG_ERROR("'.Help' can not be inside an 'if' or 'else' block");
                    return NULL;
                }

                bool bSuccess = Analyze_KVNode(Arena, Root, Context);
                if (!bSuccess)
                {
                    SLinkedList_Push(IndeterminateNext, NodeList_Create(Arena, Root, NULL));
                }
            }
            else if (Root->Type == Node_Include)
            {
                NodeList* IncludeTree = Analyze_IncludeNode(Arena, Root, Context);
                if (IncludeTree)
                {
                    SLinkedList_Push(IndeterminateNext, IncludeTree);
                }
            }
            else if (Root->Type == Node_ErrorMessage)
            {
                if (bInIf) // TODO: ensure it is at root block
                {
                    LOG_ERROR("'%S' can not be inside an 'if' or 'else' block", Root->Key);
                    return NULL;
                }

                bool bSuccess = Analyze_KVNode(Arena, Root, Context);
                if (!bSuccess)
                {
                    SLinkedList_Push(IndeterminateNext, NodeList_Create(Arena, Root, NULL));
                }
            }
            else if (Root->Type == Node_LogMessage)
            {
                StringLocal(Expanded, MAX_VALUE_LENGTH);
                bool bSuccess = false;

                LOG_INLINE("LOG: ");

                LinearAllocator Scratch = *Arena;
                {
                    String Message = String_CreateFromList(&Scratch, *Root->Value);

                    if (!ExpandBuildVariableV2(Scratch, Context.VariablesDB, Context.CmdOptionsDB, &Expanded, S(""), Message, S(""), Context.WorkingDirectory, false, false))
                    {
                        LOG_INLINE_WARNING("<indeterminate>\n");
                        bSuccess = false;
                    }
                    else
                    {
                        LOG("%S", Expanded);
                        bSuccess = true;
                    }
                }

                LOG_LINE_BREAK();

                if (bSuccess)
                {
                    String Message = String_Create(Arena, Expanded);
                    Array_Add(Context.Messages, Message);
                }
                else
                {
                    SLinkedList_Push(IndeterminateNext, NodeList_Create(Arena, Root, NULL));
                }
            }
            else if (Root->Type == Node_KeyValue)
            {
                bool bSuccess = Analyze_KVNode(Arena, Root, Context);
                if (!bSuccess)
                {
                    SLinkedList_Push(IndeterminateNext, NodeList_Create(Arena, Root, NULL));
                }
            }
        }

        Next = &(*Next)->Next;
    }

    return IndeterminateList;
}

NO_DISCARD bool ParseBuildFileV2(LinearAllocator* Arena,
                    const FileHandle H,
                    const String BuildFilePath,
                    const String WorkingDirectory,
                    TArray(FileVariable) VariablesDB,
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

    {
        Node* AST = Internal_ParseBuildFile(Arena, H);

        // now do some analysis on the tree (two-pass analysis)

        // we could just end here and pass the raw data to the main program and let it do things with it.
        // this can almost become a general language that others may find useful, it has basic support for
        // if's and namespacing keys. so all it would do it just parse the file and give you key:value array
        // that the program can use and interpret how it wants.
        // TODO: think about making this a library?

        // High level flow:
        // 1. (first pass) expand all keys possible (skip indeterminates)
        //  1a. parse include files (get the ast trees) and repeat step 1
        // 2. (second pass) expand all keys and error on vars that dont exist
        //  2a. parse include files (get the ast trees) and repeat step 1 independently then continue again with the second pass
        // 3. run asserts

        LOG("Analyzing the AST tree...");

        Clock c;
        Clock_Start(&c);

        LOG("[FIRST PASS]");

        ParsingContext Context = {0};
        Context.VariablesDB  = VariablesDB;
        Context.CmdOptionsDB = CmdOptionsDB;
        Context.Messages     = Messages;
        Context.WorkingDirectory = WorkingDirectory;
        NodeList* IndeterminateList = Analyze_List(Arena, AST, Context, false);
        (void)IndeterminateList;

        LOG("[SECOND PASS]");
        
        Clock_Tick(&c);
        Clock_PrintElapsedTime(&c, true);
    }

    return true;
}

bool ParseBuildFile(LinearAllocator* Arena,
                    const FileHandle H,
                    const String BuildFilePath,
                    const String WorkingDirectory,
                    TArray(FileVariable) VariablesDB,
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
                    /*
                    while (*Next)
                    {
                        Next = &(*Next)->Next;
                    }

                    *Next = &Entry;
                    */

                    SLinkedList_Push(Next, &Entry);

                    Array_Add(IncludeFiles, IncludeFileHandle);

                    if (!ParseBuildFile(Arena, IncludeFileHandle, BuildFilePath, WorkingDirectory,
                                        VariablesDB, CmdOptionsDB, Messages,
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

bool ExpandBuildVariableV2(LinearAllocator Scratch, TArray(FileVariable) VariablesDB, TArray(CmdOption) CmdOptionsDB,
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

        String Slice = String_Null();
        bool bWantsToLower = false;
        bool bWantsToUpper = false;
        bool bWantsPaste   = false;

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

            if (String_EatCharInline(&StrVal, C))
            {
                Offset++;
                bWantsPaste = true; 
            }

            if (Index == 0)
            {
                (void)String_IndexOfFirstNonAlphaNumeric(StrVal, &Index);
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

            const bool bHasNot     = Slice.Length > 1 ? String_EatCharInline(&Slice, '!') : false; // TODO: remove, this wont work
            //const bool bWantsPaste = Slice.Length > 1 ? String_EatCharInline(&Slice, '%') : false;

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
                        bEqualsToSomething = v.Value.Length > 0;
                        break;
                    }
                }
            }

            if (!bFoundCmd)
            {
                return false;
            }

            // run through the cmd var assert list
            // TODO: something better
            /*
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
            */

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

                if (!ExpandBuildVariableV2(Scratch, VariablesDB, CmdOptionsDB, Dest, Slice, VarValue, Root, WorkingDirectory, false, bIsAssemblyExe))
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
                //LOG_WARNING("Unrecognized build variable \"%S\". Expanded to nothing...", Slice);
                return false;
                //continue;
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
                    if (!ExpandBuildVariableV2(Scratch, VariablesDB, CmdOptionsDB, &TempDest, Slice, Var.Value, Root, WorkingDirectory, bLowerStrings, bIsAssemblyExe))
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
                // TODO: second pass, show error message
                /*
                LOG_ERROR("Could not retrieve environment variable named %S\n", Slice);

                if (LogCustomErrorMessage(VariablesDB, S("Env"), Slice, false))
                {
                    LOG_LINE_BREAK();
                }

                LogRegularEnvVarTutorialSteps();
                */

                return false;
            }

            String DestEnd = StrShiftF(*Dest, Dest->Length);
            u32 DestLengthBefore = Dest->Length;

            if (!ExpandBuildVariableV2(Scratch, VariablesDB, CmdOptionsDB, Dest, Slice, VarValue, Root, WorkingDirectory, false, bIsAssemblyExe))
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

    (void)String_EatSpacesInlineFromEnd(Dest);

    return true;
}
