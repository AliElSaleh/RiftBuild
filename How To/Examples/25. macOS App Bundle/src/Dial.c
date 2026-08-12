#include "Dial.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static DialColor MakeColor(int Red, int Green, int Blue)
{
    DialColor Color;
    Color.Red   = Red   / 255.0;
    Color.Green = Green / 255.0;
    Color.Blue  = Blue  / 255.0;

    return Color;
}

void Dial_DefaultTheme(DialTheme* Theme)
{
    strcpy(Theme->Title, "Sundial");
    Theme->Radius     = 0.38;
    Theme->Background = MakeColor(0x1B, 0x1A, 0x17);
    Theme->Face       = MakeColor(0xEC, 0xDF, 0xC2);
    Theme->Rim        = MakeColor(0x8A, 0x6F, 0x3C);
    Theme->Mark       = MakeColor(0x4A, 0x3C, 0x24);
    Theme->Shadow     = MakeColor(0x2B, 0x21, 0x18);
    Theme->Gnomon     = MakeColor(0xC2, 0x70, 0x3D);
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

static int HexDigit(char Character)
{
    int Value = -1;

    if (Character >= '0' && Character <= '9')
    {
        Value = Character - '0';
    }
    else if (Character >= 'a' && Character <= 'f')
    {
        Value = Character - 'a' + 10;
    }
    else if (Character >= 'A' && Character <= 'F')
    {
        Value = Character - 'A' + 10;
    }

    return Value;
}

/* "#RRGGBB" or "RRGGBB" -> Out. Returns 0 and leaves Out alone if the text is
   not six hex digits. */
static int ParseColor(const char* Text, DialColor* Out)
{
    int bSuccess = 0;

    if (*Text == '#')
    {
        Text++;
    }

    if (strlen(Text) == 6)
    {
        int Channels[3];
        bSuccess = 1;

        for (int i = 0; i < 3 && bSuccess; i++)
        {
            int High = HexDigit(Text[2 * i]);
            int Low  = HexDigit(Text[2 * i + 1]);

            if (High < 0 || Low < 0)
            {
                bSuccess = 0;
            }
            else
            {
                Channels[i] = High * 16 + Low;
            }
        }

        if (bSuccess)
        {
            *Out = MakeColor(Channels[0], Channels[1], Channels[2]);
        }
    }

    return bSuccess;
}

/* One "key = <colour>" line. Returns 0 when the key is not this one, so the
   caller can keep looking. */
static int ApplyColorKey(const char* Key, const char* Value, const char* Name, DialColor* Target, int* Applied)
{
    int bMatched = strcmp(Key, Name) == 0;

    if (bMatched)
    {
        if (ParseColor(Value, Target))
        {
            (*Applied)++;
        }
        else
        {
            fprintf(stderr, "theme.conf: %s = \"%s\" is not a #RRGGBB colour, keeping the default\n", Name, Value);
        }
    }

    return bMatched;
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
                double Radius = atof(Value);
                if (Radius >= 0.2 && Radius <= 0.5)
                {
                    Theme->Radius = Radius;
                    Applied++;
                }
                else
                {
                    fprintf(stderr, "theme.conf: radius %s is outside 0.2..0.5, keeping %.2f\n", Value, Theme->Radius);
                }
            }
            else if (!ApplyColorKey(Key, Value, "background", &Theme->Background, &Applied) &&
                     !ApplyColorKey(Key, Value, "face",       &Theme->Face,       &Applied) &&
                     !ApplyColorKey(Key, Value, "rim",        &Theme->Rim,        &Applied) &&
                     !ApplyColorKey(Key, Value, "mark",       &Theme->Mark,       &Applied) &&
                     !ApplyColorKey(Key, Value, "shadow",     &Theme->Shadow,     &Applied) &&
                     !ApplyColorKey(Key, Value, "gnomon",     &Theme->Gnomon,     &Applied))
            {
                fprintf(stderr, "theme.conf: unknown key \"%s\", ignoring it\n", Key);
            }
        }

        Line = NextLine;
    }

    return Applied;
}

const char* Dial_LineAt(const char* Text, int Index, int* OutLength)
{
    const char* Found = NULL;
    const char* Line = Text;
    int Current = 0;

    while (Line != NULL && *Line != '\0' && Found == NULL)
    {
        const char* LineEnd = strchr(Line, '\n');
        size_t LineLength = LineEnd != NULL ? (size_t)(LineEnd - Line) : strlen(Line);

        if (LineLength > 0 && Line[LineLength - 1] == '\r')
        {
            LineLength--;
        }

        if (Current == Index)
        {
            Found = Line;
            *OutLength = (int)LineLength;
        }

        Line = LineEnd != NULL ? LineEnd + 1 : NULL;
        Current++;
    }

    return Found;
}
