#include <stdio.h>
#include "mathlib.h"

int main(void)
{
    int Sum = Math_Add(19, 23);
    int Product = Math_Mul(6, 7);

    printf("19 + 23 = %d\n", Sum);
    printf(" 6 *  7 = %d\n", Product);

    return 0;
}
