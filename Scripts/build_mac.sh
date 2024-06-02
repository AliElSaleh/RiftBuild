#! /bin/sh

mkdir -p Build
mkdir -p Build/Dist
mkdir -p Intermediate

CompilerFlags="-std=c17 -Os -fdeclspec -fno-exceptions -fno-math-errno -fdiagnostics-absolute-paths -fno-delete-null-pointer-checks -fno-strict-overflow -fno-strict-aliasing -fstack-protector-strong -Wall -Wextra -Wshadow -Wconversion -Wpedantic -Winfinite-recursion -Wmissing-prototypes -Warray-bounds -Wmisleading-indentation -Wunused -Wuninitialized -Wno-empty-translation-unit -Wno-gnu-zero-variadic-macro-arguments -Wno-unused-parameter -Wunused-function -Werror=vla -Werror=alloca -Werror=implicit-function-declaration -Werror=pointer-arith -Werror=shadow -Werror=uninitialized -Werror=array-bounds -Werror=implicit -Werror=incompatible-pointer-types -Werror=int-conversion -Werror=float-conversion -Werror=shorten-64-to-32 -mno-stack-arg-probe -mstack-probe-size=999999999 -ferror-limit=1 -fPIE -DNO_ASSERT -DNO_PROFILING -D_NO_CRT_STDIO_INLINE -DHEADLESS -DRIFT_STATIC -DNO_LOG_FILE -DRIFTBUILD_VERSION_STRING=\"0.9.5\" -DRIFTBUILD_MAJOR_VERSION=0 -DRIFTBUILD_MINOR_VERSION=9 -DRIFTBUILD_PATCH_VERSION=5 -ISource/Core -ILibraries/Vendor"

echo "Compiling sources"

clang -c "Source/Core/Clock/Clock.c"           $CompilerFlags -o "Intermediate/Clock.c.o"
clang -c "Source/Core/Math/Math.c"             $CompilerFlags -o "Intermediate/Math.c.o"
clang -c "Source/Core/Memory/Memory.c"         $CompilerFlags -o "Intermediate/Memory.c.o"
clang -c "Source/Core/Memory/Allocators.c"     $CompilerFlags -o "Intermediate/Allocators.c.o"
clang -c "Source/Core/String/StringUtils.c"    $CompilerFlags -o "Intermediate/StringUtils.c.o"
clang -c "Source/Core/Structures/Containers.c" $CompilerFlags -o "Intermediate/Containers.c.o"
clang -c "Source/Core/Log.c"                   $CompilerFlags -o "Intermediate/Log.c.o"
clang -c "Source/Core/Globals.c"               $CompilerFlags -o "Intermediate/Globals.c.o"
clang -c "Source/Core/EngineUtils.c"           $CompilerFlags -o "Intermediate/EngineUtils.c.o"
clang -c "Source/Core/Platform/Platform_Mac.m" $CompilerFlags -o "Intermediate/Platform_Mac.m.o"
clang -c "Source/Program.c"                    $CompilerFlags -o "Intermediate/Program.c.o"
clang -c "Source/CBackend.c"                   $CompilerFlags -o "Intermediate/CBackend.c.o"
clang -c "Source/MSVCBackend.c"                $CompilerFlags -o "Intermediate/MSVCBackend.c.o"
clang -c "Source/Parse.c"                      $CompilerFlags -o "Intermediate/Parse.c.o"
clang -c "Source/Exporter.c"                   $CompilerFlags -o "Intermediate/Exporter.c.o"

echo "Building binary"
clang -o Build/Dist/riftbuild "Intermediate/Clock.c.o" "Intermediate/Math.c.o" "Intermediate/Memory.c.o" "Intermediate/Allocators.c.o" "Intermediate/StringUtils.c.o" "Intermediate/Containers.c.o" "Intermediate/Log.c.o" "Intermediate/Globals.c.o" "Intermediate/EngineUtils.c.o" "Intermediate/Platform_Mac.m.o" "Intermediate/Program.c.o" "Intermediate/CBackend.c.o" "Intermediate/MSVCBackend.c.o" "Intermediate/Parse.c.o" "Intermediate/Exporter.c.o" -Wl,-rpath,'$ORIGIN'

echo "  Done: Build/Dist/riftbuild"
