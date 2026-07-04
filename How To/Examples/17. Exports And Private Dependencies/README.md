# 17. Exports And Private Dependencies

Designing a library's *public surface*: what consumers get automatically, and
how to keep implementation details from leaking.

```
App (SaveTool)  ->  Engine  ->(private)  Compression
```

## Try it

```
cd App
riftbuild
Build/SaveTool.exe
```

Note: i have `riftbuild` symlinked to just `b`, so building literally takes two taps. `b` + `Enter`.

One command builds all three modules in dependency order.

## The two ideas

**1. `.Export` keys are usage requirements.** `Compiler.Includes.Export .`
and `Compiler.Defines.Export ENGINE_API_VERSION=3` in the Engine describe
what *consumers* need - the App gets the header path and the define without
declaring anything. (Static libraries can also forward system libraries with
`Libraries.Export`, e.g. `Libraries.Export:windows winmm`.) Export keys do
not apply to the exporting module's own compilation - use the plain keys for
that.

**2. `Depend(private)` stops the chain.** The Engine consumes Compression's
exports and links its code, but does not pass them on. The proof is in the
sources: `engine.c` has `#error` traps requiring `HAS_COMPRESSION` to be
*visible*, and `App/main.c` has traps requiring it to be *invisible*. The
whole example only builds because both are true.

Meanwhile the App still links successfully - the private library's *objects*
travel up to the final link; only its compile-time surface is contained.

## Things to try

- Change the Engine's line to a plain `Depend ../Compression` and rebuild the
  App: the trap in `main.c` fires, because `HAS_COMPRESSION` now leaks through.
- Add `#include "compression.h"` to `main.c`: it fails to compile - the App
  never received Compression's include path.
- Diamond shapes are fine: if two libraries both depend on Compression, it is
  built and linked once.
