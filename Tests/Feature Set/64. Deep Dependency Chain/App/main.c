#include <stdio.h>

int Level01(void); // every level adds its own number and calls the next one

int main(void)
{
    const int Expected = 78; // 1 + 2 + ... + 12
    const int Total = Level01();

    if (Total != Expected)
    {
        printf("FAIL Deep Dependency Chain: total %d, expected %d\n", Total, Expected);
        return 1;
    }

    printf("OK Deep Dependency Chain: 12 levels, total %d\n", Total);
    return 0;
}
