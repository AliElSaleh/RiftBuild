// Copyright (c) 2024 Ali El Saleh 

#include "Clock.h"

#ifndef UNITY_BUILD
#include "Platform.h"
#include "StringUtils.h"
#endif

void Clock_Start(Clock* C)
{
    C->StartTime = Platform_GetAbsoluteTime();
    C->ElapsedTime = 0.0;
}

void Clock_Stop(Clock* C)
{
    C->StartTime = 0;
}

void Clock_Tick(Clock* C)
{
    if (C->StartTime != 0.0)
    {
        C->ElapsedTime = Platform_GetAbsoluteTime() - C->StartTime;
    }
}

f64 Clock_GetElapsedTime(const Clock* C, bool bAutoConvertTimeUnit)
{
    if (bAutoConvertTimeUnit)
    {
        return Time_AutoConvert(C->ElapsedTime);
    }
    
    return C->ElapsedTime;
}

void Clock_GetElapsedTime_ToString(const Clock* C, bool bAutoConvertTimeUnit, String* OutString)
{
    Time_ToString(C->ElapsedTime, bAutoConvertTimeUnit, OutString);
}

void Clock_GetElapsedTime_ToStringEx(const Clock* C, bool bAutoConvertTimeUnit, String* OutString, const String Format)
{
    Time_ToStringEx(C->ElapsedTime, bAutoConvertTimeUnit, OutString, Format);
}

f64 Clock_GetElapsedTime_Milliseconds(const Clock* C)
{
    return C->ElapsedTime * 1000.0;
}

f64 Clock_GetElapsedTime_Microseconds(const Clock* C)
{
    return C->ElapsedTime * 1000000.0;
}

f64 Clock_GetElapsedTime_Nanoseconds(const Clock* C)
{
    return C->ElapsedTime * 1000000000.0;
}

f64 Time_AutoConvert(f64 Seconds)
{
    // Nanosecond detection
    // less than 1us and greater than 1ns
    if (Seconds >= 0.000000001 && Seconds < 0.000001)
    {
        return Seconds * 1000000000.0;
    }

    // Microsecond detection
    // less than 1ms and greater than 1us
    if (Seconds >= 0.000001 && Seconds < 0.001)
    {
        return Seconds * 1000000.0;
    }

    // Millisecond detection
    // greater than 1ms and less than 1s
    if (Seconds >= 0.001 && Seconds < 1.0)
    {
        return Seconds * 1000.0;
    }
    
    return Seconds;
}

void Time_ToString(f64 Seconds, bool bAutoConvertTimeUnit, String* OutString)
{
    f64 TimeAdjusted;
    char TimeUnit[4] = {0};

    TimeAdjusted = Seconds;
    TimeUnit[0] = 's';
    u8 Len = 1;

    if (bAutoConvertTimeUnit)
    {
        // Nanosecond detection
        // less than 1us and greater than 1ns
        if (Seconds >= 0.000000001 && Seconds < 0.000001)
        {
            TimeAdjusted = Seconds * 1000000000.0;

            TimeUnit[0] = 'n';
            TimeUnit[1] = 's';
            Len = 2;
        }

        // Microsecond detection
        // less than 1ms and greater than 1us
        if (Seconds >= 0.000001 && Seconds < 0.001)
        {
            TimeAdjusted = Seconds * 1000000.0;

            TimeUnit[0] = 'u';
            TimeUnit[1] = 's';
            Len = 2;
        }

        // Millisecond detection
        // greater than 1ms and less than 1s
        if (Seconds >= 0.001 && Seconds < 1.0)
        {
            TimeAdjusted = Seconds * 1000.0;

            TimeUnit[0] = 'm';
            TimeUnit[1] = 's';
            Len = 2;
        }
    }

    String_Format(OutString, S("%f%S"), OutString->Capacity, TimeAdjusted, StrSlice(TimeUnit, Len));
}

void Time_ToStringEx(f64 Seconds, bool bAutoConvertTimeUnit, String* OutString, const String Format)
{
    f64 TimeAdjusted;
    char TimeUnit[4] = {0};

    TimeAdjusted = Seconds;
    TimeUnit[0] = 's';
    u8 Len = 1;

    if (bAutoConvertTimeUnit)
    {
        // Nanosecond detection
        // less than 1us and greater than 1ns
        if (Seconds >= 0.000000001 && Seconds < 0.000001)
        {
            TimeAdjusted = Seconds * 1000000000.0;

            TimeUnit[0] = 'n';
            TimeUnit[1] = 's';
            Len = 2;
        }

        // Microsecond detection
        // less than 1ms and greater than 1us
        if (Seconds >= 0.000001 && Seconds < 0.001)
        {
            TimeAdjusted = Seconds * 1000000.0;

            TimeUnit[0] = 'u';
            TimeUnit[1] = 's';
            Len = 2;
        }

        // Millisecond detection
        // greater than 1ms and less than 1s
        if (Seconds >= 0.001 && Seconds < 1.0)
        {
            TimeAdjusted = Seconds * 1000.0;

            TimeUnit[0] = 'm';
            TimeUnit[1] = 's';
            Len = 2;
        }
    }

    StringLocal(TimeFormat, 32);
    String a = S("%S");
    String_Concat(&TimeFormat, Format, a);
    String_Format(OutString, TimeFormat, 64, TimeAdjusted, StrSlice(TimeUnit, Len));
}

void Clock_PrintElapsedTime(const Clock* C, bool bAutoConvertTimeUnit)
{
    StringLocal(Time, 64);
    Clock_GetElapsedTime_ToString(C, bAutoConvertTimeUnit, &Time);

    Platform_ConsoleWrite_CustomLength(Time.Data, Time.Length, 0, false);
    Platform_ConsoleWrite_CustomLength("\n", 1, 0, false);
}
