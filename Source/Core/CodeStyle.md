# RiftCore Code Style

Rules are numbered by a two-letter category then the number like so: XX-NN.
MUST/NEVER rules are hard rules - always follow and always flag.
SHOULD rules are judgment calls; the rule states what decides.

**Scope:** all first-party C in `Source/`. Third-party code vendored under
`How To/Examples/*/external/` and `Source/MicrosoftCraziness.*` are exempt - do not
restyle them, do not flag them.

**Language:** C99 (`-std=c99`). No C11/C17/C23 features, no compiler extensions
except the ones already wrapped in a macro in `EngineTypes.h`.

**Companion doc:** `StaticAnalysis.md` covers correctness checks (memory, lifetime,
platform containment). This file covers how code is written and named.

**Source:** the Design principles, Modules and interfaces, and Red flags sections follow
`A Philosophy of Software Design` by John Ousterhout, adapted to C and to this codebase.

---

## Design principles

These rules decide the ones that follow. Complexity is anything about the structure of
the code that makes it hard to understand or hard to change. Every other rule in this
file exists to remove some of it.

Two methods remove complexity, and this file is built from both. The first method gets
rid of it: fewer special cases, one meaning for one name, code that reads correctly the
first time. The Naming, Control flow, Diagnostics, Commenting and General sections do
that work. The second method shuts it inside a module, so that a reader meets one part at
a time. The Modules and interfaces section does that work.

### DP-01: Working code is not enough
The goal of a change is a good design that also works. A kludge that saves an hour today
costs more than an hour later, because every later change works around it. Spend a part
of every change on the design, not only on the feature.

When you modify existing code, leave the file with the structure it would have had if the
new behaviour were part of the first design. If you do not make the design better, you
make it worse.

About 10-20% of the time you spend on a change is the right size for this. Keep the
investment small and continual. One design phase at the front of a feature is the
waterfall model, and it fails here for the same reason it fails everywhere: you cannot see
the problems in a design until you build on it. The design that fits shows itself while
you write the body, and DP-03 is a sketch of an hour, not a phase.

### DP-02: Complexity is incremental
One mistake does not make a program hard to change. Hundreds of small dependencies and
small obscurities do. One extra special case, one extra parameter, one vague name is a
real cost. There is no "only this once".

### DP-03: Design it twice
Before you add a new interface, sketch a second one that is very different from the first.
A sketch is a few important signatures, not a full design. An hour of this for a small
module is cheap next to the days you spend on the body.

### DP-04: The reader decides
Code is obvious when a new reader guesses right without effort. If a reviewer says a piece
of code is not obvious, it is not obvious. Do not argue the point - change the name, the
structure or the comment. Write for ease of reading, not ease of writing.

### DP-05: Every rule here has a limit
Each rule removes one kind of complexity. Pushed to its end, each one adds another kind.
One module for the whole program hides nothing. A name of six words is precise and
unreadable. A comment on every line buries the one comment that matters.

When two judgment rules disagree, keep the side that leaves less for a new reader to hold
in mind. Name the rule you traded away in the review, and say why.

### DP-06: A red flag stops the change
The Red flags table at the end of this file lists the symptoms. When one appears, in your
own code or in a review, do not patch around it. Stop, and design that part again. Try
more than one replacement before you keep the first - see DP-03. The number of red flags
in a review measures the design. The number of broken rules does not.

### DP-07: Name the symptom, not "this is messy"
Complexity shows itself in three ways. Use these words in a review, because each one has a
different correction.
- **Change amplification.** One small change needs edits in many places. Correct it by
  moving the decision into one place - see MD-03.
- **Cognitive load.** The reader must hold too much in mind to make the change. Correct it
  with a deeper module, a better name or a missing comment.
- **Unknown unknowns.** The reader cannot tell which code the change touches, or that a
  dependency exists at all. This one is the worst, because reading the code does not find
  it. Correct it by making the dependency visible: put it in one place, give it a name
  that greps, and name the other places in a comment there - see CM-10 and DG-04.

### DP-08: Cut dependencies, and make the ones you keep obvious
Dependencies and obscurity cause all three symptoms. A dependency exists when you cannot
read or change one piece of code on its own. You cannot remove them all - a function
signature is one - so the work is to keep the number small and to leave every one of them
in sight.

Obscurity is important information that is not in front of the reader: a name that fits
anything, a number with no unit, a table that a second file must also update. A hidden
dependency is both at once, and it is the usual source of a bug that survives a review.

### DP-09: Fewer lines is not the goal
Line count does not measure complexity. A version with more lines is simpler when it asks
the reader to hold less in mind. Split the expression (GN-03), name the constant (GN-06),
write the `if` out in full (CF-01). Never compress code to make it look clever.

### DP-10: Fix the defect you find, in the change that finds it
Design work comes in two kinds. The first kind happens before the code: the second sketch
(DP-03), the interface comment (CM-09). The second kind happens during it. While you work
in a file, you find a design problem that your change did not cause. Correct it there.

Never patch around it. A patch keeps the defect and adds a second thing for the next reader
to understand, and the next patch goes around both. Never defer it either. "After this
deadline" becomes the next deadline, and the work grows while you wait. Code that lands
fast and leaves the cleanup for the next person is not fast.

A `// TODO:` marks work that nobody wrote yet (CM-06). It does not license a defect you
already found. The moment you start writing a comment that defends the code is the stop
signal - see CM-02.

**Exception:** a correction that is much larger than the change that found it, or one that
breaks other work in flight. Take the small fix, and say in the review what the real
correction is.

---

## Naming

### NM-01: Identifiers are PascalCase
Functions, variables, parameters, struct members and types are all PascalCase.
No snake_case, no camelCase, no Hungarian notation beyond the `b` bool prefix.
```c
// BAD
int source_count; void get_file_path(); usize nNumSources;

// GOOD
u32 SourceCount; void GetFilePath(void); u32 NumSources;
```
**Exception:** loop counters may be short lowercase (`i`, `n`), and the underscore
separator is used inside the naming schemes below (`Filesystem_Open`, `FileMode_Read`).

### NM-02: Public API functions are prefixed with their module
Every function exposed in a header carries its module name then an underscore.
The prefix matches the header it lives in.
```c
// GOOD
Filesystem_Open(...)      // Filesystem.h
String_Duplicate(...)     // StringUtils.h
Platform_MemAlloc(...)    // Platform.h
LinearAllocator_Allocate(...)
Array_Add(...)
```

### NM-03: File-local helpers are `static`, and shared-name helpers take `Internal_`
A helper used only inside one .c file is `static`. When its name would collide with,
or read like, a public function, prefix it `Internal_`.
```c
// GOOD
static bool IsBuildFile(const String FilePath);
static void Internal_AddOrUpdateBuildVariable(TArray(FileVariable) DB, FileVariable V);
```

### NM-04: Bool variables and members are prefixed with `b`
Applies to locals, parameters, struct members and globals. See also TP-02.
```c
// GOOD
bool bRecursive;
bool bIsAssemblyExe;
static bool bSingleThread = false;
```

### NM-05: Output parameters are prefixed with `Out`
See FN-02. Applies to every pointer the function writes through for the caller.

### NM-06: Globals use `global`, and read-only globals use `read_only global` with `g_`
`global` is `extern`. Mutable globals declared in a header use a plain PascalCase name;
read-only nil/sentinel globals use the `g_` prefix.
```c
// GOOD
global bool bQuietBuild;
global TArray(InternalVariable) InternalVariablesDB;
read_only global String g_StringNil;
read_only global FileHandle g_FileHandle;
```

### NM-07: Function-scope statics use `local_persist`
Never write a bare `static` inside a function body.
```c
// BAD
static bool bInitialized = false;

// GOOD
local_persist bool bInitialized = false;
```

### NM-08: Enum types are `E`-prefixed; members repeat the type name without the `E`
The member prefix is the type name minus `E` (abbreviated only when the full name is
unwieldy, e.g. `EComparisonType` -> `Cmp_`). A count sentinel is named `_Count`.
```c
// GOOD
ENUM(EFileMode)
{
    FileMode_Read  = 0x1,
    FileMode_Write = 0x2
};

ENUM(EMemoryTag)
{
    MemoryTag_Unknown = 0,
    MemoryTag_Array,
    // ...
    MemoryTag_Count
};
```

### NM-09: Predicate functions read as a question
`Is`, `Has`, `Does`, `Can`, `Should` for pure queries; `Try` for a function that
attempts an action and reports whether it worked. See FN-01.
```c
// GOOD
bool IsCppSource(const String Extension);
bool Filesystem_DoesFileExist(const String FilePath);
bool TryBuildMacBundle(LinearAllocator Scratch, const BuildParams* Params, ...);
```

### NM-10: A name must be precise enough to guess from
Ask what a reader thinks the name means when they see it alone, with no declaration and
no comment next to it. A name that fits many things tells the reader nothing.
```c
// BAD - count of what? true means what?
u32  GetCount(void);
bool bBlinkStatus;

// GOOD
u32  GetNumActiveJobs(void);
bool bCursorVisible;
```
**Exception:** `i` and `n` in a short loop (NM-01). A short name is safe only when the
reader sees every use of it at once.

### NM-11: One name, one meaning, across the whole codebase
When a value appears again and again, pick one name for it and use that name everywhere.
Never use that name for a second meaning. When you need two of them, add a prefix.
```c
// BAD - Path is an absolute path here and a relative path forty lines down
String Path;

// GOOD
String SourcePath;    // always absolute
String RelativePath;  // always relative to the root
```
A name that carries two behaviours is how a wrong value reaches the wrong call unseen.

### NM-12: A name you cannot find is a design signal
When no short and precise name fits a variable or a function, the thing behind it has no
clean definition. Split it, or move the work, until a name fits.

### NM-13: The rare form of a call takes the `_Ex` suffix
When most callers want one behaviour and a few want more control, the plain name gets the
common case and `_Ex` gets the full one. A caller who does not need the extra never learns
that it exists - see MD-01.
```c
// GOOD - the pair already used across Filesystem.h and Platform.h
RIFT_API void Filesystem_IterateDirectory(const String BasePath, DirectoryIterator Callback, bool bRecursive);
RIFT_API void Filesystem_IterateDirectory_Ex(const String BasePath, DirectoryIterator Callback, bool bRecursive, void* UserData);
```
Never invert the pair. The short name must be the one that most call sites want, so never
give the plain name the harder job and push the easy job into `_Ex`.

---

## Types

### TP-01: The int and unsigned int types vary in size across platforms. Explicitly-sized types are mandatory.
```c
// BAD
unsigned int Value = 0;
int Value = 0;

// GOOD
u32 Value = 0;
i32 Value = 0;
```
**Exception:** a variable that receives a value straight from a system header
(`int fd`, `DWORD Code`) keeps the system type - but only inside `Platform_*.c`.

### TP-02: Bool variables are always prefixed with 'b'
```c
// BAD
bool Value = false;

// GOOD
bool bValue = false;
```

### TP-03: Sizes, counts of bytes and offsets are `usize`; array indices follow the container
`usize` for memory sizes and byte offsets, `u32` for element counts and string lengths
(`String.Length` is `u32`).
```c
// BAD
u32 MemoryRequirement = Logging_GetMemoryRequirement();

// GOOD
usize MemoryRequirement = Logging_GetMemoryRequirement();
u32   NumSources        = 0;
```

### TP-04: Never write a bare `char`, `short`, `long`, or `float`
Use `u8`/`i8`, `u16`/`i16`, `u32`/`i32`/`u64`/`i64`, `f32`/`f64`. String bytes are
`uchar`; wide characters are `wchar` (16-bit, not the system `wchar_t`).
**Exception:** `const char*` at a system-call boundary inside `Platform_*.c`, and
`Platform_ConsoleWrite(const char*, ...)` which is deliberately narrow-char.

### TP-05: Casts are explicit and narrowing casts are deliberate
`-Wconversion` is on with `-Werror`. An implicit narrowing conversion is a build
failure, so every narrowing conversion must be written out and must be justified by
a preceding bounds check or a known-bounded source.
```c
// BAD
u32 Length = SomeU64Value;

// GOOD
ASSERT(SomeU64Value <= UINT32_MAX, ...);
u32 Length = (u32)SomeU64Value;
```

---

## Structs and enums

### SE-01: Declare types with `STRUCT` / `UNION` / `ENUM`, never a raw `typedef struct`
```c
// BAD
typedef struct SourceFileData { ... } SourceFileData;

// GOOD
STRUCT(SourceFileData)
{
    String FullPath;
    String RelativePath;
};
```

### SE-02: `ENUM` is `u8` - use `ENUM_T` when the values do not fit
`ENUM(Name)` expands to `typedef u8 Name`. Any enumerator above 255, or any flag set
needing more than 8 bits, must use `ENUM_T(Name, u32)`.
```c
// BAD - silently truncates
ENUM(EBigFlags) { BigFlag_A = 0x10000 };

// GOOD
ENUM_T(EBigFlags, u32) { BigFlag_A = 0x10000 };
```

### SE-03: Structs must be explicitly padded to a multiple of their alignment
MSVC C4820 (padding inserted) is an error on 64-bit builds because of `/Wall /WX`.
Order members largest-first and close any tail gap with a named padding member.
`u8 Padding[N]` for a general gap, `bool bPadding[N]` when the gap follows bools.
```c
// GOOD
STRUCT(SourceCountData)
{
    u32  NumSources;
    u32  NumAsmSources;
    u64  NewestHeaderWriteTime;
    bool bHasCppFiles;
    bool bIsPCHBuild;
    u8   Padding[6];
};
```

### SE-04: Group struct members by size, then by meaning
Pointers and 8-byte members first, then 4-byte, then 2-byte, then bools, then padding.
This is what keeps SE-03 satisfiable without scattered holes.

### SE-05: Align member names into a column when the struct has mixed type widths
```c
// GOOD
STRUCT(ParsingContext)
{
    LinearAllocator*     PermanentArena;
    LinearAllocator*     TempArena;
    TArray(FileVariable) VariablesDB;
    FileVariableList*    VarListHead;
    String               WorkingDirectory;
    bool                 bNoFail;
    u8                   Level;
    u8                   Padding[6];
};
```

### SE-06: Use `constant { }` for related integer constants, not a run of `#define`s
```c
// BAD
#define ArrayField_Capacity 0
#define ArrayField_Num      1

// GOOD
constant
{
    ArrayField_Capacity   = 0,
    ArrayField_Num        = 1,
    ArrayField_Count      = 4
};
```

---

## Control flow

### CF-01: Braces on all if statements
Every `if`/`else` body uses braces, even single-line.
```c
// BAD
if (Result) return;

// GOOD
if (Result) { ... }
```

### CF-02: Single-line if statement
Single line `if` statement is prohibited.
```c
// BAD
if (Result) { ... }

// GOOD
if (Result)
{
    ...
}
```

### CF-03: Single-line else
Single line `else` is prohibited.
```c
// BAD
if (Result) { ... } else { ... }

// BAD
if (Result) { ... }
else { ... }

// GOOD
if (Result)
{ 
    ...
}
else
{
    ...
}
```

### CF-04: Single point of return per function
```c
// BAD
if (!File) { return false; }
...
return true;

// GOOD
bool Result = true;
if (!File)
{
    Result = false;
}
else
{ 
    ...
}
return Result;
```

### CF-05: Always use braces on switch cases
Each case gets its own brace block, so declarations inside a case are scoped and the
fallthrough boundary is unambiguous.
```c
// BAD
switch (Type)
{
    case AssemblyType_Executable:
        String Ext = S(".exe");
        Result = Ext;
        break;
}

// GOOD
switch (Type)
{
    case AssemblyType_Executable:
    {
        String Ext = S(".exe");
        Result = Ext;
        break;
    }

    default:
    {
        break;
    }
}
```

### CF-06: Always use the `FALL_THROUGH` annotation macro for explicit fallthrough cases
`-Wimplicit-fallthrough` is an error. A case that deliberately falls through must say so.
An empty case label stacked directly on the next one does not need the annotation.
```c
// BAD
case Compiler_Clang:
{
    DoClangSetup();
}
case Compiler_GCC:
{
    DoSharedSetup();
    break;
}

// GOOD
case Compiler_Clang:
{
    DoClangSetup();
    FALL_THROUGH;
}
case Compiler_GCC:
{
    DoSharedSetup();
    break;
}

// GOOD - stacked labels, no annotation needed
case Compiler_Clang:
case Compiler_GCC:
{
    DoSharedSetup();
    break;
}
```

### CF-07: Always have a default case on switch statement
`-Wswitch-default` is an error. Note `-Wswitch-enum` is also on, so switching over an
enum must additionally name **every** enumerator explicitly - `default` does not excuse
a missing case.
```c
// GOOD
switch (Vendor)
{
    case Compiler_Generic:    { ... break; }
    case Compiler_Clang:      { ... break; }
    case Compiler_Clang_MSVC: { ... break; }
    case Compiler_GCC:        { ... break; }
    case Compiler_MINGW:      { ... break; }
    case Compiler_MSVC:       { ... break; }
    case Compiler_TCC:        { ... break; }

    default:
    {
        UNREACHABLE();
        break;
    }
}
```

### CF-08: No `goto`
Structure the function with a result variable and nested blocks instead (CF-04).

The usual argument for `goto` is cleanup: several error paths that must each close the
same handle. The goal behind that argument is right - one copy of the cleanup - and CF-04
already gives it to you. Open the resource, do the work inside the success block, and close
it once after the block. The cleanup ends up in one place because there is one way out.
```c
// BAD - the close is copied onto every failure path
bool Result = false;
if (Filesystem_Open(Path, FileMode_Read, &Handle))
{
    if (!ReadHeader(&Handle))   { Filesystem_Close(&Handle); }
    else if (!ReadBody(&Handle)){ Filesystem_Close(&Handle); }
    else                        { Filesystem_Close(&Handle); Result = true; }
}

// GOOD - one close, after the work
bool Result = false;
if (Filesystem_Open(Path, FileMode_Read, &Handle))
{
    Result = ReadHeader(&Handle) && ReadBody(&Handle);
    Filesystem_Close(&Handle);
}
```
When the nesting grows past about three levels, the body is doing too much. Take out a
subtask (MD-13) instead of reaching for a jump.

### CF-09: Loop bodies always use braces
Same as CF-01, including single-statement `for`/`while` bodies.

### CF-10: Prefer the iteration of TArray's with macros over hand-written index loops
`for each (Type, It, Array)` for a `TArray`, `for each_str (It, Array)` for a
`StringArray`, `for each_str_list (List)` for a `StringList`.
```c
// BAD
for (usize i = 0; i < Array_Num(VariablesDB); i++)
{
    FileVariable Var = VariablesDB[i];
    ...
}

// GOOD
for each (FileVariable, Var, VariablesDB)
{
    ...
}
```

---

## Return Values

### RV-01: Discard with `xx`, never `(void)`

Exception: pre-existing (void) casts in third-party code are left alone.

```c
// BAD
(void)SomeFuncWithReturn(...);

// GOOD
xx SomeFuncWithReturn(...);
```

### RV-02: A discarded return value must be genuinely uninteresting
`xx` says "this cannot fail in a way I care about". It is not a way to silence a real
error path. If the call can fail meaningfully, handle it.
```c
// BAD - the write silently vanishing is a real failure
xx Filesystem_Write(Handle, Size, Data, &BytesWritten);

// GOOD - restoring console state on a handle we already validated
xx SetConsoleMode(StdHandle, ConsoleMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
```

### RV-03: Mark functions `NO_DISCARD` when ignoring the result is a bug
Every allocating function, every `bool`-returning "did it work" function, and every
query returning a value the caller must inspect.
```c
// GOOD
RIFT_API NO_DISCARD bool  Filesystem_Open(const String FilePath, EFileMode Mode, FileHandle* OutHandle);
RIFT_API NO_DISCARD void* LinearAllocator_Allocate(LinearAllocator* Allocator, usize Size);
```

### RV-04: Functions that can fail return `bool`, with results delivered via `Out` params
Do not encode failure in a sentinel return value.
```c
// BAD
String Filesystem_GetFilePath(const FileHandle File); // empty String means... failure? empty path?

// GOOD
NO_DISCARD bool Filesystem_GetFilePath(const FileHandle File, String* OutPath);
```

---

## Functions

### FN-01: Functions returning a bool should ask a true/false question
```c
// BAD: what does true mean?
bool CheckTea(...);

// GOOD: makes it clear true means tea is fresh
bool IsTeaFresh(...);
```

### FN-02: Prefix output parameter names with "Out"
```c
// BAD
void SomeFunc(i32 Value, i32* Result);

// GOOD
void SomeFunc(i32 Value, i32* OutResult);
```

### FN-03: Take a `String` by value, and mark it `const` when you do not modify it
`String` is 16 bytes and already a view onto its data - there is no `StringView` type.
Pass `const String` for input, `String*` for a buffer you write into.
```c
// BAD
bool String_StartsWith(const String* Str, const String* SubString, bool bCaseSensitive);

// GOOD
bool String_StartsWith(const String Str, const String SubString, bool bCaseSensitive);
void String_Append(String* Dest, const String Source);
```

### FN-04: Take large structs by `const` pointer
`BuildParams` and friends are passed as `const BuildParams*`, never by value.
```c
// GOOD
bool C_Compile(const BuildParams* Params, u32* OutNumCompiled);
```

### FN-05: A parameterless function declares `(void)`, not `()`
```c
// BAD
void Platform_PreInitialize();

// GOOD
void Platform_PreInitialize(void);
```

### FN-06: Every non-static function must have a prototype in a header
`-Wmissing-prototypes` is an error. If a function is not meant to be public, make it
`static` instead of leaving it prototype-less.

### FN-07: Wrap parameter lists past ~120 columns, aligned under the first parameter
```c
// GOOD
void AddVariable(LinearAllocator* Arena,
                TArray(FileVariable) VariablesDB,
                const String Name,
                const String Value,
                u32 MaxValueLength);
```

### FN-08: Callback-style interfaces take a typedef'd function pointer plus `void* UserData`
This is the shape used for directory iteration, wildcard expansion and dependency-file
parsing. Follow it for any new iteration/parsing entry point; keep the policy at the
call site and the traversal in the callee.
```c
// GOOD
typedef bool (*DirectoryIterator)(const String FullPath, const String RelativePath,
                                  const String FileName, u64 FileSize, bool bIsDirectory, void* UserData);

void Filesystem_IterateDirectory_Ex(const String BasePath, DirectoryIterator Callback,
                                    bool bRecursive, void* UserData);
```

### FN-09: A function is honest - it reaches the outside world only through its signature (aka. a pure function)
An honest function reads and writes nothing that the caller did not give it. Mutation of a
parameter is honest. A hidden input is not. `String_StartsWith` is honest. A function that
reads `InternalVariablesDB`, the clock, the environment or a file it was not told about is
dishonest, because the signature does not say so.

Honesty buys the two things a function exists for: you can reason about the body on its
own, and you can test it. Dishonesty is infectious - a function that calls a dishonest
function is dishonest too - so honest functions collect at the leaves of the call tree.
```c
// BAD - the answer depends on parse state the caller cannot see or control
static bool IsVariableSet(const String Name);

// GOOD - the caller owns the table, so the same table always gives the same answer
static bool IsVariableSet(TArray(FileVariable) VariablesDB, const String Name);
```
**Exception:** the process-wide CLI flags in `Backend.h` (`bQuietBuild`, `bVerboseLog`,
`bNoWordWrapLogging`) and the `LOG_*` macros are read directly everywhere. They are set
during startup and do not change after. Do not add new globals of this kind - see MM-04.

### FN-10: Push the dishonesty to the top of the call tree
Filesystem access, process spawning, console output and global lookups are the point of
the program. Keep them in the outer layer. Keep the logic that feeds them honest, so the
logic can be tested without a disk.

The usual shape of the refactor: the honest half builds a result in memory, then the
dishonest half consumes that result.
```c
// BAD - one function decides what to build and also builds it
static bool CompileChangedSources(const BuildParams* Params, u32* OutNumCompiled);

// GOOD - an honest planner, then a dishonest driver
static bool PlanChangedSources(LinearAllocator* Arena, const BuildParams* Params,
                               StringArray Sources, StringArray* OutToCompile);
static bool CompileSources(const BuildParams* Params, StringArray ToCompile, u32* OutNumCompiled);
```
When the intermediate result is too large to hold, use the callback shape from FN-08
instead. The caller keeps the policy and the honest half keeps the traversal.

### FN-11: Ask for what the function uses, and no more
Two failures, opposite directions. A function that takes a whole struct to read two fields
ties every caller to that struct. A function that takes six loose parameters makes the call
site unreadable, and lets two same-typed arguments swap without a diagnostic.
```c
// BAD - needs one directory, demands the whole params block
static bool IsObjectStale(const BuildParams* Params, const String SourceFile);

// GOOD
static bool IsObjectStale(const String IntermediateBaseDirectory, const String SourceFile);

// BAD - what is the last bool?
bool Export(const String Root, const String Out, const String Name, bool bDebug, bool bClean, bool bForce);

// GOOD - one struct built at the call site, with named fields
bool Export(const ExportParams* Params);
```
FN-04 still holds. When the body genuinely uses most of the struct, take `const BuildParams*`.
The test is what the body reads, not how many parameters you want to type.

### FN-12: Do not over-constrain a parameter type
Take the weakest type that the body needs. A function that only reads a sequence takes a
pointer and a count, so a `SArray` or a plain stack array can call it too. A function that
only reads text takes `const String`, which is already a view - see FN-03.
```c
// BAD - the body only reads the items, but every caller must own a TArray
static u32 CountCppSources(TArray(String) Sources);

// GOOD - a TArray, a SArray and a stack array all work
static u32 CountCppSources(const String* Sources, usize NumSources);
```
Ask for a stronger type only when the body needs the guarantee. If the body calls
`Array_Add`, take the `TArray` and say so.

### FN-13: Every line of a function body stays at one level of abstraction
A function body stacks bricks. Each brick is a call to a lower-level function that already
exists. If you must zoom in on one brick, change it a little, then zoom back out, that zoom
belongs in its own function.

Three symptoms of a mixed body, all of them review flags:
- Section comments inside the body, such as `// convert to lower` or `// binary search`.
  Each one marks a brick that wants to be a function.
- A hand-written loop over characters or indices next to a call into high-level logic.
  See CF-10 and GN-03.
- An algorithm written out in full when the core already has one.
```c
// BAD - three levels in one body: character codes, a search, then the actual question
static bool IsAssetOfType(const String Name, EAssetType Type)
{
    // lowercase the name
    char Buffer[MAX_PATH_LENGTH];
    for (usize i = 0; i < Name.Length; i++) { ... }

    // binary search the table
    usize Low = 0, High = AssetCount;
    while (Low < High) { ... }

    // check the type
    ...
}

// GOOD - one level: find the asset, then answer the question
static bool IsAssetOfType(const AssetTable* Table, const String Name, EAssetType Type)
{
    Asset* Found = NULL;
    const bool bResult = AssetTable_Find(Table, Name, &Found) && (Found->Type == Type);
    return bResult;
}
```
When one function keeps the case rules, the search and the storage layout in step by hand,
that is an ad hoc data structure at the top level. Give it a name and a header. Then the
callers stop copying the rules.

### FN-14: Write the call you want to see, then write the function
If a step is not clear yet, call the function you wish existed and implement it after. Do
not inline the step "for now". This keeps the caller at one level of abstraction (FN-13),
and it keeps you in the thought you were already in.

Pull code into a function the first time you write it, not the third. If you can give the
block a name, it is a function. Code reuse is a side effect of good functions, not the
reason to write one.

The limit is one line. A one-line body stays where it is, however many times it repeats -
the wrapper hides nothing and costs the reader a jump. Extract a block that holds steps, a
loop, or a branch the caller does not want to see.

A second limit is the signature. When the block only survives the move by taking five
parameters, three of them by pointer so it can write back, the block is not separable. It
reads and writes too much of what surrounds it. Leave it, or change the surrounding data so
that a smaller signature becomes possible.
```c
// BAD - a name and an indirection around one statement
static void Internal_SetCmdExitCode(const BuildCmdContext* Ctx, u32 Code)
{
    if (Ctx->ExitCode) { *Ctx->ExitCode = Code; }
}

// GOOD - written out at each call site
if (Ctx->ExitCode) { *Ctx->ExitCode = 1; }
```
```c
// GOOD - written before Vector_Reflect exists
Velocity = Vector_Reflect(Velocity, Surface.Normal);
```
Mark the not-yet-written body with `TODO()`, per DG-04.

---

## Modules and interfaces

A module here is a header plus the .c file behind it, or a single function. Every module
has an interface, which is everything a caller must know, and an implementation, which is
the rest.

### MD-01: A module must be deep - a small interface over a large body
The interface is the cost the module puts on the rest of the program. The body is the
benefit. The best modules hide a lot of work behind a few functions. `Filesystem_Open` is
deep: three arguments hide path handling, access modes and the OS call.

Make the common case simple. A caller that needs the usual behaviour must not learn about
the rare behaviour to get it. An interface with a rare extra is only as complex as the
part that callers use, so put the extra behind its own entry point - see NM-13.

Most of an interface is not in the signature. The call order, the side effects, the units,
the owner of the memory and the meaning of an empty result are interface too, and comments
are the only place they can live (CM-08). A requirement that a caller must obey and cannot
read is a hidden interface, and it is the unknown unknown of DP-07.

### MD-02: A shallow function is worse than no function
A function whose interface is as complex as its body adds a name to learn and gives
nothing back. Small does not mean simple.
```c
// BAD - the caller still knows about the field, and the call costs more than the write
static void SetNoFail(ParsingContext* Ctx, bool bValue)
{
    Ctx->bNoFail = bValue;
}
```
See FN-14 for the one-line limit, which is the same rule from the other side.

### MD-03: One design decision lives inside one module
A file format, a flag spelling, a table layout, a protocol: each belongs to one .c file.
The rest of the program asks that file and never repeats the knowledge. Making a file a
little larger to hold all of one decision is the right trade.
```c
// BAD - Parse.c and Exporter.c both know how a .build value is quoted
// GOOD - Parse.c owns the quoting rules and exports a function for them
```

### MD-04: The same knowledge in two files is information leakage
When one design decision changes, exactly one file must change. Two files that both
understand the same format depend on each other, even when no header says so. Leakage
through a back door is worse than leakage through an interface, because nothing points
at it.

Two corrections work. Merge the two files when the knowledge is most of what they do.
Otherwise lift the knowledge into a module of its own, and give that module an interface
that speaks in results, not in the knowledge itself. A new module that hands the same
knowledge back through its header moved the leak. It did not close it.

### MD-05: Split by knowledge, never by order of operations
"First read it, then parse it, then write it" is temporal decomposition. It puts the same
format knowledge in three places. Group the code that shares knowledge, and let the order
live in the caller.
```c
// BAD - ReadBuildFile() then ParseBuildFileText(); both know the line format
// GOOD - Parse_BuildFile() reads and parses; the caller only asks for the result
```

### MD-06: Make a new module somewhat general-purpose
Size the body to what you need today. Size the interface so that a second caller can use
it. A function written for one call site names that call site in its arguments, and that
ties the two files together.
```c
// BAD - one function per key, each used once
static bool ReadSourceFilesKey(...);
static bool ReadIncludeDirsKey(...);

// GOOD - one function, any key
static bool ReadListKey(const String Key, StringArray* OutValues);
```
Do not go further than that. An interface that needs a loop at every call site is too
general - see FN-12.

Three questions find the line:
1. **What is the simplest interface that covers everything I need today?** When you cut
   the number of functions and keep the same capability, the result is more general. When
   the cut only works by adding arguments, you moved the complexity instead.
2. **How many call sites will use this?** A header function that serves exactly one caller
   is a red flag. Its arguments usually name that caller, which is MD-20. This question is
   about the interface only. A `static` helper called once is fine, and FN-14 asks for it.
3. **Is it still easy to use today?** When the call sites grow loops and scratch variables
   to get ordinary work done, the interface went too far down. Give it the operation they
   actually perform.

### MD-07: Different layer, different abstraction
A function that only forwards its arguments to another function with the same shape is a
pass-through. It adds an interface and no behaviour.
```c
// BAD
static bool WriteLine(FileHandle Handle, const String Line)
{
    return Filesystem_WriteLine(Handle, Line);
}
```
Correct it in one of three ways: let the caller call the lower function, move behaviour
into one of the two, or merge them.

Two shapes repeat a signature and are correct. A dispatcher chooses which function runs,
and the choice is the behaviour. One interface with several implementations repeats the
signature on purpose: `Platform_*` carries the same declaration across five platform files
(PL-04), and that repetition is what lets a reader who learned one file read the next. Both
shapes sit in one layer, and they never call each other.

A thin layer that adds one behaviour over an existing API is mostly pass-through. Before
you write one, try these in order: put the behaviour in the module that already owns the
data, put it in the single call site that wants it, add it to a layer that already exists,
or write it as a separate function that does not wrap anything.

### MD-08: Do not thread a parameter through functions that do not use it
A parameter that four functions pass along and only the fifth reads makes all four depend
on it. Put it in the context struct that the chain already carries.
```c
// BAD
static bool Stage1(const String CertPath, ...);   // never reads CertPath
static bool Stage2(const String CertPath, ...);   // never reads CertPath

// GOOD - ParsingContext already reaches the bottom of the chain
```
The context struct is the allowed place for this. A new global is not - see MM-04 and
FN-09.

A context is the least bad answer, not a good one. Its fields carry most of the faults of a
global: a reader cannot tell why a field is there, or which functions read it, and nothing
stops the struct from becoming a drawer for anything that was awkward to pass. Keep it
small. Add a field only when a real chain would otherwise thread it through functions that
ignore it. Treat a field as read-only after setup unless the whole point of the field is to
change, and say which ones change in the comment above the struct (CM-05).

### MD-09: Pull complexity downward
A module has more callers than authors. When a piece of unavoidable complexity belongs to
what the module does, keep it inside the module. Do not push it out to every caller. A
simple interface matters more than a simple body, so a harder body is the right price for
a header that asks less.

Pull a piece of complexity down only when all three are true:
1. The complexity belongs to what the module already does.
2. The move makes many other places simpler.
3. The move makes this module's interface smaller, not larger.

Condition 3 is the one that catches a bad move. Work that lands in the module and adds a
function to the header did not go down. It went sideways, and it took the caller's
knowledge with it - that is the leakage of MD-04.
```c
// BAD - every caller joins the parts and picks the platform separator
// GOOD - String_BuildPath does it once (ST-04)
```
Pull down what the caller does not care about. Leave in sight what the caller does care
about. These are the same rule, and the second half is the one that gets forgotten: a
detail the caller acts on is not complexity to hide, and hiding it produces the false
abstraction of MD-15. Two extra words at the call site are a fair price for a caller that
can read what happens - see DP-09.

Complexity costs what it costs to read, and nobody reads a file they never open. Hard code
that sits in one place, behind a clean interface, and gets touched twice a year is close to
free. The same work spread over twenty call sites is not. This is why the OS handling stays
inside `Platform_*.c` (PL-01), and why a larger body is the right price for a smaller
header.

### MD-10: Do not export a knob you can compute
A new option, key or parameter moves the problem to the user. Before you add one, ask
whether the user knows the answer better than the code does. When they do not, compute a
default, and keep the option only for the rare override.

Derive the value from something the code can see. A timeout follows the times you already
measured. A buffer size follows the input you already counted. A worker count follows the
CPU count you already asked for. A derived value also follows the conditions, and a
configured value goes stale the day the conditions change.

A module that needs the user to finish it has not finished the job. When you cannot decide
what the right behaviour is, the caller is unlikely to know either, and a knob only spreads
your uncertainty to everyone who runs the tool. The same failure wearing the other hat is
an error reported upward because the author could not decide what to do with it - see
DG-06.

In this repository the expensive form of a knob is a reserved `.build` key. A key is
user-facing language: it goes into build files you do not own, it shows up in the
documentation, and you cannot withdraw it without breaking builds that already use it. A
`.build` key must earn more than a C parameter does.

### MD-11: Join code that shares information; separate code that does not
Bring two pieces together when they share knowledge, when one interface replaces two, when
it removes a repeated block, or when it raises the level of the interface. Keep them apart
when only one of them is general.

Four tests say that two pieces are related. One is usually enough.
1. **They share knowledge.** Both depend on the same format, table or rule.
2. **Each one pulls in the other.** This test only counts in both directions. A build job
   always needs a string array, and string arrays serve many things that are not build
   jobs, so they stay apart. Keep two pieces together only when each is the usual company
   of the other.
3. **One plain word covers both.** Path building and path splitting are both path handling.
4. **You cannot read one without the other.** That is the Conjoined red flag, and joining
   them is the honest answer.

Raising the level is the one people miss. Three functions that every caller must call in
order are one function that nobody has to order.
```c
// BAD - the caller owns the sequence, and the order is interface nobody wrote down
OpenBuildFile(...); ReadBuildFileText(...); ParseBuildFileText(...);

// GOOD
Parse_BuildFile(...);
```
```c
// BAD - one logging helper per call site, each called once, each needing its own comment
static void LogOpenError(const String Path, u32 Code);

// GOOD - LOG_ERROR at the place that finds the error
```

### MD-12: Keep special-purpose code above general-purpose code
Lower layers stay general. When one file holds both a mechanism and one use of that
mechanism, move the use up to the caller and leave the mechanism alone.

This rule is about one mechanism at a time. A file that holds a general mechanism of its
own is free to hold special-purpose code for somebody else's mechanism, when that code
belongs to what the file does. A parser that owns the general `.build` format and also
holds the few lines that describe its own values to the exporter is correct. The same
parser holding the exporter's output rules is not.

### MD-13: Length alone is not a reason to split a function
A long function with a simple signature that reads from top to bottom is deep, and deep is
the goal. Split it when the parts are separable, not when the line count crosses a number.

Every split costs four things: one more name to learn, the code that now moves state
between the parts, the distance the reader must travel, and the lines that both parts need
a copy of. The distance is the worst of them. Two parts that still depend on each other are
easier to get wrong once they sit apart, because the reader no longer sees the dependency
at all - that is an unknown unknown (DP-07).

Two splits are worth making, and each has a test.
- **Take out a subtask.** The caller keeps its own signature and calls the new function.
  This is right when a reader of the new function needs nothing from the old one, and a
  reader of the old one needs nothing from the new body. The new function is usually a
  little general.
- **Cut one function into two that callers see.** This is right only when the original did
  two jobs that are not related, and each new interface is simpler than the old one. Most
  callers must end up calling one of the two. When callers have to call both and carry
  state between them, the split is wrong - put it back.

A split that makes the reader jump between two bodies to understand either one is a bad
split.

### MD-14: Give the caller the right default instead of a question
When the module can work out the answer, work it out. The best behaviour is the behaviour
a caller never has to ask for.

### MD-15: A small interface that hides a fact the caller needs is a false abstraction
MD-01 asks you to leave out the details that do not matter. It does not ask you to leave
out the details that do. An interface that looks simple because it dropped something the
caller must know is worse than a larger honest one: the caller reads the body to find the
fact, and the abstraction bought nothing.
```c
// BAD - looks clean, but the caller must still know that Arena is the one that must
//       outlive the result, and the signature and the name both stay quiet about it
String GetOutputName(const BuildParams* Params);

// GOOD - the arena is in the signature, so the lifetime rule is in front of the reader
String GetOutputName(LinearAllocator* Arena, const BuildParams* Params);
```
Ask which facts a caller acts on. Hide the rest, and the more of it the better. State what
is left, in the signature when it fits there, and in the interface comment when it does
not. DG-09 is this rule for failures, and ST-06 is this rule for lifetimes.

### MD-16: Every element you add must pay for itself
A file, a header, a type, a function, a parameter, a macro: each one is something the next
reader must learn. An element earns its place by removing more complexity than it adds. If
the code would be no harder to read without it, it does not belong there.

"More modules" is not the same as "better modules". Every header adds a name to learn, an
include to place and a boundary to keep. Split a subsystem when the parts hide different
knowledge (MD-03), never because one file grew long.
```c
// BAD - four headers, each with one function, all of them about .build values
// GOOD - one Parse.h that owns the whole of the .build format
```
The test is the same as MD-02, one level up: what does the new element remove?

### MD-17: Never hand out the thing you store
A function that returns your container, your buffer or a pointer into your storage has
published the way you store it. Every later change to the storage is now a change to the
interface, and every caller carries a rule that no header states: do not write to this.
```c
// BAD - the caller can Array_Add into our table, and we can never change the container
TArray(FileVariable) Parse_GetVariables(const ParsingContext* Ctx);

// GOOD - answer the question the caller has, and keep the table
NO_DISCARD bool Parse_GetVariable(const ParsingContext* Ctx, const String Name, String* OutValue);
NO_DISCARD bool Parse_GetVariableU32(const ParsingContext* Ctx, const String Name, u32* OutValue);
```
The second form is deeper. It hides the container, and it does the conversion that every
caller would otherwise write again. When a caller genuinely must walk the whole set, give
it the callback shape of FN-08, which keeps the traversal inside the module.

**Exception:** the container types themselves. `Array_*` and `String_*` exist to hand out
their storage, because that storage is what they are.

### MD-18: `static` is not information hiding
`static` controls who can call a function. It does not control who has to know things.
Knowledge still escapes through a type in a header, an out-parameter shaped like your
storage, a required call order, or a field a caller reads and writes directly.
```c
// BAD - the helper is static, and the knowledge is in the header anyway
static u32 HashBuildKey(const String Key);   // private
STRUCT(BuildKeyTable) { KeyBucket* Buckets; u32 NumBuckets; };  // public, so the scheme is public
```
Ask what a caller must know, not what a caller can call - see MD-01.

### MD-19: Cut the number of places that touch each variable
Information hiding applies inside a file too. A struct member or file-scope variable that
six functions read and write is a dependency with no interface: any of the six can break
the other five. Keep each one inside as few functions as you can, and give the ones that
remain a function that owns them.

### MD-20: Write the interface in the words of the module, not the words of the caller
A parameter type or a function name that carries one caller's vocabulary ties the module to
that caller. The body rarely needs it. Take the plain thing that the body actually reads.
```c
// BAD - Backend.c now owns a word that belongs to the CLI, and the next caller cannot use it
static bool ExpandKey(const CommandLineOptions* Opts, const String Key, String* OutValue);

// GOOD - the body only needs the table and the key
static bool ExpandKey(TArray(FileVariable) VariablesDB, const String Key, String* OutValue);
```
The test is the same as FN-11, one level up: FN-11 asks how much of the struct the body
reads, and this rule asks whose idea the type is. A name like `Cursor` where `Position`
would do is the same defect - it puts the caller inside the callee's header.

### MD-21: The interface must not be shaped like the storage
MD-17 stops you handing out the container. This rule stops you building the interface in
the shape of it. When the functions are named after your storage units, callers do the work
you declined to do, and the same splitting and joining appears at every call site.
```c
// BAD - the interface says "lines", because that is how the body stores text
NO_DISCARD bool Text_GetLine(const TextBuffer* Buf, u32 LineIndex, String* OutLine);
void Text_PutLine(TextBuffer* Buf, u32 LineIndex, const String Line);

// GOOD - the interface says "a range of text"; the body still stores lines
void Text_Insert(TextBuffer* Buf, TextPos At, const String NewText);
void Text_Delete(TextBuffer* Buf, TextPos Start, TextPos End);
```
The distance between the two shapes is the depth of the module. When the header reads like
the struct, there is no distance, and there is no module - see MD-01.

---

## Strings

### ST-01: String literals use `S()`; aggregate initializers use `SC()`
`S()` is a compound literal, which is not valid inside a static aggregate initializer -
that is what `SC()` is for.
```c
// BAD
static const String Table[2] = { S("None"), S("Executable") };

// GOOD
String Name = S("None");
static const String Table[2] = { SC("None"), SC("Executable") };
```

### ST-02: Never call `StrMake` on something that is already a `String`
`StringLocal` and `String` variables are already the right type. `StrMake` exists to
convert a differently-shaped string struct; using it elsewhere is noise.
```c
// BAD
StringLocal(Path, MAX_PATH_LENGTH);
xx Filesystem_DoesFileExist(StrMake(Path));

// GOOD
StringLocal(Path, MAX_PATH_LENGTH);
xx Filesystem_DoesFileExist(Path);
```

### ST-03: Path buffers are sized `MAX_PATH_LENGTH`
Not a hand-picked number. Use `MAX_PATH_LENGTH_EX` only where the long-path form is
genuinely required.
```c
// BAD
StringLocal(FullPath, 512);

// GOOD
StringLocal(FullPath, MAX_PATH_LENGTH);
```

### ST-04: Build strings with the provided builders, not manual concatenation
`String_Format`, `String_Concat`, `String_BuildPath`, `String_Append*`. Path joining
goes through `String_BuildPath` so the platform separator is handled.
```c
// BAD
String_Append(&Path, Directory);
String_AppendChar(&Path, '/');
String_Append(&Path, FileName);

// GOOD
String_BuildPath(&Path, Directory, FileName);
```

### ST-05: `%S` formats a `String`; the format string itself is an `S()` literal
```c
// GOOD
String_Format(&Msg, S("%S\n        Error Code: %i"), Prefix, Code);
LOG_ERROR("Failed to open '%S'", FilePath);
```

### ST-06: A `String` returned from a function must outlive the call
Return either a slice of an input the caller owns, or a copy made from an arena the
caller passed in. Never return a `String` pointing at a `StringLocal` buffer.
```c
// BAD
static String MakeName(void)
{
    StringLocal(Name, 64);
    String_Copy(&Name, S("thing"));
    return Name;              // points at a dead stack buffer
}

// GOOD
static String MakeName(LinearAllocator* Arena)
{
    return String_Duplicate(Arena, S("thing"));
}
```

---

## Memory and allocators

### MM-01: Bounded work uses a stack buffer, not an allocation
When the maximum size is known and reasonable, declare a fixed stack array of the
struct type (so alignment is correct) and degrade gracefully if it does not fit.
Never do the query-size-then-allocate dance for a bounded query.
```c
// BAD
usize Size = 0;
xx QueryThing(NULL, &Size);
Thing* Buffer = MemAlloc(Size, MemoryTag_Unknown);

// GOOD
Thing Buffer[64] = {0};
u32   Count      = 0;
if (!QueryThing(Buffer, SArray_Capacity(Buffer), &Count))
{
    LOG_WARNING("Too many things to enumerate, skipping");
}
```

### MM-02: `LinearAllocator*` means "allocate from this"; `LinearAllocator` by value means scratch
Passing an arena by value gives the callee a private copy of the bump offset, so
everything it allocates is implicitly released when it returns. Take the arena by
pointer only when the allocation must survive the call.
```c
// GOOD - the returned String must outlive the call
String String_Duplicate(LinearAllocator* Arena, const String Source);

// GOOD - temporary working memory, freed by returning
void LogString_WordWrapped(LinearAllocator Scratch, const String Name, const String Value, bool bAddNewLine);
```

### MM-03: Local scratch arenas are created with `ScratchLocal`
```c
// GOOD
ScratchLocal(Scratch, Kibibytes(8));
```

### MM-04: No global fixed-size state
Caches and mutable state hang off a context struct, sized to the workload, allocated
from the arena after a headroom check, and degrade gracefully when memory is tight.
```c
// BAD
static CacheEntry GCache[4096];

// GOOD
STRUCT(BuildContext)
{
    CacheEntry* Cache;
    u32         CacheCapacity;
};
// ... sized from the workload and allocated from Arena at setup
```

### MM-05: Zero-initialize aggregates at declaration
```c
// BAD
BuildParams Params;
MemZero(&Params, sizeof(Params));

// GOOD
BuildParams Params = {0};
```

### MM-06: Use the `Mem*` / `Platform_Mem*` wrappers, never libc memory functions
`MemAlloc`/`MemFree`/`MemZero`/`MemCopy`/`MemMove`/`MemEqual` in engine code.
Windows links the static CRT, so `malloc` and friends do link. That is not permission to
call them. The tool owns one arena, and a CRT allocation does not belong to it.
`memset`/`memcpy`/`memmove`/`memcmp` come from the CRT, because the compiler emits calls
to them for struct copies and zeroing whether you write them or not. Call the `Mem*`
wrappers instead. See PL-02.

### MM-07: Every `MemAlloc` names a real `EMemoryTag`
`MemoryTag_Unknown` is for genuinely uncategorized allocations only; if a fitting tag
exists, use it, and add a new enumerator if a new subsystem needs one.

---

## Containers

### CT-01: Dynamic arrays are declared with `TArray(Type)`
```c
// GOOD
TArray(FileVariable) VariablesDB;
TArray(CompileProcess) Jobs;
```

### CT-02: `Array_Add` takes the address of its argument - never pass an rvalue
```c
// BAD
Array_Add(Messages, S("failed"));            // takes &(compound literal)

// GOOD
String Message = S("failed");
Array_Add(Messages, Message);
```

### CT-03: Arena-backed arrays use `ArrayLocal_Arena`; heap arrays must be destroyed
Prefer arena-backed. If you use `Array_Create`/`ArrayLocal`, there must be a matching
`Array_Destroy` on every path out of the scope.

### CT-04: Fixed-capacity arrays use the `SArray`/`StackLocal` helpers
`SArray_Capacity` for element count, `SArray_Add`/`Stack_Push` for bounded insertion -
these already refuse to overflow. Do not hand-roll a `Count` variable next to an array.

---

## Platform layer

### PL-01: OS-specific code lives in `Platform_*.c` behind a platform-neutral API
Per-OS probing, `#if PLATFORM_*` branches around system calls, and direct OS API calls
belong in `Source/Core/Platform_Windows.c`, `Platform_Unix.c`, `Platform_Linux.c`,
`Platform_macOS.c`, `Platform_BSD.c` or `Platform_Core.c`, exposed through `Platform.h`.

**The calculator test:** the platform API is for OS functionality *any* program would
need - files, processes, time, memory, environment. RiftBuild-*specific* per-OS logic
(which linker flag spelling MSVC wants, which icon format a desktop entry needs) is not
platform-layer material and stays at its call site under `#if PLATFORM_*`.
```c
// BAD - in Program.c
#ifdef _WIN32
    HANDLE H = CreateFileA(...);
#else
    int fd = open(...);
#endif

// GOOD - in Program.c
FileHandle Handle = {0};
if (!Filesystem_Open(Path, FileMode_Read, &Handle)) { ... }

// GOOD - RiftBuild-specific policy, stays at the call site
#if PLATFORM_WINDOWS
    String_Append(&LinkerFlags, S("/SUBSYSTEM:CONSOLE"));
#endif
```

### PL-02: System headers are included only inside `Platform_*.c`
`<unistd.h>`, `<sys/*.h>`, `<stdio.h>`, `<stdlib.h>`, `<string.h>` and the Win32 headers
(via `Win32Types.h`) appear in the platform layer and nowhere else. `<stdarg.h>` is the
one exception, permitted wherever a variadic function is implemented.

### PL-03: Platform detection uses the `PLATFORM_*` macros, never raw compiler predefines
```c
// BAD
#ifdef _WIN32
#if defined(__APPLE__)

// GOOD
#if PLATFORM_WINDOWS
#if PLATFORM_APPLE
```
Same for `COMPILER_MSVC`/`COMPILER_CLANG`/`COMPILER_GCC`/`COMPILER_TCC` and
`CPU_X64`/`CPU_ARM64`/`PLATFORM_64_BIT`.

### PL-04: Every platform function must exist on every supported platform
Windows, Linux, macOS, FreeBSD, NetBSD and OpenBSD. Adding a `Platform_*` function
means implementing it in all of them - a platform that cannot support it returns a
documented failure value, it does not get a missing symbol.

### PL-05: Fallback behaviour must match across platforms
When a query fails, every implementation returns the same fallback. A function that
returns `CACHE_LINE_SIZE` on Windows must not return `0` on macOS.

---

## File layout and headers

### HD-01: .c and .h files open with the two-line copyright header
```c
// Copyright (c) Artisan Softworks
// Licensed under the BSD 3-Clause License. See the LICENSE file for details.
```

### HD-02: Headers use an all-caps include guard matching the filename, closed with a comment
No `#pragma once`.
```c
#ifndef STRINGUTILS_H
#define STRINGUTILS_H
...
#endif // STRINGUTILS_H
```

### HD-03: Includes are wrapped in `#ifndef UNITY_BUILD`
The project builds as a unity build by default; unguarded includes break it.
```c
// GOOD
#ifndef UNITY_BUILD
#include "EngineTypes.h"
#include "StringUtils.h"
#endif
```
**Exception:** `Win32Types.h` in `Platform_Windows.c`, which is included unconditionally.

### HD-04: Public core API declarations are marked `RIFT_API`
Everything exported from `Source/Core` headers. Attribute order is
`RIFT_API`, then `NO_DISCARD`, then the return type.
```c
// GOOD
RIFT_API NO_DISCARD bool Filesystem_DoesFileExist(const String FilePath);
RIFT_API            void Filesystem_Close(FileHandle* Handle);
```

### HD-05: Align declaration columns in headers
Return types, `NO_DISCARD` and function names line up so the header reads as a table -
see HD-04. Keep the alignment when adding a declaration to an aligned block.

### HD-06: Group header declarations with a banner comment
```c
// Compiler/Building functions --------------------
```

### HD-07: Sources are added to the `.build` file, not discovered implicitly
A new .c file in `Source/Core` must be listed in `Source/Core/.build` under `SourceFiles`.

---

## Diagnostics and errors

### DG-01: Report failures with `LOG_ERROR`, not by crashing
User-facing failures (a missing file, a bad build key, a compiler that will not run)
log and return `false`. Crashing is for programmer error only.

### DG-02: `ASSERT`/`ENSURE` are compiled out in release - never put side effects inside
`NO_ASSERT` is defined for all non-debug builds and the whole expression disappears.
Use `ALWAYS`/`NEVER` when the expression must still execute in release.
```c
// BAD - the call vanishes in release
ASSERT(Filesystem_Open(Path, FileMode_Read, &Handle), ...);

// GOOD
bool bOpened = Filesystem_Open(Path, FileMode_Read, &Handle);
ASSERT(bOpened, "Failed to open the file");

// GOOD - ALWAYS keeps the expression in release builds
if (ALWAYS(Handle.Data != NULL))
{
    ...
}
```

### DG-03: `ASSERT` is for invariants that cannot fail unless the code is wrong
A malformed build file is not an assert - it is a `LOG_ERROR`. A null arena inside a
private helper is an assert.

### DG-04: Use `UNREACHABLE()` for impossible switch defaults, `TODO()` for unfinished work
```c
// GOOD
default:
{
    UNREACHABLE();
    break;
}
```

### DG-05: Log messages address the user, and name the thing that failed
No internal function names, no jargon. Include the path/key/value that went wrong.
```c
// BAD
LOG_ERROR("ExpandVariable failed (ret -1)");

// GOOD
LOG_ERROR("Could not expand the variable '%S' used by key '%S'", VarName, Key);
```

### DG-06: Define errors out of existence
The cheapest error to handle is the one that cannot happen. Widen what the function
accepts, and the error case disappears with it.
```c
// BAD - the caller tests the range first, at every call site
NO_DISCARD bool String_Slice(const String Str, u32 Start, u32 End, String* OutResult);

// GOOD - an out-of-range request returns the part that overlaps, or an empty String
String String_Slice(const String Str, u32 Start, u32 End);
```
A remove function that is asked to remove something absent has no work to do. That is
success, not an error.

### DG-07: Design special cases out of existence
A special case becomes an `if` in every function that touches the data. Choose a
representation where the normal path already covers it. An empty range is better than a
`bHasRange` flag next to a range.

### DG-08: Handle an error at the level where it stops spreading
Two shapes work. Mask it low: correct the condition inside the module, and no caller
learns about it. Masking is MD-09 applied to failures, and it makes the module deeper.
Aggregate it high: let one kind of failure travel up to one handler that reports it. Do not
write the same handler at every call site.

### DG-09: Do not hide an error the caller needs
DG-06 to DG-08 apply only when the information is of no use outside the module. When a
caller must act on the failure, report it, even though the interface grows.

### DG-10: The failures you report are part of the interface
Every distinct way a function can fail is one more thing its callers must learn and handle,
and it does not stop at your caller. A failure travels up until somebody deals with it, so
it reaches the interface of every function on the way. A module with many failure modes is
a shallow module, for the same reason as one with many functions.

Reporting a failure is easy, and handling one is hard, so the count is the thing to watch.
Before you add a failure mode, ask whether DG-06 removes it, whether DG-08 hides it lower
down, or whether it joins a failure that already exists.

### DG-11: Error paths are the code that never runs
Normal-case code runs every build. An error path might run once a year, and code that has
never run does not work. Most catastrophic failures come from error handling that was
wrong the whole time and nobody noticed. Two things follow.

Prefer the design with fewer error paths, even when the body costs more. A path that cannot
exist cannot be broken, which is the whole of DG-06 and DG-07.

Keep the failure path free of new failure. A handler that allocates, opens or parses can
fail while it cleans up after the first failure, and that second failure arrives with the
state already half undone. Clean up with calls that cannot fail, and never report an error
out of a cleanup path - log it and keep the first error, which is the one the user needs.

### DG-12: A function that cannot usefully fail must not report failure
When every caller would answer a failure the same way, answer it once inside the function.
The arena allocators work this way: `LinearAllocator_Allocate` is `RETURN_NON_NULL`, so no
call site null-checks it, and no call site carries a path for a condition it could not fix.
```c
// BAD - a check at every call site for something no caller can do anything about
void* Block = LinearAllocator_Allocate(Arena, Size);
if (!Block) { LOG_ERROR("out of memory"); Result = false; }

// GOOD - the allocator keeps the promise, and the caller reads straight through
void* Block = LinearAllocator_Allocate(Arena, Size);
```
This works only while the function really keeps the promise. A function that says
`RETURN_NON_NULL` must stop the program when it cannot keep it, in every build
configuration, not only in debug - see DG-02. Do not widen this rule. It covers conditions
that mean the program is finished, such as an exhausted arena or a broken invariant. A
missing file, a bad key and a compiler that will not run are user-facing, and DG-01 governs
those.

---

## Macros

### MC-01: A multi-statement macro is wrapped in `do { ... } while (0)`
```c
// GOOD
#define Array_Empty(Array) do { ... } while (0)
```

### MC-02: Macro parameters are parenthesized at every use
```c
// BAD
#define Kilobytes(x) (x*(usize)1000)

// GOOD
#define Kilobytes(x) ((x)*(usize)1000)
```

### MC-03: Macro-local variables use `MACRO_VAR` to avoid shadowing
`-Wshadow` is an error, so a macro that expands inside a scope holding a similarly
named variable will break the build without this.
```c
// GOOD
#define String_Concat(Dest, ...) \
    do { String MACRO_VAR(SArgs)[32] = {__VA_ARGS__}; ... } while (0)
```

### MC-04: Do not use the `is` / `isnt` / `and` / `or` / `not` aliases
They exist in `EngineTypes.h` but are used nowhere in the codebase. Write `==`, `!=`,
`&&`, `||`, `!`.

### MC-05: Prefer a `static` function over a function-like macro
Reach for a macro only when you need the token pasting, the variadic pack, or the
type-genericity that C99 cannot express.

---

## Commenting
Comments are communication for humans to read and communication is vital.

### CM-01: Keep comments short, readable and easy to understand
Use simple language.
Do not use jargon or terms that require extra context for the reader to understand where possible.
```c
// BAD
// Normalize the ACLE-encoded minor rev to its major ordinal for predicate evaluation.

// GOOD
// EBX bits [15:8] = CLFLUSH line size in 8-byte units
```

### CM-02: Do not write long comments for workaround fixes.
If you need a paragraph-long comment to justify why a particular workaround is OK, the code is wrong - fix the code.
```c
// BAD
// We call this twice because the first call sometimes doesn't take effect
// when the handle was inherited from a parent process, which happens when
// the build is launched from a shell wrapper, and in that case the mode
// bits get reset by the time we... (etc.)
xx SetConsoleMode(H, Mode);
xx SetConsoleMode(H, Mode);
```
**Exception:** an external bug you cannot fix in your own code (a compiler bug, an OS
quirk) is worth a paragraph, because the reader otherwise has no way to discover it.
Those live in the `.build` file or next to the flag that works around them, and they
cite the bug.

### CM-03: Do not over comment bad code - rewrite it instead.
A comment that labels a section inside a function body is a special case of this. It marks
a block that wants to be its own function - see FN-14.

```c
// BAD
// total number of leaves is sum of
// small and large leaves less the
// number of leaves that are both
t = s + l - b;
    
// GOOD
TotalLeaves = SmallLeaves + LargeLeaves - SmallAndLargeLeaves;
```

### CM-04: Comment the why, never the what
The code already says what it does.
```c
// BAD
// increment the index
Index += 1;

// GOOD
// Redirected handles fail GetConsoleMode and are left alone.
if (GetConsoleMode(StdHandle, &ConsoleMode))
```

### CM-05: Document a struct's contract above the struct, not member by member
Units, ownership and whether a path is absolute or relative go in a short block above
the type, or as a trailing comment on the member it qualifies.
```c
// GOOD
STRUCT(BuildParams)
{
    String RootDirectory;         // absolute
    String SourceDirectory;       // relative
    String IntermediateBaseDirectory; // absolute (it is root + intermediate combined)
};
```

### CM-06: Mark unfinished work with `// TODO:` and say what is missing
```c
// GOOD
// TODO: implement
ENUM(EResourceCompiler)
```

### CM-07: Never write a comment addressed to a reviewer
No "changed this to fix the bug", no "as requested", no "this is now correct".
Comments are for the next reader of the code.

### CM-08: Every public declaration carries an interface comment
A declaration alone does not say what a function does. Write a short block above each
`RIFT_API` function, each public struct and each public enum. State the behaviour a caller
sees, the units, who owns any memory, what an empty or zero result means, and any call
that must come first.
```c
// GOOD
// Reads the whole file into OutData. The caller sizes the buffer from
// Filesystem_GetFileSize. OutBytesRead gets the number of bytes read, which is
// less than the file size when the file shrank after the size query.
// Returns false and writes nothing when the read fails.
RIFT_API NO_DISCARD bool Filesystem_ReadEntireFile(const FileHandle Handle, void* OutData, usize* OutBytesRead);
```
State the limits too. What the function will not do is interface: it is not reentrant, it
handles at most N entries, it works on one thread. A caller who finds that out by testing
found it too late.

When a function only runs in unusual circumstances, say which circumstances reach it. The
call sites are hard to find and the conditions are harder, so a line of "this runs when..."
saves the next reader the search.

Keep the body out of it. An interface comment that describes the body says the function is
shallow - see MD-01.

### CM-09: Write the interface comment before the body
The comment is the design check. Write the signature and the comment, read them back, then
write the body. When the comment is long, or when you cannot write it at all, the design is
wrong. Correct the design, not the comment.

### CM-10: Document a decision once, then point at it
A decision spread over two comments goes stale in one of them. Put the text in the single
most obvious place, and write `// See the comment on <name>.` at the others. When no single
place is obvious, the module split is wrong - see MD-03.

Never repeat text that already exists outside the code. Cite the manual, the header or the
bug number instead.

### CM-11: Put the explanation in the code, not in the commit message
A reader who is about to undo your fix looks at the code, not at the log. When a commit
message holds something the next reader needs, the code needs it too.

### CM-12: The farther a comment sits from the code, the more abstract it must be
A comment next to a line describes that line exactly. A comment at the top of a function
describes the steps. A comment in a header describes behaviour only. Detail that sits far
from its code goes stale first.

### CM-13: Comment a local variable that outlives a screen
Most locals need nothing. When every use of a variable sits within a few lines, the code is
the documentation. When one lives across a long body, say what it holds.

Say what it represents, not how the code changes it. A comment that tracks the assignments
is longer than the one that states the meaning, and it goes stale at the first edit.
```c
// BAD - a record of the writes
// Set to true in the loop below when a source compiles, cleared again by the retry path.
bool bDidWork = false;

// GOOD - the meaning, from which the writes follow
// True once any target produced a new artifact, so the caller knows a relink is needed.
bool bDidWork = false;
```

---

## General

### GN-01: Braces on new lines always
```c
// BAD
i32 GetSize() { ... }
 
// GOOD
i32 GetSize()
{
    ...
}
```

### GN-02: Leave a blank line at the end of the file.
All .c/.cpp and .h files should include a blank line, to coordinate with gcc.

### GN-03: Use intermediate variables to simplify complicated expressions.

If you have a complicated expression, it can be easier to understand if you split it into sub-expressions, that are assigned to intermediate variables, with names describing the meaning of the sub-expression within the parent expression.

For example:

```c
  if ((Blah->BlahP->WindowExists->Etc && Stuff) &&
      !(bPlayerExists && bGameStarted && bPlayerStillHasPawn &&
      IsTuesday())))
  {
      DoSomething();
  }
```

Should be replaced with:

```c
const bool bIsLegalWindow = Blah->BlahP->WindowExists->Etc && Stuff;
const bool bIsPlayerDead = bPlayerExists && bGameStarted && bPlayerStillHasPawn && IsTuesday();
if (bIsLegalWindow && !bIsPlayerDead)
{
    DoSomething();
}
```

### GN-04: Pointers and references should only have one space to the right of the pointer or reference.

This makes it easy to quickly use Find in Files for all pointers or references to a certain type. 

For example:

```c
  // BAD
  ShaderType *Ptr
  ShaderType * Ptr

  // GOOD
  ShaderType* Ptr
```

### GN-05: Shadowed variables are prohibited

`-Wshadow` is on with `-Werror`, so this is a build failure, not a style preference.
It also applies to a local shadowing a global or a parameter.

### GN-06: Avoid using anonymous literals in function calls.
Prefer named constants which describe their meaning. This makes intent more obvious to a casual reader as it avoids the need to look up the function declaration to understand it.

```c
// BAD
Trigger(S("Soldier"), 5, true);
    
// GOOD
const String ObjectName               = S("Soldier");
const f32 CooldownInSeconds           = 5;
const bool bVulnerableDuringCooldown  = true;
Trigger(ObjectName, CooldownInSeconds, bVulnerableDuringCooldown);
```

### GN-07: Indent with 4 spaces, never tabs

### GN-08: Declare variables close to first use, and initialize at declaration
C99 allows it. An uninitialized declaration at the top of a long function is how
`-Wuninitialized` failures and stale-value bugs happen.
```c
// BAD
String Path;
u32 Count;
... 40 lines ...
Path = ...;

// GOOD
u32 Count = 0;
...
StringLocal(Path, MAX_PATH_LENGTH);
```

### GN-09: Use `const` on locals that never change after initialization
Especially the intermediate variables introduced by GN-03 and the named constants of GN-06.

### GN-10: One blank line between functions; blank lines inside a function separate steps
Never two or more consecutive blank lines inside a function body.

### GN-11: Byte-size arithmetic uses the size macros
`Kilobytes`/`Megabytes`/`Gigabytes` for decimal, `Kibibytes`/`Mebibytes`/`Gibibytes` for
binary. Never a bare `1024*1024`.

### GN-12: Follow the local convention, even when you prefer another one
When you open a file, look at how it is already written, then write the same way. A better
idea is not a reason to add a second style. Change a convention only when you have new
information, and only when you change every old use in the same change.

### GN-13: Code that surprises the reader must say so
When a function keeps working after it returns, holds a lock, or leaves a handle open, the
reader has no way to guess it. Say it in the interface comment, and say it again at the
call site when the call site looks finished.

### GN-14: Optimize the critical path by removing special cases
When a measured path is too slow, write down the smallest amount of code that the common
case needs. Then move the real code toward that shape. Most of the cost is the special
cases, and the shallow layers between the caller and the work - see MD-01 and MD-07.

---

## Red flags

Symptoms to look for in review. Each one says the design is probably wrong, not that one
rule is broken.

| Red flag | What you see | Rule |
| --- | --- | --- |
| Shallow module | The interface is as complex as the body. | MD-01, MD-02 |
| False abstraction | The interface looks simple, and the caller still reads the body. | MD-15 |
| Information leakage | Two files know the same format or rule. | MD-03, MD-04 |
| Exposed representation | A function hands back the container or a pointer into it. | MD-17 |
| Temporal decomposition | The file split follows the order of operations. | MD-05 |
| Overexposure | A common call makes the caller learn a rare feature. | MD-01, MD-10 |
| Pass-through function | A function only forwards its arguments. | MD-07 |
| Pass-through parameter | A parameter travels through functions that never read it. | MD-08 |
| Repetition | The same block appears again and again. | MD-11, FN-14 |
| Special-general mixture | One file holds a mechanism and one use of it. | MD-12 |
| Interface mirrors storage | The header reads like the struct behind it. | MD-21 |
| Caller's word in a header | A parameter type or name belongs to one call site. | MD-20 |
| One-caller header function | A public function serves exactly one call site. | MD-06 |
| Conjoined functions | You cannot read one body without the other. | MD-13 |
| Comment repeats code | The comment says what the next line says. | CM-04 |
| Body detail in a header | An interface comment describes the implementation. | CM-08 |
| Vague name | The name fits many different things. | NM-10 |
| Hard to name | No short and precise name fits. | NM-12 |
| Hard to describe | The comment must be long to be complete. | CM-09 |
| Nonobvious code | A quick read gives the wrong answer. | DP-04, GN-13 |
| Many failure modes | One function reports several distinct failures. | DG-10 |
| Failure inside a handler | The cleanup path can itself fail. | DG-11 |
