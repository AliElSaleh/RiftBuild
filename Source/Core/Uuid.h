#ifndef _UUID_H_
#define _UUID_H_

#ifndef UNITY_BUILD
#include "EngineTypes.h"
#endif

#define GUID_LENGTH 37

STRUCT(Uuid)
{
    u32 Data1;
    u16 Data2;
    u16 Data3;
    u8 Data4[8];
};

RIFT_API NO_DISCARD Uuid UUID_Generate(void);
RIFT_API NO_DISCARD bool UUID_IsEqual(Uuid First, Uuid Second);
RIFT_API NO_DISCARD Uuid UUID_FromString(const String IDString);
RIFT_API void UUID_ToString(Uuid ID, String* OutString);

#endif // _UUID_H_
