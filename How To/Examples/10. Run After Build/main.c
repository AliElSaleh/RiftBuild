#include <stdio.h>

int main(int ArgC, char** ArgV)
{
    printf("Echo ran with %d argument(s):\n", ArgC - 1);

    for (int Index = 1; Index < ArgC; Index++)
    {
        printf("  [%d] %s\n", Index, ArgV[Index]);
    }

    return 0;
}
