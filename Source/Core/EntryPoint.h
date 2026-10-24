#ifndef ENTRY_POINT_H
#define ENTRY_POINT_H

#ifndef UNITY_BUILD
#include "Platform.h"
#include "Memory.h"
#include "Log.h"
#endif

typedef u32 (*ApplicationFn)(const StringArray Arguments);

#if !PLATFORM_WINDOWS
extern void pre_main(int argc, char* argv[], char* env[]);
#endif

FORCEINLINE static i32 Entry_Run(ApplicationFn Application,
                                 usize MemoryAmount,
                                 usize ScratchAmount,
                                 i32 ArgC,
                                 char* ArgV[],
                                 char* ArgEnv[])
{
    #if !PLATFORM_WINDOWS
    pre_main(ArgC, ArgV, ArgEnv);
    #else
    xx ArgC;
    xx ArgV;
    xx ArgEnv;
    #endif

    if (MemoryAmount == 0)
    {
        Platform_ConsoleWrite("Max memory amount given was 0. Aborting...\n", 4, true);
        Platform_Abort(1);
        return 1;
    }

    // initialize platform specific stuff just before the program starts
    // for example, clock frequency, critical sections, etc.
    Platform_PreInitialize();

    // Note: one giant dynamic allocation. then let the engine dish the memory out
    void* EngineMemory = Platform_MemAlloc(MemoryAmount);

    if (!EngineMemory)
    {
        Platform_ConsoleWrite("Failed to acquire memory from the OS. Aborting...\n", 4, true);
        Platform_Abort(1);
        return 1;
    }

    u32 ReturnVal = 0;

    StringArray Arguments = Platform_GetCommandLineArgs();

    // "Use" the memory so the OS assigns it all to us
    Platform_MemZero(EngineMemory, MemoryAmount);

    // Initialize core engine subsystems
    // Memory subsystem
    if (!Memory_Initialize(EngineMemory, MemoryAmount, ScratchAmount))
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

    ReturnVal = Application(Arguments);

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

    return (i32)ReturnVal;
}

#endif // ENTRY_POINT_H
