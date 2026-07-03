#include <stdio.h>

extern int util_value(void);
extern int nested_value(void);

int main(void)
{
    int Result = 1;

    if (util_value() == 10 && nested_value() == 20)
    {
        printf("OK auto source discovery\n");
        Result = 0;
    }
    else
    {
        printf("FAIL auto source discovery: wrong values\n");
    }

    return Result;
}
