@echo off

set CompilerFlags= -std=c17 -Os -fdeclspec -fno-exceptions -fno-math-errno -fdiagnostics-absolute-paths -fno-delete-null-pointer-checks -fno-strict-overflow -fno-strict-aliasing -fstack-protector-strong -Wall -Wextra -Wshadow -Wconversion -Wpedantic -Winfinite-recursion -Wmissing-prototypes -Warray-bounds -Wmisleading-indentation -Wunused -Wuninitialized -Wno-empty-translation-unit -Wno-gnu-zero-variadic-macro-arguments -Wno-unused-parameter -Wunused-function -Werror=vla -Werror=alloca -Werror=implicit-function-declaration -Werror=pointer-arith -Werror=shadow -Werror=uninitialized -Werror=array-bounds -Werror=implicit -Werror=incompatible-pointer-types -Werror=int-conversion -Werror=float-conversion -Werror=shorten-64-to-32 -mno-stack-arg-probe -mstack-probe-size=999999999 -ferror-limit=1 -DNO_ASSERT -DNO_PROFILING -D_NO_CRT_STDIO_INLINE -DRIFT_STATIC -DNO_LOG_FILE -DWIN32_LEAN_AND_MEAN -DRIFTBUILD_VERSION_STRING=\"0.9.6-alpha\" -DRIFTBUILD_MAJOR_VERSION=0 -DRIFTBUILD_MINOR_VERSION=9 -DRIFTBUILD_PATCH_VERSION=6 -ISource/Core -ISource/Libraries/Vendor

echo Compiling sources (Windows)

clang "Source/Core/Clock/Clock.c" "Source/Core/Math/Math.c" "Source/Core/Memory/Memory.c" "Source/Core/Memory/Allocators.c" "Source/Core/String/StringUtils.c" "Source/Core/Structures/Containers.c" "Source/Core/Log.c" "Source/Core/Globals.c" "Source/Core/EngineUtils.c" "Source/Core/Platform/Platform_Windows.c" "Source/Program.c" "Source/CBackend.c" "Source/MSVCBackend.c" "Source/Parse.c" "Source/Exporter.c" %CompilerFlags% -o RiftBuild.exe -nostdlib -Wl,-entry:ProgramStart,-subsystem:console -Xlinker /stack:0x400000,0x400000 -lkernel32 -luser32 -lopengl32 -lshell32 -lgdi32 -lcomdlg32 -lcomctl32 -lws2_32 -lwinmm -lnetapi32 -lole32 -ladvapi32 -lwldap32 -lcrypt32 -lrpcrt4 -lshlwapi -ldbghelp -lbcrypt -lversion -limm32 -lcfgmgr32 -lsetupapi -loleaut32 -luuid -lodbc32 -lodbccp32 -ldelayimp -lpathcch || exit /b 1

echo [32m  Done: RiftBuild.exe[0m

:: pause if double clicked
setlocal enabledelayedexpansion
set testl=%cmdcmdline:"=%
set testr=!testl:%~nx0=!
if not "%testl%" == "%testr%" pause
