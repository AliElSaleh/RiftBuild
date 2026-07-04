#include <stdio.h>
#include "config.h"

int main(void)
{
    printf("%s\n", GREETING);

    for (int Index = 0; Index < MAX_ITEMS; Index++)
    {
        printf("item %d of %d\n", Index + 1, MAX_ITEMS);
    }

    return 0;
}
