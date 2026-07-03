#include <stdio.h>

int main(int ArgCount, char** Args)
{
    int Result = 1;

    FILE* File = fopen("run_args.txt", "w");
    if (File)
    {
        int i;
        for (i = 1; i < ArgCount; i++)
        {
            fprintf(File, "%s%s", (i > 1) ? " " : "", Args[i]);
        }
        fclose(File);

        printf("OK run after build\n");
        Result = 0;
    }

    return Result;
}
