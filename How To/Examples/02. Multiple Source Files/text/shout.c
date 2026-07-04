#include "shout.h"

void Shout(const char* Text, char* Buffer, int BufferSize)
{
    int Index = 0;

    while (Text[Index] != '\0' && Index < BufferSize - 2)
    {
        char C = Text[Index];
        if (C >= 'a' && C <= 'z')
        {
            C = (char)(C - 32);
        }
        Buffer[Index] = C;
        Index++;
    }

    Buffer[Index] = '!';
    Buffer[Index + 1] = '\0';
}
