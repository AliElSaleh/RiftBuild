/* Seven-segment digit rendering built entirely out of Render_Rect - no font
   files, no textures. Segment layout and the bit assigned to each:

        _a_          a = 1   b = 2   c = 4   d = 8
      f|   |b        e = 16  f = 32  g = 64
       |_g_|
      e|   |c
       |_d_|
*/

#include "Engine.h"

static const int SegmentsForDigit[10] =
{
    0x3F, /* 0: a b c d e f */
    0x06, /* 1: b c         */
    0x5B, /* 2: a b d e g   */
    0x4F, /* 3: a b c d g   */
    0x66, /* 4: b c f g     */
    0x6D, /* 5: a c d f g   */
    0x7D, /* 6: a c d e f g */
    0x07, /* 7: a b c       */
    0x7F, /* 8: all         */
    0x6F  /* 9: a b c d f g */
};

static void RenderDigit(float X, float Y, float Size, int Digit, float R, float G, float B)
{
    float W = Size * 0.55f;
    float T = Size * 0.12f;
    float Half = Size * 0.5f;

    int Segments = SegmentsForDigit[Digit];

    if (Segments & 1)  { Render_Rect(X,         Y,              W, T,    R, G, B); } /* a */
    if (Segments & 2)  { Render_Rect(X + W - T, Y,              T, Half, R, G, B); } /* b */
    if (Segments & 4)  { Render_Rect(X + W - T, Y + Half,       T, Half, R, G, B); } /* c */
    if (Segments & 8)  { Render_Rect(X,         Y + Size - T,   W, T,    R, G, B); } /* d */
    if (Segments & 16) { Render_Rect(X,         Y + Half,       T, Half, R, G, B); } /* e */
    if (Segments & 32) { Render_Rect(X,         Y,              T, Half, R, G, B); } /* f */
    if (Segments & 64) { Render_Rect(X, Y + Half - T * 0.5f,    W, T,    R, G, B); } /* g */
}

static int CountDigits(int Value)
{
    int Count = 1;

    while (Value >= 10)
    {
        Value /= 10;
        Count++;
    }

    return Count;
}

float Render_NumberWidth(float Size, int Value)
{
    if (Value < 0)
    {
        Value = 0;
    }

    return (float)CountDigits(Value) * Size * 0.8f - Size * 0.25f;
}

void Render_Number(float X, float Y, float Size, int Value, float R, float G, float B)
{
    if (Value < 0)
    {
        Value = 0;
    }

    int NumDigits = CountDigits(Value);
    float Advance = Size * 0.8f;

    for (int i = NumDigits - 1; i >= 0; i--)
    {
        RenderDigit(X + (float)i * Advance, Y, Size, Value % 10, R, G, B);
        Value /= 10;
    }
}
