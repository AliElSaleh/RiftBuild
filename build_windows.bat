@echo off

if not exist Build mkdir Build
if not exist Build\Dist mkdir Build\Dist
if not exist Intermediate mkdir Intermediate

set CompilerFlags= -std=c17 -Os -fdeclspec -fno-exceptions -fno-math-errno -fdiagnostics-absolute-paths -fno-delete-null-pointer-checks -fno-strict-overflow -fno-strict-aliasing -fstack-protector-strong -Wall -Wextra -Wshadow -Wconversion -Wpedantic -Winfinite-recursion -Wmissing-prototypes -Warray-bounds -Wmisleading-indentation -Wunused -Wuninitialized -Wno-empty-translation-unit -Wno-gnu-zero-variadic-macro-arguments -Wno-unused-parameter -Wunused-function -Werror=vla -Werror=alloca -Werror=implicit-function-declaration -Werror=pointer-arith -Werror=shadow -Werror=uninitialized -Werror=array-bounds -Werror=implicit -Werror=incompatible-pointer-types -Werror=int-conversion -Werror=float-conversion -Werror=shorten-64-to-32 -mno-stack-arg-probe -mstack-probe-size=999999999 -ferror-limit=1 -DNO_ASSERT -DNO_PROFILING -D_NO_CRT_STDIO_INLINE -DRIFT_STATIC -DNO_LOG_FILE -DWIN32_LEAN_AND_MEAN -DRIFTBUILD_VERSION_STRING=\"0.9.6\" -DRIFTBUILD_MAJOR_VERSION=0 -DRIFTBUILD_MINOR_VERSION=9 -DRIFTBUILD_PATCH_VERSION=6 -ISource/Core -ISource/Libraries/Vendor

echo Compiling sources (Windows)

clang -c "Source/Core/Clock/Clock.c"               %CompilerFlags% -o "Intermediate/Clock.c.o"            || exit /b 1
clang -c "Source/Core/Math/Math.c"                 %CompilerFlags% -o "Intermediate/Math.c.o"             || exit /b 1
clang -c "Source/Core/Memory/Memory.c"             %CompilerFlags% -o "Intermediate/Memory.c.o"           || exit /b 1
clang -c "Source/Core/Memory/Allocators.c"         %CompilerFlags% -o "Intermediate/Allocators.c.o"       || exit /b 1
clang -c "Source/Core/String/StringUtils.c"        %CompilerFlags% -o "Intermediate/StringUtils.c.o"      || exit /b 1
clang -c "Source/Core/Structures/Containers.c"     %CompilerFlags% -o "Intermediate/Containers.c.o"       || exit /b 1
clang -c "Source/Core/Log.c"                       %CompilerFlags% -o "Intermediate/Log.c.o"              || exit /b 1
clang -c "Source/Core/Globals.c"                   %CompilerFlags% -o "Intermediate/Globals.c.o"          || exit /b 1
clang -c "Source/Core/EngineUtils.c"               %CompilerFlags% -o "Intermediate/EngineUtils.c.o"      || exit /b 1
clang -c "Source/Core/Platform/Platform_Windows.c" %CompilerFlags% -o "Intermediate/Platform_Windows.c.o" || exit /b 1
clang -c "Source/Program.c"                        %CompilerFlags% -o "Intermediate/Program.c.o"          || exit /b 1
clang -c "Source/CBackend.c"                       %CompilerFlags% -o "Intermediate/CBackend.c.o"         || exit /b 1
clang -c "Source/MSVCBackend.c"                    %CompilerFlags% -o "Intermediate/MSVCBackend.c.o"      || exit /b 1
clang -c "Source/Parse.c"                          %CompilerFlags% -o "Intermediate/Parse.c.o"            || exit /b 1
clang -c "Source/Exporter.c"                       %CompilerFlags% -o "Intermediate/Exporter.c.o"         || exit /b 1

echo Building binary
clang -o Build/Dist/RiftBuild.exe "Intermediate/Clock.c.o" "Intermediate/Math.c.o" "Intermediate/Memory.c.o" "Intermediate/Allocators.c.o" "Intermediate/StringUtils.c.o" "Intermediate/Containers.c.o" "Intermediate/Log.c.o" "Intermediate/Globals.c.o" "Intermediate/EngineUtils.c.o" "Intermediate/Platform_Windows.c.o" "Intermediate/Program.c.o" "Intermediate/CBackend.c.o" "Intermediate/MSVCBackend.c.o" "Intermediate/Parse.c.o" "Intermediate/Exporter.c.o" -nostdlib -Wl,-entry:ProgramStart,-subsystem:console -Xlinker /stack:0x400000,0x400000 -lkernel32 -luser32 -lopengl32 -lshell32 -lgdi32 -lcomdlg32 -lcomctl32 -lws2_32 -lwinmm -lnetapi32 -lole32 -ladvapi32 -lwldap32 -lcrypt32 -lrpcrt4 -lshlwapi -ldbghelp -lbcrypt -lversion -limm32 -lcfgmgr32 -lsetupapi -loleaut32 -luuid -lodbc32 -lodbccp32 -ldelayimp -lpathcch || exit /b 1

echo [32m  Done: Build/Dist/RiftBuild.exe[0m
