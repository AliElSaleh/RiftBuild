# 11. Variables And Expansion

The build file's string toolkit: user variables, environment variables, and
parse-time command output.

## Try it

```
riftbuild
Build/Nametag.exe
```

The program prints values that all originated *outside* `main.c`: a variable
from the build file, an environment variable, and your machine's hostname.

## The three expansions

| Syntax | Meaning |
|--------|---------|
| `$Name` | Paste the user variable `Name` (any non-built-in key defines one) |
| `$-name` / `$^NAME` | Same, forced to lower / UPPER case |
| `@NAME` | Paste the environment variable `NAME` |
| `!command` | Run `command` at parse time, paste its stdout |

Notes:

- Variables expand anywhere a value is read - including inside `\"...\"`
  string defines and inside `WriteFile` blocks (example 09).
- `!command` takes a single plain word: `!hostname` works, `!git status`
  does not. For real commands with arguments, use a hook verb such as
  `PreBuild.Cmd git describe` instead.
- Sharing variables between build files: put them in a `.buildvars` file and
  pull them in with `include shared.buildvars` (or the `import` alias).

## Things to try

- Add `Compiler.Defines BUILT_BY=\"@USERNAME\"` and print it.
- Define `Version 2.1.0` and reuse it in two places (`Compiler.Defines` and a
  `WriteFile` block) - change it once, both update.

That wraps the fundamentals. Examples 12-21 each showcase one production
feature: icons, version stamping, license generation, copyright enforcement,
asserts, exports, file operations, and more - dip in wherever your project
needs it.
