# Parser Fuzzer

A fuzzer for the `.build` lexer and parser in [Parse.c](../../Source/Parse.c). It mutates real
build files into malformed ones and parses them in a loop, to find the inputs that crash the
parser, hang it, or leave state behind.

## Build it

```bash
cd Tests/Fuzz
b asan debug compiler=clang
```

Always build it with `asan`. The address sanitizer and the undefined-behavior sanitizer are what
turn a silent out-of-bounds write into a report with a stack trace. Without them the fuzzer only
finds the bugs that happen to end the process. `debug` adds the asserts and the arena bounds
check, which catch an overrun of the parse arena at the allocation instead of after it.

`b` on its own builds the release version. It runs about four times faster and finds less.

## Run it

The build file has a `.Run` key, so the build above also runs the harness. Put arguments after
that key to fix them for every build.

To run it on its own, from anywhere:

```bash
Tests/Fuzz/Build/RiftFuzz.exe --iterations 200000 --seed 7
```

The harness finds the repository root itself, by walking up from the working directory until it
sees `RiftBuild.build`, so the two default paths below do not depend on where you start it. A
path you pass yourself is taken as it is, which means a relative one is read against the working
directory.

| Option | What it does |
|---|---|
| `--corpus <dir>` | Read the seed `.build` files from here. The default is `Tests/Feature Set` under the repository root. |
| `--input <file>` | Parse one named file and stop. This is how a saved finding comes back. |
| `--out <dir>` | Where the harness saves a finding. The default is `Tests/Fuzz/Build/Findings` under the repository root. |
| `--seed <number>` | The seed of the run. The same seed gives the same inputs in the same order. |
| `--iterations <n>` | How many mutated inputs to parse. The default is 100000. |
| `--trace` | Name every input and write it to `last-input.build` before the parse. |

## The two runs to make

1. The **regression run** parses the inputs that earlier runs found, which live in
   [Corpus](Corpus). Every seed file is parsed as it is before any mutation starts, so a bug that
   comes back shows itself at once.

   ```bash
   Tests/Fuzz/Build/RiftFuzz.exe --corpus Tests/Fuzz/Corpus --iterations 20000
   ```

   That `--corpus` path is read against the working directory, so it assumes the repository root.
   From the build directory the same corpus is `--corpus ../Corpus`.

2. The **discovery run** uses the feature-set tests as seeds, which cover far more of the syntax.

   ```bash
   Tests/Fuzz/Build/RiftFuzz.exe --iterations 200000 --seed 1
   ```

Change the seed between discovery runs. One seed always makes the same inputs, so a second run
with the same seed finds nothing new.

## What the harness counts as a finding

- **A crash.** The sanitizer or the arena bounds check ends the process and names the line.
- **An unstable parse.** The harness parses every input twice. The two answers must agree. They
  disagree when the parser leaves state behind, in a global or in a node the process shares, and
  that state reaches the next build file of the same build.
- **A slow parse.** More than 0.25 seconds. A 48KiB build file parses in about a millisecond, so
  a slow parse found a loop that the input made quadratic.

An input the parser **rejects** is not a finding. The parser is supposed to refuse malformed text.

The harness saves an unstable or slow input into the findings directory and goes on. A crash ends
the process, and the input is gone with it. To get that input back, run again with the same
`--seed` and with `--trace`: the file `last-input.build` then holds the input it died on.

## What it covers, and what it does not

It covers the lexer and the parser, which is `LexAndParseBuildText`: the text goes in, a tree
comes out. Nothing in that path reads the disk or starts a process, which is what makes it safe
to call on arbitrary bytes in a loop.

It does **not** cover the analysis passes that run after the tree is built. Those passes read
include files, run `$(...)` command expansion and spawn programs to answer `program_exists(...)`,
so a fuzzer cannot drive them without also running whatever the mutated text names.

It cannot detect a true hang either. The harness parses in its own process, so a parser that
never returns takes the harness with it. A run that stops making progress is that case, and
`--trace` names the input.

## How it works

- [FuzzMain.c](Source/FuzzMain.c) reads the options, loads the corpus, and runs the loop. It hands
  the parser a zeroed arena of exactly `BUILD_PARSE_SCRATCH_SIZE`, which is the arena the tool
  itself gives the parser, so a finding here is a finding in the tool.
- [FuzzMutator.c](Source/FuzzMutator.c) holds the generator and the ten mutations. The generator
  is seeded, and nothing in the harness reads the clock to make a choice, so a run is a pure
  function of its seed.
- [Fuzz.build](Fuzz.build) links the tool's own sources and leaves out `Source/Main.c`, which
  holds the tool's `main`. FuzzMain.c holds this binary's `main`, and both of them call
  `Entry_Run` to bring the engine up. The lexer and the parser under test are the real ones.

## What it found

All four are fixed. The corpus holds a small input for each one.

| Finding | Where |
|---|---|
| Nested blocks recursed with no limit and overflowed the stack | `Parse_Block` and `Parse_If` |
| An unterminated log message looped past the end of the tokens | `Parse_Special_LogMessage` |
| An unterminated `[` list and an unterminated `.help {` did the same | `Parse_Block`, `Parse_Special_Help` |
| A new node kept the arena bytes of the node before it | `Node_Create` |
