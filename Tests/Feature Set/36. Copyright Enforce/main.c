// Copyright (c) RiftBuild Tests

#include <stdio.h>

extern int helper_ok(void);

int main(void)
{
    int Result = 1;

    if (helper_ok() == 1)
    {
        printf("OK copyright enforce\n");
        Result = 0;
    }

    return Result;
}
