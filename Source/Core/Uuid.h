#ifndef _UUID_H_
#define _UUID_H_

#include "EngineTypes.h"
#include "String/BaseString.h"

#define GUID_LENGTH 37

STRUCT(Uuid)
{
	u32 Data1;
	u16 Data2;
	u16 Data3;
	u8 Data4[8];
};

RIFT_API Uuid UUID_Generate(void);

RIFT_API bool UUID_IsEqual(Uuid First, Uuid Second);

RIFT_API void UUID_ToString(Uuid ID, String* OutString);
RIFT_API Uuid UUID_FromString(const String IDString);

#endif // _UUID_H_
