# 16. Build Asserts

Failing fast with a clear message when the build environment is wrong.

## Try it

```
riftbuild
```

Note: i have `riftbuild` symlinked to just `b`, so building literally takes two taps. `b` + `Enter`.

The asserts all hold on a Windows x64 machine, so the build proceeds.

## Now break one

Change `Assert.Program cmd` to `Assert.Program some_missing_tool` and run
again: the build stops immediately - before any compilation - and names the
missing tool. Compare that with what users normally get: a weird error from
deep inside the build, or worse, a successful build that misbehaves.

## The assert family

| Key | Checks |
|-----|--------|
| `Assert.Platform windows` | The build machine's OS |
| `Assert.Arch x64` | The target architecture |
| `Assert.EnvVarExists NAME` | An environment variable is set |
| `Assert.Program tool` | A program is findable on PATH |
| `Assert.File path` / `Assert.Directory path` | A file / folder exists (relative to the build file) |
| `Assert.Compiler name` (+ `.Version`) | Which compiler was selected, and how new it is |

A caution: **misspelled assert names are silently ignored** - an
`Assert.Programm` checks nothing. If an assert never fires when you expect it
to, check the spelling first.

## Things to try

- Guard a texture pipeline: `Assert.Program nasm` or `Assert.Directory assets`.
- Prefer a *conditional* to a hard failure? `if program_exists(tool)` (see
  example 19) lets the build adapt instead of stopping.
