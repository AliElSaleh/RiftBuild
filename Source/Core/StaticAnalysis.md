# RiftBuild Static Analysis Checks

A checklist to run against a diff, a file, or a whole subsystem. Companion to
`CodeStyle.md` - that file governs how code is written, this one governs whether it is
correct. A finding here is a bug; a finding there is a style violation.

## How to run

Default target is the working diff:

```bash
git diff --stat && git diff
```

Work through the checks below in order. Each check states how to detect it. Where a
grep is given, run it against the changed files only unless auditing a whole subsystem.

## Report format

One line per finding, most severe first:

```
SA-NN [severity] path/to/File.c:123 - what is wrong and what happens as a result
```

Severities:
- **error** - a real defect. Wrong behaviour, corruption, a leak, or a build break.
- **warn** - a latent hazard. Correct today, breaks under a plausible change.
- **info** - worth knowing, no action required.

Report `error` and `warn`. Mention `info` only if asked for a thorough audit.

## Verification protocol

Do not report a finding you have not verified. For each candidate:

1. Read the enclosing function, not just the matched line.
2. Check whether an earlier guard already makes the failure impossible.
3. State a concrete failure: the input or state that triggers it, and the result.

If you cannot write that failure sentence, the finding is not real - drop it.

## Scope and exclusions

Always excluded - never report findings in these:
- `How To/Examples/*/external/**` - vendored third-party (glfw and friends)
- `Source/MicrosoftCraziness.c` / `.h` - third-party, deliberately left in its own style
- `Source/Core/Win32Types.h` - hand-transcribed Win32 declarations
- `**/Build/**`, `**/Intermediate/**` - generated output
- `Source/Resources/*.vsix` - binary

Partially excluded:
- `Source/Core/Platform_*.c` - exempt from SA-20 and SA-21 (system headers and system
  types are expected there), subject to everything else.

## Repository caveats

**Grep can silently miss tracked files.** `.gitignore` is whitelist-style (`*` followed
by `!` re-includes), and ripgrep honours it, so some git-tracked files are skipped -
`Source/Resources/riftbuild.vim` is a confirmed miss. **Any negative grep result that a
finding depends on must be re-verified with plain `grep` via Bash**, not the Grep tool.

**Asserts do not exist in release.** `NO_ASSERT` is defined for every non-debug build.
Reasoning about runtime behaviour must assume `ASSERT`/`ENSURE` are absent.

**Windows links the static CRT.** clang and gcc take the driver default, and MSVC uses `/MT`.
`memcpy` and friends come from the CRT. Tool code still calls the `Mem*` wrappers, and it
never calls `malloc`, `free`, or `printf`.

---

## What the build already catches - do not spend review effort here

Warnings are errors (`-Werror`, `/WX`). If the code compiles, these are already clean,
so do not report them as findings:

| Already enforced | By |
| --- | --- |
| Shadowed variables | `-Wshadow` |
| Implicit narrowing / sign conversion | `-Wconversion` |
| Missing `default:` in a switch | `-Wswitch-default` |
| Missing enumerator in a switch over an enum | `-Wswitch-enum` |
| Unannotated fallthrough | `-Wimplicit-fallthrough` |
| Non-static function with no prototype | `-Wmissing-prototypes` |
| Obvious use of uninitialized value | `-Wuninitialized` |
| Unused variables | `-Wunused` |
| Struct padding on 64-bit MSVC | C4820 + `/WX` |

Deeper analysers exist behind the `analyze` build option (`/analyze` on MSVC,
`--analyze` on clang, `--analyzer` on gcc):

```bash
b analyze
```

Everything below is what those tools **cannot** see.

---

## Memory and lifetime

### SA-01: Stack buffer escapes its scope   [error]
A `String` created by `StringLocal`, or any pointer into a local array, must not be
returned, stored in a struct that outlives the function, appended to a `TArray`, or
handed to a callback that retains it. The buffer dies at the closing brace.
**Detect:** for each `StringLocal` in the diff, find every later use of that name and
confirm none of them are `return`, `Array_Add`, an assignment to a struct member, or a
`UserData` payload.
**Fix:** `String_Duplicate(Arena, Local)` into an arena the caller owns.
**Scope:** all first-party source.

### SA-02: Scratch-by-value arena used for memory that must survive   [error]
`LinearAllocator Scratch` (by value) gives the callee a private copy of the bump offset.
Everything allocated from it is gone when the function returns. Only `LinearAllocator*`
allocations outlive the call.
**Detect:** `grep -n "LinearAllocator Scratch" <files>` - for each hit, check whether
anything allocated from that arena is returned or stored beyond the function.
**Scope:** all first-party source.

### SA-03: Arena allocation without a headroom check   [warn]
`LinearAllocator_Allocate` is `RETURN_NON_NULL` - it does not fail gracefully. A
workload-sized allocation must check remaining space first and degrade with a message.
**Detect:** `grep -n "LinearAllocator_Allocate\|ArrayLocal_Arena" <files>`, then check
the size expression - a constant is fine, anything scaled by input count needs a check.
**Scope:** all first-party source.

### SA-04: Query-size-then-allocate for a bounded query   [warn]
A query whose maximum result size is known must use a fixed stack array of the struct
type with a graceful fallback, not a two-call size-then-allocate dance (CodeStyle MM-01).
**Detect:** `grep -n "MemAlloc\|Platform_MemAlloc" <files>` and look for a preceding
sizing call against the same subject.
**Scope:** all first-party source.

### SA-05: Global fixed-size mutable state   [error]
Caches and state tables must hang off a context struct sized to the workload, allocated
from an arena (CodeStyle MM-04). A file-scope fixed array silently truncates.
**Detect:** `grep -nE "^static [A-Za-z_]+ [A-Za-z_]+\[[0-9]+\]" <files>`
**Exception:** immutable lookup tables (`static const`/`read_only`) are fine - the
`ReservedKeys` and `AssemblyTypeStringTable` pattern is correct.
**Scope:** all first-party source.

### SA-06: Heap allocation without a matching free on every path   [error]
`MemAlloc` needs `MemFree`, `Array_Create`/`ArrayLocal` needs `Array_Destroy`, on every
exit path including error paths.
**Detect:** `grep -n "MemAlloc(\|Array_Create(\|ArrayLocal(" <files>`, then trace each
`return` in the enclosing function.
**Note:** arena-backed allocations (`ArrayLocal_Arena`, `String_Duplicate`) need no free
- do not report those.
**Scope:** all first-party source.

### SA-07: Resource handle leaked on an error path   [error]
`Filesystem_Open` needs `Filesystem_Close`; `Platform_RunCommand`/`Platform_RunProcess`
need `Platform_CloseHandle`; `Platform_PipeInit` needs `Platform_ClosePipe`;
`Filesystem_Open_MemoryMapped` needs `Filesystem_Close_MemoryMapped`. The single-return
rule (CodeStyle CF-04) makes this easy to get right - the cleanup goes before the
final `return`.
**Detect:**
```bash
grep -n "Filesystem_Open\|Platform_RunCommand\|Platform_RunProcess\|Platform_PipeInit" <files>
```
For each, confirm the matching close runs on the failure path too.
**Scope:** all first-party source.

---

## Bounds and containers

### SA-08: Write into a `String` that can exceed its capacity   [error]
`String` carries `Capacity`. A `String_Copy`/`String_Append`/`String_Format` whose source
length is driven by input data must be bounded by a buffer big enough for the worst case.
Path buffers use `MAX_PATH_LENGTH`; command lines use `UINT16_MAX`.
**Detect:** `grep -n "StringLocal(" <files>` and compare each declared size against
what gets written into it.
**Scope:** all first-party source.

### SA-09: `SArray_Capacity` applied to a pointer   [error]
`SArray_Capacity` is `sizeof(Array)/sizeof(Array[0])`. On a pointer - including an array
parameter, which decays - it yields 1 (or 0), silently truncating every loop over it.
**Detect:** `grep -n "SArray_Capacity(" <files>`; for each, confirm the argument is a
real array *declared in the same scope*, not a parameter or a member.
**Scope:** all first-party source.

### SA-10: `Array_Add` called with an rvalue   [error]
`Array_Add(Array, Value)` expands to `&Value`. A literal, a compound literal, or a
function call cannot have its address taken and will not compile - but a macro argument
with side effects will be evaluated in a surprising way.
**Detect:** `grep -n "Array_Add(" <files>` and check the second argument is a plain
named variable.
**Scope:** all first-party source.

### SA-11: Fixed-size table initializer out of sync with its declared bound   [error]
Tables like `static ReservedKeyTable ReservedKeys[84]` are iterated with
`SArray_Capacity`. Adding an entry without bumping the bound is a compile error, but
*removing* or commenting one out leaves a zero-filled tail that the loop still visits
and compares against an empty key.
**Detect:** `grep -nE "\[[0-9]+\] *=$" <files>` - for each table, count the initializers
and compare to the declared bound.
**Scope:** all first-party source.

### SA-12: Off-by-one on `Length` vs `Capacity`   [warn]
`StringLocal(Name, n)` allocates `n+1` bytes and sets `Capacity = n`, reserving room for
a terminator. Code that indexes `Data[Capacity]` or copies `Capacity + 1` bytes is
reaching into that reserved byte.
**Scope:** all first-party source.

---

## Return values and error handling

### SA-13: `NO_DISCARD` result ignored   [error]
Every allocating or fallible Core function is `NO_DISCARD`. Ignoring one means an
unchecked failure. Note `-Wno-unused-result` is set for gcc, so the compiler will *not*
catch this on a gcc build - it must be caught by review.
**Detect:** `grep -nE "^\s+(Filesystem_|Platform_|String_|LinearAllocator_)[A-Za-z_]+\(" <files>`
- a call on its own line with no assignment and no `xx` prefix.
**Scope:** all first-party source.

### SA-14: `xx` hiding a failure that matters   [error]
`xx` means "cannot fail in a way I care about" (CodeStyle RV-02). Discarding the result
of a write, an open, a spawn, or a delete is a silent data-loss path.
**Detect:** `grep -n "xx Filesystem_\|xx Platform_Run\|xx Platform_Set" <files>`
**Scope:** all first-party source.

### SA-15: Failure reported without telling the user what failed   [warn]
A `return false` on an error path with no `LOG_ERROR`, or a log that names an internal
function instead of the offending path/key (CodeStyle DG-05).
**Scope:** `Source/*.c` (user-facing tool code), not Core.

### SA-16: Side effect inside `ASSERT` / `ENSURE`   [error]
Both compile to nothing under `NO_ASSERT`, which is every release build. Any call,
assignment, or increment inside the expression disappears from the shipped binary.
**Detect:**
```bash
grep -nE "(ASSERT|ENSURE)(_MSG)?\([^)]*(\+\+|--|= )" <files>
grep -nE "(ASSERT|ENSURE)(_MSG)?\([A-Za-z_]+_[A-Za-z_]+\(" <files>
```
The second pattern finds a function call as the asserted expression - check whether the
call is pure. `ALWAYS`/`NEVER` keep their expression in release and are the correct tool.
**Scope:** all first-party source.

---

## Platform layer

### SA-17: Platform-specific code outside the platform layer   [error]
Per-OS probing, `#if PLATFORM_*` around system calls, and direct OS API calls belong in
`Source/Core/Platform_*.c` behind a `Platform.h` function.
**Apply the calculator test before reporting:** if the behaviour is generic OS
functionality any program would need (files, processes, time, memory, environment), it
must move to the platform layer. If it is RiftBuild-specific per-OS *policy* - which
linker flag spelling a toolchain wants, which icon format a desktop entry needs - it
correctly stays at the call site under `#if PLATFORM_*`. Do not report those.
**Detect:**
```bash
grep -n "#if PLATFORM_\|#ifdef _WIN32\|#if defined(__linux__)" Source/*.c
```
**Scope:** `Source/*.c` and any non-platform Core file. Never `Platform_*.c`.

### SA-18: System header included outside the platform layer   [error]
`<unistd.h>`, `<sys/*.h>`, `<stdio.h>`, `<stdlib.h>`, `<string.h>`, Win32 headers.
There is no CRT on Windows, so these do not merely violate layering - they fail to link.
**Detect:** `grep -rn "#include <" Source/*.c Source/Core/[!P]*.c Source/Core/*.h`
**Exception:** `<stdarg.h>` is permitted anywhere a variadic function is implemented.
**Scope:** everything except `Source/Core/Platform_*.c` and `Win32Types.h`.

### SA-19: libc function called outside the platform layer   [error]
`malloc`, `free`, `strlen`, `printf`, `sprintf` and friends. Engine code uses
`MemAlloc`/`MemFree`/`MemCopy`/`MemZero` and the `String_*` API.
With no CRT on Windows most of these fail to link, which makes them loud. The quiet ones
are `memset`/`memcpy`/`memmove`: `Platform_Windows.c` defines them itself because the
compiler emits calls to them for struct copies, so a direct call compiles, links, and
runs — and still has to be reported.
**Detect:**
```bash
grep -nE "\b(malloc|calloc|realloc|free|memcpy|memset|strlen|strcpy|strcmp|sprintf|printf|fopen)\(" Source/*.c
```
**Scope:** everything except `Source/Core/Platform_*.c`.

### SA-20: Raw compiler predefine instead of the project macro   [warn]
`_WIN32`, `__APPLE__`, `__linux__`, `_MSC_VER`, `__GNUC__` in place of `PLATFORM_WINDOWS`,
`PLATFORM_APPLE`, `PLATFORM_LINUX`, `COMPILER_MSVC`, `COMPILER_GCC`.
**Detect:** `grep -n "_WIN32\|__APPLE__\|__linux__\|_MSC_VER\|__GNUC__" <files>`
**Exception:** `EngineTypes.h` itself, which is where the mapping is defined, and the
`#if defined(...)` probes inside `Platform_*.c` that select a system API variant.
**Scope:** all first-party source.

### SA-21: A `Platform_*` function added to one OS but not all   [error]
Supported: Windows, Linux, macOS, FreeBSD, NetBSD, OpenBSD. A new entry in `Platform.h`
needs an implementation in every backing file, or a documented failure return.
**Detect:** for each new declaration in `Platform.h`:
```bash
grep -ln "Platform_NewThing" Source/Core/Platform_*.c
```
Expect Windows, Unix (shared) or per-OS, and any OS-specific override.
**Scope:** `Source/Core/Platform.h` changes.

### SA-22: Divergent fallback values across platforms   [warn]
The same function failing on two platforms must produce the same fallback.
`Platform_GetCpuCacheLineSize` has a known divergence: Windows/Linux return
`CACHE_LINE_SIZE`, macOS returns 0.
**Detect:** for a `Platform_*` function touched in the diff, read all implementations
and compare their failure returns.
**Scope:** `Source/Core/Platform_*.c`.

---

## Types and arithmetic

### SA-23: Signed right shift used for bit extraction   [error]
Right-shifting a signed value is implementation-defined for negatives. CPUID results
land in `i32 info[4]` and must be cast before shifting.
```c
// BAD                              // GOOD
u32 Size = (info[1] >> 8) & 0xFF;   u32 Size = ((u32)info[1] >> 8) & 0xFF;
```
**Detect:** `grep -nE "\b(i8|i16|i32|i64|int) .*>>|info\[[0-9]\] *>>" <files>`
**Scope:** all first-party source.

### SA-24: Narrowing cast without a bounds check   [warn]
`-Wconversion` forces the cast to be written, which means every narrowing cast in the
tree was waved through by a human. Check that a `(u32)` on a `u64`, or a `(u8)` on a
`u32`, is backed by a range guarantee.
**Detect:** `grep -nE "\((u8|u16|u32|i8|i16|i32)\)" <files>`
**Scope:** all first-party source.

### SA-25: Enumerator exceeds the `ENUM` backing type   [error]
`ENUM(Name)` is `typedef u8`. A value above 255, or a flag set needing more than 8 bits,
silently truncates. Use `ENUM_T(Name, u32)`.
**Detect:** `grep -n -A20 "^ENUM(" <files>` and check the largest value, counting
implicit increments in long enums.
**Scope:** all first-party source.

### SA-26: Struct with an implicit padding hole   [error]
MSVC C4820 is an error on 64-bit builds. A new or reordered struct needs its members
ordered largest-first and any tail gap closed with an explicit `Padding`/`bPadding`
member (CodeStyle SE-03). Adding a `bool` to an existing struct usually breaks this.
**Detect:** for each `STRUCT(` touched in the diff, sum the member sizes and check the
total against the alignment of the widest member.
**Scope:** all first-party source. Exempt: `Platform_*.c` structs mirroring a system layout.

### SA-27: Bit flag defined without `BIT()`   [info]
Flag enumerators should use `BIT(n)` or explicit hex, not decimal powers of two typed
by hand.
**Scope:** all first-party source.

---

## Build-system-specific

### SA-28: New reserved key not wired through the whole chain   [error]
A `.build` key is not just a table entry. Adding one requires all of:
1. An entry in `ReservedKeys[N]` in `Source/Parse.c` - `Key`, `MaxValueLength`, `Impact` -
   with the array bound `N` bumped to match.
2. A correct `EBuildKeyImpact`: `Recompile` if changing it invalidates object files,
   `Relink` if it only affects the link, `None` if it affects neither. A wrong value
   here means stale builds or needless full rebuilds.
3. Consumption at the call site via `GetMaxValueLengthForReservedKey(S("Key"))` when the
   value is stored with `AddVariable`/`AddOrAppendVariable`.
4. Documentation in `How To/Reference.md`.
5. The editor syntax files under `Source/Resources/` - `riftbuild.vim`,
   `riftbuild-mode.el`, and the VS Code extension.
6. A case in `Tests/Feature Set/` covering it.
**Detect:** `grep -n "ReservedKeys\[" Source/Parse.c` for the bound, then confirm each of
the six. **Re-check steps 4 and 5 with plain `grep` via Bash** - the Grep tool skips
`riftbuild.vim` (see Repository caveats).
**Scope:** `Source/Parse.c` reserved-key changes.

### SA-29: A tool's keys reused for another tool   [error]
Every tool gets its own namespace - `Compiler.*`, `Linker.*`, `Archiver.*`, `Resource.*`,
`Assembler.*`. A resource-compiler path must read `Resource.Defines`, never
`Compiler.Defines`. Reusing another tool's values passes flags the tool cannot parse.
**Detect:** `grep -n "Compiler\.\(Flags\|Defines\|Includes\)" <files>` and check the
consuming code path actually belongs to the compiler.
**Scope:** `Source/Program.c`, `Source/Parse.c`, `Source/Backend.c`.

### SA-30: Test coverage missing for a parser or key change   [warn]
Parser changes and new keys need a case under `Tests/Feature Set/`. Note `.gitignore` is
whitelist-style, so a new test directory is invisible to git until `git add -f`.
**Detect:** `git status --porcelain` after adding tests; if the new directory does not
appear, it was silently ignored.
**Scope:** `Source/Parse.c`, `Source/Program.c` changes.

---

## Concurrency

### SA-31: Shared state touched without a critical section   [error]
The compile process pool runs jobs concurrently. State written from more than one job -
counters, shared output buffers, arena bumps - needs
`Platform_EnterCriticalSection`/`Platform_ExitCriticalSection`, entered and exited on
every path.
**Detect:** `grep -n "CompileProcessPool\|Platform_EnterCriticalSection" <files>`
**Scope:** `Source/Backend.c`, `Source/Program.c`.

### SA-32: Arena shared across concurrent jobs   [error]
`LinearAllocator` is a bump pointer with no synchronization. Two jobs allocating from
one arena will hand out overlapping memory. Each job needs its own arena or its own
scratch copy.
**Scope:** `Source/Backend.c`, `Source/Program.c`.
