// Copyright (c) Artisan Softworks
// Licensed under the BSD 3-Clause License. See the LICENSE file for details.

#include "Fuzz.h"

#ifndef UNITY_BUILD
#include "Core/Memory.h"
#include "Core/StringUtils.h"
#include "Core/Array.h"
#endif

ENUM(EFuzzMutation)
{
    FuzzMutation_FlipBits,
    FuzzMutation_SetByte,
    FuzzMutation_InsertFragment,
    FuzzMutation_InsertSigilRun,
    FuzzMutation_EraseRange,
    FuzzMutation_DuplicateRange,
    FuzzMutation_SpliceTail,
    FuzzMutation_Truncate,
    FuzzMutation_NestBlocks,
    FuzzMutation_RepeatLine,

    FuzzMutation_Count
};

// Fragments the lexer and the parser give their own meaning to. A mutation that only flips bytes
// reaches `if windows` about as often as it reaches any other nine bytes, which is never, so the
// interesting branches stay unvisited without a table like this one.
//
// The source of truth for the spellings is ReservedKeywordsTable, ReservedStartingKeywordsTable,
// ReservedEndingKeywordsTable, CallConditionsTable and ReservedKeysTable in Source/Parse.c. This
// table is a copy, and it goes stale when a keyword changes. A stale entry costs the harness some
// coverage and nothing else, so it is not wired to the parser tables - the harness must not reach
// into the parser's private tables to stay correct.
read_only static String FuzzFragmentTable[52] =
{
    SC("if "),
    SC("else"),
    SC("import "),
    SC("include "),
    SC("or "),
    SC("contains "),
    SC("starts_with "),
    SC("ends_with "),
    SC("v>= "),
    SC("v<= "),
    SC("v> "),
    SC("v< "),
    SC("v== "),
    SC(".stop"),
    SC(".abort"),
    SC(".help"),
    SC("assert "),
    SC("Assert.Equal "),
    SC("Assert.NotEmpty "),
    SC("ErrorMessage"),
    SC(".ErrorMessage"),
    SC("program_exists(clang)"),
    SC("find_system_header(stdio.h)"),

    SC("Assembly "),
    SC("Compiler "),
    SC("Compiler.Flags "),
    SC("Compiler.Defines "),
    SC("Compiler.Includes.Export "),
    SC("Linker.Flags "),
    SC("Linker.Subsystem "),
    SC("Linker.Stack "),
    SC("SourceFiles "),
    SC("SourceDirectory "),
    SC("BuildDirectory "),
    SC("Libraries "),
    SC("Depend "),
    SC("Type "),
    SC("option.debug "),
    SC("PostBuild.Copy "),
    SC("Version(define) "),

    SC("$Variable"),
    SC("%option"),
    SC("%%option"),
    SC("&(Platform)"),
    SC("&args"),
    SC("^%debug"),
    SC("$(Assembly)"),
    SC("`echo hi`"),
    SC(":windows:!asan"),
    SC("# comment\n"),
    SC("## block\n##"),
    SC("{\n}\n"),
};

// Bytes that sit on a lexer boundary. The lexer branches on most of these by name, and the rest
// are the usual edges of a byte: a NUL in the middle of the text, and a byte with the top bit set.
read_only static uchar FuzzEdgeByteTable[24] =
{
    0x00, 0x01, 0x09, 0x0A, 0x0D, 0x20, 0x7F, 0x80, 0xFE, 0xFF,
    '"', '\\', '{', '}', '[', ']', '(', ')', '%', '$', '&', '^', '#', ':'
};

read_only static uchar FuzzSigilTable[12] =
{
    '%', '$', '&', '^', '#', '"', '{', '}', '[', ']', ':', '\\'
};

void FuzzRandom_Seed(FuzzRandom* Random, u64 Seed)
{
    // xorshift cannot leave an all-zero state, so fold a zero seed onto a usable one
    Random->State = Seed == 0 ? 1 : Seed;
}

NO_DISCARD u64 FuzzRandom_Next(FuzzRandom* Random)
{
    u64 X = Random->State;

    X ^= X >> 12;
    X ^= X << 25;
    X ^= X >> 27;

    Random->State = X;

    const u64 Result = X * 2685821657736338717ULL;

    return Result;
}

NO_DISCARD u32 FuzzRandom_Below(FuzzRandom* Random, u32 Bound)
{
    u32 Result = 0;

    if (Bound > 0)
    {
        Result = (u32)(FuzzRandom_Next(Random) % (u64)Bound);
    }

    return Result;
}

// Puts Insert into Text at Offset and pushes the tail right, dropping whatever no longer fits.
static void Internal_InsertText(String* Text, u32 Offset, const String Insert)
{
    const u32 At        = Min(Offset, Text->Length);
    const u32 RoomAt    = Text->Capacity - At;
    const u32 NumToPut  = Min(Insert.Length, RoomAt);
    const u32 TailBytes = Min(Text->Length - At, RoomAt - NumToPut);

    if (NumToPut > 0)
    {
        MemMove(&Text->Data[At + NumToPut], &Text->Data[At], TailBytes);
        MemCopy(&Text->Data[At], Insert.Data, NumToPut);

        Text->Length = At + NumToPut + TailBytes;
        Text->Data[Text->Length] = 0;
    }
}

// Puts Count copies of Byte into Text at Offset.
static void Internal_InsertByteRun(String* Text, u32 Offset, uchar Byte, u32 Count)
{
    StringLocal(Run, 256);

    const u32 NumToPut = Min(Count, Run.Capacity);
    MemSet(Run.Data, (i32)Byte, NumToPut);
    Run.Length = NumToPut;
    Run.Data[Run.Length] = 0;

    Internal_InsertText(Text, Offset, Run);
}

// Drops the Count bytes that start at Offset.
static void Internal_EraseRange(String* Text, u32 Offset, u32 Count)
{
    const u32 At         = Min(Offset, Text->Length);
    const u32 NumToErase = Min(Count, Text->Length - At);

    if (NumToErase > 0)
    {
        const u32 TailBytes = Text->Length - At - NumToErase;

        MemMove(&Text->Data[At], &Text->Data[At + NumToErase], TailBytes);

        Text->Length -= NumToErase;
        Text->Data[Text->Length] = 0;
    }
}

// Returns the line that holds Offset, as a slice of Text. An empty Text gives an empty slice.
NO_DISCARD static String Internal_FindLine(const String Text, u32 Offset)
{
    String Line = String_Null();

    if (Text.Length > 0)
    {
        const u32 At = Min(Offset, Text.Length - 1);

        u32 Start = At;
        while (Start > 0 && Text.Data[Start - 1] != '\n')
        {
            Start -= 1;
        }

        u32 End = At;
        while (End < Text.Length && Text.Data[End] != '\n')
        {
            End += 1;
        }

        Line = StrSub(Text, Start, End - Start);
    }

    return Line;
}

void FuzzMutator_Mutate(FuzzRandom* Random,
                        const String SourceText,
                        const String SpliceText,
                        String* OutText,
                        String* OutMutationName)
{
    String_Copy(OutText, SourceText);

    const EFuzzMutation Mutation = (EFuzzMutation)FuzzRandom_Below(Random, FuzzMutation_Count);

    switch (Mutation)
    {
        case FuzzMutation_FlipBits:
        {
            *OutMutationName = S("FlipBits");

            if (OutText->Length > 0)
            {
                const u32   At   = FuzzRandom_Below(Random, OutText->Length);
                const uchar Mask = (uchar)(1u << FuzzRandom_Below(Random, 8));

                OutText->Data[At] ^= Mask;
            }
            break;
        }

        case FuzzMutation_SetByte:
        {
            *OutMutationName = S("SetByte");

            if (OutText->Length > 0)
            {
                const u32 At   = FuzzRandom_Below(Random, OutText->Length);
                const u32 Pick = FuzzRandom_Below(Random, (u32)SArray_Capacity(FuzzEdgeByteTable));

                OutText->Data[At] = FuzzEdgeByteTable[Pick];
            }
            break;
        }

        case FuzzMutation_InsertFragment:
        {
            *OutMutationName = S("InsertFragment");

            const u32 At   = FuzzRandom_Below(Random, OutText->Length + 1);
            const u32 Pick = FuzzRandom_Below(Random, (u32)SArray_Capacity(FuzzFragmentTable));

            Internal_InsertText(OutText, At, FuzzFragmentTable[Pick]);
            break;
        }

        case FuzzMutation_InsertSigilRun:
        {
            *OutMutationName = S("InsertSigilRun");

            const u32 At    = FuzzRandom_Below(Random, OutText->Length + 1);
            const u32 Pick  = FuzzRandom_Below(Random, (u32)SArray_Capacity(FuzzSigilTable));
            const u32 Count = 1 + FuzzRandom_Below(Random, 64);

            Internal_InsertByteRun(OutText, At, FuzzSigilTable[Pick], Count);
            break;
        }

        case FuzzMutation_EraseRange:
        {
            *OutMutationName = S("EraseRange");

            if (OutText->Length > 0)
            {
                const u32 At    = FuzzRandom_Below(Random, OutText->Length);
                const u32 Count = 1 + FuzzRandom_Below(Random, OutText->Length - At);

                Internal_EraseRange(OutText, At, Count);
            }
            break;
        }

        case FuzzMutation_DuplicateRange:
        {
            *OutMutationName = S("DuplicateRange");

            if (OutText->Length > 0)
            {
                const u32 At    = FuzzRandom_Below(Random, OutText->Length);
                const u32 Count = 1 + FuzzRandom_Below(Random, OutText->Length - At);

                // the slice comes out of OutText and goes back into it, so it must land past the
                // end, where the copy cannot overlap the slice it reads. Middle insertion is
                // what InsertFragment, InsertSigilRun and RepeatLine already do.
                String_Append(OutText, StrSub(*OutText, At, Count));
            }
            break;
        }

        case FuzzMutation_SpliceTail:
        {
            *OutMutationName = S("SpliceTail");

            const u32 Head = FuzzRandom_Below(Random, OutText->Length + 1);
            const u32 Tail = FuzzRandom_Below(Random, SpliceText.Length + 1);

            OutText->Length = Head;
            OutText->Data[OutText->Length] = 0;

            String_Append(OutText, StrSub(SpliceText, Tail, SpliceText.Length - Tail));
            break;
        }

        case FuzzMutation_Truncate:
        {
            *OutMutationName = S("Truncate");

            OutText->Length = FuzzRandom_Below(Random, OutText->Length + 1);
            OutText->Data[OutText->Length] = 0;
            break;
        }

        case FuzzMutation_NestBlocks:
        {
            *OutMutationName = S("NestBlocks");

            // the parser walks a block with one stack frame per level and has no depth limit, so
            // this is the mutation that says how deep a build file can go before the stack ends
            const u32    Depth  = 1 + FuzzRandom_Below(Random, 512);
            const String Opener = S("if windows\n{\n");
            const String Closer = S("}\n");

            for (u32 i = 0; i < Depth; i++)
            {
                Internal_InsertText(OutText, 0, Opener);
                Internal_InsertText(OutText, OutText->Length, Closer);
            }
            break;
        }

        case FuzzMutation_RepeatLine:
        {
            *OutMutationName = S("RepeatLine");

            const u32    At    = FuzzRandom_Below(Random, OutText->Length + 1);
            const u32    Count = 1 + FuzzRandom_Below(Random, 256);
            const String Line  = Internal_FindLine(*OutText, At);

            StringLocal(Repeated, 1024);
            String_Copy(&Repeated, Line);
            String_AppendChar(&Repeated, '\n');

            for (u32 i = 0; i < Count; i++)
            {
                Internal_InsertText(OutText, At, Repeated);
            }
            break;
        }

        case FuzzMutation_Count:
        default:
        {
            *OutMutationName = S("None");

            UNREACHABLE();
            break;
        }
    }

}
