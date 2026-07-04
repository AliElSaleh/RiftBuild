#include <stdio.h>

#if defined(HAVE_IMAGINARY_TOOL)
    #error "zz_no_such_tool_zz cannot possibly exist on this machine"
#endif

int main(void)
{
#if defined(HAVE_GIT)
    printf("git found - version stamping could be enabled\n");
#else
    printf("no git on PATH - building without version stamping\n");
#endif

    return 0;
}
