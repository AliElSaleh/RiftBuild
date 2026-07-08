#include <stdio.h>
#include "src/parser.h"

int main(int ArgCount, char** Args)
{
    int ExitCode = 0;

    if (ArgCount < 2)
    {
        printf("usage: calc \"<expression>\" ...\n");
        printf("example: calc \"(1 + 2) * 3.5 - 4 / 2\"\n");
        ExitCode = 1;
    }
    else
    {
        for (int i = 1; i < ArgCount; i++)
        {
            EvalResult Result = Evaluate(Args[i]);

            if (Result.bOk)
            {
                printf("%s = %g\n", Args[i], Result.Value);
            }
            else
            {
                printf("%s = error: %s\n", Args[i], Result.Error);
                ExitCode = 1;
            }
        }
    }

    return ExitCode;
}
