# Feature Set Tests

End-to-end tests for RiftBuild. Each numbered folder is one self-contained test
case: `cd` into it (or into its `App` subfolder for multi-module tests) and run
`riftbuild`. A test passes when the build succeeds and the produced executable
prints its `OK ...` line and exits 0.

To build the whole suite with one command, run `riftbuild` from *this* folder:
the [.build](.build) here is a phony aggregator that `Depend`s on every test.
Tests 1 and 57 are excluded (they have no .build file - that is their feature),
and the tests that need extra invocations (18, 19, 28, 43-46, 54) only get their
plain build.

Wherever possible the tests are self-verifying at *compile time*: the sources
contain `#error` traps that fire when the feature under test misbehaves, so a
regression turns into a build failure. Ignore IDE/clangd diagnostics in these
folders - the sources only compile with the defines the .build file provides.

| # | Test | Feature under test |
|---|------|--------------------|
| 1 | No Build File | Building a lone .c file with no .build file |
| 2 | Icon | `Icon` embedding into the exe |
| 3 | ParallelSample | Dependency chain App -> B -> C, parallel build graph |
| 4 | Minimal Executable | Smallest valid .build (Assembly + SourceFiles) |
| 5 | Auto Source Discovery | No SourceFiles key; recursive discovery; `__` dirs skipped |
| 6 | Source Directory | `SourceDirectory` + extensionless SourceFiles |
| 7 | Defines | Flag and value macros via `Compiler.Defines` |
| 8 | UnDefines | per-file `<file>.Compiler.UnDefines` strips a compiler-predefined macro |
| 9 | Includes | Multiple `Compiler.Includes` directories |
| 10 | Static Library | `Type static_lib` produces a .lib |
| 11 | Shared Library | `Type shared_lib` produces a .dll |
| 12 | Output Naming | Nested custom Build/Intermediate directories |
| 13 | Conditional Values | `Key:cond` / `Key:!cond` suffixes (platform, bit-width) |
| 14 | If Else Blocks | Block `if/else` and single-line `if` |
| 15 | Custom Variables | `$Var` expansion, `$-lower` / `$^UPPER` case modifiers |
| 16 | Environment Variables | `@ENVVAR` expansion |
| 17 | Command Expansion | `!command` runs a shell command at parse time |
| 18 | Options Binary | `option.name` as a build condition (run with/without `turbo`) |
| 19 | Options Values | `%option` value paste (run with `level=7`) |
| 20 | Per File Overrides | `<file>.Compiler.Defines` scoped to one translation unit |
| 21 | Compiler Flags | Global `Compiler.Flags` + per-file `<file>.Compiler.Flags` |
| 22 | Block Namespaces | `Linker { ... }` block = `Linker.*` keys |
| 23 | Multiline And Comments | `## ##` comments, block values, `` Key` `` value reset |
| 24 | Linker Settings | `Linker.Subsystem`, `Linker.Stack` |
| 25 | Phase Hooks Order | PreBuild/PreCompile/PostCompile/PreLink/PostLink/PostBuild order |
| 26 | WriteFile Codegen | `PreBuild.WriteFile` generates a header the build consumes |
| 27 | File Operations | NewDir/NewFile/WriteFile/Copy/Rename/Delete verbs |
| 28 | Run After Build | `.Run args` executes the fresh build with arguments |
| 29 | Public Propagation | `Compiler.Defines.Export`/`Compiler.Includes.Export` reach dependents; plain keys don't (also keeps one bare `Defines.Public` key covering the deprecated alias and bare-key forms) |
| 30 | Private Dependencies | `Depend(private)` links but does not re-export |
| 31 | Diamond Dependency | Same library depended on via two paths builds/links once |
| 32 | Dependency Filter Args | `Depend path \| options` forwards options to the dependency |
| 33 | Mixed C And Cpp | .c + .cpp in one assembly, C++ link driver auto-selected |
| 34 | Version Info | `Version(define)` macros + version resource metadata |
| 35 | License Generate | `License(generate) MIT` writes a LICENSE file |
| 36 | Copyright Enforce | `Copyright(enforce)` header check on every source |
| 37 | Asserts | `Assert.Platform` / `Assert.EnvVarExists` / `Assert.ProgramExists` / `Assert.BuildVarExists` |
| 38 | Include Buildvars | `include file.buildvars` shares variables |
| 39 | SourceFiles Exclude | Auto discovery minus `SourceFiles.Exclude` entries |
| 40 | Compiler GCC | `Compiler gcc` toolchain selection |
| 41 | Compiler TCC | `Compiler tcc` toolchain selection |
| 42 | Max Cores | `Compiler.MaxCores` throttled parallel compilation |
| 43 | Incremental Rebuild | mtime-based skip; touch one file -> only it recompiles |
| 44 | Clean Command | `riftbuild clean` manifest-driven artifact removal |
| 45 | Export Compile Commands | `riftbuild export:cc` generates compile_commands.json |
| 46 | Presets | `preset:name` bundles command-line options |
| 47 | Program Exists Condition | `if program_exists(...)` blocks |
| 48 | Nasm Assembly | .asm source assembled by nasm, linked with C |
| 49 | Import Keyword | `import` as an alias for `include` |
| 50 | Flat Intermediate Objects | `IntermediateDirectory X/.` flat object dump; link finds flattened objects |
| 51 | Resource Keys | `Resource.Includes/Defines/UnDefines` reach the resource compiler; RCDATA read back at runtime |
| 52 | Assert Args | `Assert.Arg(count)` parameters (`=N` `>N` `>=N` `<N` `<=N`, combined); multiple lines assert independently |
| 53 | Install Packages | `InstallPackage` resolves the system package manager, queries, installs only what is missing |
| 54 | Many Sources | 1000 translation units: header dependency tracking at scale, per-file incremental skips, ~24KB link line |
| 55 | Wildcard File Operations | `*`/`?`/`**` wildcards in Copy/Move/Delete sources; if_not_exist per match; wildcard Delete never touches directories |
| 56 | Block Phase Commands | Any command verb under a Pre*/Post* phase followed by a `{ }` block runs one command per line; lines share the verb's parameters |
| 57 | Cpp Files | A lone .cpp with no .build file builds as C++ and links the C++ runtime automatically |
| 58 | Depend Two Token Options | Two-token `Depend <name>.build <dir>` combined with `\| options` forwarding |
| 59 | No Assembly Type | `Type no_assembly` runs a tool per source, produces nothing, never links; "Transforming" UI |

Tests 18, 19, 28, 43, 44, 45, 46, 52, 54 need extra invocations beyond a plain build to
exercise their feature (documented in each .build header comment).

## Semantics verified while writing this suite

- `.Export` keys (`Compiler.Defines.Export`, `Compiler.Includes.Export`, ...) are
  **export-only**: they apply to consumers of the module, not to the module's own
  compilation. (`.Public` is a deprecated alias for `.Export` and warns on use;
  bare `Defines`/`Includes`/`UnDefines` are deprecated in favor of the
  `Compiler.*` namespaced forms.)
- Auto discovery skips files whose *name* starts with `__` (directories with
  that prefix are still traversed).
- `!command` expansion accepts a single plain token (no arguments, dots or
  path separators) and pastes its stdout.
- Inside `WriteFile`/`AppendFile` heredocs, `#` still starts a comment - write
  `\#define` to emit preprocessor lines.
- `*.Copy src dst`: `dst` is a destination *directory*.
- `.Run`'s default working directory is the Build directory; use `| .` to run
  from the build-file directory.
- Real assert keys: `Assert.Platform`, `Assert.Arch`, `Assert.EnvVarExists`,
  `Assert.Program`, `Assert.Compiler(.Version)`, `Assert.File`, ... Unknown
  `Assert.*` names are a hard error that lists the available asserts.
- Split version defines are named `<ASSEMBLY>_MAJOR_VERSION` (level in the
  middle), plus `<ASSEMBLY>_VERSION_STRING`.

## Known issues found by this suite

1. (FIXED) Global `UnDefines` was displayed/exported but never added to the
   compile command (Backend.c:640 omitted `Params->UnDefineFlags`). Test 8 now
   covers both the global and per-file forms and requires the fixed binary.
2. (FIXED) A no-op incremental build used to rewrite `.artifact_paths` without
   the link artifacts, so `clean` right after a no-op build left the exe
   behind. Fixed via `RecordSkippedLinkArtifacts` (Backend.c), which re-records
   the linker outputs when the link step is skipped.
3. (FIXED) Dependency modules built via `Depend` never wrote their own
   `.artifact_paths` manifest (the manifest path was a stack-backed StringLocal
   captured into the shallow-copied ModuleNode, dangling by execution time), so
   `clean_all` left dependency Build outputs behind. Fixed by making the path
   arena-backed (StringArena) in Program.c; `clean_all` on the diamond test now
   removes every Build output.
4. (FIXED) `IntermediateDirectory X/.` flat-object mode was honored when
   compiling (Internal_DoCompile dropped the source's relative path) but not
   when linking/archiving (Internal_AppendObjSourceFiles still built
   `Intermediate/./sub/file.c.o`), so any flat-mode build with sources in
   subdirectories failed at link with "no such file or directory". Fixed by
   mirroring the compile-side flattening rules in Internal_AppendObjSourceFiles.
   Test 50 covers it.
