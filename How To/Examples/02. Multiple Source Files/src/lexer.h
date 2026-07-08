#pragma once

typedef enum
{
    Token_Number,
    Token_Plus,
    Token_Minus,
    Token_Star,
    Token_Slash,
    Token_OpenParen,
    Token_CloseParen,
    Token_End,
    Token_Error
} TokenType;

typedef struct
{
    TokenType Type;
    double    Value; /* only set for Token_Number */
} Token;

typedef struct
{
    const char* Text;
    int         Position;
} Lexer;

void  Lexer_Init(Lexer* Lex, const char* Text);
Token Lexer_Next(Lexer* Lex);
