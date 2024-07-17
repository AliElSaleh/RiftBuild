#ifndef _CLOCK_H_
#define _CLOCK_H_

#include "EngineTypes.h"
#include "String/BaseString.h"

#ifdef _DEBUG
#define TIME_SCOPE_SLOW(name, code) { Clock CONCAT(c, __LINE__); Clock_Start(&CONCAT(c, __LINE__)); code Clock_Tick(&CONCAT(c, __LINE__)); char ElapsedTimeString[16] = { 0 }; Time_ToString(CONCAT(c, __LINE__).ElapsedTime, true, ElapsedTimeString); LOG_INFO(name " took: %s", ElapsedTimeString); }
#define TIME_SCOPE(name, code) { Logging_Disable(); Clock CONCAT(c, __LINE__); Clock_Start(&CONCAT(c, __LINE__)); code Clock_Tick(&CONCAT(c, __LINE__)); Logging_Enable(); StringLocal(ElapsedTimeString, 16); Time_ToString(CONCAT(c, __LINE__).ElapsedTime, true, &ElapsedTimeString); LOG_INFO(name " took: %s", ElapsedTimeString.Data); }
#else
#define TIME_SCOPE_SLOW(name, code) code;
#define TIME_SCOPE(name, code) code;
#endif

STRUCT(Clock)
{
	f64 StartTime;
	f64 ElapsedTime;
};

RIFT_API void Clock_Start(Clock* C);
RIFT_API void Clock_Stop(Clock* C);

RIFT_API void Clock_Tick(Clock* C);

RIFT_API f64 Clock_GetElapsedTime(const Clock* C, bool bAutoConvertTimeUnit);
RIFT_API f64 Clock_GetElapsedTime_Milliseconds(const Clock* C);
RIFT_API f64 Clock_GetElapsedTime_Microseconds(const Clock* C);
RIFT_API f64 Clock_GetElapsedTime_Nanoseconds(const Clock* C);

RIFT_API void Clock_PrintElapsedTime(const Clock* C, bool bAutoConvertTimeUnit);

RIFT_API void Clock_GetElapsedTime_ToString(const Clock* C, bool bAutoConvertTimeUnit, String* OutString);
RIFT_API void Clock_GetElapsedTime_ToStringEx(const Clock* C, bool bAutoConvertTimeUnit, String* OutString, const String Format);

RIFT_API f64 Time_AutoConvert(f64 Seconds);
RIFT_API void Time_ToString(f64 Seconds, bool bAutoConvertTimeUnit, String* OutString);
RIFT_API void Time_ToStringEx(f64 Seconds, bool bAutoConvertTimeUnit, String* OutString, const String Format);

#endif // _CLOCK_H_
