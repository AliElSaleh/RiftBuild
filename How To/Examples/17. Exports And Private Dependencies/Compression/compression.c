#include "compression.h"

int Compression_Bound(int InputSize)
{
    return InputSize + (InputSize / 8) + 64;
}
