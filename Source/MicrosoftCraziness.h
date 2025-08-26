/*
   Pure C port of Jonathan Blow's microsoft_craziness.h helper header file library.
   - No C++; only Win32 + COM + Registry.

   https://gist.github.com/ActuallyaDeviloper/cd25b190743234d58079d6b08a8631e3

*/

#ifndef MICROSOFT_CRAZINESS_H
#define MICROSOFT_CRAZINESS_H

#ifndef UNITY_BUILD
#include "Core/EngineTypes.h"
#endif

#if PLATFORM_WINDOWS

STRUCT(MicrosoftVisualStudioPaths)
{
    String InstallPath;      // e.g. VS install root
    String ToolBasePath;     // e.g. ..\VC\Tools\MSVC\<ver>
    String ExePath;          // e.g. ..\VC\Tools\MSVC\<ver>\bin\Hostx64\x64
    String LibraryPath;      // e.g. ..\VC\Tools\MSVC\<ver>\lib\x64
    String IncludePath;      // e.g. ..\VC\Tools\MSVC\<ver>\include
};

STRUCT(MicrosoftWindowsSDKPaths)
{
    String RootPath;           // e.g. ..\10.0.22621.0
    String BinPath;            // e.g. ..\10.0.22621.0\bin
    String IncludePath;        // e.g. ..\10.0.22621.0\Include
    String UM_LibraryPath;     // e.g. ..\10.0.22621.0\Lib\um\x64
    String UCRT_LibraryPath;   // e.g. ..\10.0.22621.0\Lib\ucrt\x64
    i32 Version;               // 0 if not found, else 10 or 8
    i32 Padding;
};

bool FindVisualStudio(LinearAllocator* Arena, MicrosoftVisualStudioPaths* Result);
bool FindWindowsSDK(LinearAllocator* Arena, MicrosoftWindowsSDKPaths* Result);

#endif

#endif
