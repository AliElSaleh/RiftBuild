#include <stdio.h>
#include "greeter.h"

void Greeter_Hello(const char* Name)
{
    printf("hello, %s! (from inside Greeter.dll)\n", Name);
}

int Greeter_Version(void)
{
    return 1;
}
