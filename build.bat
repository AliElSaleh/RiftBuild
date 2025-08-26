@echo off

where /q clang || (
  echo "clang" not found - download the latest release at: https://releases.llvm.org/download.html
  goto :end
)

set ScriptPath=%~dp0
set CompilerFlags=-std=c99 -fno-builtin-memcpy -fno-omit-frame-pointer -fno-exceptions -fno-math-errno -funroll-loops -fno-rtti -fno-strict-overflow -fno-strict-aliasing -Wall -Wextra -Wshadow -Wconversion -Wmissing-prototypes -Wunused -Wuninitialized -Werror -Wpedantic -Wno-typedef-redefinition -Wno-unused-parameter -Wno-gnu-zero-variadic-macro-arguments -Wno-unused-command-line-argument -nostdlib -fno-builtin -ffreestanding -msse2 -mstack-probe-size=999999999 -Os -finline-functions -finline-hint-functions
set IncludeFlags=-ISource
set LinkerFlags=-nostdlib -Wl,-entry:EntryPoint,-subsystem:console -Xlinker /stack:0x800000,0x800000
set Defines=-DRIFT_STATIC -DNO_ASSERT -D_NO_CRT_STDIO_INLINE -DRIFTBUILD_VERSION_STRING=\"0.1.0-beta\" -DRIFTBUILD_MAJOR_VERSION=0 -DRIFTBUILD_MINOR_VERSION=1 -DRIFTBUILD_PATCH_VERSION=0
set Libraries=-lkernel32 -luser32 -lshell32 -lole32 -ladvapi32 -lntdll -lshlwapi -lbcrypt -loleaut32
set LibraryPaths=

echo Compiling sources (Windows)

clang "Source/Core/Memory.c" "Source/Core/StringUtils.c" "Source/Core/Log.c" "Source/Core/Platform_Core.c" "Source/Core/Platform_Windows.c" "Source/Program.c" "Source/Backend.c" "Source/Parse.c" "Source/Exporter.c" "Source/MicrosoftCraziness.c" %CompilerFlags% %Defines% %IncludeFlags% -o RiftBuild.exe %LinkerFlags% %LibraryPaths% %Libraries% || goto end

echo [32m  Done: %ScriptPath%RiftBuild.exe[0m

:end
:: pause if we double clicked this in a file explorer
setlocal enabledelayedexpansion
set testl=%cmdcmdline:"=%
set testr=!testl:%~nx0=!
if not "%testl%" == "%testr%" pause
