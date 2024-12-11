#!/bin/sh

set -e

if [ -z "$(command -v clang)" ]; then
    printf "\"clang\" not found - download the latest release at: https://releases.llvm.org/download.html\n"
    exit 1
fi

Platform="Unknown"
unamestr=$(uname)
if [ "$unamestr" = 'Linux' ]; then
    Platform='Linux'
    LinuxLinkerFlags="-LSource/Libraries/Linux -luuidS -lpthread"

   if [ -f /usr/bin/gnome-terminal ]; then
        LinuxDEDefines="-DPLATFORM_LINUX_GNOME"
   elif [ -f /usr/bin/konsole ]; then
        LinuxDEDefines="-DPLATFORM_LINUX_KDE"
   fi

elif [ "$unamestr" = 'Darwin' ]; then
    Platform='Mac'
    MacLinkerFlags="-framework Foundation"
elif [ "$unamestr" = 'OpenBSD' ]; then
    Platform='BSD'
    BSDLinkerFlags="-lpthread"

    # the compiler (and specifically on OpenBSD) for some reason trips up and replaces memmove with memcpy when using -O1 or higher optimizations causing a SIGABRT crash in memcpy because of overlapping memory whenever i remove something from my dynamic array. Obviously the fix is to use memmove, but it straight up yeets it out of existance when you turn on optimizations. **Every** other OS doesnt seem to have this problem with my code except this shitty one, (it was a pain to install as well compared to NetBSD and FreeBSD), spent about two hours tryin to fix it... i just wanna punch the screen...
    MiscFlags='-fno-builtin-memcpy' # prevents clang/gcc from replacing memmove with memcpy
elif [ "$unamestr" = 'NetBSD' ]; then
    Platform='BSD'
    BSDLinkerFlags="-lpthread"
elif [ "$unamestr" = 'FreeBSD' ]; then
    Platform='BSD'
    BSDLinkerFlags="-lpthread"
else
    printf "\n[ERROR] Compiling on \"$unamestr\" is not supported.\n"
    printf "\nHere is a list of supported platforms:\n"
    printf "  Windows (7 and above)\n  Linux   (All Debian, Red Hat, Fedora, SUSE and Arch)\n  macOS   (10.12 and above)\n  FreeBSD\n  NetBSD\n  OpenBSD\n"
    exit 1
fi

# clang only flags, for reference
# -fdeclspec -fdiagnostics-absolute-paths -ferror-limit=1 -Wno-empty-translation-unit -Werror=shorten-64-to-32
# -Werror=compare-distinct-pointer-types -Wpedantic 

CompilerFlags="-std=c99 -Os ${MiscFlags} -fno-omit-frame-pointer -fno-exceptions -fno-math-errno -fno-strict-overflow -fno-strict-aliasing -Wall -Wextra -Wshadow -Wconversion -Wmissing-prototypes -Wno-nonportable-include-path -Warray-bounds -Wno-variadic-macros -Wno-missing-field-initializers -Wno-missing-braces -Wno-typedef-redefinition -Wno-gnu-zero-variadic-macro-arguments -Wno-unused-parameter -Wunused-function -Werror=vla -Werror=implicit-function-declaration -Werror=pointer-arith -Werror=shadow -Werror=uninitialized -Werror=array-bounds -Werror=implicit -Werror=incompatible-pointer-types -Werror=int-conversion -fPIE -DNO_ASSERT -DNO_PROFILING -D_NO_CRT_STDIO_INLINE -DRIFT_STATIC -DRIFTBUILD_VERSION_STRING=\"0.9.9-alpha\" -DRIFTBUILD_MAJOR_VERSION=0 -DRIFTBUILD_MINOR_VERSION=9 -DRIFTBUILD_PATCH_VERSION=9 -ISource ${LinuxIncludeFlags}"

printf "Compiling sources (${Platform})\n"

clang "Source/Core/Memory.c" "Source/Core/StringUtils.c" "Source/Core/Log.c" "Source/Core/Globals.c" "Source/Core/Platform_Core.c" "Source/Core/Platform_Unix.c" "Source/Core/Platform_${Platform}.c" "Source/Program.c" "Source/Backend.c" "Source/Parse.c" "Source/Exporter.c" ${CompilerFlags} ${LinuxDEDefines} ${CoverageCompilerFlags} -o riftbuild ${LinuxLinkerFlags} ${BSDLinkerFlags} ${MacLinkerFlags} -Wl,-rpath,'$ORIGIN'

SCRIPT_PATH="$(cd "$(dirname "$0")" && pwd)"
printf "\033[0;32m  Done: ${SCRIPT_PATH}/riftbuild\033[0m\n"
