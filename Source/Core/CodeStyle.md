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
