#pragma once

/* The dial's data model: a theme read out of theme.conf and the text files
   the window draws. Deliberately plain C - nothing in here knows about
   Cocoa, and nothing in DialView.m knows about parsing. */

typedef struct
{
    double Red;
    double Green;
    double Blue;
} DialColor;

typedef struct
{
    char      Title[64];  /* shown above the dial                       */
    double    Radius;     /* fraction of the window's shorter side      */
    DialColor Background; /* behind everything                          */
    DialColor Face;       /* the disc                                   */
    DialColor Rim;        /* the ring around it                         */
    DialColor Mark;       /* hour ticks and numerals                    */
    DialColor Shadow;     /* the gnomon's shadow                        */
    DialColor Gnomon;     /* the pin in the middle                      */
} DialTheme;

/* Every field filled in with something sensible, so a theme file only has to
   mention the keys it wants to change. */
void Dial_DefaultTheme(DialTheme* Theme);

/* Applies a "key = value" resource file on top of Theme. Blank lines and
   lines starting with '#' are ignored; an unrecognised key is reported on
   stderr and skipped. Returns the number of keys applied.

   Colour values are "#RRGGBB" (the '#' is optional). Warnings printed here
   land in Console.app when the app is launched from Finder, and in the
   terminal when it is not. */
int Dial_ApplyTheme(const char* Text, DialTheme* Theme);

/* Line Index (0-based) of a text resource, or NULL if the file is too short.
   The result points into Text and is not NUL-terminated - OutLength has it.
   Used for both the hour numerals and the hourly mottos. */
const char* Dial_LineAt(const char* Text, int Index, int* OutLength);
