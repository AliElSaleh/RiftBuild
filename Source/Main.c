// Copyright (c) Artisan Softworks
// Licensed under the BSD 3-Clause License. See the LICENSE file for details.

#ifndef UNITY_BUILD
#include "Backend.h"

#include "Core/EntryPoint.h"
#endif

#define RIFTBUILD_ENGINE_MEMORY  Kibibytes(128)
#define RIFTBUILD_ENGINE_SCRATCH Kibibytes(8)

int main(i32 ArgC, char* ArgV[], char* ArgEnv[])
{
    return Entry_Run(RunApplication,
                    RIFTBUILD_ENGINE_MEMORY,
                    RIFTBUILD_ENGINE_SCRATCH,
                    ArgC, ArgV, ArgEnv);
}
