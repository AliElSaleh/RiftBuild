#include "Math.h"

#include "pt_math.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wbad-function-cast"

f32 Sqrt(f32 X)
{
	return PT_sqrtf(X);
}

f32 Pow(f32 X, f32 Y)
{
	return PT_powf(X, Y);
}

f64 Powd(f64 X, f64 Y)
{
	return PT_pow(X, Y);
}

f32 Exp(f32 X)
{
	return PT_expf(X);
}

f32 Abs(f32 Value)
{
	i32 Temp = *((i32*)&Value);
	Temp &= 0x7FFFFFFF;
	return *(f32*)&Temp;
}

f64 Absf64(f64 Value)
{
	i64 Temp = *((i64*)&Value);
	Temp &= 0x7FFFFFFFFFFFFFFF;
	return *(f64*)&Temp;
}

i32 Absi32(i32 Value)
{
	i32 Temp = (i32)Value >> 31;
	Value ^= Temp;
	Value += Temp & 1;
	return Value;
}

i64 Absi64(i64 Value)
{
	i64 Temp = (i64)Value >> 63;
	Value ^= Temp;
	Value += Temp & 1;
	return Value;
}

#pragma GCC diagnostic pop
