#include "Bundle.h"
#include "Dial.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/* A sundial that cannot run without its resources: the banner, the theme and
   the mottos all live in Contents/Resources inside the .app, and the program
   finds them from its own executable path (see Bundle.c). */
int main(void)
{
    int ExitCode = 1;

    char ResourceDirectory[BUNDLE_PATH_MAX];
    if (!Bundle_FindResourceDirectory(ResourceDirectory, sizeof(ResourceDirectory)))
    {
        fprintf(stderr, "Sundial: the OS would not say where I am.\n");
    }
    else
    {
        char* Banner = Bundle_ReadTextFile(ResourceDirectory, "dial.txt");
        char* ThemeText = Bundle_ReadTextFile(ResourceDirectory, "theme.conf");
        char* Mottos = Bundle_ReadTextFile(ResourceDirectory, "mottos.txt");

        if (Banner == NULL || ThemeText == NULL || Mottos == NULL)
        {
            fprintf(stderr,
                    "Sundial: missing resources in\n  %s\n"
                    "The bundle is incomplete - build again to stage them.\n",
                    ResourceDirectory);
        }
        else
        {
            DialTheme Theme;
            Dial_DefaultTheme(&Theme);
            Dial_ApplyTheme(ThemeText, &Theme);

            time_t Now = time(NULL);
            struct tm* Local = localtime(&Now);

            printf("%s\n", Banner);

            /* Version(define) in the build file put this macro here, and the
               same number is CFBundleShortVersionString in Info.plist. */
            Dial_Print(&Theme, SUNDIAL_VERSION_STRING, Local->tm_hour, Local->tm_min);

            int MottoLength = 0;
            const char* Motto = Dial_MottoForHour(Mottos, Local->tm_hour, &MottoLength);
            if (Motto != NULL)
            {
                printf("\n  %.*s\n", MottoLength, Motto);
            }

            printf("\n  (resources read from %s)\n", ResourceDirectory);

            ExitCode = 0;
        }

        free(Banner);
        free(ThemeText);
        free(Mottos);
    }

    return ExitCode;
}
