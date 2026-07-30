#include <stdio.h>
#include <string.h>

int main(void)
{
    // '&' must reach built-ins in both stores
    if (AMP_PLATFORM[0] == 0 || AMP_DATE[0] == 0 || AMP_FOLDER[0] == 0)
    {
        printf("FAIL internal var sigil: a built-in expanded to nothing\n");
        return 1;
    }

    // the folder built-in lives in the option store, so this proves '&' searches both
    if (strcmp(AMP_FOLDER, "60. Internal Var Sigil") != 0)
    {
        printf("FAIL internal var sigil: &FolderName was [%s]\n", AMP_FOLDER);
        return 1;
    }

    // the case modifiers act on the pasted copy
    if (strcmp(AMP_UPPER, AMP_LOWER) == 0)
    {
        printf("FAIL internal var sigil: &^Name and &-Name produced the same text\n");
        return 1;
    }

    // "a && b" must survive untouched - a bare ampersand is not a sigil
    if (strcmp(AMP_LITERAL, "a && b") != 0)
    {
        printf("FAIL internal var sigil: literal ampersand was eaten -> [%s]\n", AMP_LITERAL);
        return 1;
    }

    printf("OK internal var sigil\n");
    return 0;
}
