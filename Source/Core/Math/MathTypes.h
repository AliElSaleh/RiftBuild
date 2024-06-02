#pragma once

#include "EngineTypes.h"

#define PI						3.14159265358979323846f
#define PI_2					6.28318530717958647692f
#define PI_HALF					1.57079632679489661923f
#define PI_QUARTER				0.78539816339744830961f
#define ONE_OVER_PI				0.31830988618379067153f
#define ONE_OVER_TWO_PI			0.15915494309189533576f

#define SQRT_TWO				1.41421356237309504880f
#define SQRT_THREE				1.73205080756887729352f
#define SQRT_ONE_OVER_TWO		0.70710678118654752440f
#define SQRT_ONE_OVER_THREE		0.57735026918962576450f

#define DEG2RAD (PI / 180.0f)
#define RAD2DEG (180.0f / PI)

#define SEC2MS 1000.0f
#define MS2SEC 0.001f

#define INFINITY 1e30f

#define RAND_MAX 0x7fff

// Stolen from <cmath.h>
#define HUGE_VAL   ((double)INFINITY)
#define HUGE_VALF  ((float)INFINITY)
#define HUGE_VALL  ((long double)INFINITY)
#define NAN        ((float)(INFINITY * 0.0F))

// Smallest positive number where 1.0f + FLOAT_EPSILON != 0.0f
#define FLOAT_EPSILON 1.192092896e-07f

#define Clamp(Value, Min, Max) ((Value) < (Min)) ? (Min) : ((Value) < (Max)) ? (Value) : (Max)
#define ClampMin(Value, Min) ((Value) < (Min)) ? (Min) : (Value)
#define ClampMax(Value, Max) ((Value) > (Max)) ? (Max) : (Value)

#define Min(A, B) ((A) < (B) ? (A) : (B))
#define Max(A, B) ((A) > (B) ? (A) : (B))
