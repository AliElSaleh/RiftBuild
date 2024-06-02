#pragma once

#include "EngineTypes.h"
#include "Memory/Memory.h"

u64 MemoryUtils_CalculatePaddingWithHeader(u64 Ptr, u64 Alignment, u64 HeaderSize);
u64 GetAligned(u64 Operand, u64 Granularity);
MemoryRange GetAlignedRange(u64 Offset, u64 Size, u64 Granularity);
