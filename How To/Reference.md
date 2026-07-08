
# The .build File Reference

This is the full reference for the `.build` file syntax. If you are new, do the [Examples](Examples/) first - they teach the same material as small projects you can poke at. This document is for looking things up, including the edge cases.

---

### Basics
A .build file is made up of key-value pairs. Before the first whitespace is the Key, anything after that is the Value. Keys are case-insensitive.

Note: keywords like `if`, `else`, `import`, `assert`, etc are not considered variables.
```make
--------------------------------------
|     Key      |        Value        |
--------------------------------------
Compiler.Flags  -std=c99 -O3 -Wall ...
```

There is no `=` between key and value - the first run of whitespace is the separator. A key may contain letters, digits, `_` and `.` (max 128 characters), and must start with a letter or `.`. A `;` ends a statement just like a newline, so you can put two statements on one line if you must.

Every key has a default. An empty (or missing) .build file still builds: RiftBuild finds a compiler, discovers your sources, names the output after the file (or folder), and puts everything in `Build/` and `Intermediate/`. Only write down what differs from the defaults.

Any key that is not a built-in becomes a **user variable** (see [Variables](#variables)). This is also the trap: a misspelled built-in key does not error, it silently becomes an inert variable. If a key seems to do nothing, check the spelling first (and see the [old key names appendix](#appendix-old-key-names)).

---

### Comments
`#` comments to the end of the line. `##` opens a **block comment** that swallows everything - including lines that look like keys - until the next `##` (or the end of the file).

```make
# a line comment
Compiler.Defines REAL=1   # trailing comments work too

##
Compiler.Defines FAKE=1   this entire region is dead
##
```

To write a literal `#`, escape it: `\#`. You will need this inside `WriteFile` blocks that emit preprocessor lines (see [Writing Files](#writing-files)).

`\` is the escape character in general: `\x` becomes a literal `x` for any character. The important consequence: **a lone backslash gets eaten**, so Windows-style paths lose their separators. Always use forward slashes in paths - RiftBuild normalizes them per platform. If you truly need a literal backslash, write `\\`.

---

### Values

**Values accumulate.** Repeating a key appends to it (space-joined). This is what makes conditional lines compose:
```make
Compiler.Defines          CORE=1
Compiler.Defines:windows  ON_WINDOWS=1
Compiler.Defines          EXTRA=1     # final value: CORE=1 ON_WINDOWS=1 EXTRA=1 (on windows)
```

**The backtick reset.** When you want to discard what accumulated and start over, put a backtick after the key:
```make
Compiler.Defines  FIRST_TRY=1
Compiler.Defines` FINAL_ONLY=1        # value is now just FINAL_ONLY=1
```

**List blocks** split a long value over many lines. Newlines inside become single spaces:
```make
SourceFiles
[
    main
    src/lexer
    src/parser
]
```

**Namespace blocks** group related keys instead of repeating a prefix:
```make
Linker
{
    Subsystem Console
    Stack     4194304
}
```
is exactly `Linker.Subsystem Console` + `Linker.Stack 4194304`. The same works for `Compiler { ... }` and friends, and blocks nest.

**Quotes** preserve whitespace inside a value, and the quote characters themselves are kept. To define a C *string* macro, escape the quotes: `Compiler.Defines NAME=\"Rift\"`.

**Log messages.** A bare quoted string on its own line prints a message during the build:
```make
"Configuring the renderer module..."
```

**The `(parameter)` pattern.** Several keys take a parameter in parentheses, directly after the key name, that extends what the key does:

| Key | What the parameter adds |
|-----|-------------------------|
| `Version(define) 2.4.1` | Also inject the version as macros into every source file |
| `Copyright(enforce) ...` | Also check every source carries the notice |
| `License(generate) MIT` | Also write the LICENSE file |
| `Depend(private) path` | Depend, but don't re-export its usage requirements |
| `Copy(if_not_exist) src dst` | Copy only when the destination doesn't already exist |

**Value length limits.** Every built-in key has a maximum value length (generous - `SourceFiles` allows 32767 characters, most others 1-8k). A value that exceeds the limit is **silently truncated**, so if the tail of a very long flag list goes missing, this is why. Split things into variables or use conditions instead of building megabyte-long lines.

---

### Variables
To reference a variable in another variable, place a `$` before the name of the variable. (Case Insensitive)
```make
CommonFlags some flags
Compiler.Flags $CommonFlags
```

This will expand to
```make
CommonFlags some flags
Compiler.Flags some flags
```

Sometimes you would want to concatenate using a variable. Wrap the variable around with `$()`.
```make
ThirdPartyFolder Source/ThirdParty
Library.Paths $(ThirdPartyFolder)/SomeLib/bin
```

Variables expand recursively - a variable may reference other variables and they all resolve. Referencing a variable from itself (directly or through a chain) is an error, and an undefined `$variable` expands to nothing.

**Name parsing is greedy.** An unbracketed name consumes letters, digits, `_` *and* `.` - so `$Mode.buildvars` looks up a variable literally named `Mode.buildvars`. When a `.` follows the variable, use the bracketed form: `$(Mode).buildvars`. (`${Mode}` also works.)

**Case forcing.** Put `-` or `^` between the sigil and the name to force the pasted value to lower or UPPER case:
```make
ProjectTag Rift
Compiler.Defines TAG_LOWER_$-ProjectTag TAG_UPPER_$^ProjectTag   # rift / RIFT
```
This works for all three sigils (`$` `@` `%`).

Variables expand anywhere a value is read - including inside `\"...\"` string defines and inside `WriteFile` blocks.

---

### Environment Variables
To reference environment variables, place an `@` before the name of the environment variable. (Case sensitive)
```make
Library.Paths @(CURL_PATH)/lib
```
This will expand to
```ini
Library.Paths "C:/Program Files/curl/lib"
```
If the value is a path containing spaces, RiftBuild wraps it in quotes for you (as above). Referencing an environment variable that does not exist is an error.

---

### Internal Variables/Command Line Arguments
To reference a command line argument passed into `riftbuild` or an internal variable, place a `%` before the variable. (Case Insensitive)
```make
Assembly %_FileName # an internal variable that will expand to whatever the .build is called (without the extension)
```

Sometimes you need to access a value inside the build file from what was given on the command line. Command line arguments can be a singular phrase or a `Key=Value` option.
```make
# on the cmd line
> riftbuild someapp.build somearg
> riftbuild someapp.build somekey=somevalue

# inside the .build file
Compiler.Flags %somekey # this will expand to somevalue (or nothing if not mentioned on the cmd line)
Hello %somearg          # this will expand to 1         (or 0 if not mentioned on the cmd line)
Hello %%somearg         # this will expand to somearg   (or nothing if not mentioned on the cmd line)
```

Command line arguments can come in handy when you want to do some basic control flow. Like enabling address sanitizer for example.
```make
# on the cmd line
> riftbuild someapp.build asan mode=debug

# inside the .build file
if asan AsanFlags -fsanitize=address -fsanitize-trap
if mode == debug   Compiler.Flags -O0
if mode == release Compiler.Flags -O3

Compiler.Flags -std=c99 -Wall $AsanFlags
```
Notice how `%` was not present in the `asan` and `mode` if statement. This is because we search all variables whether it be a user made build file variable or a command line argument, therefore to save on typing and to simplify the syntax, the `%` or `$` is optional.

**Built-in internal variables** all start with `_`. Run `riftbuild --internals` to see the full list with live values on your machine. The highlights:

| Variable | Expands to |
|----------|-----------|
| `%_FileName` / `%_FileNameExt` | The .build file's name, without / with extension |
| `%_DirectoryName` (or `%_FolderName`) | The name of the folder containing the .build file |
| `%_FileDirectory` / `%_FileDirectoryFull` | The .build file's directory, relative / absolute |
| `%_WorkingDirectory` | The directory riftbuild was run from |
| `%_Platform` (+ `.Version`, `.Major` ...) | Host OS name and version |
| `%_Arch`, `%_Bit` | CPU architecture, 64 or 32 |
| `%_CPU`, `%_CPUBrand`, `%_CPUVendor` | Processor identification |
| `%_Ram` (+ `.KB` `.MB` `.GB`) | Installed memory |
| `%_Date` (+ `.Year` `.Month` `.Day` `.MonthName` `.DayName` `.DayOfYear` `.NoSep` ...) | Build date |
| `%_Time` (+ `.Hour` `.Minute` `.Second` `.Millisecond` `.NoSep`) | Build time |
| `%_Timestamp` (+ `.Zone`, `.NoSep`) | Date and time combined |
| `%_Version` (+ `.Major` `.Minor` `.Patch`) | RiftBuild's own version |
| `%_ExeExtension`, `%_ExeType`, `%_LibC` | `.exe`/empty, pe/macho/elf, the libc |
| `%_Distro`, `%_DesktopEnvironment` | Linux distro / desktop environment |
| `%_NativeLibs` (also `%_Win32Libs`, `%_LinuxLibs`, ...) | The platform's standard system libraries |
| `%_UUID` | A UUID, fixed for this run |
| `%_uuid.gen` | A fresh UUID at *every* mention |
| `%_Args` | The full command line |

The greedy-name rule applies here too, but in your favor: `.` is part of the name, so `%_Date.Year` just works.

---

### Command Expansion
To paste the output of a program at parse time, place a `!` before its name:
```make
BuildHost !hostname
Compiler.Defines BUILT_ON=\"$BuildHost\"
```
The rules:
- **Single plain word only** - no arguments, no paths, no dots. `!hostname` works, `!git status` does not. For real commands with arguments, use a hook verb such as `PreBuild.Cmd git describe` instead.
- Runs in the build file's directory (via `cmd.exe /c` on Windows).
- Trailing newlines are trimmed from the output.
- If the program fails (non-zero exit), nothing is pasted.
- Case forcing works: `!-hostname` pastes it lowercased.

---

### Conditions
Three ways to write a condition, in order of preference:

**1. Suffix on a key** - best for a single conditional value:
```make
Compiler.Defines:windows  ON_WINDOWS=1
Compiler.Defines:!windows ON_WINDOWS=0
```

**2. if/else block** - best when several keys change together:
```make
if windows
{
    Compiler.Defines PLATFORM_NAME=\"Windows\"
    Libraries        winmm
}
else
{
    Compiler.Defines PLATFORM_NAME=\"Other\"
}
```

**3. Single-line if** - best for one-offs. **It must stay on one line**, and so must its `else`:
```make
if x64 Compiler.Defines POINTER_BITS=64
if debug Compiler.Flags -O0 else Compiler.Flags -O3
```
Do not mix the forms: `{` after a single-line if is an error. And note there is no block-form `else if` - write `else { if ... }` instead.

##### What can be a condition
- **Built-in platform names**: `windows`, `linux`, `macos` (also `apple`, `mac`, `osx`), `bsd`, `unix`, `posix`, `win32`, `win64`.
- **Built-in architecture names**: `x64`, `x86`, `x86_64`, `arm`, `arm64`, `ppc`, ... plus `64_bit`/`32_bit` and `big_endian`/`little_endian`.
- **Options and command line arguments** (see [Options](#options)).
- **Any variable**: a variable holding `1`/`on`/`yes`/`true` is true, `0`/`off`/`no`/`false` is false, any other non-empty value counts as "defined, therefore true".
- **A path**: any condition containing a path separator becomes an existence check. A trailing `/` checks for a *directory*, otherwise a *file*:
```make
if !external/glfw/ PreDepend.Cmd git clone https://github.com/glfw/glfw external/glfw
```
- **`program_exists(name)`**: true when the program is findable on PATH (or give it an explicit path). The adaptive cousin of `Assert.Program`.
- **`find_system_header(name.h)`**: true when the header exists in the system include directories.

`!` negates any condition. Prefixes narrow where the name is searched: `$name` only user variables, `%name` only internals/command-line, `@name` only environment.

##### Comparisons
```make
if mode == debug        Compiler.Flags -O0
if level >  5           Compiler.Defines HIGH_LEVEL
if %_Platform.Version v>= 10.0   Compiler.Defines MODERN_OS
if $CommonFlags contains -Wall   Compiler.Defines STRICT
```

| Operator | Meaning |
|----------|---------|
| `==` `!=` | String equality (case-insensitive by default) |
| `>` `>=` `<` `<=` | Numeric comparison |
| `v==` `v>` `v>=` `v<` `v<=` | Semantic version comparison (`1.10.0 v> 1.9.9`) |
| `contains` / `starts_with` / `ends_with` | Substring tests |

Note: a single `=` is not a comparison operator. Prefix the test value with `^` to make a comparison case-sensitive. Quote test values that contain spaces.

##### Combining conditions
- **OR**: separate with `or` or `|`. Works between whole conditions (`if windows or linux ...`) and between test values (`if mode == debug|dev ...`).
- **AND**: nest the ifs (`if windows if x64 ...`), or chain colons in the suffix form (`Compiler.Defines:windows:x64 WIN64=1`).
- There is no `and` keyword and no general parenthesized boolean expressions. If your condition logic outgrows this, compute an intermediate variable in an if-block and test that.

One subtlety worth knowing: conditions are evaluated top to bottom, but an if whose variable does not exist *yet* is deferred and re-evaluated after the whole file is read. So testing a variable defined further down works. Testing a variable *before* redefining it uses the value at that point - normal reading order.

---

### Options
`option.<name> <help text>` declares a switch the *user* of your build file controls from the command line. The help text is what `riftbuild options` (and `riftbuild help`) prints.

```make
option.turbo Enables turbo mode.
option.level Sets the launch level (pass level=N on the command line).

# A binary option is a condition, same syntax as the built-in ones:
Compiler.Defines:turbo  TURBO=1
Compiler.Defines:!turbo TURBO=0

# A value option is pasted with % - it expands to 0 when not passed:
Compiler.Defines LEVEL=%level
```
```
> riftbuild                 # turbo off, level 0
> riftbuild turbo level=7   # both set
```

**Restricting and defaulting values.** A parenthesized list on the declaration names the accepted values; the first one doubles as the default. Passing anything else stops the build and prints the accepted list:
```make
option.mode(debug release) Selects the build configuration.
```

Changing options changes the resolved key values, which automatically triggers exactly the right amount of recompilation - you never need `rebuild` after flipping an option.

Forward an option to a dependency with `Depend ../Lib | mode=%mode` (see [Dependencies](#dependencies)). Require one to be present with `Option.<name>.Assert` (see [Asserts](#asserts)).

---

### Preset Options
Preset options can be defined if you have a lot of options that can get long and messy to type out for various configurations of your program. Or perhaps the user does not know which one to use or what best suits them.

```make
# in the .build file
preset:max    turbo level=9
preset:apples debug some_option another_option max_apples=100 enable_asan alwaysfullscreen
```
```
# on the command line
> riftbuild preset:apples
```

You can stack extra options on top of a preset (`riftbuild preset:apples some_arg another_one=8`) and the order of arguments does not matter. On a conflict, what you typed wins over what the preset bundles: `riftbuild preset:max level=2` uses level 2 even if the preset says `level=9`. Presets can also be picked by their position in the file: `preset:0` is the first one. Running `riftbuild help` presents the list of presets (if available).

Opinion: you should avoid structuring your software around many options like this where possible, and rarely use this feature, if you can help it. This feature is here to help shorten the command line and to make the users lives easier by not having to think about the correct combination of options to use.

---

### Help Text
`.help` attaches a description to the build file, printed by `riftbuild help` above the option list:

```make
.help Builds the RiftBuild engine. Use "riftbuild release" for an optimized build.
```
The value can also be a `{ }` block, in which whitespace and line breaks are preserved exactly.

---

### Asserts
Asserts stop the build immediately - before any compilation - with a clear message when the environment is wrong. Compare that with what users normally get: a weird error from deep inside the build, or worse, a successful build that misbehaves.

```make
Assert.Platform     windows
Assert.Arch         x64
Assert.Program      nasm
Assert.EnvVarExists VULKAN_SDK
```

| Key | Checks |
|-----|--------|
| `Assert.Platform names...` | The build machine's OS. A token may pin the arch too: `windows:x64` |
| `Assert.Platform.Version >=10.0` | OS version (prefix with `>` `>=` `<` `<=`) |
| `Assert.Arch names...` | The target architecture |
| `Assert.EnvVarExists NAMES...` | Environment variables are set (aliases: `Assert.EnvVar`, `Assert.Environment`) |
| `Assert.Program tools...` | Programs are findable on PATH |
| `Assert.File paths...` / `Assert.Directory paths...` | Files / folders exist (relative to the build file) |
| `Assert.Compiler names...` / `Assert.Assembler names...` | Which toolchain was selected |
| `Assert.Compiler.Version >=17` | How new the compiler is |
| `Assert.Version 0.2.0` | Minimum RiftBuild version |
| `Assert.WorkingDirectory path` | The build must be run from this directory |
| `Assert.Arg [names...]` | At least one (or these specific) command line arguments were given |
| `Assert.BuildVar names...` | Build variables exist |
| `Assert.Desktop names...` | The desktop environment (Linux/BSD) |
| `Assert.CPUVendor` / `Assert.CPUExtensions` | Processor requirements |
| `Option.<name>.Assert` | The option must be supplied on the command line |

Values are space-separated lists ("any of these passes") and matching is case-insensitive. Asserts run after the whole file is parsed and stop at the first failure.

A misspelled assert name is an error: `Assert.Programm` stops the build and lists the available asserts. Prefer a *conditional* to a hard failure? `if program_exists(tool)` lets the build adapt instead of stopping.

##### Custom error messages
Attach your own message to a failing assert with a `<Context>.<Name>.ErrorMessage` key. The context is the assert family (`Platform`, `Arch`, `Program`, `File`, `Directory`, `Env`, `Compiler`, `Option`, ...), the name is the specific thing that failed - or `*` for a catch-all, with `|` separating several names:

```make
Assert.Program nasm
Program.nasm.ErrorMessage Install nasm from nasm.us and make sure it is on your PATH.
Platform.*.ErrorMessage   This project only builds on the platforms listed above.
```
The message can be a `{ }` block for multiple lines, and variables expand inside it.

---

### Imports
`import` (or `include` - same keyword) pulls another file's key lines into this one. Use it for product-wide values shared between build files:

```make
# shared/common.buildvars
ProductVersion 2.1.0
CommonDefines  PRODUCT_NAME=\"Family\"

# any .build file
import shared/common.buildvars
Version $ProductVersion
```

The path is variable-expanded and resolves relative to the build file. Imports can nest, and a file that was already imported is skipped (so diamond imports and accidental cycles are harmless). You **cannot** import a `.build` file - imports are for variable files (`.buildvars` by convention); modules are composed with `Depend` instead.

---

### The Assembly
The *assembly* is the thing being built. All of these are optional:

```make
Assembly         MyApp        # output name. Default: the .build file's name,
                              # or the first source file's name if the .build file is unnamed
Assembly.Prefix  lib          # pasted before the name
Assembly.Postfix _d           # pasted after it
Type             executable   # what kind of thing to build
Extension        .plugin      # override the output extension
```

`Type` accepts these (each row lists the accepted spellings):

| Type | Spellings |
|------|-----------|
| Executable (the default) | `executable`, `exe`, `app`, `application`, `bin`, `binary` |
| Static library | `static_lib`, `static`, `static_library` |
| Shared library (DLL / .so / .dylib) | `shared_lib`, `shared`, `shared_library`, `dynamic`, `dynamic_lib`, `dynamic_library` |
| Both static + shared | `lib`, `library` |
| Precompiled header | `pch`, `gch`, `pre_compiled_header` |
| Custom tool output (codegen) | `object`, `compiler_object` |
| No output (grouping/phony) | `null`, `none`, `phony` |

If `Type` is absent but `Extension` is set, the type is inferred from the extension. On non-Windows platforms, libraries automatically get the conventional `lib` prefix.

`Type object` turns the module into a custom code-generation step: the "compiler" is whatever tool you name with the `Compiler` key, and outputs are named `Assembly.Prefix` + the extensionless source name + `Extension`. See [Choosing Tools](#choosing-tools).

---

### Sources
With no keys at all, RiftBuild **recursively discovers** every source file under the build file's folder. Discovered extensions: `.c .cc .cxx .c++ .cpp .asm .s` (plus `.rc`/`.manifest` on Windows and `.m`/`.mm` on Apple).

One exception: files whose **name** starts with `__` are skipped. Use this for scratch files you want to keep around without building.

```make
SourceDirectory  src                 # root the discovery somewhere else (must be a relative path)
SourceDirectories       gfx audio    # only these subdirectories
SourceDirectories.Exclude tests      # ...or everything except these
SourceFiles      main src/lexer      # explicit whitelist (extension optional)
SourceFiles.Exclude broken.c         # discovery minus these
```

The whitelist/exclude matching rules, precisely:
- `SourceFiles` is a **whitelist over discovery** - it can only select files discovery would have found. An entry pointing outside `SourceDirectory` (e.g. `../other/foo.c`) is **silently dropped**. To pull in sibling-directory sources, move `SourceDirectory` up (`SourceDirectory ..`) and whitelist from there.
- Wildcard entries (`*.c`, `*PhysX.rc`) match against the **bare filename** in any directory. Directory-prefixed wildcards (`gfx/*.c`) never match - the `*` and the path cannot mix.
- Non-wildcard entries match against the **relative path** (from `SourceDirectory`). With an extension it must match exactly; without one, both sides are compared extension-stripped.
- The same rules apply to `SourceFiles.Exclude`.

---

### Directories
```make
BuildDirectory        Build          # where the final output goes (default: Build)
IntermediateDirectory Intermediate   # object files and state (default: Intermediate)
```
Both must be **relative** paths (they resolve against the build file's folder). Give each configuration its own pair and they stay independently incremental:
```make
if release { BuildDirectory Build/Release } else { BuildDirectory Build/Debug }
```

**Flat object mode:** end the intermediate path with `/.` (`IntermediateDirectory Intermediate/.`) and every object file is written directly into that folder, ignoring the source tree's structure. Useful for codegen modules that emit into a committed directory.

---

### Choosing Tools
```make
Compiler gcc          # or clang, tcc, cl, a path like tools/bin/mycc, ...
```
The `Compiler` key picks the toolchain. With no key, RiftBuild searches for the first available of: `clang, gcc, egcc, cc, clang++, g++, cl, clang-cl` (`cl`/`msvc` locates Visual Studio automatically - no vcvars shell needed). The linker, archiver and assembler are derived from the chosen compiler; each can also be forced by name the same way (`Assembler nasm`, `Linker`, `Archiver`).

Explicit paths go **in the value**: a value containing a path separator is treated as a path (`.exe` appended if needed, relative paths resolved against the build file's folder). A conditional pick is one line:
```make
if program_exists(gcc) Compiler gcc
```

For custom codegen (`Type object`), the tool's command line is shaped by `Compiler.CompileFlag` (the flag before the source file), `Compiler.OutputFlag` (the flag before the output), `Compiler.Flags`, and `Compiler.ObjectDirectory`/`Compiler.ObjectExtension` for where and what the outputs are.

---

### Compiler Keys
```make
Compiler.Flags     -std=c99 -Wall      # passed through verbatim
Compiler.Includes  include thirdparty  # -I directories, relative to the build file
Compiler.Defines   NDEBUG MAX=4       # -D macros; NAME or NAME=value
Compiler.UnDefines __STDC__            # -U, strips a macro (even predefined ones)
Compiler.MaxCores  4                   # cap parallel compiles (clamped to your actual core count)
```
Everything here is per-source-file and triggers exactly the recompiles it should when changed. Includes and defines also come in `.Export` flavors - see [Exports](#exports).

Advanced (mainly for custom toolchains): `Compiler.OutputFlag`, `Compiler.CompileFlag`, `Compiler.ObjectExtension`, `Compiler.ObjectDirectory`.

---

### Per-file compiler settings
Sometimes one file needs extra flags, includes or defines on top of the shared ones. Name the file (with its extension) and give it its own `Compiler.Flags`, `Compiler.Includes`, `Compiler.Defines` and/or `Compiler.UnDefines` - as a block:
```make
Compiler.Flags   -O2
Compiler.Defines NDEBUG

Parse.c
{
    Compiler.Flags   -O0 -fno-inline   # this one file builds unoptimised
    Compiler.Defines PARSER_TRACE
    Compiler.Includes thirdparty/pcre
}
```
...or inline, one key at a time:
```make
Parse.c.Compiler.Flags   -O0 -fno-inline
Parse.c.Compiler.Defines PARSER_TRACE
```
Both forms are equivalent. These are added *on top of* the shared settings, only for that file.

Files are matched by bare filename (case-insensitive) - no directory paths, no wildcards. If two source files share a name across different folders, both get the override. The full filename with extension is required: `main.Compiler.Defines` (no `.c`) is silently ignored.

---

### Assembler Keys
```make
Assembler          nasm
Assembler.Flags    -f win64
Assembler.Includes asm/include
Assembler.Defines  SOME_SYMBOL
```
`.asm` and `.s` files go to the assembler; `.S` files are run through the C preprocessor first. Note nasm's default output format is a flat binary - you almost always want an explicit `-f` flag.

---

### Linker Keys
```make
Linker.Flags        -Wl,--gc-sections   # passed through verbatim
Linker.Subsystem    Console             # portable: emits /subsystem:console (msvc),
                                        # -Wl,-subsystem:console (clang), etc.
Linker.Stack        4194304             # reserve size; add a second number for commit
Linker.EntryPoint   my_main
Linker.NoStdLib                         # flag key, presence = on
Linker.NoDefaultLibs
Linker.DelayLoadDLL big.dll rare.dll    # windows: /DELAYLOAD per dll
Linker.Manifest     app.manifest        # windows: embedded manifest
Linker.Manifest.NoEmbed
Linker.RPath        ../lib              # unix: adds -Wl,-rpath entries
Linker.RPathOrigin  @loader_path        # override $ORIGIN / @executable_path
Linker.Defines      SOME_LINK_SYMBOL
```
Prefer `Linker.Subsystem` over hand-rolled per-compiler subsystem flags - it emits the right spelling for whichever toolchain is active. One classic case where you need it: if `main` lives inside a static library, MSVC-style linkers cannot infer the subsystem and fail with LNK1561; `Linker.Subsystem Console` in the executable's .build fixes it.

Flag keys (`Linker.NoStdLib`, `Linker.NoDefaultLibs`, `Linker.Manifest.NoEmbed`) are on by presence - they take no value, and anything written after them is ignored.

Advanced: `Linker.OutputFlag`, `Linker.Flags.Export` (see [Exports](#exports)).

---

### Archiver Keys
```make
Archiver          llvm-ar     # rarely needed - derived from the compiler
Archiver.Flags    ...
Archiver.OutputFlag ...
```

---

### Libraries
```make
Libraries        winmm opengl32     # system/prebuilt libraries to link
Library.Paths    thirdparty/bin     # -L directories
Apple.Frameworks Cocoa Metal        # macOS frameworks
```
Platform-conditional linking is the usual pattern:
```make
Libraries:windows winmm
Libraries:linux   m pthread
```
`%_NativeLibs` expands to the platform's standard system library set if you want "just give me the usual ones".

---

### Exports
`.Export` keys are **usage requirements**: they describe what *consumers* of a module need, and they do **not** apply to the module's own compilation - declare the plain key too if the module itself needs the same thing.

```make
# in a library's .build
Compiler.Includes.Export .                    # consumers get this include path
Compiler.Defines.Export  ENGINE_API_VERSION=3 # ...and this define
Libraries.Export:windows winmm                # ...and link winmm on windows
```
A consumer just writes `Depend ../Engine` and receives all of it - no duplicated include paths, one source of truth per module. Exported *relative* paths are automatically rebased so they are correct from the consumer's directory.

The full export set: `Compiler.Includes.Export`, `Compiler.Defines.Export`, `Libraries.Export`, `Library.Paths.Export`, `Linker.Flags.Export`, `Apple.Frameworks.Export`.

Details and edge cases:
- Exports travel transitively up the dependency chain - unless a link in the chain used `Depend(private)` (see [Dependencies](#dependencies)).
- For a **static library**, the plain `Libraries`/`Library.Paths` also travel to the consumer (a static lib cannot link anything itself - its link-time needs are the consumer's problem). If the `.Export` variant is declared, it takes **precedence** and only the export list travels.
- `.Public` is the deprecated old name for `.Export`; it still works but warns.

---

### Dependencies
`Depend` (or `Depends`) points at another module. Each dependency goes on its own line.

```make
Depend ../MathLib                  # folder containing the module's .build file
Depend external/glfw               # relative to this build file
Depend(private) ../Compression     # consume, but do not re-export (see below)
Depend physics.build engines/      # name the .build file when a folder has several
Depend ../Lib | turbo level=9      # forward options to the dependency's build
```

The rules:
- One command builds the whole tree: dependencies are parsed and built first, in dependency order. Diamond shapes are fine - a module shared by two dependents is built and linked once.
- A value with a path separator is a **directory** (the `.build` inside is found automatically). A bare name is a **.build filename**. The two-token form `Depend <name> <dir>` picks a specific `.build` file in a directory that holds several.
- **`Depend(private)` stops the export chain.** The dependent consumes the private module's exports and links its objects, but passes nothing on to *its* consumers - implementation details stay contained.
- Everything after `|` is passed to the dependency as its command line options. Combine with your own options to forward configuration down: `Depend ../ | double_precision=%double_precision`.
- Circular dependencies are detected and rejected.
- Dependencies build in the order listed. On unix-style linkers, list a static library before the libraries it uses.

To fetch a dependency that is not on disk yet, guard-clone it in `PreDepend` - it runs before dependencies are parsed:
```make
if !external/glfw/ PreDepend.Cmd git clone https://github.com/glfw/glfw external/glfw
Depend external/glfw
```

---

### Build Phase Hooks
Seven hook points, always in this order:

```
PreDepend  (before dependencies are parsed)
PreBuild
PreCompile
    ... compilation ...
PostCompile
PreLink
    ... link ...
PostLink
PostBuild
```

Each hook takes a **verb**, either inline (`PostBuild.Copy src dst`) or in a block where the statements run in written order:

```make
PostBuild
{
    NewDir dist
    Copy   Build/Packer.exe dist
    Copy   assets/config.ini dist
}
```

| Verb | Does |
|------|------|
| `NewDir dir` (or `NewDirectory`) | Create a folder |
| `NewFile file` | Create an empty file |
| `WriteFile file { ... }` | Write the block's contents (overwrites) - see [Writing Files](#writing-files) |
| `AppendFile file { ... }` | Same, but appends |
| `Copy src dst-dir` | Copy `src` *into* the folder `dst-dir` |
| `Rename old new` (or `Move`) | Rename / move a file |
| `Delete file` | Delete a file (refuses `*`, `.`, `..`, `/`) |
| `Cmd command args...` (or `Exec`, `Command`, `Execute`) | Run an arbitrary command - the escape hatch when no verb fits |
| `Log message` | Print a message |
| `Wait ms` (or `Sleep`) | Pause |
| `Download url dest` | Fetch a file from the web (dest may be a directory; skips if the file exists) |
| `Zip` / `Unzip` | Archive / extract |

The big gotcha: **`Copy`'s second argument is a directory**, not a target filename. `Copy Build/Packer.exe dist` produces `dist/Packer.exe`; to change the name too, `Copy` then `Rename`.

Verb parameters, in parentheses after the verb: `Copy(if_not_exist)`, `Rename(if_not_exist)`, `WriteFile(if_not_exist)` skip when the destination exists; `Cmd(no_wait)` doesn't wait for the command; `(ignore_errors)` on any verb keeps the build going on failure.

Hooks run on every successful build, including no-op builds where nothing recompiled. They do not run on `clean`. All paths and commands resolve relative to the build file's folder.

##### .Run
`.Run` runs the freshly built executable after the build - the tight loop you want for tools, demos, and test runners:

```make
.Run hello world | .
```
The part after `|` is the **working directory** for the run. It matters for programs that load files by relative path:
- omit it -> the program runs from its `Build/` folder
- `| .`  -> runs from the build file's folder
- `| ..` -> runs from the parent folder, and so on.

`.Run` fires on every successful build, even no-op ones - exactly what you want for a test runner. `.Run(only_done_work)` skips the run when nothing was actually compiled, and `.Run(external)` launches in a new window. Combine with options for an opt-in test pass: `if run_tests .Run --run-all-tests`.

---

### Writing Files
`WriteFile` and `AppendFile` take a path and a `{ }` block whose contents are written verbatim - newlines preserved, variables expanded, balanced braces (C code) pass through fine:

```make
PreBuild.WriteFile config.h
{
    \#pragma once
    \#define BUILD_DATE "%_Date"
    \#define BUILD_LEVEL %level
}
```

The one rule to remember: **`#` still starts a build-file comment inside the block.** Escape preprocessor lines as `\#define` / `\#pragma` / `\#include`, or the line silently vanishes into a comment.

For version stamping specifically, skip the hook entirely - `Version(define)` (see below) generates the macros for you.

---

### Version And Metadata
```make
Version      2.4.1
TitleName    My Application
InternalName myapp
Description  Does the thing, fast
CompanyName  Artisan Softworks
Copyright    Copyright (C) 2026 Artisan Softworks
```
On Windows these fill the executable's **version resource** - the Properties -> Details panel. RiftBuild writes and compiles the `.rc` for you; you never touch a resource file. `Version` defaults to `1.0.0`.

**`Version(define)`** also injects the version into every translation unit as macros named after the assembly (uppercased, `-` becomes `_`):
```c
STAMP_VERSION_STRING   // "2.4.1"
STAMP_MAJOR_VERSION    // 2   - plain integers,
STAMP_MINOR_VERSION    // 4     so they work in #if
STAMP_PATCH_VERSION    // 1
```
Extra dotted components become `<NAME>_EXTRA_VERSION_<n>`. The integer macros only appear when the version contains at least one `.` - a non-dotted version produces just the string macro.

---

### Copyright
The `Copyright` key records the notice as metadata - the version resource and `License(generate)` both use it. Two parameters extend it:

- **`Copyright(define) <text>`** injects `<NAME>_COPYRIGHT_STRING` as a macro.
- **`Copyright(enforce) <text>`** turns the notice into a build rule: every compiled source file must contain the text, or the build fails naming the offending file. The build-system version of a lint rule - new files without the header simply do not build, in anyone's checkout, with zero CI configuration.

---

### License
```make
License(generate) MIT
```
Writes a complete LICENSE file next to the build file, with the copyright line filled from the `Copyright` key (set it, or the license ships with a placeholder). Delete the file and it comes back on the next build.

Known licenses: `MIT`, `BSD2`, `BSD3`, `Unlicense`, and `FuckYou` (yes, really). Unknown names warn and skip. The plain `License` key just records the name as metadata.

`License.Path` picks the output directory and `License.FileName` the filename (default `LICENSE`).

---

### Icon
To set an icon for an executable, specify the name of the icon file (with or without the extension)
```make
Icon someicon.ico # or path/to/icon/file.ico
```
The fact that other build systems are unable to do this is fucking pathetic and embarrassing.

If no extension was specified, RiftBuild will automatically choose the correct extension based on the operating system. So that means you can have an icon.ico and icon.png in the same directory and the correct one will always be chosen.

| Windows | Linux  | Mac    | BSD    |
|---------|--------|--------|--------|
| `.ico`  | `.png` | `.png` | `.png` |

On Linux and BSD, the following Desktop Environments are supported:
- GNOME
- KDE
- XFCE4
- MATE
- Cinnamon

The path is relative to the build file (and must stay relative - absolute Windows paths lose their backslashes to escaping, see [Comments](#comments)).

---

### Precompiled Headers
```make
PCH   pch/precompiled.pch
PCH.h pch/precompiled.h
```
`PCH.h` names the header to precompile, `PCH` where the result goes (`.pch` for MSVC, `.gch` otherwise). A dedicated module can also build one with `Type pch`.

---

### macOS Bundles
```make
Bundle                       # flag: produce a .app bundle instead of a bare executable
Bundle.IsTerminal            # flag: a terminal app bundle
Bundle.InfoPlist    my/Info.plist      # use your own plists...
Bundle.VersionPlist my/version.plist
Bundle.PkgInfo      my/PkgInfo
Info.plist          <inline content>   # ...or provide the content inline
Version.plist       <inline content>
```
With just `Bundle`, RiftBuild generates the plists from your [metadata keys](#version-and-metadata).

---

### Incremental Builds And Rebuild Control
Every key is classified by what its change invalidates, and RiftBuild diffs the resolved values between runs:

- **Recompile keys** (flags, defines, includes, compiler selection, source layout) - changing one recompiles the affected sources. This is why flipping an option or editing a define never needs a manual `rebuild`.
- **Relink keys** (libraries, linker flags, output name, metadata) - changing one relinks, keeping the objects.
- **Neutral keys** (hooks, `License`, `Compiler.MaxCores`) - changing one rebuilds nothing.

Per-run controls:
```make
AlwaysRebuild        # flag: this module always rebuilds from scratch
Compiler.MaxCores 4  # cap parallel compile processes (clamped to your core count)
```
And from the command line: `riftbuild rebuild` forces a full rebuild, `riftbuild clean` deletes everything the build produced, and the `_all` variants (`rebuild_all`, `clean_all`) propagate through every dependency. `riftbuild export:cc` writes a `compile_commands.json` for your editor/LSP.

One caveat for codegen modules: incremental detection is output-vs-source timestamps plus the saved command line, so `@include`-style *implicit* dependencies inside generated sources are invisible - document `rebuild` for those cases.

---

### Appendix: Old Key Names
Keys from older RiftBuild versions that were **removed without a deprecation warning** - they parse as inert user variables and silently do nothing:

| Old (dead) | Current |
|------------|---------|
| `CompilerFlags` | `Compiler.Flags` |
| `IncludeFlags` | `Compiler.Includes` |
| `LinkerFlags` | `Linker.Flags` (move `/subsystem:x` to `Linker.Subsystem x`, `/DELAYLOAD:x.dll` to `Linker.DelayLoadDll x.dll`) |
| `LibraryDirectories` | `Library.Paths` |
| `IncludedSourceDirectories` | `SourceDirectories` |
| `ExcludedSourceDirectories` | `SourceDirectories.Exclude` |
| `ExcludedSourceFiles` | `SourceFiles.Exclude` |
| `Multithread false` | `Compiler.MaxCores 1` |

These still work but **warn** (update them when you see the warning):
- Bare `Includes` / `Defines` / `UnDefines` -> the namespaced `Compiler.Includes` / `Compiler.Defines` / `Compiler.UnDefines`
- The `.Public` suffix -> `.Export`

---

### Appendix: Gotchas
The short list of things that bite, collected from the sections above:

- **Backslashes get eaten.** `\` is the escape character. Use forward slashes in paths; write `\#` for a literal `#` (mandatory in `WriteFile` blocks).
- **A misspelled key is a silent no-op** - it just becomes a user variable. (`Assert.*` names are the exception: an unknown one is a hard error.)
- **`%Var.suffix` parses greedily through the dot.** `%Mode.buildvars` reads a variable named `Mode.buildvars`. Use `%(Mode).buildvars`.
- **Single-line ifs must stay on one line** - the `else` too. And there is no block-form `else if`; nest it: `else { if ... }`.
- **`Copy`'s destination is a directory**, never a filename.
- **`.Export` keys do not apply to the module that declares them** - declare the plain key too if the module needs it itself.
- **`SourceFiles` entries outside the source directory are silently dropped**, and directory-prefixed wildcards (`gfx/*.c`) never match.
- **Per-file overrides need the full filename** - `main.c.Compiler.Defines`, not `main.Compiler.Defines`.
- **`Compiler.Path` does not choose the compiler** - the `Compiler` key does.
- **Over-long values truncate silently** - if the tail of a huge flag list disappears, split it up.
- **`.stop` and `.abort`** are reserved keywords that currently do nothing.
