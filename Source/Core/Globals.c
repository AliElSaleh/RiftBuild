// Copyright (c) 2024 Ali El Saleh 

#include "Globals.h"
#include "Memory/LinearAllocator.h"
#include "String/StringUtils.h"
#include "Structures/Array.h"
#include "Platform/Filesystem.h"
#include "Clock/Clock.h"
#include "Memory/Memory.h"
#include "Log.h"

internal LinearAllocator GlobalsAllocator = {0};

internal EngineGlobals GGlobals = {0};
internal EngineGlobals GGlobals_InternalCopy = {0};

void InitializeGlobals(void* Memory, usize Size)
{
    LinearAllocator_Create(Size, Memory, &GlobalsAllocator);

    // Array
    {
        usize* Array = (usize*)LinearAllocator_Allocate(&GlobalsAllocator, 64);
        Array[0] = 1;
        Array[2] = 8;
        GGlobals.NullArray = &Array[4];
    }

    // File Handle 
    {
        //FileHandle* Handle = LinearAllocator_Allocate(&GlobalsAllocator, sizeof(FileHandle));
        FileHandle Handle = {0};
        Handle.Data = LinearAllocator_Allocate(&GlobalsAllocator, 8);
        Handle.Data2 = NULL;
        //Handle.Size = 0;
        GGlobals.NullFileHandle = Handle;
    }

    // Clock
    {
        Clock c = {0};
        GGlobals.NullClock = c;//LinearAllocator_Allocate(&GlobalsAllocator, sizeof(Clock));
    }

    // String
    {
        //String* Str = LinearAllocator_Allocate(&GlobalsAllocator, sizeof(String));
        String Str = {0};
        Str.Data = LinearAllocator_Allocate(&GlobalsAllocator, sizeof(char) * 256);
        Str.Length = 0;
        Str.Capacity = 255;
        GGlobals.NullString = Str;
    }

    // String Array
    {
        //StringArray* Str = LinearAllocator_Allocate(&GlobalsAllocator, sizeof(StringArray));
        StringArray Str = {0};
        Str.Num = 0;
        Str.List = LinearAllocator_Allocate(&GlobalsAllocator, sizeof(String));
        Str.List->Data = LinearAllocator_Allocate(&GlobalsAllocator, sizeof(char) * 256);
        Str.List->Capacity = 255;
        Str.List->Length = 0;
        Str.IterIndex = 0;
        Str.IterCurrent = NULL;
        GGlobals.NullStringArray = Str;
    }

    // String List
    {
        //StringList* Str = LinearAllocator_Allocate(&GlobalsAllocator, sizeof(StringList));
        StringList Str = {0};
        Str.String = GGlobals.NullString;
        GGlobals.NullStringList = Str;
        GGlobals.NullStringList.Next = &GGlobals.NullStringList;
    }

    MemCopy(&GGlobals_InternalCopy, &GGlobals, sizeof(EngineGlobals));
}

bool IsValidFileHandle(const FileHandle Handle)
{
    if (Handle.Data == GGlobals.NullFileHandle.Data)
        return false;
    
    return IsValid(Handle.Data);
}

void Globals_AssertNullString(void)
{
    // GGlobals.NullString was somehow modified... you broke it somewhere in user code, fix it
    /*
    ASSERT_MSG(GGlobals.NullString == GGlobals_InternalCopy.NullString, "You have modified GGlobals.NullString, further code relying on this will result in undefined behaviour, investigate where this was modified to fix and ensure this doesnt happen going forward");
    ASSERT_MSG(((String*)GGlobals.NullString)->Data == ((String*)GGlobals_InternalCopy.NullString)->Data, "You have modified GGlobals.NullString.Data, further code relying on this will result in undefined behaviour, investigate where this was modified to fix and ensure this doesnt happen going forward");
    ASSERT_MSG(((String*)GGlobals.NullString)->Length == ((String*)GGlobals_InternalCopy.NullString)->Length, "You have modified GGlobals.NullString.Length, further code relying on this will result in undefined behaviour, investigate where this was modified to fix and ensure this doesnt happen going forward");
    ASSERT_MSG(((String*)GGlobals.NullString)->Capacity == ((String*)GGlobals_InternalCopy.NullString)->Capacity, "You have modified GGlobals.NullString.Capacity, further code relying on this will result in undefined behaviour, investigate where this was modified to fix and ensure this doesnt happen going forward");
    */
}

void Globals_AssertNullStringArray(void)
{
    // GGlobals.NullStringArray was somehow modified... you broke it somewhere in user code, fix it
    /*
    ASSERT_MSG(GGlobals.NullStringArray == GGlobals_InternalCopy.NullStringArray, "You have modified GGlobals.NullStringArray, further code relying on this will result in undefined behaviour, investigate where this was modified to fix and ensure this doesnt happen going forward");
    ASSERT_MSG(((StringArray*)GGlobals.NullStringArray)->List == ((StringArray*)GGlobals_InternalCopy.NullString)->List, "You have modified GGlobals.NullStringArray.List, further code relying on this will result in undefined behaviour, investigate where this was modified to fix and ensure this doesnt happen going forward");
    ASSERT_MSG(((StringArray*)GGlobals.NullStringArray)->Num == ((StringArray*)GGlobals_InternalCopy.NullString)->Num, "You have modified GGlobals.NullStringArray.Num, further code relying on this will result in undefined behaviour, investigate where this was modified to fix and ensure this doesnt happen going forward");
    */
}

void Globals_AssertNullStringList(void)
{
    // GGlobals.NullStringList was somehow modified... you broke it somewhere in user code, fix it
    /*
    ASSERT_MSG(GGlobals.NullStringList == GGlobals_InternalCopy.NullStringList, "You have modified GGlobals.NullStringList, further code relying on this will result in undefined behaviour, investigate where this was modified to fix and ensure this doesnt happen going forward");
    ASSERT_MSG(((StringList*)GGlobals.NullStringList)->Next == ((StringList*)GGlobals_InternalCopy.NullStringList)->Next, "You have modified GGlobals.NullStringList.Next, further code relying on this will result in undefined behaviour, investigate where this was modified to fix and ensure this doesnt happen going forward");
    ASSERT_MSG(((StringList*)GGlobals.NullStringList)->String.Data == ((StringList*)GGlobals_InternalCopy.NullStringList)->String.Data, "You have modified GGlobals.NullStringList.String.Data, further code relying on this will result in undefined behaviour, investigate where this was modified to fix and ensure this doesnt happen going forward");
    ASSERT_MSG(((StringList*)GGlobals.NullStringList)->String.Length == ((StringList*)GGlobals_InternalCopy.NullStringList)->String.Length, "You have modified GGlobals.NullStringList.String.Length, further code relying on this will result in undefined behaviour, investigate where this was modified to fix and ensure this doesnt happen going forward");
    ASSERT_MSG(((StringList*)GGlobals.NullStringList)->String.Capacity == ((StringList*)GGlobals_InternalCopy.NullStringList)->String.Capacity, "You have modified GGlobals.NullStringList.String.Capacity, further code relying on this will result in undefined behaviour, investigate where this was modified to fix and ensure this doesnt happen going forward");
    */
}

String String_Null(void)
{
    #ifdef DEVELOPER
    Globals_AssertNullString();
    #endif
    
    //return *((String*)GGlobals.NullString);
    return GGlobals.NullString;
}

StringArray StringArray_Null(void)
{
    #ifdef DEVELOPER
    Globals_AssertNullStringArray();
    #endif

    //return *((StringArray*)GGlobals.NullStringArray);
    return GGlobals.NullStringArray;
}

StringList StringList_Null(void)
{
    #ifdef DEVELOPER
    Globals_AssertNullStringList();
    #endif

    //return *((StringList*)GGlobals.NullStringList);
    return GGlobals.NullStringList;
}

void* Array_Null(void)
{
	return GGlobals.NullArray;
}

FileHandle FileHandle_Null(void)
{
    //return *((FileHandle*)GGlobals.NullFileHandle);
    return GGlobals.NullFileHandle;
}

EngineGlobals Globals_Get(void)
{
    return GGlobals;
}
