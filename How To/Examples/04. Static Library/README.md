# 04. Static Library

Splitting a project into modules: an executable (`App/`) that depends on a
static library (`MathLib/`), each with its own `.build` file.

## Try it

```
riftbuild app/
Build/Calculator.exe
```

Note: i have `riftbuild` symlinked to just `b`, so building literally takes two taps. `b` + `Enter`.

One command builds *both* modules: RiftBuild follows the `Depend` line, builds
`MathLib` first (into `MathLib/Build/MathLib.lib`), then compiles and links
the app against it.

## The key ideas

- `Type static_lib` in `MathLib/.build` archives objects instead of linking
  an executable.
- `Depend ../MathLib` in `App/.build` points at the folder containing the
  library's `.build` file.
- `Compiler.Includes.Export .` in the library is a **usage requirement**: it
  applies to whoever depends on the library, not to the library itself. That
  is why the app needs no `Compiler.Includes` line at all.
  `Compiler.Defines.Export` and `Libraries.Export` work the same way (e.g. a
  library that needs a system library can export it so every consumer links
  it automatically).

## Things to try

- Build again from `App/` - both modules are incremental; nothing recompiles.
- Edit `mul.c` and rebuild from `App/` - only the library recompiles and the
  app relinks.
- Add a second dependency: each `Depend` goes on its own line.
- `Depend(private) ../MathLib` links the library but does *not* re-export its
  usage requirements to modules further up the chain - useful for
  implementation-detail dependencies.

Next: [05. Shared Library](../05.%20Shared%20Library/) - building a DLL / shared object.
