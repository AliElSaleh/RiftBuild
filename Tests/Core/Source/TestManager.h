#pragma once

#include "Core/EngineTypes.h"

#define REGISTER_TEST(Category, Func) TestManager_RegisterTest(Func, S(Category), S(#Func))

#define BYPASS 2

typedef u8 (*TestFuncCallback)(void);

global String GExpectString;

void TestManager_Init(void);
void TestManager_RegisterTest(TestFuncCallback TestFunction, const String Category, const String Description);
bool TestManager_Run(void);
void* TestManager_MemAlloc(u64 Size);

#define TestManager_SetExpectString(ExpectString, ...) String_Empty(&GExpectString); String_Format(&GExpectString, ExpectString, __VA_ARGS__)
