#pragma once

typedef struct
{
    double      Value;
    int         bOk;   /* 1 on success, 0 on failure */
    const char* Error; /* set when bOk is 0 */
} EvalResult;

EvalResult Evaluate(const char* Expression);
