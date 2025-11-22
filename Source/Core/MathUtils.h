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
	FORCEINLINE NO_DISCARD static Type CONCAT(Min, Suffix)(Type A, Type B) { return A < B ? A : B; } \
	FORCEINLINE NO_DISCARD static Type CONCAT(Max, Suffix)(Type A, Type B) { return A > B ? A : B; }

	DECLARE_MinMax(i32, I32)
	DECLARE_MinMax(u32, U32)
	DECLARE_MinMax(f64, F64)
#undef DECLARE_MinMax

// Generic min/max 'functions' that work on any primitive type
#define Min(A, B) ((A) < (B) ? (A) : (B))
#define Max(A, B) ((A) > (B) ? (A) : (B))

#define DECLARE_Clamp(Type, Suffix) \
	FORCEINLINE NO_DISCARD static Type CONCAT(Clamp, Suffix)(Type Value, Type Min, Type Max)     { return Value < Min ? Min : (Value < Max ? Value : Max); } \
	FORCEINLINE NO_DISCARD static Type CONCAT(CONCAT(Clamp, Suffix), _Min)(Type Value, Type Min) { return Value < Min ? Min : Value; } \
	FORCEINLINE NO_DISCARD static Type CONCAT(CONCAT(Clamp, Suffix), _Max)(Type Value, Type Max) { return Value > Max ? Max : Value; }

	DECLARE_Clamp(i32, I32)
	DECLARE_Clamp(u32, U32)
	DECLARE_Clamp(u16, U16)
	DECLARE_Clamp(f64, F64)
#undef DECLARE_Clamp

// Generic clamp 'functions' that work on any primitive type
/*
#define Clamp(Value, Min, Max) (((Value) < (Min)) ? (Min) : ((Value) < (Max)) ? (Value) : (Max))
#define Clamp_Min(Value, Min)   (((Value) < (Min)) ? (Min) : (Value))
#define Clamp_Max(Value, Max)   (((Value) > (Max)) ? (Max) : (Value))
*/

#endif // MATH_H
