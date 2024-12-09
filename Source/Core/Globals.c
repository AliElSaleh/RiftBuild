// Copyright (c) 2024 Artisan Softworks 
// Licensed under the BSD 3-Clause License. See the LICENSE file for details.

#ifndef UNITY_BUILD
#include "Globals.h"
#include "Allocators.h"
#include "StringUtils.h"
#include "Array.h"
#include "Filesystem.h"
#include "Clock.h"
#include "Memory.h"
#include "Platform.h"
#include "Log.h"
#endif

STRUCT(EngineGlobals)
{
    struct FileHandle NullFileHandle;
    String NullString;
    StringArray NullStringArray;
    StringList NullStringList;
};

static LinearAllocator GlobalsAllocator = {0};
static EngineGlobals   GGlobals = {0};

static void Internal_InitGlobals(EngineGlobals* G)
{
    ENSURE_NO_REENTRY();
    
    // File Handle 
    {
        FileHandle Handle = {0};
        Handle.Data = LinearAllocator_Allocate(&GlobalsAllocator, 8);
        Handle.Data2 = NULL;
        G->NullFileHandle = Handle;
    }

    // String
    {
        String Str = {0};
        Str.Data = LinearAllocator_Allocate(&GlobalsAllocator, sizeof(char) * 256);
        Str.Length = 0;
        Str.Capacity = 255;
        G->NullString = Str;
    }

    // String Array
    {
        StringArray Str = {0};
        Str.Num = 0;
        Str.List = LinearAllocator_Allocate(&GlobalsAllocator, sizeof(String));
        Str.List->Data = LinearAllocator_Allocate(&GlobalsAllocator, sizeof(char) * 256);
        Str.List->Capacity = 255;
        Str.List->Length = 0;
        Str.IterIndex = 0;
        Str.IterCurrent = NULL;
        G->NullStringArray = Str;
    }

    // String List
    {
        StringList Str = {0};
        Str.String = G->NullString;
        G->NullStringList = Str;
        G->NullStringList.Next = &G->NullStringList;
    }
}

void Globals_Init(void* Memory, usize Size)
{
    ENSURE_NO_REENTRY();
    
    LinearAllocator_Create(Size, Memory, &GlobalsAllocator);

    Internal_InitGlobals(&GGlobals);
}

bool IsValidFileHandle(const FileHandle Handle)
{
    bool bValid = Handle.Data != NULL;

    if (Handle.Data == GGlobals.NullFileHandle.Data)
    {
        bValid = false;
    }
    
    return bValid;
}

String String_Null(void)
{
    #ifdef DEVELOPER
    // GGlobals.NullString was somehow modified... you broke it somewhere in user code, fix it
    ASSERT(GGlobals.NullString.Data[0]  == 0);
    ASSERT(GGlobals.NullString.Length   == 0);
    ASSERT(GGlobals.NullString.Capacity == 255);
    #endif
    
    return GGlobals.NullString;
}

StringArray StringArray_Null(void)
{
    #ifdef DEVELOPER
    // GGlobals.NullStringArray was somehow modified... you broke it somewhere in user code, fix it
    ASSERT(GGlobals.NullStringArray.Num          == 0);
    ASSERT(GGlobals.NullStringArray.IterIndex    == 0);
    ASSERT(GGlobals.NullStringArray.IterCurrent  == NULL);
    #endif

    return GGlobals.NullStringArray;
}

StringList StringList_Null(void)
{
    #ifdef DEVELOPER
    // GGlobals.NullStringList was somehow modified... you broke it somewhere in user code, fix it
    ASSERT(GGlobals.NullStringList.Next            == &GGlobals.NullStringList);
    ASSERT(GGlobals.NullStringList.String.Data[0]  == 0);
    ASSERT(GGlobals.NullStringList.String.Length   == 0);
    ASSERT(GGlobals.NullStringList.String.Capacity == 255);
    #endif

    return GGlobals.NullStringList;
}

FileHandle FileHandle_Null(void)
{
    #ifdef DEVELOPER
    // GGlobals.NullFileHandle was somehow modified... you broke it somewhere in user code, fix it
    ASSERT(GGlobals.NullFileHandle.Data  != NULL);
    ASSERT(GGlobals.NullFileHandle.Data2 == NULL);
    #endif

    return GGlobals.NullFileHandle;
}

/*
EngineGlobals Globals(void)
{
    return GGlobals;
}
*/
