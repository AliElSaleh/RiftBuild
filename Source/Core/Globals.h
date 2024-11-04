#ifndef _GLOBALS_H_
#define _GLOBALS_H_

#ifndef UNITY_BUILD
#include "EngineTypes.h"
#endif

struct EngineGlobals;

RIFT_API void InitializeGlobals(void* Memory, usize Size);

RIFT_API NO_DISCARD struct String String_Null(void);
RIFT_API NO_DISCARD struct StringArray StringArray_Null(void);
RIFT_API NO_DISCARD struct StringList StringList_Null(void);

//RIFT_API NO_DISCARD struct EngineGlobals Globals(void);

void Globals_AssertNullString(void);
void Globals_AssertNullStringArray(void);
void Globals_AssertNullStringList(void);

#endif // _GLOBALS_H_
