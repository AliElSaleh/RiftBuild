// Copyright (c) Artisan Softworks
// Licensed under the BSD 3-Clause License. See the LICENSE file for details.

#ifndef FUZZ_H
#define FUZZ_H

#ifndef UNITY_BUILD
#include "Core/EngineTypes.h"
#endif

// A seeded generator, so that a run is a pure function of its seed. Every number the harness
// needs comes out of one of these, and nothing in the harness reads the clock to make a choice.
// That is what lets a crash from iteration 90000 of seed 7 come back with "--seed 7".
STRUCT(FuzzRandom)
{
    u64 State;
};

// Seed 0 gives the same stream as seed 1, because the generator cannot leave a zero state.
void FuzzRandom_Seed(FuzzRandom* Random, u64 Seed);

NO_DISCARD u64 FuzzRandom_Next(FuzzRandom* Random);

// Returns a number below Bound. A Bound of 0 gives 0.
NO_DISCARD u32 FuzzRandom_Below(FuzzRandom* Random, u32 Bound);

// Builds one mutated build file in OutText from two corpus entries, and names the mutation it
// chose in OutMutationName. SourceText is the entry under mutation and SpliceText is a second
// entry that some mutations paste from - pass the same entry twice when there is only one.
// OutText must be a reserved buffer, and the mutation never writes past its capacity. Both out
// parameters are required. The result is bytes, not valid syntax: a mutation is free to make
// text that no build file would ever contain, which is the point of it.
void FuzzMutator_Mutate(FuzzRandom* Random,
                        const String SourceText,
                        const String SpliceText,
                        String* OutText,
                        String* OutMutationName);

#endif // FUZZ_H
