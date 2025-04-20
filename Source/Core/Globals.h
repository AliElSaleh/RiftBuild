#ifndef GLOBALS_H
#define GLOBALS_H

#ifndef UNITY_BUILD
#include "EngineTypes.h"
#endif

struct EngineGlobals;

RIFT_API void Globals_Init(void* Memory, usize Size);

RIFT_API NO_DISCARD struct String String_Null(void);
RIFT_API NO_DISCARD struct StringArray StringArray_Null(void);
RIFT_API NO_DISCARD struct StringList StringList_Null(void);

#endif // GLOBALS_H
