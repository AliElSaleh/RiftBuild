#include "lexer.h"
#include <stdlib.h>

void Lexer_Init(Lexer* Lex, const char* Text)
{
    Lex->Text = Text;
    Lex->Position = 0;
}

Token Lexer_Next(Lexer* Lex)
{
    Token Result;
    Result.Type = Token_Error;
    Result.Value = 0.0;

    while (Lex->Text[Lex->Position] == ' ' || Lex->Text[Lex->Position] == '\t')
    {
        Lex->Position++;
    }

    char C = Lex->Text[Lex->Position];

    if (C == '\0')
    {
        Result.Type = Token_End;
    }
    else if ((C >= '0' && C <= '9') || C == '.')
    {
        char* End = NULL;
        Result.Type = Token_Number;
        Result.Value = strtod(Lex->Text + Lex->Position, &End);
        Lex->Position = (int)(End - Lex->Text);
    }
    else
    {
        switch (C)
        {
            case '+': Result.Type = Token_Plus;       break;
            case '-': Result.Type = Token_Minus;      break;
            case '*': Result.Type = Token_Star;       break;
            case '/': Result.Type = Token_Slash;      break;
            case '(': Result.Type = Token_OpenParen;  break;
            case ')': Result.Type = Token_CloseParen; break;
            default:  Result.Type = Token_Error;      break;
        }

        Lex->Position++;
    }

    return Result;
}
