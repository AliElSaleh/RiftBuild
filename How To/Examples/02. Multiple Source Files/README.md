# 02. Multiple Source Files

A little expression calculator split across several source files and subfolders - a lexer, a recursive descent parser, and a math module.

You actually do not need a `.build` file at all for simple programs like this. This is perfect for when you have an idea about something and you want to quickly start writing code without having to spend any energy thinking about how you are going to build it.

## Try it

```
riftbuild
Build/calc.exe "(1 + 2) * 3.5 - 4 / 2"
Build/calc.exe "2 + 3 * 4" "-(2 + 3) * 4" "10 / (5 - 5)"
```

Note: i have `riftbuild` symlinked to just `b`, so building literally takes two taps. `b` + `Enter`.

## How it works

When there is no `SourceFiles` key, RiftBuild recursively discovers every
source file under the build file's folder: `calc.c`, `src/lexer.c`,
`src/parser.c`, and `src/math/ops.c` are all found and compiled automatically.

With no `Assembly` key either, the output is named after the first source
file discovered - here that is `calc.c`, so the build produces `calc.exe`.

For source files whose name starts with `__` are skipped.
`src/__scratch.c` contains an `#error` directive, so the fact that this project
builds at all proves it was never compiled. Use this for scratch files you
want to keep around without building.

## Things to try

- Rename `src/__scratch.c` to `scratch.c` and build - the build now fails, because
  discovery picks it up. Rename it back.
- Prefer an explicit list? Add `SourceFiles calc src/lexer src/parser src/math/ops` - now only
  those files build, discovery becomes a whitelist.
- Want discovery *minus* a few files? Use `SourceFiles.Exclude broken.c`
  instead of listing everything.

Next: [03. Defines And Includes](../03.%20Defines%20And%20Includes/) - passing configuration to the compiler.
