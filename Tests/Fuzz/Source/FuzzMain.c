// Copyright (c) Artisan Softworks
// Licensed under the BSD 3-Clause License. See the LICENSE file for details.

#include "Fuzz.h"

#ifndef UNITY_BUILD
#include "Backend.h"

#include "Core/Allocators.h"
#include "Core/Array.h"
#include "Core/Clock.h"
#include "Core/Filesystem.h"
#include "Core/Log.h"
#include "Core/Memory.h"
#include "Core/Platform.h"
#include "Core/EntryPoint.h"
#include "Core/StringUtils.h"
#endif

constant
{
    FuzzDefaultIterations  = 100000,
    FuzzMaxCorpusEntries   = 1024,
    FuzzMaxFindings        = 64,
    FuzzMaxRootSearchDepth = 8
};

// Corpus text, the mutation buffer and the entry list all live here. The parse arena is separate,
// because the harness resets that one between inputs and this one must last the whole run.
#define FUZZ_HARNESS_ARENA_SIZE Mebibytes(16)

// A parse that takes longer than this is a finding. A 48KiB build file parses in about a
// millisecond, so a parse anywhere near this number found a loop that the input made quadratic.
#define FUZZ_SLOW_PARSE_SECONDS 0.25

STRUCT(FuzzOptions)
{
    String CorpusDirectory;
    String InputPath;
    String OutputDirectory;
    u64    Seed;
    u32    NumIterations;
    bool   bTrace;
    u8     Padding[3];
};

STRUCT(FuzzCorpus)
{
    LinearAllocator* Arena;
    TArray(String)   Entries;
    u32              NumFilesSkipped;
    u8               Padding[4];
};

STRUCT(FuzzReport)
{
    u32 NumInputs;
    u32 NumAccepted;
    u32 NumRejected;
    u32 NumUnstable;
    u32 NumSlow;
    u32 NumFindingsSaved;
    u32 PeakArenaBytes;
    u8  Padding[4];
};

// What one input needs to run: the arena the parser works out of, the tally it adds itself to,
// and where a finding goes. Both pointers stay alive for the whole run.
STRUCT(FuzzRun)
{
    LinearAllocator* ParseArena;
    FuzzReport*      Report;
    String           OutputDirectory;
};

// Reads a whole build file into Arena, and reports whether it got there. A file over
// MAX_BUILD_FILE_SIZE is refused, because the parse arena cannot hold one and the harness must
// not report its own overrun as a parser defect. A file that no longer fits in Arena is refused
// as well: LinearAllocator_Allocate does not fail, it walks off the end.
NO_DISCARD static bool Internal_ReadCorpusFile(LinearAllocator* Arena, const String FilePath, String* OutText)
{
    bool bSuccess = false;

    FileHandle Handle = {0};
    if (Filesystem_Open(FilePath, FileMode_Read, &Handle))
    {
        usize FileSize = 0;

        const bool bGotSize = Filesystem_GetFileSize(Handle, &FileSize);

        // the reserve takes the text plus a terminator, and the bump can take up to 8 bytes of
        // alignment on top of that
        const usize Remaining  = Arena->TotalSize - Arena->Allocated;
        const usize NeededSize = FileSize + 1 + 8;

        if (bGotSize && FileSize > 0 && FileSize <= MAX_BUILD_FILE_SIZE && NeededSize <= Remaining)
        {
            String Text = String_Reserve(Arena, (u32)FileSize);

            usize NumBytesRead = 0;
            if (Filesystem_ReadEntireFile(Handle, Text.Data, &NumBytesRead))
            {
                Text.Length = (u32)Min(NumBytesRead, FileSize);
                *OutText    = Text;
                bSuccess    = true;
            }
        }

        Filesystem_Close(&Handle);
    }

    return bSuccess;
}

static bool Internal_CollectCorpusFile(const String FullPath,
                                       const String RelativePath,
                                       const String FileName,
                                       u64 FileSize,
                                       bool bIsDirectory,
                                       void* UserData)
{
    xx RelativePath;
    xx FileSize;

    FuzzCorpus* Corpus = (FuzzCorpus*)UserData;

    const bool bIsBuildFile = String_EndsWith(FileName, S(".build"), false) ||
                              String_EndsWith(FileName, S(".buildvars"), false);

    if (!bIsDirectory && bIsBuildFile && Array_Num(Corpus->Entries) < FuzzMaxCorpusEntries)
    {
        String Text = String_Null();
        if (Internal_ReadCorpusFile(Corpus->Arena, FullPath, &Text))
        {
            Array_Add(Corpus->Entries, Text);
        }
        else
        {
            Corpus->NumFilesSkipped += 1;
        }
    }

    return true;
}

static void Internal_SaveFinding(const String OutputDirectory, const String Kind, u32 Index, const String Text)
{
    StringLocal(FileName, 64);
    String_Format(&FileName, S("%S-%u.build"), Kind, Index);

    StringLocal(FilePath, MAX_PATH_LENGTH);
    String_BuildPath(&FilePath, OutputDirectory, FileName);

    FileHandle Handle = {0};
    if (Filesystem_Open(FilePath, FileMode_Write, &Handle))
    {
        usize NumBytesWritten = 0;
        if (!Filesystem_Write(Handle, Text.Length, Text.Data, &NumBytesWritten))
        {
            LOG_ERROR("Failed to write the saved input '%S'", FilePath);
        }

        Filesystem_Close(&Handle);
    }
    else
    {
        LOG_ERROR("Failed to create the saved input '%S'", FilePath);
    }
}

// Hands the parser an arena in the state the tool hands it one: empty and zeroed. The tool takes
// this arena from Platform_MemAllocZero once per build file, so a parser that reads a byte it did
// not write reads a zero there. Without this the harness would report the bytes of the input
// before it as a finding, and the tool could never reach that state.
static void Internal_ResetParseArena(LinearAllocator* ParseArena)
{
    // zero first, because LinearAllocator_Reset poisons the memory it gives back under asan
    MemZero(ParseArena->Memory, ParseArena->Allocated);

    LinearAllocator_Reset(ParseArena, 0);
}

// Parses Text twice and reports what came out. Twice, because the only thing the harness knows
// about a malformed build file is that the answer must not change between runs: a parse that
// accepts text it rejected a moment ago left state behind the arena, in a global or in a shared
// node, and that state reaches the next build file of the same build. The caller owns ParseArena,
// and this resets it, so nothing of the parse survives the call.
static void Internal_MeasureParse(LinearAllocator* ParseArena,
                                  const String Text,
                                  const String ReportedPath,
                                  bool* OutAccepted,
                                  bool* OutStable,
                                  f64* OutSeconds,
                                  u32* OutArenaBytes)
{
    Clock ParseClock = {0};
    Clock_Start(&ParseClock);

    Logging_Disable();

    Internal_ResetParseArena(ParseArena);
    const bool bFirstAccepted = LexAndParseBuildText(ParseArena, Text, ReportedPath);

    const u32 ArenaBytes = (u32)ParseArena->Allocated;

    Internal_ResetParseArena(ParseArena);
    const bool bSecondAccepted = LexAndParseBuildText(ParseArena, Text, ReportedPath);

    Logging_Enable();

    Clock_Tick(&ParseClock);

    *OutAccepted   = bFirstAccepted;
    *OutStable     = bFirstAccepted == bSecondAccepted;
    *OutSeconds    = ParseClock.ElapsedTime;
    *OutArenaBytes = ArenaBytes;
}

// Runs one input, adds it to the tally in Run->Report and saves it when it is a finding. Label
// names where the input came from, for the message and for the name of the saved file.
static void Internal_RunInput(const FuzzRun* Run, const String Text, const String Label, u32 Index)
{
    bool bAccepted  = false;
    bool bStable    = false;
    f64  Seconds    = 0.0;
    u32  ArenaBytes = 0;
    Internal_MeasureParse(Run->ParseArena, Text, S("fuzz.build"),
                          &bAccepted, &bStable, &Seconds, &ArenaBytes);

    FuzzReport* Report = Run->Report;

    Report->NumInputs += 1;
    if (bAccepted)
    {
        Report->NumAccepted += 1;
    }
    else
    {
        Report->NumRejected += 1;
    }

    if (ArenaBytes > Report->PeakArenaBytes)
    {
        Report->PeakArenaBytes = ArenaBytes;
    }

    const bool bIsSlow  = Seconds > FUZZ_SLOW_PARSE_SECONDS;
    const bool bIsClean = bStable && !bIsSlow;

    if (!bStable)
    {
        Report->NumUnstable += 1;
        LOG_ERROR("[%u] %S gave an unstable parse (%u bytes).", Index, Label, Text.Length);
    }

    if (bIsSlow)
    {
        Report->NumSlow += 1;
        LOG_WARNING("[%u] %S took %fs to parse (%u bytes).", Index, Label, Seconds, Text.Length);
    }

    if (!bIsClean && Report->NumFindingsSaved < FuzzMaxFindings)
    {
        Internal_SaveFinding(Run->OutputDirectory, bStable ? S("slow") : S("unstable"), Index, Text);
        Report->NumFindingsSaved += 1;
    }
}

static void Internal_PrintUsage(void)
{
    LOG("RiftBuild parser fuzzer - feeds mutated .build text to the lexer and the parser.\n");
    LOG("  --corpus <dir>      directory to read seed .build files from");
    LOG("  --input <file>      parse one file and stop, instead of fuzzing");
    LOG("  --out <dir>         directory the harness saves findings into");
    LOG("  --seed <number>     seed of the run, so that a run repeats exactly");
    LOG("  --iterations <n>    number of mutated inputs to parse");
    LOG("  --trace             name every input and write it out before the parse");
    LOG_LINE_BREAK();
    LOG("A hard crash ends the process, so the input that caused it is gone. Run again with the");
    LOG("same --seed and with --trace, and last-input.build holds the input it died on.");
    LOG_LINE_BREAK();
}

// Walks up from the working directory until a parent holds the repository's own build file, and
// reports whether it found one. The default corpus and findings paths hang off the root, and the
// harness gets launched both from the root and from its own build directory, where the same
// relative path means somewhere else. OutRoot must hold MAX_PATH_LENGTH.
NO_DISCARD static bool Internal_FindRepositoryRoot(String* OutRoot)
{
    Platform_GetWorkingDirectory(OutRoot);

    bool bFound = false;

    for (u32 i = 0; i < FuzzMaxRootSearchDepth && !bFound; i++)
    {
        StringLocal(Marker, MAX_PATH_LENGTH);
        String_BuildPath(&Marker, *OutRoot, S("RiftBuild.build"));

        if (Filesystem_DoesFileExist(Marker))
        {
            bFound = true;
        }
        else
        {
            // the directory part always starts at index 0 of the same buffer, so this shortens
            // the path in place
            const String Parent = Filesystem_ExtractFilePath(*OutRoot, false);

            if (Parent.Length == 0 || Parent.Length == OutRoot->Length)
            {
                break;
            }

            String_Copy(OutRoot, Parent);
        }
    }

    return bFound;
}

// Fills OutOptions from the command line, and reports whether every argument was understood.
// DefaultCorpus and DefaultFindings are the paths to use when the command line names none; the
// caller owns their buffers, which must outlive OutOptions.
NO_DISCARD static bool Internal_ReadOptions(const StringArray Arguments,
                                           const String DefaultCorpus,
                                           const String DefaultFindings,
                                           FuzzOptions* OutOptions)
{
    bool bSuccess = true;

    OutOptions->CorpusDirectory = DefaultCorpus;
    OutOptions->InputPath       = String_Null();
    OutOptions->OutputDirectory = DefaultFindings;
    OutOptions->Seed            = 1;
    OutOptions->NumIterations   = FuzzDefaultIterations;
    OutOptions->bTrace          = false;

    for (u32 i = 0; i < Arguments.Num && bSuccess; i++)
    {
        const String Name      = StringArray_GetStringFromIndex(Arguments, i);
        const bool   bHasValue = (i + 1) < Arguments.Num;
        const String Value     = bHasValue ? StringArray_GetStringFromIndex(Arguments, i + 1) : String_Null();

        if (String_IsEqual(Name, S("--trace"), false))
        {
            OutOptions->bTrace = true;
        }
        else if (!bHasValue)
        {
            LOG_ERROR("'%S' needs a value after it.", Name);
            bSuccess = false;
        }
        else if (String_IsEqual(Name, S("--corpus"), false))
        {
            OutOptions->CorpusDirectory = Value;
            i += 1;
        }
        else if (String_IsEqual(Name, S("--input"), false))
        {
            OutOptions->InputPath = Value;
            i += 1;
        }
        else if (String_IsEqual(Name, S("--out"), false))
        {
            OutOptions->OutputDirectory = Value;
            i += 1;
        }
        else if (String_IsEqual(Name, S("--seed"), false))
        {
            bSuccess = String_ToU64(Value, &OutOptions->Seed);
            if (!bSuccess)
            {
                LOG_ERROR("'%S' is not a number.", Value);
            }
            i += 1;
        }
        else if (String_IsEqual(Name, S("--iterations"), false))
        {
            bSuccess = String_ToU32(Value, &OutOptions->NumIterations);
            if (!bSuccess)
            {
                LOG_ERROR("'%S' is not a number.", Value);
            }
            i += 1;
        }
        else
        {
            LOG_ERROR("'%S' is not an option of this program.", Name);
            bSuccess = false;
        }
    }

    return bSuccess;
}

static void Internal_LogReport(const FuzzReport* Report, const FuzzOptions* Options)
{
    LOG_LINE_BREAK();
    LOG("[Fuzz Results]");
    LOG("    Seed:             %llu", Options->Seed);
    LOG("    Inputs parsed:    %u", Report->NumInputs);
    LOG("    Accepted:         %u", Report->NumAccepted);
    LOG("    Rejected:         %u", Report->NumRejected);
    LOG("    Peak parse arena: %u of %u bytes", Report->PeakArenaBytes, (u32)BUILD_PARSE_SCRATCH_SIZE);

    if (Report->NumUnstable > 0)
    {
        LOG_ERROR("    *** %u UNSTABLE PARSES ***", Report->NumUnstable);
    }

    if (Report->NumSlow > 0)
    {
        LOG_WARNING("    *** %u SLOW PARSES ***", Report->NumSlow);
    }

    if (Report->NumFindingsSaved > 0)
    {
        LOG("    Saved %u inputs into %S", Report->NumFindingsSaved, Options->OutputDirectory);
    }

    if (Report->NumUnstable == 0 && Report->NumSlow == 0)
    {
        LOG_SUCCESS("    *** NO FINDINGS ***");
    }

    LOG_LINE_BREAK();
}

// Writes Text where a later run can find it, so that a crash leaves its input behind.
static void Internal_WriteTrace(const String OutputDirectory, const String Text)
{
    StringLocal(FilePath, MAX_PATH_LENGTH);
    String_BuildPath(&FilePath, OutputDirectory, S("last-input.build"));

    FileHandle Handle = {0};
    if (Filesystem_Open(FilePath, FileMode_Write, &Handle))
    {
        usize NumBytesWritten = 0;
        xx Filesystem_Write(Handle, Text.Length, Text.Data, &NumBytesWritten);
        Filesystem_Close(&Handle);
    }
}

static u32 RunFuzzer(const StringArray Arguments)
{
    Logging_ToggleLogFile(false);
    Logging_ToggleLogTimeStamp(false);
    Logging_ToggleLogCategory(false);

    u32 ExitCode = 0;

    // These buffers back the default paths in Options, so they live for the whole run.
    StringLocal(DefaultCorpus, MAX_PATH_LENGTH);
    StringLocal(DefaultFindings, MAX_PATH_LENGTH);
    {
        StringLocal(Root, MAX_PATH_LENGTH);
        if (Internal_FindRepositoryRoot(&Root))
        {
            String_BuildPath(&DefaultCorpus, Root, S("Tests/Feature Set"));
            String_BuildPath(&DefaultFindings, Root, S("Tests/Fuzz/Build/Findings"));
        }
        else
        {
            // no repository around us - fall back to paths relative to the working directory
            String_Copy(&DefaultCorpus, S("Tests/Feature Set"));
            String_Copy(&DefaultFindings, S("Tests/Fuzz/Build/Findings"));
        }
    }

    FuzzOptions Options = {0};
    if (!Internal_ReadOptions(Arguments, DefaultCorpus, DefaultFindings, &Options))
    {
        Internal_PrintUsage();
        ExitCode = 1;
    }

    void* HarnessMemory = NULL;
    void* ParseMemory   = NULL;

    if (ExitCode == 0)
    {
        HarnessMemory = Platform_MemAllocZero(FUZZ_HARNESS_ARENA_SIZE);
        ParseMemory   = Platform_MemAllocZero(BUILD_PARSE_SCRATCH_SIZE);

        if (!HarnessMemory || !ParseMemory)
        {
            LOG_ERROR("Failed to get memory for the fuzz harness.");
            ExitCode = 1;
        }
    }

    if (ExitCode == 0)
    {
        LinearAllocator HarnessArena = {0};
        LinearAllocator ParseArena   = {0};
        LinearAllocator_Create(FUZZ_HARNESS_ARENA_SIZE, HarnessMemory, &HarnessArena);
        LinearAllocator_Create(BUILD_PARSE_SCRATCH_SIZE, ParseMemory, &ParseArena);

        FuzzReport Report = {0};

        FuzzRun Run         = {0};
        Run.ParseArena      = &ParseArena;
        Run.Report          = &Report;
        Run.OutputDirectory = Options.OutputDirectory;

        // --input is the replay path: one named file, parsed as it is. It is how a saved finding
        // comes back, so it must reach the parser the same way a fuzzed input does.
        if (String_IsValid(Options.InputPath))
        {
            String Text = String_Null();
            if (Internal_ReadCorpusFile(&HarnessArena, Options.InputPath, &Text))
            {
                LOG("Parsing '%S' (%u bytes)...", Options.InputPath, Text.Length);

                Internal_RunInput(&Run, Text, S("input"), 0);

                LOG("Parser %S the input. The parse arena used %u of %u bytes.",
                    Report.NumAccepted > 0 ? S("accepted") : S("rejected"),
                    Report.PeakArenaBytes,
                    (u32)BUILD_PARSE_SCRATCH_SIZE);

                if (Report.NumUnstable > 0 || Report.NumSlow > 0)
                {
                    ExitCode = 1;
                }
            }
            else
            {
                LOG_ERROR("Could not read '%S', or it is over %u bytes.",
                          Options.InputPath, (u32)MAX_BUILD_FILE_SIZE);
                ExitCode = 1;
            }
        }
        else
        {
            // the mutation buffer and the entry list come out of the harness arena before the
            // corpus does, so that a corpus big enough to fill the arena cannot take their room
            String Input = String_Reserve(&HarnessArena, (u32)MAX_BUILD_FILE_SIZE);

            FuzzCorpus Corpus = {0};
            Corpus.Arena      = &HarnessArena;
            ArrayLocal_Arena(String, CorpusEntries, FuzzMaxCorpusEntries, &HarnessArena);
            Corpus.Entries    = CorpusEntries;

            Filesystem_IterateDirectory_Ex(Options.CorpusDirectory, Internal_CollectCorpusFile, true, &Corpus);

            const u32 NumEntries = (u32)Array_Num(Corpus.Entries);

            if (NumEntries == 0)
            {
                LOG_ERROR("Found no .build files under '%S'. Give a corpus directory with --corpus.",
                          Options.CorpusDirectory);
                ExitCode = 1;
            }
            else
            {
                LOG("Read %u seed files from '%S' (%u skipped: unreadable, empty, or over %u bytes).",
                    NumEntries, Options.CorpusDirectory, Corpus.NumFilesSkipped,
                    (u32)MAX_BUILD_FILE_SIZE);

                // Parse every seed as it is, before any mutation. A seed the parser cannot
                // survive is a regression and not a discovery, so it must show itself at once
                // instead of waiting for a mutation to reach it. That is what makes the files
                // under Tests/Fuzz/Corpus a regression suite for what earlier runs found.
                for (u32 i = 0; i < NumEntries; i++)
                {
                    Internal_RunInput(&Run, Corpus.Entries[i], S("seed"), i);
                }

                LOG("Fuzzing the parser with seed %llu for %u iterations...",
                    Options.Seed, Options.NumIterations);
                LOG_LINE_BREAK();

                FuzzRandom Random = {0};
                FuzzRandom_Seed(&Random, Options.Seed);

                Clock RunClock = {0};
                Clock_Start(&RunClock);

                for (u32 i = 0; i < Options.NumIterations; i++)
                {
                    const String Source = Corpus.Entries[FuzzRandom_Below(&Random, NumEntries)];
                    const String Splice = Corpus.Entries[FuzzRandom_Below(&Random, NumEntries)];

                    String MutationName = String_Null();
                    FuzzMutator_Mutate(&Random, Source, Splice, &Input, &MutationName);

                    if (Options.bTrace)
                    {
                        LOG("[%u] %S (%u bytes)", i, MutationName, Input.Length);
                        Internal_WriteTrace(Options.OutputDirectory, Input);
                    }

                    Internal_RunInput(&Run, Input, MutationName, i);
                }

                Clock_Tick(&RunClock);

                StringLocal(RunTime, 32);
                Time_ToString(RunClock.ElapsedTime, true, &RunTime);
                LOG("Fuzzed for %S.", RunTime);

                Internal_LogReport(&Report, &Options);

                if (Report.NumUnstable > 0)
                {
                    ExitCode = 1;
                }
            }
        }

        LinearAllocator_Destroy(&ParseArena);
        LinearAllocator_Destroy(&HarnessArena);
    }

    if (ParseMemory)
    {
        Platform_MemFree(ParseMemory);
    }

    if (HarnessMemory)
    {
        Platform_MemFree(HarnessMemory);
    }

    return ExitCode;
}

// The harness is the top level of this binary. The build tool's own main lives in
// Source/Main.c, which Fuzz.build leaves out of its source list.
int main(i32 ArgC, char* ArgV[], char* ArgEnv[])
{
    return Entry_Run(RunFuzzer, Kibibytes(128), Kibibytes(8), ArgC, ArgV, ArgEnv);
}
