#ifndef CLOCK_H
#define CLOCK_H

#ifndef UNITY_BUILD
#include "EngineTypes.h"
#endif

STRUCT(Clock)
{
	f64 StartTime;
	f64 ElapsedTime;
};

RIFT_API NO_DISCARD Clock Clock_Null(void);

RIFT_API void Clock_Start(Clock* C);
RIFT_API void Clock_Stop(Clock* C);

RIFT_API void Clock_Tick(Clock* C);

RIFT_API NO_DISCARD f64 Clock_GetElapsedTime(const Clock* C, bool bAutoConvertTimeUnit);
RIFT_API NO_DISCARD f64 Clock_GetElapsedTime_Milliseconds(const Clock* C);
RIFT_API NO_DISCARD f64 Clock_GetElapsedTime_Microseconds(const Clock* C);
RIFT_API NO_DISCARD f64 Clock_GetElapsedTime_Nanoseconds(const Clock* C);

RIFT_API void Clock_PrintElapsedTime(const Clock* C, bool bAutoConvertTimeUnit);

RIFT_API void Clock_GetElapsedTime_ToString(const Clock* C, bool bAutoConvertTimeUnit, String* OutString);
RIFT_API void Clock_GetElapsedTime_ToStringEx(const Clock* C, bool bAutoConvertTimeUnit, String* OutString, const String Format);

RIFT_API NO_DISCARD f64 Time_AutoConvert(f64 Seconds);
RIFT_API void Time_ToString(f64 Seconds, bool bAutoConvertTimeUnit, String* OutString);
RIFT_API void Time_ToStringEx(f64 Seconds, bool bAutoConvertTimeUnit, String* OutString, const String Format);

#endif // CLOCK_H
