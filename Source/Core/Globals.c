// Copyright (c) 2024 Ali El Saleh 

#ifndef UNITY_BUILD
#include "Globals.h"
#include "Allocators.h"
#include "StringUtils.h"
#include "Array.h"
#include "Filesystem.h"
#include "Clock.h"
#include "Memory.h"
#include "Log.h"
#endif

STRUCT(EngineGlobals)
{
    TArray(void) NullArray;
    struct FileHandle NullFileHandle;
    String NullString;
    StringArray NullStringArray;
    StringList NullStringList;
};

internal LinearAllocator GlobalsAllocator = {0};
internal EngineGlobals   GGlobals = {0};

internal void InitGlobals(EngineGlobals* G)
{
    // Array
    {
        usize* Array = (usize*)LinearAllocator_Allocate(&GlobalsAllocator, 64);
        Array[0] = 1;
        Array[2] = 8;
        G->NullArray = &Array[4];
    }

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

void InitializeGlobals(void* Memory, usize Size)
{
    LinearAllocator_Create(Size, Memory, &GlobalsAllocator);

    InitGlobals(&GGlobals);
}

bool IsValidFileHandle(const FileHandle Handle)
{
    if (Handle.Data == GGlobals.NullFileHandle.Data)
        return false;
    
    return IsValid(Handle.Data);
}

String String_Null(void)
{
    #ifdef DEVELOPER
    // GGlobals.NullString was somehow modified... you broke it somewhere in user code, fix it
    ASSERT_MSG(GGlobals.NullString.Data[0]  == 0,   "You have modified GGlobals.NullString.Data, further code relying on this will result in undefined behaviour, investigate where this was modified to fix and ensure this doesnt happen going forward");
    ASSERT_MSG(GGlobals.NullString.Length   == 0,   "You have modified GGlobals.NullString.Length, further code relying on this will result in undefined behaviour, investigate where this was modified to fix and ensure this doesnt happen going forward");
    ASSERT_MSG(GGlobals.NullString.Capacity == 255, "You have modified GGlobals.NullString.Capacity, further code relying on this will result in undefined behaviour, investigate where this was modified to fix and ensure this doesnt happen going forward");
    #endif
    
    return GGlobals.NullString;
}

StringArray StringArray_Null(void)
{
    #ifdef DEVELOPER
    // GGlobals.NullStringArray was somehow modified... you broke it somewhere in user code, fix it
    ASSERT_MSG(GGlobals.NullStringArray.Num          == 0,    "You have modified GGlobals.NullStringArray.Num, further code relying on this will result in undefined behaviour, investigate where this was modified to fix and ensure this doesnt happen going forward");
    ASSERT_MSG(GGlobals.NullStringArray.IterIndex    == 0,    "You have modified GGlobals.NullStringArray.IterIndex, further code relying on this will result in undefined behaviour, investigate where this was modified to fix and ensure this doesnt happen going forward");
    ASSERT_MSG(GGlobals.NullStringArray.IterCurrent  == NULL, "You have modified GGlobals.NullStringArray.IterCurrent, further code relying on this will result in undefined behaviour, investigate where this was modified to fix and ensure this doesnt happen going forward");
    #endif

    return GGlobals.NullStringArray;
}

StringList StringList_Null(void)
{
    #ifdef DEVELOPER
    // GGlobals.NullStringList was somehow modified... you broke it somewhere in user code, fix it
    ASSERT_MSG(GGlobals.NullStringList.Next            == &GGlobals.NullStringList, "You have modified GGlobals.NullStringList.Next, further code relying on this will result in undefined behaviour, investigate where this was modified to fix and ensure this doesnt happen going forward");
    ASSERT_MSG(GGlobals.NullStringList.String.Data[0]  == 0,                        "You have modified GGlobals.NullStringList.String.Data, further code relying on this will result in undefined behaviour, investigate where this was modified to fix and ensure this doesnt happen going forward");
    ASSERT_MSG(GGlobals.NullStringList.String.Length   == 0,                        "You have modified GGlobals.NullStringList.String.Length, further code relying on this will result in undefined behaviour, investigate where this was modified to fix and ensure this doesnt happen going forward");
    ASSERT_MSG(GGlobals.NullStringList.String.Capacity == 255,                      "You have modified GGlobals.NullStringList.String.Capacity, further code relying on this will result in undefined behaviour, investigate where this was modified to fix and ensure this doesnt happen going forward");
    #endif

    return GGlobals.NullStringList;
}

void* Array_Null(void)
{
    return GGlobals.NullArray;
}

FileHandle FileHandle_Null(void)
{
    return GGlobals.NullFileHandle;
}

EngineGlobals Globals_Get(void)
{
    return GGlobals;
}
