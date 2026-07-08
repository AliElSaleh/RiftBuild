/* A recursive descent parser that evaluates as it goes:

       Expression = Term   (('+' | '-') Term)*
       Term       = Factor (('*' | '/') Factor)*
       Factor     = Number | '-' Factor | '(' Expression ')'
*/

#include "parser.h"
#include "lexer.h"
#include "math/ops.h"
#include <stddef.h>

typedef struct
{
    Lexer       Lex;
    Token       Current;
    const char* Error;
} Parser;

static void Advance(Parser* P)
{
    if (P->Error == NULL)
    {
        P->Current = Lexer_Next(&P->Lex);

        if (P->Current.Type == Token_Error)
        {
            P->Error = "unexpected character";
        }
    }
}

static double ParseExpression(Parser* P);

static double ParseFactor(Parser* P)
{
    double Result = 0.0;

    if (P->Error == NULL)
    {
        if (P->Current.Type == Token_Number)
        {
            Result = P->Current.Value;
            Advance(P);
        }
        else if (P->Current.Type == Token_Minus)
        {
            Advance(P);
            Result = Ops_Negate(ParseFactor(P));
        }
        else if (P->Current.Type == Token_OpenParen)
        {
            Advance(P);
            Result = ParseExpression(P);

            if (P->Error == NULL)
            {
                if (P->Current.Type == Token_CloseParen)
                {
                    Advance(P);
                }
                else
                {
                    P->Error = "expected ')'";
                }
            }
        }
        else
        {
            P->Error = "expected a number, '-' or '('";
        }
    }

    return Result;
}

static double ParseTerm(Parser* P)
{
    double Result = ParseFactor(P);

    while (P->Error == NULL && (P->Current.Type == Token_Star || P->Current.Type == Token_Slash))
    {
        TokenType Op = P->Current.Type;
        Advance(P);

        double Right = ParseFactor(P);

        if (P->Error == NULL)
        {
            if (Op == Token_Star)
            {
                Result = Ops_Multiply(Result, Right);
            }
            else if (!Ops_Divide(Result, Right, &Result))
            {
                P->Error = "division by zero";
            }
        }
    }

    return Result;
}

static double ParseExpression(Parser* P)
{
    double Result = ParseTerm(P);

    while (P->Error == NULL && (P->Current.Type == Token_Plus || P->Current.Type == Token_Minus))
    {
        TokenType Op = P->Current.Type;
        Advance(P);

        double Right = ParseTerm(P);

        if (P->Error == NULL)
        {
            if (Op == Token_Plus)
            {
                Result = Ops_Add(Result, Right);
            }
            else
            {
                Result = Ops_Subtract(Result, Right);
            }
        }
    }

    return Result;
}

EvalResult Evaluate(const char* Expression)
{
    Parser P;
    P.Error = NULL;
    Lexer_Init(&P.Lex, Expression);
    Advance(&P);

    EvalResult Result;
    Result.Value = ParseExpression(&P);

    if (P.Error == NULL && P.Current.Type != Token_End)
    {
        P.Error = "unexpected trailing input";
    }

    Result.bOk = P.Error == NULL;
    Result.Error = P.Error;

    if (!Result.bOk)
    {
        Result.Value = 0.0;
    }

    return Result;
}
