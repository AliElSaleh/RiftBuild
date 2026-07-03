#include <stdio.h>

extern int add_numbers(int A, int B);

int main(void)
{
    int Result = 1;

    if (add_numbers(30, 12) == 42)
    {
        printf("OK nasm assembly\n");
        Result = 0;
    }
    else
    {
        printf("FAIL nasm assembly\n");
    }

    return Result;
}
