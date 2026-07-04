# 19. Detecting Programs

Adapting to the machine: enable features when a tool is present, degrade
gracefully when it is not.

## Try it

```
riftbuild
Build/Prospector.exe
```

If `git` is on your PATH the program says so; if not, it falls back - same
build file either way.

## How it works

`if program_exists(name)` is an ordinary if-condition (examples 06/07), so
anything can hang off it: defines, source file lists, hooks. The negative
case here is a built-in proof: `zz_no_such_tool_zz` never exists, so the
`#error` trap in `main.c` confirms the condition was really false.

**Pick the right tool for requirements:**

- `Assert.Program nasm` (example 16) - *hard* requirement, halt with a clear
  message.
- `if program_exists(nasm)` - *optional* dependency, adapt and keep building.

## Related: choosing the compiler

Tool selection goes further than detection. The `Compiler` key forces a
toolchain regardless of what would be auto-detected:

```
Compiler gcc        # or clang, tcc, cl, ...
```

and `Compiler.MaxCores 4` caps parallel compilation (useful on laptops or in
shared CI). Combine the two ideas and a build file can prefer a compiler
only when it is actually installed:

```
if program_exists(gcc) Compiler gcc
```
