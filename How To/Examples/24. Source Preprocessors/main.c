#include <stdio.h>

// The printf calls below use string interpolation - not a C feature. The
// PreCompileAllFiles hook in this folder's .build rewrites them into plain
// printf("%d", x) style calls before the compiler sees this file. Build
// once, then reopen this file to see the rewritten result.

void PrintStats(void);

int main(void)
{
    const char* level  = "Dungeon of Eternal Linking";
    int         width  = 1920;
    int         height = 1080;
    float       scale  = 1.5f;

    printf("loading {level}...\n");
    printf("viewport: {width} x {height} (ui scale {scale})\n");

    PrintStats();
    return 0;
}
