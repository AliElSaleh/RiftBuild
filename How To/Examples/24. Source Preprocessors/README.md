# 24. Source Preprocessors

`PreCompileFile` / `PreCompileAllFiles`: run a tool over the module's source
files right before compilation. The tool here is a metaprogram (sip) that
rewrites string-interpolated printf calls -
`printf("viewport: {width} x {height}")` - into the plain
`printf("viewport: %d x %d", width, height)` form a C compiler can digest.

The whole pipeline builds in one command: the `sip/` subfolder holds the
metaprogram, which is built against this repository's own `Source/Core`
library. The root `.build` names the tool as a `Depend`,
so RiftBuild builds it first (Core, then the exe), then runs the freshly
built exe over this module's sources, then compiles them. Building your
codegen tools as part of the build that uses them is exactly what the
`Depend` + `PreCompile*` combination is for. (Windows-only: the metaprogram
is a Windows program, and the `.build` asserts as much.)

## Try it

```
riftbuild
Build/Interpolated.exe
```

Then reopen `main.c` and `stats.c`: the interpolated printf calls have been
rewritten *in place* into plain C - that is the pre-pass doing its work
before the compiler ran. Type a new interpolation (any local variable, e.g.
`printf("{scale}\n");`), rebuild, and watch it expand.

## The key ideas

- The build file names only the program and its own arguments; RiftBuild
  appends the source paths:
  - `PreCompileAllFiles.Exec tool` - one run, every source file's full path
    appended.
  - `PreCompileFile.Exec tool` - one run per source file, that file's full
    path appended last.
- Both fire on every build, right before compilation (after `PreCompile`),
  and a failing command stops the build.
- Hooks run after dependencies are built, so a `Depend` on the tool's own
  module guarantees the exe exists before the hook needs it.
- The appended paths are absolute, and quoted when they contain spaces.
- The `SourceFiles main stats` whitelist matters: without it, recursive
  source discovery would sweep the tool's own sources under
  `sip/` into this module.
- A dependency that is an *executable* is a tool dependency: it is built
  first, but nothing links against it and none of its compile/link settings
  leak into this module.
- The tool only rewrites files that actually contain interpolations and
  leaves the rest untouched. That matters for any tool used in these hooks:
  one that blindly rewrote every file would dirty the timestamps and
  re-trigger a full recompile on every build.
- IDE squiggles about "unused" variables in the un-built sources are the
  usual expected noise - the variables are used as soon as the pre-pass
  expands the interpolation tokens.

## Things to try

- Switch the hook to the commented-out `PreCompileFile.Exec` line and
  rebuild: same result, but the tool launches once per file.
  `PreCompileAllFiles` is the better fit for this tool - one process, and it
  parallelizes across files internally.
- Interpolate a name that does not exist (`printf("{nope}\n");`). The tool
  leaves the token untouched (so the file still compiles) and reports a
  warning.
- Open `sip/Program.c` - the metaprogram is a single heavily commented C
  file, and its header comment documents the full supported syntax and type
  table.

Next: [25. macOS App Bundle](../25.%20macOS%20App%20Bundle/) - packaging an
executable, its icon and its data files into a `.app`.
