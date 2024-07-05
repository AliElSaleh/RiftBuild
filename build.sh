#!/bin/sh

set -e

Platform='unknown'
unamestr=$(uname)
if [ "$unamestr" = 'Linux' ]; then
   Platform='Linux'
   LinuxLinkerFlags="-LSource/Libraries/Linux -lUUIDS"

   if [ -f /usr/bin/gnome-terminal ]; then
      LinuxDEDefines="-DPLATFORM_LINUX_GNOME"
   elif [ -f /usr/bin/konsole ]; then
      LinuxDEDefines="-DPLATFORM_LINUX_KDE"
   fi

elif [ "$unamestr" = 'Darwin' ]; then
   Platform='Mac'
elif [ "$unamestr" = 'FreeBSD' ]; then
   Platform='BSD'
fi

CompilerFlags="-std=c17 -Os -fdeclspec -fno-exceptions -fno-math-errno -fdiagnostics-absolute-paths -fno-delete-null-pointer-checks -fno-strict-overflow -fno-strict-aliasing -fstack-protector-strong -Wall -Wextra -Wshadow -Wconversion -Wpedantic -Winfinite-recursion -Wmissing-prototypes -Warray-bounds -Wmisleading-indentation -Wunused -Wuninitialized -Wno-empty-translation-unit -Wno-gnu-zero-variadic-macro-arguments -Wno-unused-parameter -Wunused-function -Werror=vla -Werror=alloca -Werror=implicit-function-declaration -Werror=pointer-arith -Werror=shadow -Werror=uninitialized -Werror=array-bounds -Werror=implicit -Werror=incompatible-pointer-types -Werror=int-conversion -Werror=float-conversion -Werror=shorten-64-to-32 -mno-stack-arg-probe -mstack-probe-size=999999999 -ferror-limit=1 -fPIE -DNO_ASSERT -DNO_PROFILING -D_NO_CRT_STDIO_INLINE -DRIFT_STATIC -DNO_LOG_FILE -DRIFTBUILD_VERSION_STRING=\"0.9.6-alpha\" -DRIFTBUILD_MAJOR_VERSION=0 -DRIFTBUILD_MINOR_VERSION=9 -DRIFTBUILD_PATCH_VERSION=6 -ISource/Core -ISource/Libraries/Vendor -ISource/Libraries/Linux"

printf "Compiling sources (${Platform})\n"

clang "Source/Core/Clock/Clock.c" "Source/Core/Memory/Memory.c" "Source/Core/String/StringUtils.c" "Source/Core/Log.c" "Source/Core/Globals.c" "Source/Core/Platform/Platform_${Platform}.c" "Source/Program.c" "Source/Backend.c" "Source/Parse.c" "Source/Exporter.c" ${CompilerFlags} ${LinuxDEDefines} -o riftbuild ${LinuxLinkerFlags} -Wl,-rpath,'$ORIGIN'

printf "\033[0;32m  Done: riftbuild\033[0m\n"
