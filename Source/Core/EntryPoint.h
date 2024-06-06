#ifndef META_GENERATED
#pragma once

#ifndef HEADLESS
#include "AppTypes.h"
#endif

#include "Platform/Platform.h"
#include "Memory/Memory.h"
#include "Memory/LinearAllocator.h"
#include "Clock/Clock.h"
#include "Log.h"

global u64 GEngineMemoryAmount;
global u64 GEngineScratchAmount;

#ifdef RIFT_DEBUG_MEMORY
global u64 GEngineDebugMemoryAmount;
#endif

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-prototypes"

#ifndef RIFT_STATIC
C_LINKAGE_BEGIN
int _fltused = 0;
C_LINKAGE_END

void* nullptr_z = NULL;
#endif

#ifdef HEADLESS
extern u32 RunApplication(const StringArray Arguments);
#else
extern void InitializeApplication(App* Instance);
#endif

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
    const u64 MemoryAmount = GEngineMemoryAmount;
    const u64 ScratchAmount = GEngineScratchAmount;

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

    u64 MemoryDumpAmount = Kibibytes(8);
    
    #ifdef RIFT_DEBUG_MEMORY
    u64 MemoryDebugAmount = GEngineDebugMemoryAmount;
    #else
    u64 MemoryDebugAmount = 0;
    #endif

    void* EngineMemory = Platform_MemAlloc(MemoryAmount + MemoryDebugAmount + ScratchAmount + MemoryDumpAmount);
    
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
    
    #ifdef RIFT_DEBUG_MEMORY
    void* DebugMemory = ((u8*)EngineMemory) + MemoryAmount;
    #endif
    
    void* EngineScratch = ((u8*)EngineMemory) + MemoryAmount + MemoryDebugAmount;

    void* EngineMemoryDump = ((u8*)EngineMemory) + MemoryAmount + MemoryDebugAmount + ScratchAmount;
    
    #ifndef RIFT_STATIC
    nullptr_z = EngineMemoryDump;
    #endif

    u32 ReturnVal = 0;
    
    StringArray Arguments = Platform_GetCommandLineArgs();

    #ifndef HEADLESS
    do
    #endif
    {
        // "Use" the memory so the OS assigns it all to us
        Platform_MemZero(EngineMemory, MemoryAmount + MemoryDebugAmount + MemoryDumpAmount + ScratchAmount);

        // fill the memory debug with the address of it's own
        /*
        for (u64 i = 0; i < MemoryDumpAmount/sizeof(u64); i++)
        {
            ((u64*)EngineMemoryDump)[i] = *(u64*)&EngineMemoryDump;
        }
        */
        
        // Initialize core engine subsystems
        // Memory subsystem
        #ifdef RIFT_DEBUG_MEMORY
        if (!Memory_Initialize(EngineMemory, MemoryAmount, DebugMemory, MemoryDebugAmount, EngineMemoryDump, EngineScratch, GEngineScratchAmount))
        #else
        if (!Memory_Initialize(EngineMemory, MemoryAmount, EngineMemoryDump, EngineScratch, GEngineScratchAmount))
        #endif
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

        #ifdef HEADLESS
        ReturnVal = RunApplication(Arguments);
        goto Shutdown_Lvl2;
        #else
        LOG_INFO("Engine Startup");
        
        App Instance = { 0 };
        {
            // Initialize and setup application
            InitializeApplication(&Instance);

            // Ensure proper values are set
            if (Instance.AppConfig.WindowWidth <= 0 || Instance.AppConfig.WindowHeight <= 0)
            {
                LOG_FATAL("Cannot create an application with a window size of 0 in any dimension");
                ReturnVal = 1;
                goto Shutdown_Lvl2;
            }
            
            if (!Instance.Initialize || !Instance.Shutdown || !Instance.Tick || !Instance.Render || !Instance.OnWindowResize)
            {
                LOG_FATAL("One or more application callbacks were not assigned. Cannot proceed until all callbacks for the app are assigned");
                ReturnVal = 1;
                goto Shutdown_Lvl2;
            }

            Instance.Arguments = Arguments;

            #ifdef _DEBUG
            Clock StartupClock;
            Clock_Start(&StartupClock);
            #endif

            if (!Application_Create(&Instance))
            {
                LOG_FATAL("Failed to create application. Aborting...");
                ReturnVal = 1;
                goto Shutdown_Lvl2;
            }
            
            #ifdef _DEBUG
            Clock_Tick(&StartupClock);
            StringLocal(TimeString, 16);
            Time_ToString(StartupClock.ElapsedTime, true, &TimeString);
            LOG_INFO("Engine Startup Time: %s", TimeString.Data);
            #endif
        }
        
        Application_Run();

        LOG_SUCCESS("Engine Shutdown");
        #endif // HEADLESS

Shutdown_Lvl2:
        #ifndef NO_LOG
        Logging_Shutdown();
        MemFree(LogSubsystemState, MemoryTag_Engine);
        #endif

#ifndef NO_LOG
Shutdown_Lvl1:
#endif
        Memory_Shutdown();
    }
    #ifndef HEADLESS
    while (Application_ShouldRestart());
    #endif

Shutdown_Lvl0:
    Platform_MemFree(EngineMemory);

    Platform_Abort(ReturnVal);

    #if USE_MAIN
    return (i32)ReturnVal;
    #endif
}

#pragma clang diagnostic pop

#endif // META_GENERATED
