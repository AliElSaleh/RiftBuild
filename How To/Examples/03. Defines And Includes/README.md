# 03. Defines And Includes

Passing configuration into your code: include directories and preprocessor
macros.

## Try it

```
riftbuild
Build/Pantry.exe
```

Note: i have `riftbuild` symlinked to just `b`, so building literally takes two taps. `b` + `Enter`.

## How it works

- `Compiler.Includes include` puts the `include/` folder on the compiler's
  include path, so `main.c` can say `#include "config.h"` directly. List as
  many folders as you like on one line.
- `Compiler.Defines ENABLE_GREETING MAX_ITEMS=4` defines two macros for every
  source file. `ENABLE_GREETING` is a flag (just defined), `MAX_ITEMS=4`
  carries a value.

## Things to try

- Remove `ENABLE_GREETING` from the `.build` file and rebuild - the program
  now prints the disabled message. Note that changing defines *triggers a
  recompile* automatically; you never need `rebuild` for this.
- Change `MAX_ITEMS=4` to `MAX_ITEMS=10` and rebuild.
- Scope a define to a single file: `main.c.Compiler.Defines ONLY_FOR_MAIN=1`
  applies only when compiling `main.c` (note: per-file prefixes use the full
  filename, extension included).
- The opposite also exists: `Compiler.UnDefines` strips a macro (even a
  compiler-predefined one) with `-U`.

Next: [04. Static Library](../04.%20Static%20Library/) - splitting a project into modules.
