// Copyright (c) Artisan Softworks
// Licensed under the BSD 3-Clause License. See the LICENSE file for details.

#ifndef UNITY_BUILD
#include "Backend.h"

#include "Core/Allocators.h"
#include "Core/Platform.h"
#include "Core/StringUtils.h"
#include "Core/Array.h"
#include "Core/Clock.h"
#include "Core/Log.h"
#endif

#if PLATFORM_WINDOWS
#include "MicrosoftCraziness.h"
#endif

// todos
// provide examples with every parser error message
// how to detect multiple inclusions of a file?
// prevent including .build files
// log .build file path for parser error logs

void AddVariable(LinearAllocator* Arena,
                TArray(FileVariable) VariablesDB,
                const String Name,
                const String Value,
                const String Params,
                u32 MaxValueLength)
{
    FileVariable var;
    var.Params       = String_Create(Arena, Params);
    var.Name         = String_CreateMax(Arena, Name, MAX_KEY_LENGTH);
    var.Value        = String_ReserveAndCopy(Arena, MaxValueLength, Value);

    // todo: can be made static?
    Array_Add(VariablesDB, var);
}

void AddOrAppendVariable(LinearAllocator* Arena,
                        TArray(FileVariable) VariablesDB,
                        const String Name,
                        const String Value,
                        const String Params,
                        u32 MaxValueLength)
{
    FileVariable* Ref = NULL;
    for each (FileVariable, v, VariablesDB)
    {
        if (String_IsEqual(Name, v.Name, false))
        {
            Ref = v_;
            break;
        }
    }

    if (Ref)
    {
        xx String_EatSpacesInlineFromEnd(&Ref->Value);
        if (Ref->Value.Length) { String_AppendSpace(&Ref->Value); }
        String_Append(&Ref->Value, Value);
        xx String_EatSpacesInlineFromEnd(&Ref->Value);
    }
    else
    {
        AddVariable(Arena, VariablesDB, Name, Value, Params, MaxValueLength);
    }
}

static FileVariable* GetVarInList(FileVariableList* List, const String Key, bool bStartsWith)
{
    FileVariable* Result = &FileVariable_Empty;
    {
        SLinkedList_Each(FileVariableList, This, &List)
        {
            FileVariable* Var = &(*This)->Var;

            if (bStartsWith)
            {
                if (String_StartsWith(Var->Name, Key, false))
                {
                    Result = Var;
                    break;
                }
            }
            else
            {
                if (String_IsEqual(Var->Name, Key, false))
                {
                    Result = Var;
                    break;
                }
            }
        }
    }
    return Result;
}

static String GetVarValueInList(FileVariableList* List, const String Key)
{
    String Result = String_Null();
    {
        SLinkedList_Each(FileVariableList, This, &List)
        {
            const FileVariable Var = (*This)->Var;

            if (String_IsEqual(Var.Name, Key, false))
            {
                Result = Var.Value;
                break;
            }
        }
    }
    return Result;
}

static bool DoesVarExistInList(FileVariableList* List, const String Slice)
{
    bool bFound = false;
    {
        SLinkedList_Each(FileVariableList, This, &List)
        {
            const FileVariable Var = (*This)->Var;

            if (String_IsEqual(Var.Name, Slice, false))
            {
                bFound = true;
                break;
            }
        }
    }
    return bFound;
}

FORCEINLINE NO_DISCARD RETURN_NON_NULL static FileVariableList* FileVariableList_Create(LinearAllocator* Arena, FileVariable Var)
{
    FileVariableList* List = LinearAllocator_Allocate(Arena, sizeof(struct FileVariableList));
    List->Var  = Var;
    List->Next = NULL;
    return List;
}

static void AddVariableToList(LinearAllocator* Arena, ParsingContext* Context, const String Key, const String Value, const String Params)
{
    FileVariable var = {0};
    var.Params       = String_Create(Arena, Params);
    var.Name         = String_Create(Arena, Key);
    var.Value        = String_Create(Arena, Value);

    SLinkedList_Push(Context->VarListTail, FileVariableList_Create(Arena, var));
}

ENUM_T(ETokenType, u32)
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
    Token_Assert,
    Token_ErrorMessage,

    Token_Whitespace,
    Token_Newline,

    Token_Max
};

#define MAX_TOKENS        4096
#define Token_Char_Not    '!'
#define Token_Char_At     '@'
#define Token_Char_Mod    '%'
#define Token_Char_Dollar '$'

STRUCT(Token) // 24 bytes
{
    String     Lexeme;
    u32        Line;
    ETokenType Type;
};

read_only static Token Token_Null = { .Line = 0, .Lexeme = SC(""), .Type = Token_None };

read_only static String TokenTypeEnumStringTable_NoPrefix[Token_Max] =
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
    SC("Assert"),
    SC("ErrorMessage"),

    SC("Whitespace"),
    SC("Newline"),
};

read_only static String TokenTypeEnumStringTable[Token_Max] =
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
    SC("Token_Assert"),
    SC("Token_ErrorMessage"),

    SC("Token_Whitespace"),
    SC("Token_Newline"),
};

STRUCT(Lexer)
{
    Token* Tokens;
    String Text;
    u32 Current;
    u32 Start;
    u16 Line;
    u16 NumTokens;
    ETokenType Type;
};

ENUM_T(ENodeType, u32)
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
    StringList* TestValues;
    ETokenType ComparisonOp;
    u8 Prefix;
    u8 Padding[3];
};

STRUCT(IfConditionList)
{
    IfConditionData Data;
    struct IfConditionList* Next;
};

FORCEINLINE NO_DISCARD RETURN_NON_NULL static IfConditionList* IfConditionList_Create(LinearAllocator* Arena, IfConditionData Value)
{
    IfConditionList* List = LinearAllocator_Allocate(Arena, sizeof(struct IfConditionList));
    List->Data     = Value;
    List->Next     = NULL;
    return List;
}

STRUCT(Node)
{
    ENodeType   Type;
    u8          Padding[4];

    String      Key;
    StringList* Value;
    StringList* Parameters;
    IfConditionList* ConditionList;

    Node*       Parent;
    Node*       Left;
    Node*       Right;

    NodeList*   List;

    bool        bPreserveOrder;
    u8          Padding2[7];
};

read_only static Node Node_Null = { .Type = Node_None, .Left = &Node_Null, .Right = &Node_Null };

STRUCT(Parser)
{
    u32    Current;
    u32    NumTokens;
    Token* Tokens;
};

FORCEINLINE NO_DISCARD RETURN_NON_NULL static NodeList* NodeList_Create(LinearAllocator* Arena, Node* InNode, NodeList* Next)
{
    NodeList* List = LinearAllocator_Allocate(Arena, sizeof(struct NodeList));
    List->Node     = InNode;
    List->Next     = Next;
    return List;
}

FORCEINLINE NO_DISCARD RETURN_NON_NULL static NodeList* NodeList_CreateNull(LinearAllocator* Arena)
{
    NodeList* List = LinearAllocator_Allocate(Arena, sizeof(struct NodeList));
    List->Node     = NULL;
    List->Next     = NULL;
    return List;
}

FORCEINLINE NO_DISCARD RETURN_NON_NULL static Node* Node_Create(LinearAllocator* Arena, ENodeType Type)
{
    Node* NewNode = LinearAllocator_Allocate(Arena, sizeof(struct Node));
    NewNode->Type = Type;
    return NewNode;
}

FORCEINLINE NO_DISCARD RETURN_NON_NULL static Node* Node_Create_KeyValue(LinearAllocator* Arena, String Key, StringList* Value)
{
    Node* NewNode       = LinearAllocator_Allocate(Arena, sizeof(struct Node));
    NewNode->Type       = Node_KeyValue;
    NewNode->Key        = Key;
    NewNode->Value      = Value;
    return NewNode;
}

STRUCT(KeywordTableEntry)
{
    String     Name;
    ETokenType Type;
    u8         Padding[4];
};

static KeywordTableEntry ReservedKeywordsTable[12] =
{
    { .Type = Token_If,           .Name = SC("if")          },
    { .Type = Token_Else,         .Name = SC("else")        },
    { .Type = Token_Include,      .Name = SC("include")     },
    { .Type = Token_Or,           .Name = SC("or")          },
    { .Type = Token_Contains ,    .Name = SC("contains")    },
    { .Type = Token_StartsWith,   .Name = SC("starts_with") },
    { .Type = Token_EndsWith,     .Name = SC("ends_with")   },
    { .Type = Token_Stop,         .Name = SC(".stop")       },
    { .Type = Token_Abort,        .Name = SC(".abort")      },
    { .Type = Token_Help,         .Name = SC(".help")       },
    { .Type = Token_Assert,       .Name = SC("assert")      },
    { .Type = Token_ErrorMessage, .Name = SC("ErrorMessage")},
};

static KeywordTableEntry ReservedStartingKeywordsTable[1] =
{
    { .Type = Token_Assert, .Name = SC("Assert.")},
};

static KeywordTableEntry ReservedEndingKeywordsTable[1] =
{
    { .Type = Token_ErrorMessage, .Name = SC(".ErrorMessage")},
};

STRUCT(ReservedKeyTable)
{
    String Key;
    u32    MaxValueLength;
    u32    Padding;
};

static ReservedKeyTable ReservedKeys[71] =
{
    { .Key = SC("Assembly"),                  .MaxValueLength = 256 },
    { .Key = SC("Assembly.Prefix"),           .MaxValueLength = 128 },
    { .Key = SC("Assembly.Postfix"),          .MaxValueLength = 128 },
    { .Key = SC("Extension"),                 .MaxValueLength = 64 },
    { .Key = SC("Type"),                      .MaxValueLength = 64 },
    { .Key = SC("SourceDirectory"),           .MaxValueLength = 256 },
    { .Key = SC("BuildDirectory"),            .MaxValueLength = 256 },
    { .Key = SC("IntermediateDirectory"),     .MaxValueLength = 256 },
    // { .Key = SC("Compiler"),                  .MaxValueLength = 256 },
    { .Key = SC("Compiler.Path"),             .MaxValueLength = 1024 },
    { .Key = SC("Compiler.Flags"),            .MaxValueLength = 4096 },
    { .Key = SC("Compiler.MaxCores"),         .MaxValueLength = 16 },
    { .Key = SC("Compiler.OutputFlag"),       .MaxValueLength = 16 },
    { .Key = SC("Compiler.CompileFlag"),      .MaxValueLength = 16 },
    { .Key = SC("Compiler.ObjectExtension"),  .MaxValueLength = 32 },
    { .Key = SC("Compiler.ObjectDirectory"),  .MaxValueLength = 1024 },
    // TODO: Linker.RPath. can specify multiple, spearated by spaces
    { .Key = SC("Linker.Path"),               .MaxValueLength = 1024 },
    { .Key = SC("Linker.Flags"),              .MaxValueLength = 8192 },
    { .Key = SC("Linker.Flags.Public"),       .MaxValueLength = 8192 },
    { .Key = SC("Linker.Defines"),            .MaxValueLength = 4096 },
    { .Key = SC("Linker.EntryPoint"),         .MaxValueLength = 256 },
    { .Key = SC("Linker.Subsystem"),          .MaxValueLength = 128 },
    { .Key = SC("Linker.Stack"),              .MaxValueLength = 64 },
    { .Key = SC("Linker.NoStdLib"),           .MaxValueLength = 0 },
    { .Key = SC("Linker.NoDefaultLibs"),      .MaxValueLength = 0 },
    { .Key = SC("Assembler.Path"),            .MaxValueLength = 1024 },
    { .Key = SC("Assembler.Flags"),           .MaxValueLength = 4096 },
    { .Key = SC("Assembler.Includes"),        .MaxValueLength = 8192 },
    { .Key = SC("Assembler.Defines"),         .MaxValueLength = 4096 },
    { .Key = SC("Archiver.Path"),             .MaxValueLength = 1024 },
    { .Key = SC("Archiver.Flags"),            .MaxValueLength = 4096 },
    { .Key = SC("Defines"),                   .MaxValueLength = 8192 },
    { .Key = SC("Defines.Public"),            .MaxValueLength = 8192 },
    { .Key = SC("UnDefines"),                 .MaxValueLength = 2048 },
    { .Key = SC("Includes"),                  .MaxValueLength = 8192 },
    { .Key = SC("Includes.Public"),           .MaxValueLength = 8192 },
    { .Key = SC("Libraries"),                 .MaxValueLength = 2048 },
    { .Key = SC("Libraries.Public"),          .MaxValueLength = 2048 },
    { .Key = SC("Library.Paths"),             .MaxValueLength = 8192 },
    { .Key = SC("Library.Paths.Public"),      .MaxValueLength = 8192 },
    { .Key = SC("SourceFiles"),               .MaxValueLength = 32767 },
    { .Key = SC("SourceFiles.Exclude"),       .MaxValueLength = 8192 },
    { .Key = SC("SourceDirectories"),         .MaxValueLength = 8192 },
    { .Key = SC("SourceDirectories.Exclude"), .MaxValueLength = 8192 },
    { .Key = SC("Icon"),                      .MaxValueLength = 256 },
    { .Key = SC("PCH"),                       .MaxValueLength = 256 },
    { .Key = SC("PCH.h"),                     .MaxValueLength = 256 },
    { .Key = SC("Bundle"),                    .MaxValueLength = 0 },
    { .Key = SC("Bundle.IsTerminal"),         .MaxValueLength = 0 },
    { .Key = SC("Bundle.InfoPlist"),          .MaxValueLength = 256 },
    { .Key = SC("Bundle.VersionPlist"),       .MaxValueLength = 256 },
    { .Key = SC("Bundle.PkgInfo"),            .MaxValueLength = 256 },
    { .Key = SC("Info.plist"),                .MaxValueLength = 8192 },
    { .Key = SC("Version.plist"),             .MaxValueLength = 8192 },
    { .Key = SC("TitleName"),                 .MaxValueLength = 1024 },
    { .Key = SC("InternalName"),              .MaxValueLength = 256 },
    { .Key = SC("Description"),               .MaxValueLength = 1024 },
    { .Key = SC("CompanyName"),               .MaxValueLength = 128 },
    { .Key = SC("Copyright"),                 .MaxValueLength = 256 },
    { .Key = SC("Version"),                   .MaxValueLength = 256 },
    { .Key = SC("License"),                   .MaxValueLength = 128 },
    { .Key = SC("License.Path"),              .MaxValueLength = 256 },
    { .Key = SC("License.FileName"),          .MaxValueLength = 128 },
    { .Key = SC("AlwaysRebuild"),             .MaxValueLength = 0 },
    { .Key = SC("AlwaysRebuildAll"),          .MaxValueLength = 0 },
    { .Key = SC("PreDepend"),                 .MaxValueLength = 256 },
    { .Key = SC("PreBuild"),                  .MaxValueLength = 256 },
    { .Key = SC("PostBuild"),                 .MaxValueLength = 256 },
    { .Key = SC("PreCompile"),                .MaxValueLength = 256 },
    { .Key = SC("PostCompile"),               .MaxValueLength = 256 },
    { .Key = SC("PreLink"),                   .MaxValueLength = 256 },
    { .Key = SC("PostLink"),                  .MaxValueLength = 256 },
};

u32 GetMaxValueLengthForReservedKey(const String Key)
{
    u32 MaxLength = 0;
    for (u8 i = 0; i < SArray_Capacity(ReservedKeys); i++)
    {
        if (String_IsEqual(Key, ReservedKeys[i].Key, false))
        {
            MaxLength = ReservedKeys[i].MaxValueLength;
            break;
        }
    }

    return MaxLength;
}

STRUCT(DeferredKVData)
{
    Token* Key;
    StringList* Params;
    Node* FilterNode;
    Node* LastIfNode;
};

read_only static DeferredKVData DeferredKVData_Null = 
{
    .Key = &Token_Null,
    .Params = NULL,
    .FilterNode = &Node_Null,
    .LastIfNode = &Node_Null
};

/*
static bool Parser_IsTokenDisallowedInIfElseBlock(ETokenType Type)
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
*/

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

FORCEINLINE static void Lexer_Advance(Lexer* L)
{
    L->Current += 1;
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
    const u32 Diff = L->Current - L->Start;

    Token NewToken;
    NewToken.Lexeme.Data     = L->Text.Data + L->Start;
    NewToken.Lexeme.Length   = Diff;
    NewToken.Lexeme.Capacity = 0;
    NewToken.Line            = L->Line;
    NewToken.Type            = Type;

    L->Tokens[L->NumTokens] = NewToken;
    L->NumTokens++;
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

//
// PARSER --------------------------------------------------------
//

static void Parser_Advance(Parser* P)
{
    //if (P->Current < P->NumTokens)
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

    xx Parser_Match(P, Token_Newline);

    StringList* ValueList = NULL;
    StringList** Next = &ValueList;

    if (Parser_Match(P, Token_LCurly) || 
        Parser_Match(P, Token_LSquare))
    {
        while (Parser_Peek(P).Type != Token_None &&
               Parser_Peek(P).Type != Token_RCurly &&
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
        while (Parser_Peek(P).Type != Token_None &&
               !(Parser_Peek(P).Type == Token_Newline ||
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

    xx Parser_Match(P, Token_Newline);

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
        while (Parser_Peek(P).Type != Token_None &&
               !(Parser_Peek(P).Type == Token_Newline ||
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

NO_DISCARD RETURN_NON_NULL static Node* Parse_If(LinearAllocator* Arena, Parser *P, u32 Offset);
NO_DISCARD RETURN_NON_NULL static Node* Parse_Block(LinearAllocator* Arena, Parser *P, u32 Offset, bool bInIf);

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
        LOG_ERROR("\n[Parser] [Line %u]: '%S' was unexpected after 'include'. Expected a file path or expression.", Parser_Peek(P).Line, Parser_Peek(P).Lexeme);
        return &Node_Null;
    }

    return Root;
}


NO_DISCARD RETURN_NON_NULL static Node* Parse_If(LinearAllocator* Arena, Parser* P, u32 Offset)
{
    Node* Root = Node_Create(Arena, Node_If);

    IfConditionList* ConditionList = NULL;
    IfConditionList** NextCondition = &ConditionList;

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
                // todo get line number for the else token
                LOG_ERROR("\n[Parser] [Line %u]: '}' is missing for '%S' block.", Parser_LookBack(P).Line, LastTokenType == Token_If ? S("if") : S("else"));
                return &Node_Null;
            }

            if (LastTokenType == Token_If)
            {
                Root->Left = BlockNode;
            }
            else
            {
                Root->Right = BlockNode;
                break;
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
                    LOG_ERROR("\n[Parser] [Line %u]: '{' are not allowed for inline if statements.", Parser_LookBack(P).Line);
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

            bInlineIf = false;
        }
        else if (t.Type == Token_If)
        {
            break;
        }
        // evaluate if conditions
        else if (t.Type is Token_Text         or
                 t.Type is Token_Assert       or
                 t.Type is Token_ErrorMessage or
                 t.Type is Token_Not          or
                 t.Type is Token_At           or
                 t.Type is Token_Mod          or
                 t.Type is Token_Dollar       or
                 t.Type is Token_Quote)
        {
            if (bInlineIf)
            {
                break;
            }

            Token NextToken = t;
            while (NextToken.Type == Token_Text   ||
                   NextToken.Type == Token_Assert ||
                   NextToken.Type == Token_Not    ||
                   NextToken.Type == Token_At     ||
                   NextToken.Type == Token_Mod    ||
                   NextToken.Type == Token_Dollar ||
                   NextToken.Type == Token_Quote)
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
                    LOG_ERROR("\n[Parser] [Line %u]: '%S' are not allowed within 'if' statements. Please delete.", Parser_Peek(P).Line, Parser_Peek(P).Lexeme);
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
                    Comparison.Type != Token_Assert &&
                    Comparison.Type != Token_ErrorMessage &&
                    Comparison.Type != Token_Help &&
                    Comparison.Type != Token_Include &&
                    Comparison.Type != Token_Stop &&
                    Comparison.Type != Token_Abort &&
                    Comparison.Type != Token_LCurly &&
                    Comparison.Type != Token_Pipe &&
                    Comparison.Type != Token_Or &&
                    Comparison.Type != Token_Quote &&
                    Comparison.Type != Token_Newline &&

                    !(Comparison.Type == Token_EqualEqual   || Comparison.Type == Token_NotEqual    ||
                    Comparison.Type == Token_GreaterOrEqual || Comparison.Type == Token_LessOrEqual ||
                    Comparison.Type == Token_GreaterThan    || Comparison.Type == Token_LessThan    ||
                    Comparison.Type == Token_StartsWith     || Comparison.Type == Token_EndsWith    ||
                    Comparison.Type == Token_Contains))
                {
                    LOG_ERROR("\n[Parser] [Line %u]: '%S' was unexpected after '%S'. Expected either another 'if', comparison operator, Key Value, or new block after '%S'. Please delete.", Comparison.Line,Comparison.Lexeme, Parser_LookBack(P).Lexeme, Parser_LookBack(P).Lexeme);
                    return &Node_Null;
                }

                if (Comparison.Type == Token_If)
                {
                    SLinkedList_Push(NextCondition, IfConditionList_Create(Arena, Condition));

                    Node* IfNode = Parse_If(Arena, P, 1);
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
                            StringList* ValueList = NULL;
                            StringList** NextValue = &ValueList;

                            SLinkedList_Push(NextValue, StringList_Create(Arena, TestToken.Lexeme, NULL));

                            Parser_Advance(P);

                            while (Parser_Match(P, Token_Pipe) ||
                                   Parser_Match(P, Token_Or))
                            {
                                SLinkedList_Push(NextValue, StringList_Create(Arena, Parser_Peek(P).Lexeme, NULL));
                            }

                            Condition.TestValues = ValueList;

                            Parser_Advance(P);
                        }
                        else
                        {
                            LOG_ERROR("\n[Parser] [Line %u]: '%S' was unexpected after '%S'. Please delete.", TestToken.Line, TestToken.Lexeme, Comparison.Lexeme);
                            return &Node_Null;
                        }
                    }

                    SLinkedList_Push(NextCondition, IfConditionList_Create(Arena, Condition));

                    Parser_SkipWhitespace(P);

                    // todo: relook at how i can simplify this across other areas of teh codebase
                    if (Parser_Peek(P).Type == Token_Text ||
                        Parser_Peek(P).Type == Token_Assert ||
                        Parser_Peek(P).Type == Token_ErrorMessage ||
                        Parser_Peek(P).Type == Token_Quote ||
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
                LOG_ERROR("\n[Parser] [Line %u]: '%S' was unexpected after 'if'", t.Line, t.Lexeme);
            }
            else
            {
                LOG_ERROR("\n[Parser] [Line %u]: Missing expression after 'if'", t.Line);
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

    return Root;
}

static void Internal_AssignParentToChildrenRecursively(Node* Parent, NodeList* Children)
{
    NodeList** This = &Children;
    while (*This)
    {
        if (!(*This)->Node->Parent)
        {
            (*This)->Node->Parent = Parent;

            if ((*This)->Node->Left)
            {
                (*This)->Node->Left->Parent = Parent;
                if ((*This)->Node->Left->List)
                {
                    Internal_AssignParentToChildrenRecursively(Parent, (*This)->Node->Left->List);
                }
            }

            if ((*This)->Node->Right)
            {
                (*This)->Node->Right->Parent = Parent;
                if ((*This)->Node->Right->List)
                {
                    Internal_AssignParentToChildrenRecursively(Parent, (*This)->Node->Right->List);
                }
            }
        }

        This = &(*This)->Next;
    }
}

NO_DISCARD RETURN_NON_NULL static NodeList* Internal_CreateNodeListFromDeferred(LinearAllocator* Arena, DeferredKVData Deferred)
{
    NodeList* List = NodeList_CreateNull(Arena);
    Node* KV_Node = Node_Create_KeyValue(Arena, Deferred.Key->Lexeme, NULL);
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

    return List;
}

NO_DISCARD RETURN_NON_NULL static Node* Parse_Block(LinearAllocator* Arena, Parser* P, u32 Offset, bool bInlineIf)
{
    Node* Root = Node_Create(Arena, Node_Block);
    NodeList** NextNode = &Root->List;

    P->Current += Offset;

    u32 Start = 0;
    u32 NumTokens = P->NumTokens;
    Token LastRootToken = Token_Null;
    bool bSkipRootTokenUpdate = false;

    DeferredKVData Deferred = DeferredKVData_Null;

    while (P->Current < NumTokens)
    {
        Start = P->Current;
        Token t = P->Tokens[P->Current];
        Token* tPtr = &P->Tokens[P->Current];

        if (t.Type == Token_Semicolon  ||
            t.Type == Token_Whitespace ||
            t.Type == Token_Stop       ||
            t.Type == Token_Abort)
        {
        }
        else if (t.Type == Token_Newline)
        {
            if (bInlineIf && Deferred.Key == &Token_Null)
            {
                break;
            }
        }
        else if (t.Type == Token_LCurly)
        {
            ETokenType PrevTokenType = LastRootToken.Type;

            if (PrevTokenType != Token_Text &&
                PrevTokenType != Token_Assert)
            {
                LOG_ERROR("\n[Parser] [Line %u]: Anonymous blocks are not allowed. Missing <key> before '{'.", t.Line);
                return &Node_Null;
            }

            NodeList* List = NodeList_CreateNull(Arena);
            Node* BlockNode = Parse_Block(Arena, P, 1, false);
            if (BlockNode == &Node_Null)
            {
                return &Node_Null;
            }

            if (!Parser_Match(P, Token_RCurly))
            {
                LOG_ERROR("\n[Parser] [Line %u]: '}' is missing for '%S' block.", Parser_LookBack(P).Line, ETokenTypeNoPrefix_ToString(PrevTokenType));
                return &Node_Null;
            }

            bool bIsDeferring = Deferred.Key != &Token_Null;
            if (bIsDeferring)
            {
                BlockNode->Key = Deferred.Key->Lexeme;
                BlockNode->Parameters = Deferred.Params;

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

            // assign this block to all children nodes (recursively)
            Internal_AssignParentToChildrenRecursively(BlockNode, BlockNode->List);

            const String KeysThatPreserveOrder[7] =
            {
                S("PreDepend"),
                S("PreBuild"),
                S("PostBuild"),
                S("PreCompile"),
                S("PostCompile"),
                S("PreLink"),
                S("PostLink"),
            };

            bool bKeyMatch = false;
            for EachElement(i, KeysThatPreserveOrder)
            {
                if (String_IsEqual(BlockNode->Key, KeysThatPreserveOrder[i], false))
                {
                    bKeyMatch = true;
                    break;
                }
            }

            BlockNode->bPreserveOrder = bKeyMatch;

            SLinkedList_Push(NextNode, List);

            Deferred = DeferredKVData_Null;

            if (bInlineIf && bIsDeferring)
            {
                break;
            }
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
                }

                Parser_Advance(P); // go past ']'

                Node* KV_Node = Node_Create_KeyValue(Arena, Deferred.Key->Lexeme, ValueList);
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

                if (bInlineIf)
                {
                    break;
                }
            }
            else
            {
                LOG_ERROR("\n[Parser] [Line %u]: Unexpected token '['. Please delete.", t.Line);
                return &Node_Null;
            }
        }
        // the positioning of this if statement is important, do not move this!!
        // this means this key has no value
        else if (Deferred.Key != &Token_Null)
        {
            NodeList* List = Internal_CreateNodeListFromDeferred(Arena, Deferred);
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

            if (bInlineIf)
            { 
                break;
            }
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
                LOG_ERROR("\n[Parser] [Line %u]: '%S' must be paired with a key.", t.Line, t.Lexeme);
                
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

            if (bInlineIf) { break; }
        }
        else if (t.Type == Token_Text || t.Type == Token_Assert)
        {
            Parser_SkipWhitespace(P);

            // this means we are the key
            xx Parser_Match(P, Token_Text);
            xx Parser_Match(P, Token_Assert);

            if (!IsAlphabet(t.Lexeme.Data[0]) && t.Lexeme.Data[0] != '.')
            {
                LOG_ERROR("\n[Parser] [Line %u]: Key '%S' can only start with an alphabet character or '.'. Please remove '%c'", t.Line, t.Lexeme, t.Lexeme.Data[0]);
                return &Node_Null;
            }

            if (t.Lexeme.Length > MAX_KEY_LENGTH)
            {
                LOG_ERROR("\n[Parser] [Line %u]: Key '%S' exceeds %u characters. Please shorten to %u or less characters", t.Line, t.Lexeme, MAX_KEY_LENGTH, MAX_KEY_LENGTH);
                return &Node_Null;
            }

            StringList* ParamList = NULL;
            if (Parser_Match(P, Token_LParen))
            {
                StringList** Next = &ParamList;
                while (Parser_Peek(P).Type == Token_Text       ||
                    //    Parser_Peek(P).Type == Token_Whitespace ||
                       Parser_Peek(P).Type == Token_Colon)
                {
                    /*
                    if (Parser_Peek(P).Type == Token_Whitespace)
                    {
                        SLinkedList_Push(Next, StringList_Create(Arena, S(" "), NULL));
                    }
                    else
                    */
                    {
                        SLinkedList_Push(Next, StringList_Create(Arena, Parser_Peek(P).Lexeme, NULL));
                    }

                    Parser_Advance(P);
                    Parser_SkipWhitespace(P);
                }

                if (!Parser_Match(P, Token_RParen))
                {
                    LOG_ERROR("\n[Parser] [Line %u]: '%S' was unexpected within parameter list.", Parser_Peek(P).Line, Parser_Peek(P).Lexeme);
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
                            LOG_ERROR("\n[Parser] [Line %u]: Text was expected after '%S'. Please delete '%S'", Parser_Peek(P).Line, Parser_LookBack(P).Lexeme, Parser_Peek(P).Lexeme);
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

                    Parser_SkipWhitespace(P);
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

                Node* KV_Node = Node_Create_KeyValue(Arena, tPtr->Lexeme, ValueList);
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
            if (!bInlineIf)
            {
                LOG_ERROR("\n[Parser] [Line %u]: Illegal 'else' without matching 'if'", t.Line);
                return &Node_Null;
            }

            break;
        }
        else if (t.Type == Token_If)
        {
            bSkipRootTokenUpdate = true;

            NodeList* List = NodeList_CreateNull(Arena);
            Node* IfNode = Parse_If(Arena, P, 1);
            List->Node = IfNode;
            if (IfNode == &Node_Null)
            {
                return &Node_Null;
            }

            SLinkedList_Push(NextNode, List);
        }
        else
        {
            LOG_ERROR("\n[Parser] [Line %u]: Keys can not start with '%S'. Please delete.", t.Line, t.Lexeme);
            return &Node_Null;
        }

        // only advance when nothing happened
        if (Start == P->Current)
        {
            Parser_Advance(P);
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

    // if we reached the end of this block but we haven't made a node out of it yet, in the case of EOF
    // this means this key has no value
    if (Deferred.Key != &Token_Null)
    {
        NodeList* List = Internal_CreateNodeListFromDeferred(Arena, Deferred);
        SLinkedList_Push(NextNode, List);
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

    LOG("\n%S [KEY]      %S", Spaces, Root->Key);

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
                LOG(" %S[%S]", Spaces, Root->Key);

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
                StringLocal(Conditions, 512);
                if (c.TestValues)
                {
                    for each_string_in_list (*c.TestValues)
                    {
                        String_Append(&Conditions, It.String);
                        String_AppendChar(&Conditions, '|');
                    }
                    xx String_EatCharInlineFromEnd(&Conditions, '|');
                }

                LOG_INLINE("%S %S %S || ", c.Condition, ETokenType_ToString(c.ComparisonOp), Conditions);
            }
            else
            {
                LOG_INLINE("%S ", c.Condition);
            }

            Next = Next->Next;
        }
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

NO_DISCARD RETURN_NON_NULL static Node* Internal_ParseFile(LinearAllocator* Arena, const FileHandle H)
{
    Node* Result = &Node_Null;

    //Clock c;
    //Clock_Start(&c);

    usize FileSize = 0;
    xx Filesystem_GetFileSize(H, &FileSize);

    if (FileSize > Kibibytes(48))
    {
        // todo: error
    }

    String Text = String_Reserve(Arena, (u32)FileSize);
    
    bool bLexSuccess = false;
    usize Length = 0;
    if (Filesystem_ReadEntireFile(H, Text.Data, &Length))
    {
        Text.Length = (u32)Min(Length, FileSize);

        bool bAllowWhitespace = false;
        bool bInsideWhitespaceAllowedBlock = false;

        bLexSuccess = true;

        // tokenize the text
        Lexer l = {0};
        l.Text = Text;
        l.Line = 1;

        Token* Tokens = LinearAllocator_Allocate(Arena, sizeof(struct Token) * MAX_TOKENS);
        l.Tokens = Tokens;

        while (l.Current < l.Text.Length)
        {
            l.Start = l.Current;
            
            const uchar Char = l.Text.Data[l.Current];
            const uchar PrevChar = l.Text.Data[ClampI32_Min((i32)l.Current-1, 0)];
            Lexer_Advance(&l);

            if (Char == '\n')
            {
                l.Line += 1;
            }

            if (l.NumTokens >= MAX_TOKENS)
            {
                LOG_ERROR("[Lexer] Max tokens of 2048 has been reached. Aborting the lexer...\n");
                LOG("    We have purposefully limited the amount of tokens that our lexer can store.\n"
                    "    So this means you will have to simpily your .build file by reducing the amount of text that is present.\n\n"
                    "    One way to do this is to make another file, move some text over there, and then add an include statement like so:\n"
                    "      include my_file.buildvars");

                bLexSuccess = false;
                break;
            }

            ETokenType TokenToAdd = Token_None;
            
            const usize LastIndex = l.NumTokens > 0 ? l.NumTokens-1 : 0;
            ETokenType LastTokenType = l.Tokens[LastIndex].Type;

            if (LastTokenType == Token_Help || LastTokenType == Token_ErrorMessage)
            {
                bInsideWhitespaceAllowedBlock = true;
            }

            if      (Char == '(')               { TokenToAdd = Token_LParen;    }
            else if (Char == ')')               { TokenToAdd = Token_RParen;    }
            else if (Char == '^')               { TokenToAdd = Token_Caret;     }
            else if (Char == ';')               { TokenToAdd = Token_Semicolon; }
            else if (Char == ':')               { TokenToAdd = Token_Colon;     }
            else if (Char == Token_Char_Dollar) { TokenToAdd = Token_Dollar;    }
            else if (Char == Token_Char_At)     { TokenToAdd = Token_At;        }
            else if (Char == Token_Char_Mod)    { TokenToAdd = Token_Mod;       }
            else if (Char == '|')               { TokenToAdd = Token_Pipe;      }
            else if (Char == '"')
            {
                if (!bInsideWhitespaceAllowedBlock)
                {
                    if (PrevChar != '\\')
                    {
                        bAllowWhitespace = !bAllowWhitespace;
                    }
                }

                TokenToAdd = Token_Quote;
            }
            else if (Char == '{')
            {
                if (bInsideWhitespaceAllowedBlock)
                {
                    bAllowWhitespace = true;
                }

                TokenToAdd = Token_LCurly;
            }
            else if (Char == '}')
            {
                if (bInsideWhitespaceAllowedBlock && PrevChar != '\\')
                {
                    bInsideWhitespaceAllowedBlock = false;
                    bAllowWhitespace = false;
                }

                TokenToAdd = Token_RCurly;
            }
            else if (Char == '[')
            { 
                if (bInsideWhitespaceAllowedBlock)
                {
                    bAllowWhitespace = true;
                }

                TokenToAdd = Token_LSquare;
            }
            else if (Char == ']')
            {
                if (bInsideWhitespaceAllowedBlock && PrevChar != '\\')
                {
                    bInsideWhitespaceAllowedBlock = false;
                    bAllowWhitespace = false;
                }

                TokenToAdd = Token_RSquare;
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
                            Lexer_Advance(&l);
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

                        Lexer_Advance(&l);
                    }
                }
                else
                {
                    while (!IsNewline(Lexer_Peek(&l)) && Lexer_Peek(&l) != 0) // EOF
                    {
                        Lexer_Advance(&l);
                    }
                }
            }
            else if (Char == Token_Char_Not)
            {
                TokenToAdd = Lexer_Match(&l, '=') ? Token_NotEqual : Token_Not;
            }
            else if (Char == '=')
            {
                TokenToAdd = Lexer_Match(&l, '=') ? Token_EqualEqual : Token_Equal;
            }
            else if (Char == '<')
            {
                TokenToAdd = Lexer_Match(&l, '=') ? Token_LessOrEqual : Token_LessThan;
            }
            else if (Char == '>')
            {
                TokenToAdd = Lexer_Match(&l, '=') ? Token_GreaterOrEqual : Token_GreaterThan;
            }
            else if ((!bAllowWhitespace && IsWhitespace(Char)) || IsNewline(Char) || Char == '\0')
            {
                if (Char == '\n')
                {
                    if (bAllowWhitespace || LastTokenType != Token_Newline)
                    {
                        TokenToAdd = Token_Newline;
                    }
                }
                else
                {
                    if ((Char == ' ' || Char == '\t') && LastTokenType != Token_Newline)
                    {
                        while (Lexer_Peek(&l) == ' ' ||
                               Lexer_Peek(&l) == '\t')
                        {
                            Lexer_Advance(&l);
                        }

                        TokenToAdd = Token_Whitespace;
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

                    Lexer_Advance(&l);
                    Peek = Lexer_Peek(&l);
                }

                ETokenType FinalType = Token_Text;
                {
                    const u32 Diff = l.Current - l.Start;
                    String Lexeme = StrSub(l.Text, l.Start, Diff);

                    // TODO: relook at this code
                    for (u8 j = 0; j < SArray_Capacity(ReservedKeywordsTable); j++)
                    {
                        if (String_IsEqual(Lexeme, ReservedKeywordsTable[j].Name, false))
                        {
                            FinalType = ReservedKeywordsTable[j].Type;
                            break;
                        }
                    }

                    for (u8 j = 0; j < SArray_Capacity(ReservedStartingKeywordsTable); j++)
                    {
                        if (String_StartsWith(Lexeme, ReservedStartingKeywordsTable[j].Name, false))
                        {
                            FinalType = ReservedStartingKeywordsTable[j].Type;
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
                }

                TokenToAdd = FinalType;
            }

            if (TokenToAdd != Token_None)
            {
                Lexer_AddToken(&l, TokenToAdd);
            }
        }

        //Clock_Tick(&c);
        //Clock_PrintElapsedTime(&c, true);

        /*
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
        */

        // Parse the tokens into a tree
        if (bLexSuccess)
        {
            //Clock_Start(&c);

            Parser p    = {0};
            p.Tokens    = Tokens;
            p.NumTokens = l.NumTokens;

            Result = Parse_Block(Arena, &p, 0, false);

            //Clock_Tick(&c);
            //Clock_PrintElapsedTime(&c, true);

            //Print_BlockNode(Result->List, 0);
        }
    }

    return Result;
}

NO_DISCARD static String GetOptionParamsFromVarList(FileVariableList* VarList, String OptionName, bool* bFound)
{
    String Params = String_Null();
    if (bFound) { *bFound = false; }

    FileVariable* Ref = GetVarInList(VarList, OptionName, false);
    if (Ref == &FileVariable_Empty)
    {
        // not found
    }
    else
    {
        Params = Ref->Params;

        if (bFound) { *bFound = Params.Length > 0; }
    }

    return Params;
}

NO_DISCARD static NodeList* Analyze_IfNode(LinearAllocator* Arena, Node* Root, ParsingContext* Context, bool bInIf);
NO_DISCARD static NodeList* Analyze_List(LinearAllocator* Arena, Node* Block, ParsingContext* Context, bool bInIf);
NO_DISCARD static bool      Analyze_Indeterminates(LinearAllocator* Arena, NodeList* List, ParsingContext* Context);
           static void      Analyze_Options(LinearAllocator* Arena, Node* Block, ParsingContext* Context);

NO_DISCARD static NodeList* Analyze_IncludeNode(LinearAllocator* Arena, Node* Root, ParsingContext* Context)
{
    bool bSuccess = false;

    NodeList* IndeterminateList = NULL;
    NodeList** IndeterminateNext = &IndeterminateList;

    StringLocal(Expanded, MAX_PATH_LENGTH);

    if (Root->Value)
    {
        StringLocal(Val, MAX_PATH_LENGTH);
        for each_string_in_list (*Root->Value)
        {
            String_Append(&Val, It.String);
        }

        bool bFailed = false;
        bSuccess = ExpandBuildVariable(*Arena, Context->VarListHead, Context->CmdOptionsDB, &Expanded, Root->Key, Val, Root->Key, Context->WorkingDirectory, false, false, &bFailed);
        if (bFailed || !bSuccess)
        {
            bSuccess = false;
        }
        else
        {
            bSuccess = true;
        }
    }

    if (bSuccess)
    {
        // parse the include file

        // TODO: close the file??
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
            Array_Add(Context->IncludeFiles, f);
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

                Node* AST = Internal_ParseFile(Arena, f);
                if (AST && AST != &Node_Null)
                {
                    u8 Level = Context->Level;
                    Context->Level = 0;

                    Analyze_Options(Arena, AST, Context);

                    NodeList* List = Analyze_List(Arena, AST, Context, false);
                    if (List)
                    {
                        SLinkedList_Push(IndeterminateNext, List);
                    }

                    Context->Level = Level;
                }
                else
                {
                    // TODO: something better
                    _Crash_;
                }
            }
        }
    }
    else
    {
        // dont do this when we're in a no fail state to prevent infinite loop
        if (!Context->bNoFail)
        {
            SLinkedList_Push(IndeterminateNext, NodeList_Create(Arena, Root, NULL));
        }
    }

    return IndeterminateList;
}

static bool IsOptionOn(String Str)
{
    bool bOn = false;

    if (String_IsEqual(Str, S("on"), false) ||
        String_IsEqual(Str, S("yes"), false) ||
        String_IsEqual(Str, S("1"), false) ||
        String_IsEqual(Str, S("true"), false))
    {
        bOn = true;
    }

    return bOn;
}

static bool IsOptionOff(String Str)
{
    bool bOff = false;

    if (String_IsEqual(Str, S("off"), false) ||
        String_IsEqual(Str, S("no"), false) ||
        String_IsEqual(Str, S("0"), false) ||
        String_IsEqual(Str, S("false"), false))
    {
        bOff = true;
    }

    return bOff;
}

static bool IsOptionBinary(StringList Parameters)
{
    bool bIsBinaryOption = false;

    u8 Num = 0;
    bool bHaveOn = false;
    bool bHaveOff = false;
    for each_string_in_list (Parameters)
    {
        if (IsOptionOn(It.String))
        {
            bHaveOn = true;
        }
        else if (IsOptionOff(It.String))
        {
            bHaveOff = true;
        }

        Num++;
    }

    if (Num < 3 && (bHaveOn || bHaveOff))
    {
        bIsBinaryOption = true;
    }

    return bIsBinaryOption;
}

static void Analyze_KVNode_Option(LinearAllocator* Arena, Node* Root, ParsingContext* Context)
{
    StringLocal(FinalKey, MAX_KEY_LENGTH);
    StringLocal(Val,      8192);
    StringLocal(Params,   MAX_META_KEY_LENGTH);

    // this is a namespace basically
    // SomeKey {
    //     AnotherKey some value
    // }
    // 
    // which will become one key: SomeKey.AnotherKey some value
    {
        Node* NextParent = Root->Parent;
        String ParentKeys[64] = {0};
        u8 i = 0;
        while (NextParent)
        {
            ParentKeys[i] = NextParent->Key;
            NextParent = NextParent->Parent;
            i++;
        }
        if (i > 0)
        {
            for (i8 j = (i8)i-1; j >= 0; j--)
            {
                String_AppendF(&FinalKey, S("%S."), ParentKeys[j]);
            }
            String_Append(&FinalKey, Root->Key);
        }
        else
        {
            FinalKey = Root->Key;
        }
    }

    if (Root->Value)
    {
        for each_string_in_list (*Root->Value)
        {
            String_Append(&Val, It.String);
        }

        xx String_EatSpacesInlineFromEnd(&Val);
    }

    if (Root->Parameters)
    {
        for each_string_in_list (*Root->Parameters)
        {
            String_Append(&Params, It.String);
            String_AppendSpace(&Params);
        }

        xx String_EatSpacesInlineFromEnd(&Params);
    }

    // make sure this is an actual option.something key with no additional children keys
    if (String_StartsWith(FinalKey, S("option."), false) && !Root->Parent)
    {
        String OptionName = StrShiftF(FinalKey, 7);
        String OptionValue = String_Null();

        // does the cmd line for this option already exist?
        CmdOption* OptionPtr = NULL;
        for each (CmdOption, o, Context->CmdOptionsDB)
        {
            bool bMatch = String_IsEqual(o.Name, OptionName, false);
            if (bMatch)
            {
                OptionPtr = o_;
                break;
            }
        }

        bool bIsOptionEnabled = false;
        bool bIsBinaryOption = true;
        if (Root->Parameters)
        {
            bIsBinaryOption = IsOptionBinary(*Root->Parameters);

            if (bIsBinaryOption)
            {
                // param default value
                bIsOptionEnabled = IsOptionOn(Root->Parameters->String);

                // does the cmd line turn this option on or off?
                if (bIsOptionEnabled)
                {
                    usize i = 0;
                    for each_i (i, CmdOption, o, Context->CmdOptionsDB)
                    {
                        if (String_IsFirst(o.Name, '!'))
                        {
                            bool bMatch = String_IsEqual(StrShiftF(o.Name, 1), OptionName, false);
                            if (bMatch)
                            {
                                bIsOptionEnabled = false;
                            }
                        }
                        else
                        {
                            bool bMatch = String_IsEqual(o.Name, OptionName, false);
                            if (bMatch && o.Value.Length)
                            {
                                bIsOptionEnabled = IsOptionOn(o.Value);
                            }
                        }

                        if (!bIsOptionEnabled)
                        {
                            Array_RemoveAt(Context->CmdOptionsDB, NULL, i);
                            break;
                        }
                    }
                }
            }
            else
            {
                bIsOptionEnabled = true;
                OptionValue = Root->Parameters->String;

                if (OptionPtr)
                {
                    OptionValue = OptionPtr->Value;
                }
            }
        }

        if (bIsOptionEnabled)
        {
            AddCmdOption(Context->CmdOptionsDB, String_Create(Arena, OptionName), String_Create(Arena, OptionValue));
        }

        if (bIsBinaryOption)
        {
            Params.Length = 0;
        }

        AddVariableToList(Arena, Context, FinalKey, Val, Params);
    }
}

static void Analyze_KVNode(LinearAllocator* Arena, Node* Root, ParsingContext* Context)
{
    StringLocal(FinalKey, MAX_KEY_LENGTH);
    StringLocal(Val,      Kibibytes(32));
    StringLocal(Params,   MAX_META_KEY_LENGTH);

    // this is a namespace basically
    // SomeKey {
    //     AnotherKey some value
    // }
    // 
    // which will become one key: SomeKey.AnotherKey some value
    {
        Node* NextParent = Root->Parent;
        String ParentKeys[64] = {0};
        u8 i = 0;
        while (NextParent)
        {
            ParentKeys[i] = NextParent->Key;
            NextParent = NextParent->Parent;
            i++;
        }
        if (i > 0)
        {
            for (i8 j = (i8)i-1; j >= 0; j--)
            {
                String_AppendF(&FinalKey, S("%S."), ParentKeys[j]);
            }
            String_Append(&FinalKey, Root->Key);
        }
        else
        {
            FinalKey = Root->Key;
        }
    }

    if (Root->Value)
    {
        for each_string_in_list (*Root->Value)
        {
            String_Append(&Val, It.String);
        }

        xx String_EatSpacesInlineFromEnd(&Val);
    }

    if (Root->Parameters)
    {
        for each_string_in_list (*Root->Parameters)
        {
            String_Append(&Params, It.String);
            String_AppendSpace(&Params);
        }

        xx String_EatSpacesInlineFromEnd(&Params);
    }

    bool bCanAddToList = true;
    
    if (!Root->Parent)
    {
        bool bIsOptionKey = String_StartsWith(FinalKey, S("option."), false);

        bCanAddToList = !bIsOptionKey;
        
        if (bIsOptionKey)
        {
            Analyze_KVNode_Option(Arena, Root, Context);
        }
    }

    if (bCanAddToList)
    {
        AddVariableToList(Arena, Context, FinalKey, Val, Params);
    }
}

NO_DISCARD static NodeList* Analyze_IfNode(LinearAllocator* Arena, Node* Root, ParsingContext* Context, bool bInIf)
{
    NodeList* IndeterminateList = NULL;
    NodeList** IndeterminateNext = &IndeterminateList;

    bool bConditionMet = false;
    bool bFoundVar = Context->bNoFail;

    // evaluate the conditions
    if (ALWAYS(Root->ConditionList != NULL))
    {
        IfConditionList* Next = Root->ConditionList;
        while (Next)
        {
            IfConditionData c = Next->Data;

            bool bNot                  = c.Prefix & BIT(1);
            bool bSearchFileVar        = c.Prefix & BIT(2);
            bool bSearchInternalVar    = c.Prefix & BIT(3);
            bool bSearchEnvironmentVar = c.Prefix & BIT(4);
            bool bPrefixedWithSymbol   = bSearchFileVar || bSearchInternalVar || bSearchEnvironmentVar;
            bool bCaseSensitive        = false; // TODO

            StringLocal(EnvVar, MAX_PATH_LENGTH);

            String VarValue = String_Null();
            const String Condition = c.Condition;

            bool bIsPath = String_ContainsPathSeparators(Condition);
            if (!bIsPath)
            {
                bool bFoundSomething = false;

                if (bSearchEnvironmentVar)
                {
                    if (Platform_GetEnvironmentVariableValue(Condition, &EnvVar))
                    {
                        bConditionMet = c.ComparisonOp == Token_None;
                        VarValue = EnvVar;
                        bFoundVar = true;
                        bFoundSomething = true;
                    }
                }

                if (!bFoundSomething && !bConditionMet && (bSearchInternalVar || !bPrefixedWithSymbol))
                {
                    String Trimmed = Condition;
                    if (String_StartsWith(Condition, S("Option."), false))
                    {
                        Trimmed = StrShiftF(Condition, 7);
                    }

                    // check the condition string against the internal build vars passed in from the command line
                    for each (CmdOption, o, Context->CmdOptionsDB)
                    {
                        bool bMatch = String_IsEqual(o.Name, Trimmed, false);
                        if (bMatch)
                        {
                            // make sure we have some value if we specified an '=' sign
                            if (!String_IsDataValid(o.Value) || o.Value.Length > 0)
                            {
                                bool bIsBinaryOption = IsOptionOn(o.Value) || IsOptionOff(o.Value);
                                if (bIsBinaryOption)
                                {
                                    bConditionMet = IsOptionOn(o.Value);
                                    bFoundSomething = true;
                                }
                                else
                                {
                                    VarValue = o.Value;
                                    bConditionMet = c.ComparisonOp == Token_None;
                                    bFoundVar = true;
                                    bFoundSomething = true;
                                }

                                break;
                            }
                        }
                    }

                    if (!bFoundSomething && !bConditionMet)
                    {
                        // check the condition string against the internal build vars native to this program
                        for each (InternalVariable, v, InternalVariablesDB)
                        {
                            if (String_IsEqual(v.Name, Condition, false))
                            {
                                // is this a binary value?
                                bool bOn = String_IsEqual(v.Value, S("1"), false);
                                bool bOff = String_IsEqual(v.Value, S("0"), false);
                                bool bIsBinary = bOn || bOff;

                                VarValue = v.Value;
                                bFoundVar = true;
                                bFoundSomething = true;
                                
                                if (c.ComparisonOp != Token_None)
                                {
                                    bConditionMet = false; // evaluated later down...
                                }
                                else
                                {
                                    bConditionMet = !bIsBinary || (bIsBinary && bOn);
                                }

                                break;
                            }
                        }
                    }
                }

                if (!bFoundSomething && !bConditionMet && (bSearchFileVar || !bPrefixedWithSymbol))
                {
                    // check the condition string against the list of current vars (non-expanded)
                    SLinkedList_Each(FileVariableList, This, &Context->VarListHead)
                    {
                        const FileVariable Var = (*This)->Var;

                        if (String_IsEqual(Var.Name, Condition, false))
                        {
                            VarValue = Var.Value;
                            bConditionMet = c.ComparisonOp == Token_None;
                            bFoundVar = true;
                            // bFoundSomething = true;

                            break;
                        }
                    }
                }
            }

            if (bFoundVar)
            {
                bIsPath = String_ContainsPathSeparators(VarValue);
            }

            if (bIsPath)
            {
                StringLocal(Expanded, MAX_PATH_LENGTH);
                if (bFoundVar)
                {
                    xx ExpandBuildVariable(*Context->TempArena, Context->VarListHead, Context->CmdOptionsDB, &Expanded, String_Null(), VarValue, String_Null(), Context->WorkingDirectory, false, false, NULL);
                }
                else
                {
                    if (bPrefixedWithSymbol)
                    {
                        StringLocal(ConditionPrefixed, MAX_PATH_LENGTH);

                        uchar Symbol = bSearchFileVar ? Token_Char_Dollar : (bSearchInternalVar ? Token_Char_Mod : (bSearchEnvironmentVar ? Token_Char_At : 0));
                        if (Symbol) { String_AppendChar(&ConditionPrefixed, Symbol); }
                        String_Append(&ConditionPrefixed, Condition);

                        xx ExpandBuildVariable(*Context->TempArena, Context->VarListHead, Context->CmdOptionsDB, &Expanded, String_Null(), ConditionPrefixed, String_Null(), Context->WorkingDirectory, false, false, NULL);
                    }
                    else
                    {
                        Expanded = Condition;
                    }
                }

                bool bIsDirectory = String_IsLast(Expanded, '/') || String_IsLast(Expanded, '\\');

                StringLocal(Temp, MAX_PATH_LENGTH);
                if (Filesystem_IsPathRelative(Expanded))
                {
                    String_BuildPath(&Temp, Context->WorkingDirectory, Expanded);
                }
                else
                {
                    String_Copy(&Temp, Expanded);
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

            if (bFoundVar && c.ComparisonOp != Token_None && c.TestValues)
            {
                LinearAllocator Scratch = *Arena;

                // if the var value has more than one value separated by a space
                StringArray Values = String_ParseIntoArray(&Scratch, VarValue, ' ', 0, 128);

                i64 LeftInt = 0, RightInt = 0;

                switch (c.ComparisonOp)
                {
                    default: {} break;

                    case Token_EqualEqual:
                    {
                        for each_string_in_list (*c.TestValues)
                        {
                            for each_str (v, Values)
                            {
                                bConditionMet = String_IsEqual(*v, It.String, bCaseSensitive);
                                if (bConditionMet)
                                {
                                    goto the_great_wall_of_china;
                                }
                            }
                        }
                    }
                    break;

                    case Token_NotEqual:
                    {
                        for each_string_in_list (*c.TestValues)
                        {
                            for each_str (v, Values)
                            {
                                bConditionMet = !String_IsEqual(*v, It.String, bCaseSensitive);
                                if (bConditionMet)
                                {
                                    goto the_great_wall_of_china;
                                }
                            }
                        }
                    }
                    break;

                    case Token_GreaterOrEqual:
                    case Token_LessOrEqual:
                    case Token_GreaterThan:
                    case Token_LessThan:
                    {
                        for each_string_in_list (*c.TestValues)
                        {
                            for each_str (v, Values)
                            {
                                bool bConverted = String_ToI64(*v, &LeftInt) &&
                                                  String_ToI64(It.String, &RightInt);
                                if (bConverted)
                                {
                                    switch (c.ComparisonOp)
                                    {
                                        default: break;
                                        case Token_GreaterOrEqual: { bConditionMet = LeftInt >= RightInt; } break;
                                        case Token_LessOrEqual:    { bConditionMet = LeftInt <= RightInt; } break;
                                        case Token_GreaterThan:    { bConditionMet = LeftInt >  RightInt; } break;
                                        case Token_LessThan:       { bConditionMet = LeftInt <  RightInt; } break;
                                    }

                                    if (bConditionMet)
                                    {
                                        goto the_great_wall_of_china;
                                    }
                                }
                            }
                        }
                    }
                    break;

                    case Token_StartsWith:
                    {
                        for each_string_in_list (*c.TestValues)
                        {
                            for each_str (v, Values)
                            {
                                bConditionMet = String_StartsWith(*v, It.String, bCaseSensitive);
                                if (bConditionMet)
                                {
                                    goto the_great_wall_of_china;
                                }
                            }
                        }
                    }
                    break;

                    case Token_EndsWith:
                    {
                        for each_string_in_list (*c.TestValues)
                        {
                            for each_str (v, Values)
                            {
                                bConditionMet = String_EndsWith(*v, It.String, bCaseSensitive);
                                if (bConditionMet)
                                {
                                    goto the_great_wall_of_china;
                                }
                            }
                        }
                    }
                    break;

                    case Cmp_Contains:
                    {
                        for each_string_in_list (*c.TestValues)
                        {
                            for each_str (v, Values)
                            {
                                bConditionMet = String_Contains(*v, It.String, bCaseSensitive);
                                if (bConditionMet)
                                {
                                    goto the_great_wall_of_china;
                                }
                            }
                        }
                    }
                    break;
                }

                the_great_wall_of_china:
                {
                    // ======================================================
                    // ||||||||||||||||||||||||||||||||||||||||||||||||||||||
                    // ======================================================

                    // very big wall indeed
                }
            }

            if (bFoundVar)
            {
                if (bNot)
                {
                    bConditionMet = !bConditionMet;
                }

                if (bConditionMet)
                {
                    break;
                }
            }

            Next = Next->Next;
        }
    }

    if (bFoundVar || Context->bNoFail)
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
                Analyze_KVNode(Arena, Left, Context);
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
                Analyze_KVNode(Arena, Right, Context);
            }
        }
    }
    else
    {
        SLinkedList_Push(IndeterminateNext, NodeList_Create(Arena, Root, NULL));
    }

    return IndeterminateList;
}

NO_DISCARD static bool Analyze_Indeterminates(LinearAllocator* Arena, NodeList* List, ParsingContext* Context)
{
    bool bSuccess = true;

    NodeList** Next = &List;
    while (*Next)
    {
        Node* Root = (*Next)->Node;

        bool bValid = Root && Root != &Node_Null;
        if (bValid)
        {
            if (Root->Type == Node_Block)
            {
                xx Analyze_List(Arena, Root, Context, false);
            }
            else if (Root->Type == Node_If)
            {
                xx Analyze_IfNode(Arena, Root, Context, true);
            }
            else if (Root->Type == Node_Help)
            {
                Analyze_KVNode(Arena, Root, Context);
            }
            else if (Root->Type == Node_Include)
            {
                xx Analyze_IncludeNode(Arena, Root, Context);
            }
            else if (Root->Type == Node_ErrorMessage)
            {
                Analyze_KVNode(Arena, Root, Context);
            }
            else if (Root->Type == Node_KeyValue)
            {
                Analyze_KVNode(Arena, Root, Context);
            }
        }

        Next = &(*Next)->Next;
   }

    return bSuccess;
}

ECompiler DetermineCompilerVendor(String CompilerPath)
{
    String CompilerName = Filesystem_ExtractFileName(CompilerPath, false);
    ECompiler CompilerVendor = Compiler_Generic;
    if (String_IsEqual(CompilerName, S("cl"), false))
    {
        CompilerVendor = Compiler_MSVC;
    }
    else if (String_IsEqual(CompilerName, S("clang"), false) ||
             String_IsEqual(CompilerName, S("clang++"), false))
    {
        CompilerVendor = Compiler_Clang;
    }
    else if (String_IsEqual(CompilerName, S("clang-cl"), false))
    {
        CompilerVendor = Compiler_Clang_MSVC;
    }
    else if (String_Contains(CompilerName, S("mingw"), false))
    {
        CompilerVendor = Compiler_MINGW;
    }
    else if (String_Contains(CompilerName, S("gcc"), false) ||
             String_Contains(CompilerName, S("g++"), false))
    {
        CompilerVendor = Compiler_GCC;
    }
    else if (String_Contains(CompilerName, S("tcc"), false))
    {
        CompilerVendor = Compiler_TCC;
    }
    else
    {
        CompilerVendor = Compiler_Generic;
    }

    return CompilerVendor;
}

bool FindFirstCompilerAvailable(const String CompilerToFind, const String AssemblerToFind, const String LinkerToFind, const String ArchiverToFind, CompilerPaths* OutCompilerPaths)
{
    bool bCompilerProgramFound = false;
    bool bLinkerProgramFound = false;
    bool bArchiverProgramFound = false;
    bool bAssemblerProgramFound = false;

    bool bNoCompilerProgramExplicityGiven  = CompilerToFind.Length == 0;
    bool bNoAssemblerProgramExplicityGiven = AssemblerToFind.Length == 0;
    bool bNoLinkerProgramExplicityGiven    = LinkerToFind.Length == 0;
    bool bNoArchiverProgramExplicityGiven  = ArchiverToFind.Length == 0;

    bool bExplicitLinkerPath = String_IndexOfFirstPathSlash(LinkerToFind, NULL);
    if (bExplicitLinkerPath)
    {
        String_Copy(&OutCompilerPaths->LinkerPath, LinkerToFind);
        Filesystem_AppendExeExtension(&OutCompilerPaths->LinkerPath);

        bLinkerProgramFound = Filesystem_DoesFileExist(OutCompilerPaths->LinkerPath);
    }
    else
    {
        bLinkerProgramFound = Platform_FindProgram_Ex(LinkerToFind, &OutCompilerPaths->LinkerPath);

        // the code below will find the appropriate linker (if we did not find the one we specified)
    }

    bool bExplicitArchiverPath = String_IndexOfFirstPathSlash(ArchiverToFind, NULL);
    if (bExplicitArchiverPath)
    {
        String_Copy(&OutCompilerPaths->ArchiverPath, ArchiverToFind);
        Filesystem_AppendExeExtension(&OutCompilerPaths->ArchiverPath);

        bArchiverProgramFound = Filesystem_DoesFileExist(OutCompilerPaths->ArchiverPath);
    }
    else
    {
        bArchiverProgramFound = Platform_FindProgram_Ex(ArchiverToFind, &OutCompilerPaths->ArchiverPath);

        // the code below will find the appropriate archiver (if we did not find the one we specified)
    }

    bool bExplicitAssemblerPath = String_IndexOfFirstPathSlash(AssemblerToFind, NULL);
    if (bExplicitAssemblerPath)
    {
        String_Copy(&OutCompilerPaths->AssemblerPath, AssemblerToFind);
        Filesystem_AppendExeExtension(&OutCompilerPaths->AssemblerPath);

        bAssemblerProgramFound = Filesystem_DoesFileExist(OutCompilerPaths->AssemblerPath);
    }


    bool bExplicitCompilerPath = String_IndexOfFirstPathSlash(CompilerToFind, NULL);
    if (bExplicitCompilerPath)
    {
        String_Copy(&OutCompilerPaths->CompilerPath, CompilerToFind);
        Filesystem_AppendExeExtension(&OutCompilerPaths->CompilerPath);

        String CompilerInstallPath = Filesystem_ExtractFilePath(OutCompilerPaths->CompilerPath, false);
        String BasePath = Filesystem_ExtractFilePath(CompilerInstallPath, false);

        OutCompilerPaths->BasePath = BasePath;
        OutCompilerPaths->ToolPath = BasePath;
        OutCompilerPaths->InstallPath = CompilerInstallPath;

        bCompilerProgramFound = Filesystem_DoesFileExist(OutCompilerPaths->CompilerPath);
    }
    else
    {
        if (String_IsEqual(CompilerToFind, S("cl"), false) ||
            String_IsEqual(CompilerToFind, S("msvc"), false))
        {
            #if PLATFORM_WINDOWS
            // IDEAS
            // .SDKVersion key? to specify an exact version to build with?
            // .MSVCVersion key?
            // auxliarry include vs paths?

            if (bWasVCVarsBatchExecuted)
            {
                // PlatformPipe StdOutHandle = {0};
                if (Platform_FindProgram_Ex(S("cl"), &OutCompilerPaths->CompilerPath))
                // PlatformHandle ShellCmd = Platform_RunCommand_Ex(S("where cl"), String_Null(), &StdOutHandle);
                // if (Platform_IsValidHandle(ShellCmd))
                {
                    // u32 ExitCode = Platform_WaitForProcessAndGetExitCode(ShellCmd);
                    // if (ExitCode == 0)
                    {
                        // StringLocal(StdOutData, 8192);
                        // usize BytesRead = 0;
                        // if (!Filesystem_ReadPipe(StdOutHandle, StdOutData.Capacity, StdOutData.Data, &BytesRead))
                        {
                            // LOG_ERROR("Failed to read from standard output pipe for command -> \"where cl\"");
                            // return false;
                        }

                        // StdOutData.Length = Min((u32)BytesRead, StdOutData.Capacity);
                        // xx String_EatNewLinesInlineFromEnd(&StdOutData);
                        
                        String InstallPath = Filesystem_ExtractFilePath(OutCompilerPaths->CompilerPath, false);
                        String_Copy(&OutCompilerPaths->InstallPath, InstallPath);

                        // String_Copy(&OutCompilerPaths->CompilerPath, InstallPath);
                        // String_Append(&OutCompilerPaths->CompilerPath, S("\\cl.exe"));

                        bCompilerProgramFound = true;

                        xx Platform_GetEnvironmentVariableValue(S("VCToolsInstallDir"), &OutCompilerPaths->ToolPath);
                        xx String_EatPathSeparatorsInlineFromEnd(&OutCompilerPaths->ToolPath);
                        String_BuildPath(&OutCompilerPaths->IncludePath, OutCompilerPaths->ToolPath, S("include"));

                        xx String_EatPathSeparatorsInlineFromEnd(&OutCompilerPaths->IncludePath);
                        StringLocal(TargetArch, 32);
                        xx Platform_GetEnvironmentVariableValue(S("VSCMD_ARG_TGT_ARCH"), &TargetArch);
                        String_BuildPath(&OutCompilerPaths->LibraryPath, OutCompilerPaths->ToolPath, S("lib"), TargetArch);
                        xx String_EatPathSeparatorsInlineFromEnd(&OutCompilerPaths->LibraryPath);

                        xx Platform_GetEnvironmentVariableValue(S("VSINSTALLDIR"), &OutCompilerPaths->BasePath);
                    }
                }
            }
            else
            {
                // find the latest version of visual studio and extract all the useful directories

                ScratchLocal(Scratch, Kibibytes(8));

                MicrosoftVisualStudioPaths VSPaths = {0};
                bool bFoundVS = FindVisualStudio(&Scratch, &VSPaths);
                if (bFoundVS)
                {
                    String_Copy(&OutCompilerPaths->CompilerPath, VSPaths.ExePath);
                    String_Append(&OutCompilerPaths->CompilerPath,  S("\\cl.exe"));

                    bCompilerProgramFound = true;

                    String_Copy(&OutCompilerPaths->ToolPath,    VSPaths.ToolBasePath);
                    String_Copy(&OutCompilerPaths->LibraryPath, VSPaths.LibraryPath);
                    String_Copy(&OutCompilerPaths->IncludePath, VSPaths.IncludePath);
                    String_Copy(&OutCompilerPaths->BasePath,    VSPaths.ToolBasePath);
                    String_Copy(&OutCompilerPaths->InstallPath, VSPaths.ExePath);
                }
            }
            #endif
        }
        else
        {
            bCompilerProgramFound = Platform_FindProgram_Ex(CompilerToFind, &OutCompilerPaths->CompilerPath);

            if (!bCompilerProgramFound && bNoCompilerProgramExplicityGiven)
            {
                // TODO: find cpp compiler first if we have cpp files (source and header)

                const String CompilerPrograms[8] =
                {
                    S("clang"),
                    S("gcc"),
                    S("egcc"),
                    S("cc"),
                    S("clang++"),
                    S("g++"),
                    S("cl"),
                    S("clang-cl"),
                };

                for (u8 i = 0; i < SArray_Capacity(CompilerPrograms); i++)
                {
                    const bool bFound = Platform_FindProgram_Ex(CompilerPrograms[i], &OutCompilerPaths->CompilerPath);
                    if (bFound)
                    {
                        bCompilerProgramFound = true;
                        break;
                    }
                }
            }

            if (bCompilerProgramFound)
            {
                String CompilerInstallPath = Filesystem_ExtractFilePath(OutCompilerPaths->CompilerPath, false);
                String CompilerName = Filesystem_ExtractFileName(OutCompilerPaths->CompilerPath, false);
                String BasePath = Filesystem_ExtractFilePath(CompilerInstallPath, false);

                OutCompilerPaths->InstallPath = CompilerInstallPath;
                OutCompilerPaths->BasePath = BasePath;
                OutCompilerPaths->ToolPath = BasePath;

                if (!String_IsEqual(CompilerName, S("cl"), false))
                {
                    String_BuildPath(&OutCompilerPaths->IncludePath, BasePath, S("include"));
                    String_BuildPath(&OutCompilerPaths->LibraryPath, BasePath, S("lib"));
                }
            }
        }
    }

    xx bArchiverProgramFound;

    if (bCompilerProgramFound)
    {
        String CompilerName = Filesystem_ExtractFileName(OutCompilerPaths->CompilerPath, false);
        if (String_IsEqual(CompilerName, S("cl"), false))
        {
            if (bNoLinkerProgramExplicityGiven)
            {
                String_BuildPath(&OutCompilerPaths->LinkerPath, OutCompilerPaths->InstallPath, S("link.exe"));

                bLinkerProgramFound = true;
            }

            if (bNoArchiverProgramExplicityGiven)
            {
                String_BuildPath(&OutCompilerPaths->ArchiverPath, OutCompilerPaths->InstallPath, S("lib.exe"));

                bArchiverProgramFound = true;
            }

            if (bNoAssemblerProgramExplicityGiven)
            {
                String_BuildPath(&OutCompilerPaths->AssemblerPath, OutCompilerPaths->InstallPath, S("ml64.exe"));
                if (!Filesystem_DoesFileExist(OutCompilerPaths->AssemblerPath))
                {
                    String_BuildPath(&OutCompilerPaths->AssemblerPath, OutCompilerPaths->InstallPath, S("ml.exe"));
                }

                bAssemblerProgramFound = true;
            }
        }
        else
        {
            if (bNoLinkerProgramExplicityGiven)
            {
                String_Copy(&OutCompilerPaths->LinkerPath, OutCompilerPaths->CompilerPath);
                bLinkerProgramFound = true;
            }

            if (bNoArchiverProgramExplicityGiven)
            {
                #if PLATFORM_WINDOWS
                String CompilerInstallPath = Filesystem_ExtractFilePath(OutCompilerPaths->CompilerPath, false);
                if (String_Contains(CompilerName, S("clang"), false))
                {
                    String_BuildPath(&OutCompilerPaths->ArchiverPath, CompilerInstallPath, S("llvm-ar.exe"));
                    bArchiverProgramFound = true;
                }
                else if (String_Contains(CompilerName, S("mingw"), false))
                {
                    // TODO: this is dumb, but whatever.. will fix later
                    String_BuildPath(&OutCompilerPaths->ArchiverPath, CompilerInstallPath, S("x86_64-w64-mingw32-gcc-ar.exe"));
                    bArchiverProgramFound = true;
                }
                else if (String_Contains(CompilerName, S("gcc"), false) ||
                         String_Contains(CompilerName, S("g++"), false))
                {
                    String_BuildPath(&OutCompilerPaths->ArchiverPath, CompilerInstallPath, S("gcc-ar.exe"));
                    bArchiverProgramFound = true;
                }
                else
                {
                }
                #else
                bArchiverProgramFound = Platform_FindProgram_Ex(S("ar"), &OutCompilerPaths->ArchiverPath);
                #endif
            }
        }
    }
    else
    {
        LOG_LINE_BREAK();

        if (bNoCompilerProgramExplicityGiven)
        {
            #if PLATFORM_WINDOWS
            LOG("\n    You don't seem to have a C nor C++ compiler installed on your machine."
                "\n    Install either \"clang/clang++\", \"gcc/g++\" or \"cl (msvc)\" and add to the path environment"
                "\n    before using RiftBuild, as we require a working compiler program to function properly. Aborting build...\n");

            /*
            LOG_ERROR(
                "You don't seem to have a C nor C++ compiler installed on your machine."
                " Install either \"clang/clang++\", \"gcc/g++\" or \"cl (msvc)\" and add to the path environment"
                " before using RiftBuild, as we require a working compiler program to function properly. Aborting build...\n");
            */
            #else
            LOG("\n    You don't seem to have a C nor C++ compiler installed on your machine."
                "\n    Install either \"clang/clang++\" or \"gcc/g++\" and add to the path environment"
                "\n    before using RiftBuild, as we require a working compiler program to function properly. Aborting build...\n");

            /*
            LOG_ERROR(
                "You don't seem to have a C nor C++ compiler installed on your machine."
                " Install either \"clang/clang++\" or \"gcc/g++\" and add to the PATH environment"
                " before using RiftBuild, as we require a working compiler program to function properly. Aborting build...\n");
            */
            #endif

            LogPathEnvVarTutorialSteps();
                
            return false;
        }

        if (String_IsEqual(CompilerToFind, S("cl"), false) ||
            String_IsEqual(CompilerToFind, S("msvc"), false))
        {
            #if PLATFORM_WINDOWS
            LOG_ERROR("Compiler program \"%S\" does not exist. Aborting build...", CompilerToFind);
            
            LOG("\n    Make sure that the Visual Studio build tools and Windows SDK are installed and "
                "\n    that you run riftbuild from a different terminal application named"
                "\n    \"x64 (or x86) Native Tools Command Prompt for VS\".");

            LOG("\n    This can be found through Windows Search.");
            #else
            LOG_ERROR("Compiler program \"cl\" does not exist on non-Windows platforms. Use a different compiler. Aborting build...");
            #endif
        }
        else
        {
            LOG_ERROR("Compiler program \"%S\" does not exist.\n"
                      "        Make sure that it is installed and added to the path environment.\n"
                      "        Alternatively, you can specify the path to the compiler executable instead. Aborting build...\n", CompilerToFind);

            LogPathEnvVarTutorialSteps();
        }

        return false;
    }

    if (bLinkerProgramFound)
    {
    }
    else
    {
        LOG_LINE_BREAK();

        if (bNoLinkerProgramExplicityGiven)
        {
            LOG_ERROR(
                "You don't seem to have a linker installed on your machine."
                " Make sure that you have linker installed add to the path environment. Aborting build...\n");
        }
        else
        {
            LOG_ERROR("Linker program \"%S\" does not exist.\n"
                      "        Make sure that it is installed and added to the path environment.\n"
                      "        Alternatively, you can specify the path to the linker executable instead. Aborting build...\n", LinkerToFind);
        }

        LogPathEnvVarTutorialSteps();

        return false;
    }

    // find an assembler
    {
        if (!bAssemblerProgramFound)
        {
            bAssemblerProgramFound = Platform_FindProgram_Ex(AssemblerToFind, &OutCompilerPaths->AssemblerPath);

            if (!bAssemblerProgramFound && bNoAssemblerProgramExplicityGiven)
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

                if (String_IsEqual(CompilerToFind, S("cl"), false) ||
                    String_IsEqual(CompilerToFind, S("msvc"), false))
                {
                    AsmPrograms = AsmPrograms_MSVC;
                    Num = SArray_Capacity(AsmPrograms_MSVC);
                }

                for (u8 i = 0; i < Num; i++)
                {
                    const bool bFound = Platform_FindProgram_Ex(AsmPrograms[i], &OutCompilerPaths->AssemblerPath);
                    if (bFound)
                    {
                        bAssemblerProgramFound = true;
                        break;
                    }
                }
            }
        }

        if (!bAssemblerProgramFound)
        {
            if (AssemblerToFind.Length > 0)
            {
                LOG_LINE_BREAK();

                if (String_IsEqual(AssemblerToFind, S("ml"), false) ||
                    String_IsEqual(AssemblerToFind, S("ml64"), false))
                {
                    #if PLATFORM_WINDOWS
                    LOG_ERROR("Assembler program \"%S\" does not exist. Aborting build...", AssemblerToFind);
                    
                    LOG("\n    Make sure that you have the Visual Studio build tools installed and "
                        "\n    that you run riftbuild from a different terminal application named"
                        "\n    \"x64 (or x86) Native Tools Command Prompt for VS\".");

                    LOG("\n    This can be found through Windows Search.");
                    #else
                    LOG_ERROR("Assembler program \"%S\" does not exist on non-Windows platforms. Use a different assembler. Aborting build...", AssemblerToFind);
                    #endif
                }
                else
                {
                    LOG_ERROR("Assembler program \"%S\" does not exist. Make sure that it is installed and added to the path environment.\n"
                            "        Alternatively, you can specify the full path to the assembler executable instead. Aborting build...\n", AssemblerToFind);

                    LogPathEnvVarTutorialSteps();
                }

                return false;
            }
        }
    }

    return true;
}

static void StoreKVNodeAsCmdOption(LinearAllocator* Arena, const String Key, Node* Block, ParsingContext* Context)
{
    // does the cmd line option for this key already exist?
    CmdOption* OptionPtr = NULL;
    for each (CmdOption, o, Context->CmdOptionsDB)
    {
        bool bMatch = String_IsEqual(o.Name, Key, false);
        if (bMatch)
        {
            OptionPtr = o_;
            break;
        }
    }

    if (OptionPtr)
    {
        if (OptionPtr->Value.Length > 0)
        {
            String CompilerName = Filesystem_ExtractFileName(OptionPtr->Value, false);
            AddCmdOption(Context->CmdOptionsDB, CompilerName, String_Null());
        }
    }
    else
    {
        String Val = GetVarValueInList(Context->VarListHead, Key);

        StringLocal(Expanded, MAX_PATH_LENGTH);
        xx ExpandBuildVariable(*Context->TempArena, Context->VarListHead, Context->CmdOptionsDB, &Expanded, Key, Val, Key, Context->WorkingDirectory, false, false, NULL);

        if (Expanded.Length > 0)
        {
            AddCmdOption(Context->CmdOptionsDB, String_Create(Arena, Key), String_Create(Arena, Expanded));

            String CompilerName = Filesystem_ExtractFileName(Expanded, false);
            AddCmdOption(Context->CmdOptionsDB, String_Create(Arena, CompilerName), String_Null());
        }


        /*
        NodeList** Next = &Block->List;
        while (*Next)
        {
            Node* Root = (*Next)->Node;

            bool bValid = Root && Root != &Node_Null;
            if (bValid)
            {
                if (Root->Type == Node_KeyValue)
                {
                    String FinalKey = Root->Key;

                    if (String_IsEqual(FinalKey, Key, false))
                    {
                        StringLocal(Val, MAX_PATH_LENGTH);

                        if (Root->Value)
                        {
                            for each_string_in_list (*Root->Value)
                            {
                                String_Append(&Val, It.String);
                            }

                            xx String_EatSpacesInlineFromEnd(&Val);
                        }

                        StringLocal(Expanded, MAX_PATH_LENGTH);
                        xx ExpandBuildVariable(*Context->TempArena, Context->VarListHead, Context->CmdOptionsDB, &Expanded, FinalKey, Val, FinalKey, Context->WorkingDirectory, false, false, NULL);

                        if (Expanded.Length > 0)
                        {
                            AddCmdOption(Context->CmdOptionsDB, String_Create(Arena, FinalKey), String_Create(Arena, Expanded));

                            String CompilerName = Filesystem_ExtractFileName(Expanded, false);
                            AddCmdOption(Context->CmdOptionsDB, String_Create(Arena, CompilerName), String_Null());
                        }

                        break;
                    }
                }
            }

            Next = &(*Next)->Next;
        }
        */
    }
}

static bool Analyze_Compiler(LinearAllocator* Arena, Node* Block, ParsingContext* Context)
{
    StoreKVNodeAsCmdOption(Arena, S("Compiler"),  Block, Context);
    StoreKVNodeAsCmdOption(Arena, S("Assembler"), Block, Context);
    StoreKVNodeAsCmdOption(Arena, S("Linker"),    Block, Context);
    StoreKVNodeAsCmdOption(Arena, S("Archiver"),  Block, Context);

    String CompilerProgram  = GetCmdOptionValue(Context->CmdOptionsDB, S("Compiler"));
    String AssemblerProgram = GetCmdOptionValue(Context->CmdOptionsDB, S("Assembler"));
    String LinkerProgram    = GetCmdOptionValue(Context->CmdOptionsDB, S("Linker"));
    String ArchiverProgram  = GetCmdOptionValue(Context->CmdOptionsDB, S("Archiver"));
    
    // right now we only care about if we explicity specified a linker
    if (String_IsValid(LinkerProgram))
    {
        AddCmdOption(Context->CmdOptionsDB, S("Linker.Explicit"), String_Null());
    }

    StringLocal(CompilerPath,        MAX_PATH_LENGTH);
    StringLocal(AssemblerPath,       MAX_PATH_LENGTH);
    StringLocal(LinkerPath,          MAX_PATH_LENGTH);
    StringLocal(ArchiverPath,        MAX_PATH_LENGTH);
    StringLocal(CompilerInstallPath, MAX_PATH_LENGTH);
    StringLocal(CompilerToolPath,    MAX_PATH_LENGTH);
    StringLocal(CompilerBasePath,    MAX_PATH_LENGTH);
    StringLocal(CompilerIncludePath, MAX_PATH_LENGTH);
    StringLocal(CompilerLibraryPath, MAX_PATH_LENGTH);

    CompilerPaths FoundCompilerPaths = {0};
    FoundCompilerPaths.CompilerPath  = CompilerPath;
    FoundCompilerPaths.AssemblerPath = AssemblerPath;
    FoundCompilerPaths.LinkerPath    = LinkerPath;
    FoundCompilerPaths.ArchiverPath  = ArchiverPath;
    FoundCompilerPaths.InstallPath   = CompilerInstallPath;
    FoundCompilerPaths.ToolPath      = CompilerToolPath;
    FoundCompilerPaths.BasePath      = CompilerBasePath;
    FoundCompilerPaths.IncludePath   = CompilerIncludePath;
    FoundCompilerPaths.LibraryPath   = CompilerLibraryPath;

    bool bSuccess = FindFirstCompilerAvailable(CompilerProgram, AssemblerProgram, LinkerProgram, ArchiverProgram, &FoundCompilerPaths);

    if (bSuccess)
    {
        AddCmdOption(Context->CmdOptionsDB, S("Compiler.Path"),        String_Create(Context->TempArena, FoundCompilerPaths.CompilerPath));
        AddCmdOption(Context->CmdOptionsDB, S("Compiler.InstallPath"), String_Create(Context->TempArena, FoundCompilerPaths.InstallPath));

        AddCmdOption(Context->CmdOptionsDB, S("Compiler.BasePath"),    String_Create(Context->TempArena, FoundCompilerPaths.InstallPath));
        AddCmdOption(Context->CmdOptionsDB, S("Compiler.ToolPath"),    String_Create(Context->TempArena, FoundCompilerPaths.ToolPath));
        AddCmdOption(Context->CmdOptionsDB, S("Compiler.LibraryPath"), String_Create(Context->TempArena, FoundCompilerPaths.LibraryPath));
        AddCmdOption(Context->CmdOptionsDB, S("Compiler.IncludePath"), String_Create(Context->TempArena, FoundCompilerPaths.IncludePath));

        AddCmdOption(Context->CmdOptionsDB, S("Assembler.Path"),       String_Create(Context->TempArena, FoundCompilerPaths.AssemblerPath));
        AddCmdOption(Context->CmdOptionsDB, S("Linker.Path"),          String_Create(Context->TempArena, FoundCompilerPaths.LinkerPath));
        AddCmdOption(Context->CmdOptionsDB, S("Archiver.Path"),        String_Create(Context->TempArena, FoundCompilerPaths.ArchiverPath));

        ECompiler CompilerVendor = DetermineCompilerVendor(FoundCompilerPaths.CompilerPath);

        switch (CompilerVendor)
        {
            default: {} break;

            case Compiler_Clang:
            {
                AddCmdOption(Context->CmdOptionsDB, S("clang"), String_Null());
            }
            break;

            case Compiler_Clang_MSVC:
            {
                AddCmdOption(Context->CmdOptionsDB, S("clang-msvc"), String_Null());
                AddCmdOption(Context->CmdOptionsDB, S("clang-cl"), String_Null());
            }
            break;
            
            case Compiler_GCC:
            {
                AddCmdOption(Context->CmdOptionsDB, S("gnu"), String_Null());
                AddCmdOption(Context->CmdOptionsDB, S("gcc"), String_Null());
            }
            break;

            case Compiler_MINGW:
            {
                AddCmdOption(Context->CmdOptionsDB, S("mingw"), String_Null());
            }
            break;

            case Compiler_MSVC:
            {
                AddCmdOption(Context->CmdOptionsDB, S("msvc"), String_Null());
                AddCmdOption(Context->CmdOptionsDB, S("cl"), String_Null());
            }
            break;

            case Compiler_TCC:
            {
                AddCmdOption(Context->CmdOptionsDB, S("tcc"), String_Null());
            }
            break;

            case Compiler_Generic:
            {
                AddCmdOption(Context->CmdOptionsDB, Filesystem_ExtractFileName(CompilerPath, false), String_Null());
            }
            break;
        }

        // now get the compiler version (by running the compiler executable)

        PlatformPipe StdOutPipe = {0};
        StringLocal(CmdLine, 2048);
        String_Append(&CmdLine, FoundCompilerPaths.CompilerPath);
        String_AppendSpace(&CmdLine);

        if (CompilerVendor != Compiler_MSVC)
        {
            String_Append(&CmdLine, S("-v"));
        }

        PlatformHandle H = Platform_RunProcess_Ex(FoundCompilerPaths.CompilerPath, CmdLine, Context->WorkingDirectory, &StdOutPipe);

        Platform_CloseHandle(StdOutPipe[1]); // not needed

        if (Platform_IsValidHandle(H))
        {
            Platform_WaitForHandle(H, -1);
            
            StringLocal(StdOutData, UINT16_MAX);

            usize BytesRead = 0;
            if (Filesystem_ReadPipe(StdOutPipe, StdOutData.Capacity, StdOutData.Data, &BytesRead))
            {
                StdOutData.Length = Min((u32)BytesRead, StdOutData.Capacity);

                u32 Index = 0;
                if (String_IndexOfSubstring(StdOutData, S("version "), false, &Index))
                {
                    String FoundVersion = StrShiftF(StdOutData, Index+8);

                    bool bFirstSpace = String_IndexOfFirstWhitespace(FoundVersion, &Index);
                    if (bFirstSpace)
                    {
                        FoundVersion = StrSlice(FoundVersion.Data, Index);
                    }

                    AddCmdOption(Context->CmdOptionsDB, S("Compiler.Version"), String_Create(Context->TempArena, FoundVersion));

                    String CompilerName = Filesystem_ExtractFileName(FoundCompilerPaths.CompilerPath, false);

                    StringLocal(Temp, 64);
                    String_AppendF(&Temp, S("%S.Version"), CompilerName);
                    AddCmdOption(Context->CmdOptionsDB, String_Create(Context->TempArena, Temp), String_Create(Context->TempArena, FoundVersion));

                    if (String_IsEqual(CompilerName, S("cl"), false))
                    {
                        AddCmdOption(Context->CmdOptionsDB, S("MSVC.Version"), String_Create(Context->TempArena, FoundVersion));
                    }
                }
            }
        }
    }

    return bSuccess;
}

static void Analyze_Options(LinearAllocator* Arena, Node* Block, ParsingContext* Context)
{
    NodeList** Next = &Block->List;
    while (*Next)
    {
        Node* Root = (*Next)->Node;

        bool bValid = Root && Root != &Node_Null;
        if (bValid)
        {
            if (Root->Type == Node_KeyValue)
            {
                Analyze_KVNode_Option(Arena, Root, Context);
            }
        }

        Next = &(*Next)->Next;
    }
}

NO_DISCARD static NodeList* Analyze_List(LinearAllocator* Arena, Node* Block, ParsingContext* Context, bool bInIf)
{
    Context->Level += 1;

    NodeList* IndeterminateList = NULL;
    NodeList** IndeterminateNext = &IndeterminateList;

    usize AllocatedBeforeLoop = Arena->Allocated;
    FileVariableList** TailBeforeLoop = Context->VarListTail;

    NodeList** Next = &Block->List;
    while (*Next)
    {
        Node* Root = (*Next)->Node;

        bool bValid = Root && Root != &Node_Null;
        if (bValid)
        {
            bool bSuccess = true;
            NodeList* ListToAdd = NULL;

            if (Root->Type == Node_Block)
            {
                // does this block have a params list? if so, add it as a KV_Node with a null value
                if (Root->Parameters)
                {
                    Analyze_KVNode(Arena, Root, Context);
                }

                NodeList* Tree = Analyze_List(Arena, Root, Context, bInIf);
                if (Tree)
                {
                    ListToAdd = Tree;
                    bSuccess = false;
                }
            }
            else if (Root->Type == Node_If)
            {
                NodeList* Tree = Analyze_IfNode(Arena, Root, Context, true);
                if (Tree)
                {
                    ListToAdd = Tree;
                    bSuccess = false;
                }
            }
            else if (Root->Type == Node_Help)
            {
                Analyze_KVNode(Arena, Root, Context);
            }
            else if (Root->Type == Node_Include)
            {
                NodeList* Tree = Analyze_IncludeNode(Arena, Root, Context);
                if (Tree)
                {
                    ListToAdd = Tree;
                    bSuccess = false;
                }
            }
            else if (Root->Type == Node_ErrorMessage)
            {
                Analyze_KVNode(Arena, Root, Context);
            }
            else if (Root->Type == Node_LogMessage)
            {
                String Message = String_CreateFromList(Arena, *Root->Value);
                Array_Add(Context->Messages, Message);

                bSuccess = true;
            }
            else if (Root->Type == Node_KeyValue)
            {
                Analyze_KVNode(Arena, Root, Context);
            }

            if (Context->Level > 1) // above root level?
            {
                if (!bSuccess)
                {
                    // does this block care about ordering of its child nodes?
                    if (Block->bPreserveOrder)
                    {
                        IndeterminateList = Block->List;

                        // "free" the memory that was allocated
                        Context->VarListTail = TailBeforeLoop;
                        LinearAllocator_Reset(Arena, AllocatedBeforeLoop);

                        break;
                    }
                }
            }

            if (!bSuccess)
            {
                if (ListToAdd)
                {
                    SLinkedList_Push(IndeterminateNext, ListToAdd);
                }
                else
                {
                    SLinkedList_Push(IndeterminateNext, NodeList_Create(Arena, Root, NULL));
                }
            }
        }

        Next = &(*Next)->Next;
    }

    return IndeterminateList;
}

UNUSED static void Internal_PrintVariables(TArray(FileVariable) VariablesDB)
{
    for each (FileVariable, vo, VariablesDB)
    {
        LOG("KEY:    %S", vo.Name);
        LOG("VALUE:  %S", vo.Value);
        if (vo.Params.Length)
        {
            LOG("PARAMS: %S", vo.Params);
        }

        LOG_LINE_BREAK();
    }
}

static bool Internal_LogCustomErrorMessage(ParsingContext* Context, const String ContextKey, const String Key, const bool bLineBreak)
{
    if (bQuietBuild) { Logging_Enable(); }

    bool bLogged = false;

    SLinkedList_Each(FileVariableList, This, &Context->VarListHead)
    {
        const FileVariable Var = (*This)->Var;

        if (String_StartsWith(Var.Name, ContextKey, false) &&
            String_EndsWith(Var.Name, S(".errormessage"), false))
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
                   (ContextKey.Length == 0 || String_StartsWith(Var.Name, ContextKey, false))))
                {
                    StringLocal(Expanded, 1024);
                    xx ExpandBuildVariable(*Context->TempArena, Context->VarListHead, Context->CmdOptionsDB, &Expanded, Var.Name, Var.Value, Var.Name, Context->WorkingDirectory, false, false, NULL);

                    LOG("%S", Expanded);
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

// TODO: pre-analyze asserts and post-analyze asserts
static bool Internal_RunAsserts(ParsingContext* Context, const String BuildFilePath)
{
    bool bAssertionFailed = false;

    SLinkedList_Each(FileVariableList, This, &Context->VarListHead)
    {
        const FileVariable Var = (*This)->Var;

        LinearAllocator Scratch = *Context->TempArena;

        if (Var.Name.Data[0] == 'A' || Var.Name.Data[0] == 'a')
        {
            if (String_IsEqual(Var.Name, S("Assert.Version"), false))
            {
                //if (String_CountChar(Var.Value, '.') >= 1) // make sure this is something sensible
                {
                    ECompareResult Result = String_CompareVersion(S(RIFTBUILD_VERSION_STRING), Var.Value);
                    if (Result == CompareResult_Less)
                    {
                        LOG_INLINE_ERROR(
                        "\n[ASSERTION FAILURE] RiftBuild version \"%S\" is less than the required version \"%S\"."
                        " Please upgrade to \"%S\" or later. Aborting build...\n",
                        S(RIFTBUILD_VERSION_STRING), Var.Value, Var.Value);

                        bAssertionFailed = true;
                        break;
                    }
                }
            }
            else if (String_IsEqual(Var.Name, S("Assert.EnvVar"), false) ||
                     String_IsEqual(Var.Name, S("Assert.EnvVarExists"), false) ||
                     String_IsEqual(Var.Name, S("Assert.Environment"), false))
            {
                StringArray EnvVarsArray = String_ParseIntoArray(&Scratch, Var.Value, ' ', 0, 128);

                for each_str (Env, EnvVarsArray)
                {
                    String Trimmed = String_EatSpaces(*Env);
                    Trimmed = String_EatSpacesFromEnd(Trimmed);

                    bool bFound = Platform_DoesEnvironmentVariableExist(Trimmed);

                    if (!bFound)
                    {
                        #ifndef HOOD
                        LOG_INLINE_ERROR("\n[ASSERTION FAILURE] Environment variable \"%S\" does not exist. Aborting build...\n\n", Trimmed);
                        #else
                        LOG_ERROR("\nyo da environment var \"%S\" don exist cuh. need to be setup n' shit ma nigga\n", Trimmed);
                        #endif

                        xx Internal_LogCustomErrorMessage(Context, S("Env"), Trimmed, true);
                        xx Internal_LogCustomErrorMessage(Context, S("Environment"), Trimmed, true);

                        LogRegularEnvVarTutorialSteps();

                        bAssertionFailed = true;
                        break;
                    }
                }
            }
            else if (Context->bNoFail && (String_IsEqual(Var.Name, S("Assert.BuildVarExists"), false) ||
                     String_IsEqual(Var.Name, S("Assert.Var"), false)))
            {
                StringArray BuildVarsArray = String_ParseIntoArray(&Scratch, Var.Value, ' ', 0, 128);

                for each_str (Str, BuildVarsArray)
                {
                    String Trimmed = String_EatSpaces(*Str);

                    bool bFound = DoesVarExistInList(Context->VarListHead, Trimmed);

                    if (!bFound)
                    {
                        #ifndef HOOD
                        LOG_INLINE_ERROR("\n[ASSERTION FAILURE] Build variable \"%S\" does not exist. Aborting build...\n", Trimmed);
                        #else
                        LOG_ERROR("\nyo da build var \"%S\" don exist cuh. dat shit not there nigga", Trimmed);
                        #endif

                        bAssertionFailed = true;
                        break;
                    }
                }
            }
            else if (String_IsEqual(Var.Name, S("Assert.Compiler"), false))
            {
                StringArray CompilersArray = String_ParseIntoArray(&Scratch, Var.Value, ' ', 0, 128);
                
                if (CompilersArray.Num > 0)
                {
                    String CompilerProgram = Filesystem_ExtractFileName(GetCmdOptionValue(Context->CmdOptionsDB, S("Compiler.Path")), false);

                    bool bAnyCompilerMatch = false;
                    for each_str (Str, CompilersArray)
                    {
                        if (String_IsEqual(*Str, CompilerProgram, false))
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
                        const String BuildFileName = Filesystem_ExtractFileName(BuildFilePath, true);

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

                        for each_str (str, CompilersArray)
                        {
                            Internal_LogCustomErrorMessage(Context, S("Compiler"), *str, true);
                        }

                        bAssertionFailed = true;
                        break;
                    }
                }
            }
            else if (String_IsEqual(Var.Name, S("Assert.Assembler"), false))
            {
                StringArray AssemblersArray = String_ParseIntoArray(&Scratch, Var.Value, ' ', 0, 128);
                
                if (AssemblersArray.Num > 0)
                {
                    String AsmProgram = Filesystem_ExtractFileName(GetCmdOptionValue(Context->CmdOptionsDB, S("Assembler.Path")), false);

                    bool bAnyAssemblerMatch = false;
                    for each_str (str, AssemblersArray)
                    {
                        if (String_IsEqual(*str, AsmProgram, false))
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
                        const String BuildFileName = Filesystem_ExtractFileName(BuildFilePath, true);

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

                        for each_str (str, AssemblersArray)
                        {
                            Internal_LogCustomErrorMessage(Context, S("Assembler"), *str, true);
                        }

                        bAssertionFailed = true;
                        break;
                    }
                }
            }
            else if (String_IsEqual(Var.Name, S("Assert.Desktop"), false) ||
                     String_IsEqual(Var.Name, S("Assert.DesktopEnvironment"), false))
            {
                StringArray DesktopsArray = String_ParseIntoArray(&Scratch, Var.Value, ' ', 0, 128);

                StringLocal(DesktopEnv, 128);
                #if PLATFORM_WINDOWS || PLATFORM_MAC
                Platform_DetectDesktopEnvironment(&DesktopEnv);
                #elif PLATFORM_LINUX || PLATFORM_BSD
                Platform_DetectDesktopEnvironment(&DesktopEnv, NULL, NULL);
                #endif
                
                if (String_IsValid(DesktopEnv))
                {

                    if (DesktopsArray.Num > 0)
                    {
                        StringLocal(DesktopsLogString, 128);
                        {
                            u8 i = 0;
                            for each_str_i (i, p, DesktopsArray)
                            {
                                String_Append(&DesktopsLogString, *p);
                                if (DesktopsArray.Num > 1 && i != DesktopsArray.Num-1)
                                {
                                    if (i == DesktopsArray.Num-2)
                                    {
                                        String_Append(&DesktopsLogString, S(" and "));
                                    }
                                    else
                                    {
                                        String_AppendChar(&DesktopsLogString, ',');
                                        String_AppendSpace(&DesktopsLogString);
                                    }
                                }
                            }
                        }

                        bool bAnyDesktopMatch = false;
                        for each_str (s, DesktopsArray)
                        {
                            String Trimmed = String_EatSpaces(*s);

                            if (String_IsEqual(Trimmed, DesktopEnv, false))
                            {
                                bAnyDesktopMatch = true;
                                break;
                            }
                        }

                        if (!bAnyDesktopMatch)
                        {
                            #ifndef HOOD
                            const String BuildFileName = Filesystem_ExtractFileName(BuildFilePath, true);
                            LOG_INLINE_ERROR("\n[ASSERTION FAILURE] %S can only be built on a %S desktop environment. You are on %S. Aborting build...\n", BuildFileName, DesktopsLogString, DesktopEnv);
                            #else
                            LOG_ERROR("yo u cant build on dis desktop envyiroment nigga. %S aint supportd bro\n", DesktopEnv);
                            #endif

                            if (Internal_LogCustomErrorMessage(Context, S("Desktop"), DesktopEnv, true))
                            {
                                break;
                            }

                            bAssertionFailed = true;
                            break;
                        }
                    }
                }
            }
            else if (String_IsEqual(Var.Name, S("Assert.Platform"), false))
            {
                StringArray PlatformsArray = String_ParseIntoArray(&Scratch, Var.Value, ' ', 0, 128);

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
                const String HostPlatform = S("Apple Mac MacOS OSX Unix");
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
                        const String BuildFileName = Filesystem_ExtractFileName(BuildFilePath, true);
                        LOG_INLINE_ERROR("\n[ASSERTION FAILURE] %S can only be built on %S. You are on %S. Aborting build...\n", BuildFileName, PlatformsLogString, S(PLATFORM_STRING));
                        #else
                        LOG_ERROR("yo u cant build on dis platform nigga. %S aint supportd bro\n", S(PLATFORM_STRING));
                        #endif

                        StringArray AdditionalPlatforms = String_ParseIntoArray(&Scratch, HostPlatform, ' ', 0, 128);
                        for each_str (p, AdditionalPlatforms)
                        {
                            if (Internal_LogCustomErrorMessage(Context, S("Platform"), *p, true))
                            {
                                break;
                            }
                        }

                        bAssertionFailed = true;
                        break;
                    }
                }
            }
            else if (String_IsEqual(Var.Name, S("Assert.Platform.Version"), false))
            {
                StringArray VersionsArray = String_ParseIntoArray(&Scratch, Var.Value, ' ', 0, 128);

                for each_str (s, VersionsArray)
                {
                    String Value = String_EatSpaces(*s);
                    if (String_CountChar(Value, '.') >= 1) // make sure this is something sensible
                    {
                        PlatformVersion OSVersion = Platform_GetVersion();

                        StringLocal(VersionString, 32);
                        String_Format(&VersionString, S("%u.%u.%u"), OSVersion.Major, OSVersion.Minor, OSVersion.Patch);

                        EComparisonType CmpType = Cmp_LessThan;
                        if (String_StartsWith(Value, S(">"), false))
                        {
                            Value = StrShiftF(Value, 1);

                            CmpType = Cmp_GreaterThan;
                            if (String_StartsWith(Value, S("="), false))
                            {
                                CmpType = Cmp_GreaterThanOrEqual;
                                Value = StrShiftF(Value, 1);
                            }
                        }
                        else if (String_StartsWith(Value, S("<"), false))
                        {
                            Value = StrShiftF(Value, 1);

                            CmpType = Cmp_LessThan;
                            if (String_StartsWith(Value, S("="), false))
                            {
                                CmpType = Cmp_LessThanOrEqual;

                                Value = StrShiftF(Value, 1);
                            }
                        }
                        else if (String_StartsWith(Value, S("="), false))
                        {
                            Value = StrShiftF(Value, 1);

                            CmpType = Cmp_Equal;
                            if (String_StartsWith(Value, S("="), false))
                            {
                                Value = StrShiftF(Value, 1);
                            }
                        }

                        ECompareResult Result = String_CompareVersion(VersionString, Value);
                        bool bCompareFailed = false;
                        String CmpString = String_Null();
                        if (CmpType == Cmp_None)
                        {
                            bCompareFailed = Result == CompareResult_Less;
                            CmpString = S("less than");
                        }
                        else
                        {
                            if (CmpType == Cmp_Equal && Result != CompareResult_Equal)
                            {
                                bCompareFailed = true;
                                CmpString = S("equal to");
                            }

                            if (CmpType == Cmp_LessThan && Result != CompareResult_Less)
                            {
                                bCompareFailed = true;
                                CmpString = S("less than");
                            }

                            if (CmpType == Cmp_LessThanOrEqual && !(Result == CompareResult_Less || Result == CompareResult_Equal))
                            {
                                bCompareFailed = true;
                                CmpString = S("less than or equal to");
                            }

                            if (CmpType == Cmp_GreaterThan && Result != CompareResult_Greater)
                            {
                                bCompareFailed = true;
                                CmpString = S("greater than");
                            }

                            if (CmpType == Cmp_GreaterThanOrEqual && !(Result == CompareResult_Greater || Result == CompareResult_Equal))
                            {
                                bCompareFailed = true;
                                CmpString = S("greater than or equal to");
                            }
                        }

                        if (bCompareFailed)
                        {
                            LOG_INLINE_ERROR(
                                "\n[ASSERTION FAILURE] Your %S version %u.%u.%u is not %S the required version of %S. Aborting...\n",
                                S(PLATFORM_STRING),
                                OSVersion.Major, OSVersion.Minor, OSVersion.Patch,
                                CmpString,
                                Value
                            );

                            bAssertionFailed = true;
                            break;
                        }
                    }
                }

                if (bAssertionFailed)
                {
                    break;
                }
            }
            else if (String_IsEqual(Var.Name, S("Assert.Arch"), false) ||
                     String_IsEqual(Var.Name, S("Assert.Architecture"), false))
            {
                StringArray ArchitecturesArray = String_ParseIntoArray(&Scratch, Var.Value, ' ', 0, 128);

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
                        const String BuildFileName = Filesystem_ExtractFileName(BuildFilePath, true);
                        LOG_INLINE_ERROR("\n[ASSERTION FAILURE] %S can only be built on %S architectures. You are on %S. Aborting build...\n", BuildFileName, ArchitecturesLogString, S(CPU_ARCHITECTURE_STRING));
                        #else
                        LOG_ERROR("yo u cant build on dis platform nigga. %S aint supportd bro\n", S(CPU_ARCHITECTURE_STRING));
                        #endif

                        StringArray AdditionalArchs = String_ParseIntoArray(&Scratch, S(CPU_ARCHITECTURE_STRING_EX), '|', 0, 128);
                        for each_str (p, AdditionalArchs)
                        {
                            if (Internal_LogCustomErrorMessage(Context, S("Arch"), *p, true))
                            {
                                break;
                            }
                        }

                        bAssertionFailed = true;
                        break;
                    }
                }
            }
            else if (String_IsEqual(Var.Name, S("Assert.WorkingDirectory"), false))
            {
                if (Var.Value.Length > 0)
                {
                    //String_ConvertSlashToPlatformSlash(&Var.Value);

                    // did we get a relative directory?
                    #if PLATFORM_WINDOWS
                    bool bDriveSymbol = String_IndexOfChar(Var.Value, ':', NULL);
                    #else
                    bool bDriveSymbol = Var.Value.Data[0] == '/';
                    #endif

                    bool bRelative = !bDriveSymbol;

                    StringLocal(AssertPath, MAX_PATH_LENGTH);

                    if (bRelative)
                    {
                        u32 LastSlash = 0;
                        if (String_IndexOfLastPathSlash(BuildFilePath, &LastSlash))
                        {
                            //String_Append(&AssertPath, StrSlice(BuildFilePathFull.Data, LastSlash+1));
                            //String_Append(&AssertPath, Var.Value);
                        }
                        
                        String_BuildPath(&AssertPath, Context->WorkingDirectory, StrSlice(BuildFilePath.Data, LastSlash), Var.Value);
                    }
                    else
                    {
                        String_Copy(&AssertPath, Var.Value);
                        String_ConvertSlashToPlatformSlash(&AssertPath);
                    }

                    xx Filesystem_ConvertRelativeToAbsolutePath(&AssertPath);
                    xx String_EatPathSeparatorsInlineFromEnd(&AssertPath);

                    if (AssertPath.Length > 0 && !String_IsEqual(Context->WorkingDirectory, AssertPath, false))
                    {
                        #ifndef HOOD
                        const String BuildFileName = Filesystem_ExtractFileName(BuildFilePath, true);

                        LOG_INLINE_ERROR("\n[ASSERTION FAILURE] %S must be ran from this directory:\n\n"
                                         "                      \"%S\"\n\n"
                                         "                    but we are in\n\n"
                                         "                      \"%S\"\n", BuildFileName, AssertPath, Context->WorkingDirectory);
                        #else
                        LOG_ERROR("yo we cant run from this dir cuh \"%S\" you gotta run from \"%S\"", Context->WorkingDirectory, AssertPath);
                        #endif

                        bAssertionFailed = true;
                        break;
                    }
                }
            }
            else if (String_IsEqual(Var.Name, S("Assert.File"), false))
            {
                StringList List = String_SplitIntoList(&Scratch, Var.Value, ' ', true);
                for each_string_in_list (List)
                {
                    if (!Filesystem_DoesFileExist(It.String))
                    {
                        LOG_INLINE_ERROR("\n[ASSERTION FAILURE] File \"%S\" does not exist. Aborting build...\n", It.String);

                        xx Internal_LogCustomErrorMessage(Context, S("File"), It.String, true);

                        bAssertionFailed = true;
                        break;
                    }
                }
            }
            else if (String_IsEqual(Var.Name, S("Assert.Directory"), false))
            {
                StringList List = String_SplitIntoList(&Scratch, Var.Value, ' ', true);
                for each_string_in_list (List)
                {
                    if (!Filesystem_DoesDirectoryExist(It.String))
                    {
                        LOG_INLINE_ERROR("\n[ASSERTION FAILURE] Directory \"%S\" does not exist. Aborting build...\n", It.String);

                        xx Internal_LogCustomErrorMessage(Context, S("Directory"), It.String, true);

                        bAssertionFailed = true;
                        break;
                    }
                }
            }
            else if (String_IsEqual(Var.Name, S("Assert.Arg"), false))
            {
                StringArray CmdVarsArray = String_ParseIntoArray(&Scratch, Var.Value, ' ', 0, 128);

                for each_str (S, CmdVarsArray)
                {
                    const String Trimmed = String_EatSpaces(*S);

                    bool bFound = false;
                    // TODO: extract into a function. replace all the other cmdoptionsdb loops
                    for each (CmdOption, o, Context->CmdOptionsDB)
                    {
                        if (String_IsEqual(o.Name, Trimmed, false))
                        {
                            bFound = true;

                            // if an '=' was specified but no value was specified after that
                            // so something like this: some_arg=
                            if (String_IsDataValid(o.Value) && o.Value.Length == 0)
                            {
                                bFound = false;
                            }
                            
                            break;
                        }
                    }

                    if (!bFound)
                    {
                        #ifndef HOOD
                        LOG_INLINE_ERROR("\n[ASSERTION FAILURE] Command line argument \"%S\" or \"%S=VALUE\" was not given."
                                        "\n                    This is needed for the build to work properly. Aborting build...\n", Trimmed, Trimmed);
                        #else
                        LOG_ERROR("yo da cmd line var \"%S\" don exist cuh. dat shit not there nigga", Trimmed);
                        #endif

                        xx Internal_LogCustomErrorMessage(Context, S("Arg"), Trimmed, true);

                        bAssertionFailed = true;
                        break;
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

                    for each (CmdOption, o, Context->CmdOptionsDB)
                    {
                        if (String_IsEqual(o.Name, Trimmed, false))
                        {
                            bFound = true;

                            // if an '=' was specified but no value was specified after that
                            // so something like this: some_arg=
                            if (String_IsDataValid(o.Value) && o.Value.Length == 0)
                            {
                                bFound = false;
                            }

                            if (bFound)
                            {
                                break;
                            }
                        }
                    }

                    if (bFound)
                    {
                        break;
                    }
                }

                if (!bFound)
                {
                    if (ArgArray.Num == 0)
                    {
                        LOG_INLINE_ERROR("\n[ASSERTION FAILURE] You must specify one or more arguments\n");
                    }
                    else
                    {
                        LOG_INLINE_ERROR("\n[ASSERTION FAILURE] Any one of these arguments must be specified: %S\n", Var.Value);
                    }
                    bAssertionFailed = true;
                    break;
                }
            }
            else if (String_IsEqual(Var.Name, S("Assert.Arg.OnlyOne"), false)) // TODO: make dynamic
            {
                StringArray ArgArray = String_ParseIntoArray(&Scratch, Var.Value, ' ', 0, 128);

                bool bFound = false;
                for each_str (Arg, ArgArray)
                {
                    const String Trimmed = String_EatSpaces(*Arg);

                    for each (CmdOption, o, Context->CmdOptionsDB)
                    {
                        if (String_IsEqual(o.Name, Trimmed, false))
                        {
                            if (bFound)
                            {
                                bFound = false;
                                goto MultipleArgsFound;
                            }

                            bFound = true;

                            // if an '=' was specified but no value was specified after that
                            // so something like this: some_arg=
                            if (String_IsDataValid(o.Value) && o.Value.Length == 0)
                            {
                                bFound = false;
                            }
                            break;
                        }
                    }
                }
                
                MultipleArgsFound:
                if (!bFound)
                {
                    LOG_INLINE_ERROR("\n[ASSERTION FAILURE] Only one of these arguments can be specified: %S\n", Var.Value);
                    bAssertionFailed = true;
                    break;
                }
            }
            else if (String_IsEqual(Var.Name, S("Assert.Program"), false))
            {
                // expand them here
                StringLocal(Expanded, 512);
                xx ExpandBuildVariable(*Context->TempArena, Context->VarListHead, Context->CmdOptionsDB, &Expanded, Var.Name, Var.Value, Var.Name, Context->WorkingDirectory, false, false, NULL);

                StringArray ProgramsArray = String_ParseIntoArray(&Scratch, Expanded, ' ', 0, 128);

                for each_str (Program, ProgramsArray)
                {
                    String Trimmed = String_EatSpaces(*Program);

                    bool bFound = Platform_FindProgram(Trimmed);

                    if (!bFound)
                    {
                        #ifndef HOOD
                        LOG_INLINE_ERROR("\n[ASSERTION FAILURE] Program \"%S\" does not exist. Make sure that \"%S\" is installed and that its directory has been set in the path environment variable. Aborting build...\n", Trimmed, Trimmed);
                        #else
                        LOG_ERROR("yo dis program \"%S\" don exist cuh. need to be installed and set in da path ma nigga\n", Trimmed);
                        #endif

                        xx Internal_LogCustomErrorMessage(Context, S("Program"), Trimmed, false);
                        
                        bAssertionFailed = true;
                        break;
                    }
                }
            }
            else if (String_IsEqual(Var.Name, S("Assert.Compiler.Version"), false))
            {
                String RequireCompilerVersion = String_Null();
                EComparisonType CompilerVersionComparisonType = Cmp_Equal;

                if (Var.Value.Length > 0)
                {
                    u8 SymbolLength = 0;

                    if (String_StartsWith(Var.Value, S("=="), false))
                    {
                        CompilerVersionComparisonType = Cmp_Equal;
                        SymbolLength = 2;
                    }
                    else if (String_StartsWith(Var.Value, S(">="), false))
                    {
                        CompilerVersionComparisonType = Cmp_GreaterThanOrEqual;
                        SymbolLength = 2;
                    }
                    else if (String_StartsWith(Var.Value, S("<="), false))
                    {
                        CompilerVersionComparisonType = Cmp_LessThanOrEqual;
                        SymbolLength = 2;
                    }
                    else if (String_StartsWith(Var.Value, S(">"), false))
                    {
                        CompilerVersionComparisonType = Cmp_GreaterThan;
                        SymbolLength = 1;
                    }
                    else if (String_StartsWith(Var.Value, S("<"), false))
                    {
                        CompilerVersionComparisonType = Cmp_LessThan;
                        SymbolLength = 1;
                    }
                    else if (String_StartsWith(Var.Value, S("="), false))
                    {
                        CompilerVersionComparisonType = Cmp_Equal;
                        SymbolLength = 1;
                    }
                    else
                    {
                        // no action required
                    }

                    RequireCompilerVersion = String_EatSpacesFromEnd(String_EatSpaces(StrShiftF(Var.Value, SymbolLength)));
                }

                if (RequireCompilerVersion.Length > 0)
                {
                    String FoundVersion = GetCmdOptionValue(Context->CmdOptionsDB, S("Compiler.Version"));
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
                    else
                    {
                        // no action required
                    }

                    if (!bCompareResultsMatch)
                    {
                        String Prefix = S("of");

                        String Extra = S(" exactly");

                        if (CompilerVersionComparisonType == Cmp_GreaterThan)
                        {
                            Prefix = S("above");
                            Extra = String_Null();
                        }
                        else if (CompilerVersionComparisonType == Cmp_GreaterThanOrEqual)
                        {
                            Extra = S(" or above");
                        }
                        else if (CompilerVersionComparisonType == Cmp_LessThan)
                        {
                            Prefix = S("below");
                            Extra = String_Null();
                        }
                        else if (CompilerVersionComparisonType == Cmp_LessThanOrEqual)
                        {
                            Extra = S(" or below");
                        }
                        else
                        {
                            // no action required
                        }

                        String CompilerProgram = GetCmdOptionValue(Context->CmdOptionsDB, S("Compiler.Path"));
                        LOG_INLINE_ERROR("\n[ASSERTION FAILURE] Compiler \"%S\" (version %S) does not meet the required version %S \"%S\"%S. Aborting build...\n", CompilerProgram, FoundVersion, Prefix, RequireCompilerVersion, Extra);

                        bAssertionFailed = true;
                        break;
                    }
                }
            }
            else
            {
                // no action is required
            }
        }
        else
        {
            if (String_StartsWith(Var.Name, S("Option."), false) && String_EndsWith(Var.Name, S(".Assert"), false))
            {
                String Trimmed = StrShiftF(Var.Name, 7);
                {
                    u32 LastDot = 0;
                    xx String_IndexOfLastChar(Trimmed, '.', &LastDot);
                    Trimmed = StrSlice(Trimmed.Data, LastDot);
                }

                bool bFound = false;
                // TODO: extract into a function. replace all the other cmdoptionsdb loops
                for each (CmdOption, o, Context->CmdOptionsDB)
                {
                    if (String_IsEqual(o.Name, Trimmed, false))
                    {
                        bFound = true;

                        // if an '=' was specified but no value was specified after that
                        // so something like this: some_arg=
                        if (String_IsDataValid(o.Value) && o.Value.Length == 0)
                        {
                            bFound = false;
                        }

                        break;
                    }
                }

                String SearchName = Var.Name;
                {
                    u32 LastDot = 0;
                    if (String_IndexOfLastChar(StrShiftF(SearchName, 7), '.', &LastDot))
                    {
                        SearchName = StrSlice(SearchName.Data, LastDot+7);
                    }
                }

                bool bFoundParams = false;
                xx GetOptionParamsFromVarList(Context->VarListHead, SearchName, &bFoundParams);

                if (!bFound)
                {
                    #ifndef HOOD
                    if (bFoundParams)
                    {
                        LOG_INLINE_ERROR("\n[ASSERTION FAILURE] Command line argument \"%S=VALUE\" was not given."
                                        "\n                    This is needed for the build to work properly. Aborting build...\n", Trimmed);
                    }
                    else
                    {
                        LOG_INLINE_ERROR("\n[ASSERTION FAILURE] Command line argument \"%S\" was not given."
                                        "\n                    This is needed for the build to work properly. Aborting build...\n", Trimmed);
                    }
                    #else
                    LOG_ERROR("yo da cmd line var \"%S\" don exist cuh. dat shit not there nigga", Trimmed);
                    #endif

                    xx Internal_LogCustomErrorMessage(Context, S("Option"), Trimmed, true);

                    bAssertionFailed = true;
                    break;
                }
            }
        }

        if (bAssertionFailed)
        {
            break;
        }
    }

    if (!bAssertionFailed)
    {
        // go through all the options and validate their values
        SLinkedList_Each(FileVariableList, This2, &Context->VarListHead)
        {
            const FileVariable Var = (*This2)->Var;

            LinearAllocator Scratch = *Context->TempArena;

            if (String_StartsWith(Var.Name, S("Option."), false))
            {
                // make sure this is just a option.something key with no children keys
                String OptionName = StrShiftF(Var.Name, 7);
                if (String_CountChar(OptionName, '.') == 0)
                {
                    String OptionValue = String_Null();

                    bool bFound = false;
                    for each (CmdOption, o, Context->CmdOptionsDB)
                    {
                        if (String_IsEqual(o.Name, OptionName, false))
                        {
                            bFound = true;

                            OptionValue = o.Value;
                            
                            break;
                        }
                    }

                    if (bFound && Var.Params.Length > 0)
                    {
                        StringList List = String_SplitIntoList(&Scratch, Var.Params, ' ', true);

                        bool bAnyMatch = false;
                        for each_string_in_list (List)
                        {
                            if (String_IsEqual(It.String, OptionValue, false))
                            {
                                bAnyMatch = true;
                                break;
                            }
                        }

                        if (!bAnyMatch)
                        {
                            LOG_INLINE_ERROR("\n[ASSERTION FAILURE] Command line argument \"%S=%S\" is not a valid option value."
                                            "\n                    A valid option is needed for the build to work properly. Aborting build...\n", OptionName, OptionValue);

                            LOG("\n    Here are the accepted options:");
                            for each_string_in_list (List)
                            {
                                LOG("      - %S=%S", OptionName, It.String);
                            }

                            xx Internal_LogCustomErrorMessage(Context, S("Option"), OptionName, true);

                            bAssertionFailed = true;
                            break;
                        }
                    }
                }
            }
        }
    }

    return bAssertionFailed;
}

static void Internal_SetDefaultBuildVariables(LinearAllocator* Arena, ParsingContext* Context)
{
    if (!DoesVarExistInList(Context->VarListHead, S("BuildDirectory")))
    {
        AddVariableToList(Arena, Context, S("BuildDirectory"), S("Build"), String_Null());
    }

    if (!DoesVarExistInList(Context->VarListHead, S("IntermediateDirectory")))
    {
        AddVariableToList(Arena, Context, S("IntermediateDirectory"), S("Intermediate"), String_Null());
    }

    const String Type = GetVarValueInList(Context->VarListHead, S("Type"));
    bool bSetExtension = false;
    if (String_IsValid(Type))
    {
        String Extension = String_Null();
        
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

        AddVariableToList(Arena, Context, S("Extension"), Extension, String_Null());
        bSetExtension = true;
    }

    if (!bSetExtension && !DoesVarExistInList(Context->VarListHead, S("Extension")))
    {
        #if PLATFORM_WINDOWS
        String Value = S(".exe");
        #elif PLATFORM_APPLE
        String Value = String_Null();
        #else
        String Value = String_Null();
        #endif

        AddVariableToList(Arena, Context, S("Extension"), Value, String_Null());
    }
}

NO_DISCARD bool ParseBuildFile(LinearAllocator* PermanentArena,
                    const FileHandle H,
                    const String BuildFilePath,
                    ParsingContext Context,
                    bool bIsIncludeFile,
                    StringList* Includes)
{
    // sanity - peace of mind checks
    bool bValidFile = ALWAYS(String_IsValid(BuildFilePath));

    // the lexer/parser will need at least 128KiB of memory to function correctly
    bool bEnoughMem = ALWAYS(Context.TempArena->TotalSize > Kibibytes(128));

    bool bSuccess = bValidFile && bEnoughMem;

    Node* AST = &Node_Null;

    if (bSuccess)
    {
        AST = Internal_ParseFile(Context.TempArena, H);
    }

    if (!AST || AST == &Node_Null)
    {
        bSuccess = false;
    }

    if (bSuccess)
    {
        // now do some analysis on the tree (two-pass analysis)

        // we could just end here and pass the raw data to the main program and let it do things with it.
        // this can almost become a general language that others may find useful, it has basic support for
        // if's and namespacing keys. so all we would do is just parse the file and give you key:value array
        // that the program can use and interpret how it wants.
        // IDEA: think about making this a library?

        // High level flow:
        // 1. (first pass) store all keys possible (skip indeterminates)
        //  1a. parse include files (get the ast trees) and repeat step 1
        // 2. (second pass) go through the indeterminate list and store all keys possible
        //  2a. parse include files (get the ast trees) and repeat step 1 independently then continue again with the second pass
        // 3. store default values of keys that were not mentioned in the tree
        // 4. run asserts
        // 5. finally expand all known keys that we care about

        //Clock c;
        //Clock_Start(&c);

        // 1. First pass
        Context.VarListTail = &Context.VarListHead;
        Analyze_Options(Context.TempArena, AST, &Context);

        if (bSuccess)
        {
            NodeList* IndeterminateList = Analyze_List(Context.TempArena, AST, &Context, false);

            if (!Analyze_Compiler(Context.TempArena, AST, &Context))
            {
                // could not find a compiler
                bSuccess = false;
            }

            if (bSuccess)
            {
                // 2. Second pass
                Context.bNoFail = true;
                bSuccess = Analyze_Indeterminates(Context.TempArena, IndeterminateList, &Context);
            }
        }

        //Clock_Tick(&c);
        //Clock_PrintElapsedTime(&c, true);
    }

    if (bSuccess && !bHelp)
    {
        // 3. check for certain keys if they exist, if they dont, add the default value
        {
            if (!DoesVarExistInList(Context.VarListHead, S("Assembly")))
            {
                String FinalName = Filesystem_ExtractFileName(BuildFilePath, false);
                AddVariableToList(Context.TempArena, &Context, S("Assembly"), FinalName, String_Null());
            }

            Internal_SetDefaultBuildVariables(Context.TempArena, &Context);
        }

        // 4. run the asserts
        bool bAssertionFailed = Internal_RunAsserts(&Context, BuildFilePath);

        if (bAssertionFailed)
        {
            bSuccess = false;
        }
    }

    if (bSuccess)
    {
        /*
        SLinkedList_Each(FileVariableList, It, &Context.VarListHead)
        {
            const FileVariable Var = (*It)->Var;
            LOG("KEY:    %S", Var.Name);
            LOG("VALUE:  %S", Var.Value);
            if (Var.Params.Length)
            {
                LOG("PARAMS: %S", Var.Params);
            }

            LOG_LINE_BREAK();
        }
        */

        SLinkedList_Each(FileVariableList, This, &Context.VarListHead)
        {
            const FileVariable Var = (*This)->Var;

            if (String_IsEqual(Var.Name, S("Assert"), false) ||
                String_StartsWith(Var.Name, S("Assert."), false))
            {
                continue;
            }

            if (String_EndsWith(Var.Name, S(".ErrorMessage"), false))
            {
                continue;
            }

            u32 MaxValueLength = 1024;
            bool bStoreInDB = false;
            bool bExcludeFromConcat = false;
            {
                local_persist const String ConcatExclusions[11] =
                {
                    SC("PreDepend"),
                    SC("PreBuild"),
                    SC("PostBuild"),
                    SC("PreCompile"),
                    SC("PostCompile"),
                    SC("PreLink"),
                    SC("PostLink"),
                    SC("Depend"),
                    SC("Depends"),
                    SC("Option."),
                };

                for EachE(i, ConcatExclusions)
                {
                    if (String_IsEqual(Var.Name, ConcatExclusions[i], false) ||
                        String_StartsWith(Var.Name, ConcatExclusions[i], false) ||
                        String_StartsWith(Var.Name, S("."), false))
                    {
                        bExcludeFromConcat = true;
                        bStoreInDB = true;
                        break;
                    }
                }
            }

            // only store known keys
            for EachE(i, ReservedKeys)
            {
                String Key = ReservedKeys[i].Key;
                if (String_IsEqual(Var.Name, Key, false))
                {
                    MaxValueLength = ReservedKeys[i].MaxValueLength;
                    bStoreInDB = true;
                    break;
                }
            }

            if (bStoreInDB)
            {
                LinearAllocator Scratch = *Context.TempArena;
                String Expanded = String_Reserve(&Scratch, MaxValueLength);
                xx ExpandBuildVariable(Scratch, Context.VarListHead, Context.CmdOptionsDB, &Expanded, Var.Name, Var.Value, Var.Name, Context.WorkingDirectory, false, false, NULL);

                if (bExcludeFromConcat)
                {
                    AddVariable(PermanentArena, Context.VariablesDB, Var.Name, Expanded, Var.Params, MaxValueLength);
                }
                else
                {
                    AddOrAppendVariable(PermanentArena, Context.VariablesDB, Var.Name, Expanded, Var.Params, MaxValueLength);
                }
            }
        }

        for each (String, m, Context.Messages)
        {
            StringLocal(Expanded, 1024);
            xx ExpandBuildVariable(*Context.TempArena, Context.VarListHead, Context.CmdOptionsDB, &Expanded, String_Null(), m, String_Null(), Context.WorkingDirectory, false, false, NULL);

            // reassign to new memory, the old memory is in a temporary buffer so we dont need to worry about leaks here.
            *m_ = String_Create(PermanentArena, Expanded);
        }

        //Internal_PrintVariables(Context.VariablesDB);
    }

    return bSuccess;
}

bool ExpandBuildVariable(LinearAllocator Scratch, FileVariableList* VariablesDB, TArray(CmdOption) CmdOptionsDB,
                            String* Dest, const String Key, const String Value, const String Root, const String WorkingDirectory,
                            bool bLowerStrings, bool bIsAssemblyExe, bool* bFailed)
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
        bool bWantsPaste_Number = false;
        bool bHasNot       = false;

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

        if (C == Token_Char_Mod || C == Token_Char_Dollar || C == Token_Char_At || C == Token_Char_Not)
        {
            u32 Index = 0;

            StrVal = StrShiftF(StrVal, 1);

            bHasNot = String_EatCharInline(&StrVal, '!');
            if (bHasNot) { Offset++; }
            
            bWantsToLower = String_EatCharInline_Single(&StrVal, '-');
            if (bWantsToLower) { Offset++; }
            bWantsToUpper = String_EatCharInline_Single(&StrVal, '^');
            if (bWantsToUpper) { Offset++; }

            if (String_EatCharInline(&StrVal, '(') ||
                String_EatCharInline(&StrVal, '{'))
            {
                Offset++;

                if (String_IndexOfChar(StrVal, ')', &Index) ||
                    String_IndexOfChar(StrVal, '}', &Index))
                {
                    Offset++;
                }
            }

            if (String_EatCharInline(&StrVal, C))
            {
                Offset++;
                bWantsPaste = true; 
            }

            if (!bWantsPaste)
            {
                if (String_EatCharInline(&StrVal, '*'))
                {
                    Offset++;
                    bWantsPaste_Number = true; 
                }
            }

            if (Index == 0)
            {
                // ignore '.' and '_' at the beginning
                u8 NumEaten = 0;
                for (u32 j = 0; j < StrVal.Length; j++)
                {
                    if (StrVal.Data[j] == '.' || StrVal.Data[j] == '_')
                    {
                        NumEaten++;
                    }
                    else
                    {
                        break;
                    }
                }

                if (String_IndexOfFirstNonAlphaNumericDotUnderscore(StrShiftF(StrVal, NumEaten), &Index))
                {
                    Index += NumEaten;
                }
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

        if (C == Token_Char_Mod)
        {
            String VarValue = String_Null();

            String Trimmed = Slice;
            if (String_StartsWith(Slice, S("Option."), false))
            {
                Trimmed = StrShiftF(Slice, 7);
            }

            bool bFoundCmd = false;
            bool bEqualsToSomething = false;
            for each (CmdOption, o, CmdOptionsDB)
            {
                if (String_IsEqual(o.Name, Trimmed, false))
                {
                    bFoundCmd = true;
                    VarValue = o.Value;
                    bEqualsToSomething = o.Value.Length > 0;
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
                if (bFailed) { *bFailed = true; }
                //return false;
            }

            if (String_IsValid(VarValue))
            {
                if (bWantsPaste)
                {
                    String DestEnd = StrShiftF(*Dest, Dest->Length);
                    String_Append(Dest, Slice);
                    DestEnd.Length = Slice.Length;

                    if (bWantsToLower) { String_ToLower(&DestEnd); }
                    if (bWantsToUpper) { String_ToUpper(&DestEnd); }
                }
                else if (bWantsPaste_Number)
                {
                    String_AppendChar(Dest, '1');
                }
                else
                {
                    // if the first letter is capitalized, then also make the first letter of the value capitalized. revert back when done
                    bool bIsVarUpper = IsAlphabetUpper(Slice.Data[0]);
                    if (bIsVarUpper)
                    {
                        VarValue.Data[0] = ToUpper(VarValue.Data[0]);
                    }

                    String DestEnd = StrShiftF(*Dest, Dest->Length);
                    u32 DestLengthBefore = Dest->Length;

                    /*
                    if (!ExpandBuildVariableV2(Scratch, VariablesDB, CmdOptionsDB, Dest, Slice, VarValue, Root, WorkingDirectory, false, bIsAssemblyExe, bFailed))
                    {
                        return false;
                    }
                    */

                    String_Append(Dest, VarValue);

                    DestEnd.Length = Dest->Length - DestLengthBefore;
                    if (bWantsToLower) { String_ToLower(&DestEnd); }
                    if (bWantsToUpper) { String_ToUpper(&DestEnd); }
                }
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
        else if (C == Token_Char_Dollar)
        {
            bool bFound = false;//DoesBuildVarExist(VariablesDB, Slice);
            {
                SLinkedList_Each(FileVariableList, This, &VariablesDB)
                {
                    const FileVariable Var = (*This)->Var;

                    if (String_IsEqual(Var.Name, Slice, false))
                    {
                        bFound = true;
                        break;
                    }
                }
            }

            if (!bFound)
            {
                //LOG_WARNING("Unrecognized build variable \"%S\". Expanded to nothing...", Slice);
                if (bFailed) { *bFailed = true; }
                //return false;
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

            SLinkedList_Each(FileVariableList, This, &VariablesDB)
            {
                const FileVariable Var = (*This)->Var;

                if (String_IsEqual(Var.Name, Slice, false))
                {
                    if (NumEntries > 0)
                    {
                        if (Dest->Length > 0)
                        {
                            xx String_EatSpacesInlineFromEnd(Dest);
                            String_AppendSpace(Dest);
                        }
                    }

                    LinearAllocator Scratch2 = Scratch;
                    String TempDest = String_Reserve(&Scratch2, Dest->Capacity);
                    if (!ExpandBuildVariable(Scratch2, VariablesDB, CmdOptionsDB, &TempDest, Slice, Var.Value, Root, WorkingDirectory, bLowerStrings, bIsAssemblyExe, bFailed))
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
        else if (C == Token_Char_At)
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

                if (bFailed) { *bFailed = true; }

                return false;
                //continue;
            }

            String DestEnd = StrShiftF(*Dest, Dest->Length);
            u32 DestLengthBefore = Dest->Length;

            if (String_CountPathSeparators(VarValue) > 0 && String_CountSpaces(VarValue) > 0)
            {
                String_WrapPath(Dest, VarValue);
            }
            else
            {
                String_Append(Dest, VarValue);
            }

            /*
            if (!ExpandBuildVariable(Scratch, VariablesDB, CmdOptionsDB, Dest, Slice, VarValue, Root, WorkingDirectory, false, bIsAssemblyExe, bFailed))
            {
                return false;
            }
            */

            DestEnd.Length = Dest->Length - DestLengthBefore;
            if (bWantsToLower) { String_ToLower(&DestEnd); }
            if (bWantsToUpper) { String_ToUpper(&DestEnd); }
        }
        else if (C == Token_Char_Not && Slice.Length > 0) // run custom shell commands and append the output of the command to Dest
        {
            bool bIgnore = false;
            if (String_IsEqual(Root, S("Depends"), false))
            {
                bIgnore = true;
            }

            if (bIgnore)
            {
                String_AppendChar(Dest, C);
                String_Append(Dest, Slice);
            }
            else
            {
                StringLocal(CmdLine, 8192);

                #if PLATFORM_WINDOWS
                String_Append(&CmdLine, S("cmd.exe /c \""));
                String_Append(&CmdLine, Slice);
                String_AppendChar(&CmdLine, '"');
                #else
                String_Append(&CmdLine, Slice);
                #endif

                // TODO: time this
                PlatformPipe StdOutHandle = {0};
                PlatformHandle ShellCmd = Platform_RunCommand_Ex(CmdLine, WorkingDirectory, &StdOutHandle);
                if (Platform_IsValidHandle(ShellCmd))
                {
                    u32 ExitCode = Platform_WaitForProcessAndGetExitCode(ShellCmd);
                    if (ExitCode == 0)
                    {
                        StringLocal(StdOutData, 8192);
                        usize BytesRead = 0;
                        if (!Filesystem_ReadPipe(StdOutHandle, StdOutData.Capacity, StdOutData.Data, &BytesRead))
                        {
                            LOG_ERROR("Failed to read from standard output pipe for command -> \"%S\"", Slice);
                            return false;
                        }

                        StdOutData.Length = Min((u32)BytesRead, StdOutData.Capacity);
                        xx String_EatNewLinesInlineFromEnd(&StdOutData);

                        String DestEnd = StrShiftF(*Dest, Dest->Length);
                        u32 DestLengthBefore = Dest->Length;

                        String_Append(Dest, StdOutData);
                        DestEnd.Length = Dest->Length - DestLengthBefore;

                        if (bWantsToLower) { String_ToLower(&DestEnd); }
                        if (bWantsToUpper) { String_ToUpper(&DestEnd); }

                        Platform_CloseHandle(StdOutHandle[0]);
                        Platform_CloseHandle(StdOutHandle[1]);
                    }
                }
            }
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
                    local_persist const String KeysToCareAbout[17] = 
                    {
                        SC("SourceDirectory"),
                        SC("BuildDirectory"),
                        SC("IntermediateDirectory"),
                        SC("Library.Paths"),
                        SC("Includes"),
                        SC("Icon"),
                        SC("Compiler"),
                        SC("PCH"),
                        SC("PCH.h"),
                        SC("SourceDirectories"),
                        SC("SourceDirectories.Exclude"),
                        SC("SourceFiles"),
                        SC("SourceFiles.Exclude"),
                        SC(".RPathOrigin"),
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

    xx String_EatSpacesInlineFromEnd(Dest);

    return true;
}
