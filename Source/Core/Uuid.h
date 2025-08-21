#ifndef UUID_H
#define UUID_H

#ifndef UNITY_BUILD
#include "EngineTypes.h"
#endif

// https://datatracker.ietf.org/doc/html/rfc4122#section-4.1.2
STRUCT(Uuid)
{
    u32 TimeLow;
    u16 TimeMid;
    u16 TimeHiAndVersion;
    u8  ClockSeqHiAndReserved;
    u8  ClockSeqLow;
    u8  Node[6];
};

RIFT_API NO_DISCARD Uuid UUID_Generate(void);
RIFT_API NO_DISCARD bool UUID_IsEqual(Uuid First, Uuid Second);
RIFT_API NO_DISCARD Uuid UUID_FromString(const String IDString);
RIFT_API            void UUID_ToString(Uuid ID, String* OutString);
RIFT_API            void UUID_ToStringFast(Uuid ID, String* OutString);

#endif // UUID_H
