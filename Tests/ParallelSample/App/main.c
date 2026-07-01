#include <stdio.h>
#include "b.h" // available transitively via B's Includes.Public

int app_helper(void); // from app_extra.c

int main(void)
{
    int result = b_compute(5) + b_double(3) + app_helper();
    printf("SampleApp result: %d\n", result);
    return 0;
}
