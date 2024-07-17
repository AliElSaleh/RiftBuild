#ifndef _GLOBALS_H_
#define _GLOBALS_H_

#include "EngineTypes.h"
#include "String/BaseString.h"

#include "Platform/Filesystem.h"
#include "Clock/Clock.h"

STRUCT(EngineGlobals)
{
	TArray(void) NullArray;
	FileHandle NullFileHandle;
	Clock NullClock;
	String NullString;
	StringArray NullStringArray;
	StringList NullStringList;
};

RIFT_API void InitializeGlobals(void* Memory, usize Size);

void Globals_AssertNullString(void);
void Globals_AssertNullStringArray(void);
void Globals_AssertNullStringList(void);

RIFT_API String String_Null(void);
RIFT_API StringArray StringArray_Null(void);
RIFT_API StringList StringList_Null(void);

RIFT_API EngineGlobals Globals_Get(void);

#endif // _GLOBALS_H_
