#include <stdio.h>
#include "greet.h"
#include "text/shout.h"

int main(void)
{
    char Buffer[64];
    Shout(Greeting(), Buffer, sizeof(Buffer));
    printf("%s\n", Buffer);
    return 0;
}
