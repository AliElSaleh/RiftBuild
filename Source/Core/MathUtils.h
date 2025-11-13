#ifndef MATH_H
#define MATH_H

#include "EngineTypes.h"

FORCEINLINE NO_DISCARD static f32 Abs(f32 Value)
{
	i32 Temp = *((i32*)&Value);
	Temp &= 0x7FFFFFFF;
	return *(f32*)&Temp;
}

FORCEINLINE NO_DISCARD static f64 Absf64(f64 Value)
{
	i64 Temp = *((i64*)&Value);
	Temp &= 0x7FFFFFFFFFFFFFFF;
	return *(f64*)&Temp;
}

FORCEINLINE NO_DISCARD static i32 Absi32(i32 Value)
{
	i32 Temp = (i32)Value >> 31;
	Value ^= Temp;
	Value += Temp & 1;
	return Value;
}

FORCEINLINE NO_DISCARD static i64 Absi64(i64 Value)
{
	i64 Temp = (i64)Value >> 63;
	Value ^= Temp;
	Value += Temp & 1;
	return Value;
}

#define DECLARE_MinMax(Type, Suffix) \
FORCEINLINE NO_DISCARD static Type Max##Suffix(Type A, Type B) { return A > B ? A : B; } \
FORCEINLINE NO_DISCARD static Type Min##Suffix(Type A, Type B) { return A < B ? A : B; }

DECLARE_MinMax(isize, S)
DECLARE_MinMax(f64, F)

#undef DECLARE_MinMax

// Generic min/max 'functions' that work on any primitive type
#define Min(A, B) ((A) < (B) ? (A) : (B))
#define Max(A, B) ((A) > (B) ? (A) : (B))

#endif // MATH_H
