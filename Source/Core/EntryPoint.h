#pragma once

#include "Platform/Platform.h"
#include "Memory/Memory.h"
#include "Memory/LinearAllocator.h"
#include "Clock/Clock.h"
#include "Log.h"

global usize GEngineMemoryAmount;
global usize GEngineScratchAmount;

PRAGMA_DISABLE_MISSING_PROTOTYPES_WARNING

#ifndef RIFT_STATIC
C_LINKAGE_BEGIN
int _fltused = 0;
C_LINKAGE_END

void* nullptr_z = NULL;
#endif

extern u32 RunApplication(const StringArray Arguments);

#if !PLATFORM_WINDOWS
#define USE_MAIN 1
extern void pre_main(int argc, char* argv[], char* env[]);
#else
#define USE_MAIN 0
#endif

// Main entry point of the application
#if USE_MAIN
int main(i32 ArgC, char* ArgV[], char* ArgEnv[])
#else
void ProgramStart(void)
#endif
{
    #if USE_MAIN
    pre_main(ArgC, ArgV, ArgEnv);
    #endif

    // Note: one giant dynamic allocation. then let the engine dish the memory out
    const usize MemoryAmount = GEngineMemoryAmount;
    const usize ScratchAmount = GEngineScratchAmount;

    if (MemoryAmount == 0)
    {
        Platform_ConsoleWrite("Max memory amount given was 0. Aborting...\n", 4, true);
        Platform_Abort(1);
        #if USE_MAIN
        return 1;
        #else
        return;
        #endif
    }

    // initialize platform specific stuff just before the program starts
    // for example, clock frequency, critical sections, etc.
    Platform_PreInitialize();

    usize MemoryDumpAmount = Kibibytes(8);
    
    void* EngineMemory = Platform_MemAlloc(MemoryAmount + ScratchAmount + MemoryDumpAmount);
    
    if (!EngineMemory)
    {
        Platform_ConsoleWrite("Failed to acquire memory from the OS. Aborting...\n", 4, true);
        Platform_Abort(1);
        #if USE_MAIN
        return 1;
        #else
        return;
        #endif
    }
    
    void* EngineScratch = ((u8*)EngineMemory) + MemoryAmount;

    void* EngineMemoryDump = ((u8*)EngineMemory) + MemoryAmount + ScratchAmount;
    
    #ifndef RIFT_STATIC
    nullptr_z = EngineMemoryDump;
    #endif

    u32 ReturnVal = 0;
    
    StringArray Arguments = Platform_GetCommandLineArgs();

    // "Use" the memory so the OS assigns it all to us
    Platform_MemZero(EngineMemory, MemoryAmount + MemoryDumpAmount + ScratchAmount);
    
    // Initialize core engine subsystems
    // Memory subsystem
    if (!Memory_Initialize(EngineMemory, MemoryAmount, EngineMemoryDump, EngineScratch, GEngineScratchAmount))
    {
        Platform_ConsoleWrite("Failed to initialize memory subsystem. Required for engine to run. Aborting...\n", 4, true);

        ReturnVal = 1;
        goto Shutdown_Lvl0;
    }
    
    // Logging subsystem
    #ifndef NO_LOG
    void* LogSubsystemState = MemAlloc(Logging_GetMemoryRequirement(), MemoryTag_Engine);
    bool bLogFileOnStartup = true;
    #ifdef NO_LOG_FILE
    bLogFileOnStartup = false;
    #endif
    if (!Logging_Initialize(LogSubsystemState, bLogFileOnStartup))
    {
        Platform_ConsoleWrite("Failed to initialize logging subsystem. Required for engine to run. Aborting...\n", 4, true);

        ReturnVal = 1;
        goto Shutdown_Lvl1;
    }
    #endif

    ReturnVal = RunApplication(Arguments);

    #ifndef NO_LOG
    Logging_Shutdown();
    MemFree(LogSubsystemState, MemoryTag_Engine);
    #endif

#ifndef NO_LOG
Shutdown_Lvl1:
#endif
    Memory_Shutdown();

Shutdown_Lvl0:
    Platform_MemFree(EngineMemory);

    Platform_Abort(ReturnVal);

    #if USE_MAIN
    return (i32)ReturnVal;
    #endif
}

PRAGMA_ENABLE_WARNINGS
