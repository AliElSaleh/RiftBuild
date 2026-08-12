#include "Dial.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DIAL_MAX_WIDTH  (4 * DIAL_MAX_RADIUS + 2)
#define DIAL_MAX_HEIGHT (2 * DIAL_MAX_RADIUS + 1)

void Dial_DefaultTheme(DialTheme* Theme)
{
    strcpy(Theme->Title, "Sundial");
    Theme->Radius = 8;
    Theme->Face   = '.';
    Theme->Mark   = 'o';
    Theme->Shadow = '#';
    Theme->Gnomon = '+';
}

/* Trims spaces and tabs off both ends of a slice, in place. */
static char* TrimSlice(char* Text, size_t Length)
{
    size_t Start = 0;
    while (Start < Length && (Text[Start] == ' ' || Text[Start] == '\t'))
    {
        Start++;
    }

    size_t End = Length;
    while (End > Start && (Text[End - 1] == ' ' || Text[End - 1] == '\t' || Text[End - 1] == '\r'))
    {
        End--;
    }

    Text[End] = '\0';

    return Text + Start;
}

int Dial_ApplyTheme(const char* Text, DialTheme* Theme)
{
    int Applied = 0;

    /* A copy, because parsing chops the buffer up in place. */
    char Buffer[4096];
    size_t Length = strlen(Text);
    if (Length >= sizeof(Buffer))
    {
        Length = sizeof(Buffer) - 1;
    }

    memcpy(Buffer, Text, Length);
    Buffer[Length] = '\0';

    char* Line = Buffer;
    while (Line != NULL && *Line != '\0')
    {
        char* NextLine = strchr(Line, '\n');
        size_t LineLength = NextLine != NULL ? (size_t)(NextLine - Line) : strlen(Line);
        if (NextLine != NULL)
        {
            NextLine++;
        }

        char* Trimmed = TrimSlice(Line, LineLength);
        char* Separator = strchr(Trimmed, '=');

        if (*Trimmed != '\0' && *Trimmed != '#' && Separator != NULL)
        {
            char* Key = TrimSlice(Trimmed, (size_t)(Separator - Trimmed));
            char* Value = TrimSlice(Separator + 1, strlen(Separator + 1));

            if (Value[0] == '\0')
            {
                fprintf(stderr, "theme.conf: \"%s\" has no value, ignoring it\n", Key);
            }
            else if (strcmp(Key, "title") == 0)
            {
                strncpy(Theme->Title, Value, sizeof(Theme->Title) - 1);
                Theme->Title[sizeof(Theme->Title) - 1] = '\0';
                Applied++;
            }
            else if (strcmp(Key, "radius") == 0)
            {
                int Radius = atoi(Value);
                if (Radius >= 4 && Radius <= DIAL_MAX_RADIUS)
                {
                    Theme->Radius = Radius;
                    Applied++;
                }
                else
                {
                    fprintf(stderr, "theme.conf: radius %s is outside 4..%d, keeping %d\n",
                            Value, DIAL_MAX_RADIUS, Theme->Radius);
                }
            }
            else if (strcmp(Key, "face") == 0)   { Theme->Face   = Value[0]; Applied++; }
            else if (strcmp(Key, "mark") == 0)   { Theme->Mark   = Value[0]; Applied++; }
            else if (strcmp(Key, "shadow") == 0) { Theme->Shadow = Value[0]; Applied++; }
            else if (strcmp(Key, "gnomon") == 0) { Theme->Gnomon = Value[0]; Applied++; }
            else
            {
                fprintf(stderr, "theme.conf: unknown key \"%s\", ignoring it\n", Key);
            }
        }

        Line = NextLine;
    }

    return Applied;
}

static void Plot(char Canvas[DIAL_MAX_HEIGHT][DIAL_MAX_WIDTH], int Width, int Height, double X, double Y, char Glyph)
{
    int Column = (int)floor(X + 0.5);
    int Row = (int)floor(Y + 0.5);

    if (Column >= 0 && Column < Width && Row >= 0 && Row < Height)
    {
        Canvas[Row][Column] = Glyph;
    }
}

void Dial_Print(const DialTheme* Theme, const char* Version, int Hour, int Minute)
{
    int Radius = Theme->Radius;
    if (Radius < 4)
    {
        Radius = 4;
    }
    if (Radius > DIAL_MAX_RADIUS)
    {
        Radius = DIAL_MAX_RADIUS;
    }

    /* Character cells are about twice as tall as they are wide, so every X
       coordinate is doubled to keep the dial round. */
    int Width = 4 * Radius + 1;
    int Height = 2 * Radius + 1;
    double CenterX = 2.0 * Radius;
    double CenterY = Radius;

    char Canvas[DIAL_MAX_HEIGHT][DIAL_MAX_WIDTH];
    for (int Row = 0; Row < Height; Row++)
    {
        memset(Canvas[Row], ' ', (size_t)Width);
        Canvas[Row][Width] = '\0';
    }

    /* The rim. */
    for (int Row = 0; Row < Height; Row++)
    {
        for (int Column = 0; Column < Width; Column++)
        {
            double X = (Column - CenterX) / 2.0;
            double Y = Row - CenterY;
            double Distance = sqrt(X * X + Y * Y);

            if (Distance > Radius - 0.5 && Distance < Radius + 0.5)
            {
                Canvas[Row][Column] = Theme->Face;
            }
        }
    }

    /* The twelve hour marks, 12 o'clock at the top and going clockwise. */
    for (int Mark = 0; Mark < 12; Mark++)
    {
        double Angle = Mark * (2.0 * 3.14159265358979323846 / 12.0);
        Plot(Canvas, Width, Height,
             CenterX + 2.0 * Radius * sin(Angle),
             CenterY - Radius * cos(Angle),
             Theme->Mark);
    }

    /* The shadow, walked outwards from the centre towards the current time. */
    double TimeAngle = ((Hour % 12) + Minute / 60.0) * (2.0 * 3.14159265358979323846 / 12.0);
    for (double Length = 1.0; Length <= Radius - 1.2; Length += 0.2)
    {
        Plot(Canvas, Width, Height,
             CenterX + 2.0 * Length * sin(TimeAngle),
             CenterY - Length * cos(TimeAngle),
             Theme->Shadow);
    }

    Plot(Canvas, Width, Height, CenterX, CenterY, Theme->Gnomon);

    printf("  %s %s - %02d:%02d\n\n", Theme->Title, Version, Hour, Minute);

    for (int Row = 0; Row < Height; Row++)
    {
        int End = Width;
        while (End > 0 && Canvas[Row][End - 1] == ' ')
        {
            End--;
        }

        Canvas[Row][End] = '\0';
        printf("  %s\n", Canvas[Row]);
    }
}

const char* Dial_MottoForHour(const char* Text, int Hour, int* OutLength)
{
    const char* Motto = NULL;
    const char* Line = Text;
    int Index = 0;

    while (Line != NULL && *Line != '\0' && Motto == NULL)
    {
        const char* LineEnd = strchr(Line, '\n');
        size_t LineLength = LineEnd != NULL ? (size_t)(LineEnd - Line) : strlen(Line);

        if (LineLength > 0 && Line[LineLength - 1] == '\r')
        {
            LineLength--;
        }

        if (Index == Hour)
        {
            Motto = Line;
            *OutLength = (int)LineLength;
        }

        Line = LineEnd != NULL ? LineEnd + 1 : NULL;
        Index++;
    }

    return Motto;
}
