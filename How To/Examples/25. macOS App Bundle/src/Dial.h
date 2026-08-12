#pragma once

#define DIAL_MAX_RADIUS 20

typedef struct
{
    char Title[64]; /* printed above the dial              */
    int  Radius;    /* dial radius, in character cells     */
    char Face;      /* the rim                             */
    char Mark;      /* the twelve hour marks               */
    char Shadow;    /* the gnomon's shadow                 */
    char Gnomon;    /* the pin in the middle               */
} DialTheme;

/* Every field filled in with something sensible, so a theme file only has to
   mention the keys it wants to change. */
void Dial_DefaultTheme(DialTheme* Theme);

/* Applies a "key = value" resource file on top of Theme. Blank lines and
   lines starting with '#' are ignored; an unrecognised key is reported on
   stderr and skipped. Returns the number of keys applied. */
int Dial_ApplyTheme(const char* Text, DialTheme* Theme);

/* Prints the dial with the shadow pointing at Hour:Minute. */
void Dial_Print(const DialTheme* Theme, const char* Version, int Hour, int Minute);

/* Line Hour (0-23) of a mottos resource, or NULL if the file is too short.
   The result points into Text and is not NUL-terminated - OutLength has it. */
const char* Dial_MottoForHour(const char* Text, int Hour, int* OutLength);
