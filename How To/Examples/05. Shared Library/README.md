# 05. Shared Library

Building a dynamic library: a DLL on Windows, a shared object elsewhere.

## Try it

```
riftbuild
```

`Build/` now contains `Greeter.dll` - and, because the source exports symbols,
an import library (`Greeter.lib`) that executables link against.

## The key ideas

- The only difference from an executable is `Type shared_lib`. (Static
  libraries, from the previous example, use `Type static_lib`.)
- Exporting symbols is a *source-level* concern on Windows: note the
  `GREETER_API` / `__declspec(dllexport)` macro in `greeter.h`. Without at
  least one exported symbol, a DLL has no import library.

## Things to try

- Consume it from another module the same way as a static library: a
  `Depend path/to/Greeter` line in the consumer's `.build`. Add
  `Compiler.Includes.Export .` here so consumers find `greeter.h`
  automatically (see example 04).
- Inspect the exports: `dumpbin /exports Build/Greeter.dll` (from a VS
  prompt) or `llvm-objdump --private-headers`.

Next: [06. Platform Conditionals](../06.%20Platform%20Conditionals/) - one build file, many platforms.
