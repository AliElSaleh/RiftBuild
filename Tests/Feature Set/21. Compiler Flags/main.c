#include <stdio.h>

#if FROM_GLOBAL_FLAG != 3
    #error "Compiler.Flags -D did not reach main.c"
#endif

#if FROM_FILE_FLAG != 4
    #error "main.c.Compiler.Flags -D did not reach main.c"
#endif

extern int other_value(void);

int main(void)
{
    int Result = 1;

    if (other_value() == 3)
    {
        printf("OK compiler flags\n");
        Result = 0;
    }
    else
    {
        printf("FAIL compiler flags\n");
    }

    return Result;
}
