#include "b.h"
#include "c.h" // available transitively via C's Includes.Public

int b_compute(int n)
{
    return c_add(n, c_square(n));
}
