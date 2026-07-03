#include <stdio.h>

extern int part_value_1(void);
extern int part_value_2(void);
extern int part_value_3(void);
extern int part_value_4(void);
extern int part_value_5(void);

int main(void)
{
    int Result = 1;
    int Total = part_value_1() + part_value_2() + part_value_3() + part_value_4() + part_value_5();

    if (Total == 15)
    {
        printf("OK max cores\n");
        Result = 0;
    }
    else
    {
        printf("FAIL max cores: total=%d\n", Total);
    }

    return Result;
}
