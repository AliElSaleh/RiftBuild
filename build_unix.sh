#! /bin/sh

set -e

mkdir -p Build
mkdir -p Build/Dist
mkdir -p Intermediate

platform='unknown'
unamestr=$(uname)
if [ "$unamestr" = 'Linux' ]; then
   platform='Linux'
   LinuxLinkerFlags="-LSource/Libraries/Linux -lUUIDS"
elif [ "$unamestr" = 'Darwin' ]; then
   platform='Mac'
elif [ "$unamestr" = 'FreeBSD' ]; then
   platform='BSD'
fi

CompilerFlags="-std=c17 -Os -fdeclspec -fno-exceptions -fno-math-errno -fdiagnostics-absolute-paths -fno-delete-null-pointer-checks -fno-strict-overflow -fno-strict-aliasing -fstack-protector-strong -Wall -Wextra -Wshadow -Wconversion -Wpedantic -Winfinite-recursion -Wmissing-prototypes -Warray-bounds -Wmisleading-indentation -Wunused -Wuninitialized -Wno-empty-translation-unit -Wno-gnu-zero-variadic-macro-arguments -Wno-unused-parameter -Wunused-function -Werror=vla -Werror=alloca -Werror=implicit-function-declaration -Werror=pointer-arith -Werror=shadow -Werror=uninitialized -Werror=array-bounds -Werror=implicit -Werror=incompatible-pointer-types -Werror=int-conversion -Werror=float-conversion -Werror=shorten-64-to-32 -mno-stack-arg-probe -mstack-probe-size=999999999 -ferror-limit=1 -fPIE -DNO_ASSERT -DNO_PROFILING -D_NO_CRT_STDIO_INLINE -DRIFT_STATIC -DNO_LOG_FILE -DRIFTBUILD_VERSION_STRING=\"0.9.6\" -DRIFTBUILD_MAJOR_VERSION=0 -DRIFTBUILD_MINOR_VERSION=9 -DRIFTBUILD_PATCH_VERSION=6 -ISource/Core -ISource/Libraries/Vendor -ISource/Libraries/Linux"

printf "Compiling sources (${platform})\n"

clang "Source/Core/Clock/Clock.c" "Source/Core/Math/Math.c" "Source/Core/Memory/Memory.c" "Source/Core/Memory/Allocators.c" "Source/Core/String/StringUtils.c" "Source/Core/Structures/Containers.c" "Source/Core/Log.c" "Source/Core/Globals.c" "Source/Core/EngineUtils.c" "Source/Core/Platform/Platform_${platform}.c" "Source/Program.c" "Source/CBackend.c" "Source/MSVCBackend.c" "Source/Parse.c" "Source/Exporter.c" %CompilerFlags% -o Build/Dist/riftbuild ${LinuxLinkerFlags} -Wl,-rpath,'$ORIGIN'

printf "\033[0;32m  Done: Build/Dist/riftbuild\033[0m\n"
