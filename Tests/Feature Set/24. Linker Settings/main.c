#include <stdio.h>

/* Uses a chunk of stack that would be risky with a tiny stack reservation but
   is trivially fine with the 16MB requested via Linker.Stack. */
static int sum_big_buffer(void)
{
    volatile char Buffer[2 * 1024 * 1024];
    int Total = 0;

    Buffer[0] = 1;
    Buffer[sizeof(Buffer) - 1] = 2;
    Total = Buffer[0] + Buffer[sizeof(Buffer) - 1];

    return Total;
}

int main(void)
{
    int Result = 1;

    if (sum_big_buffer() == 3)
    {
        printf("OK linker settings\n");
        Result = 0;
    }
    else
    {
        printf("FAIL linker settings\n");
    }

    return Result;
}
