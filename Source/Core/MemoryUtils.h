#ifndef _MEMORYUTILS_H
#define _MEMORYUTILS_H

#ifndef UNITY_BUILD
#include "Memory.h"
#endif

usize MemoryUtils_CalculatePaddingWithHeader(usize Ptr, usize Alignment, usize HeaderSize);
usize GetAligned(usize Operand, usize Granularity);
MemoryRange GetAlignedRange(usize Offset, usize Size, usize Granularity);

#endif // _MEMORYUTILS_H
