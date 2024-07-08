#pragma once

#include "EngineTypes.h"
#include "Memory/Memory.h"

usize MemoryUtils_CalculatePaddingWithHeader(usize Ptr, usize Alignment, usize HeaderSize);
usize GetAligned(usize Operand, usize Granularity);
MemoryRange GetAlignedRange(usize Offset, usize Size, usize Granularity);
