# 06. Platform Conditionals

One build file that adapts to the machine it runs on.

## Try it

```
riftbuild
Build/Compass.exe
```

## Three ways to write a condition

1. **Suffix on a key** - best for a single conditional value:
   ```
   Compiler.Defines:windows  ON_WINDOWS=1
   Compiler.Defines:!windows ON_WINDOWS=0
   ```
2. **if/else block** - best when several keys change together:
   ```
   if windows
   {
       Compiler.Defines PLATFORM_NAME=\"Windows\"
   }
   ```
3. **Single-line if** - best for one-offs. It must stay on one line:
   ```
   if x64 Compiler.Defines POINTER_BITS=64
   ```

Built-in conditions are the platform names (`windows`, `linux`, `macos`, ...)
and architectures (`x64`, `x86`, ...). `!` negates any of them. User-defined
options (next example) become conditions too, using exactly the same syntax.

Try running `riftbuild --internals` to see the full list of internal variables
that you can use as conditions!

Note the `\"` escapes: `PLATFORM_NAME=\"Windows\"` defines a C *string* macro.
Without the escaped quotes the macro would be a bare identifier.

## Things to try

- Conditions work on *any* key, not just Compiler.Defines: try
  `SourceFiles:windows main win_impl` style lists, or platform-specific
  `Libraries:windows winmm`.
- Sanity-check your environment instead of failing mysteriously:
  `Assert.Platform windows` or `Assert.Arch x64` stop the build with a clear
  message on the wrong machine.

Next: [07. Build Options And Presets](../07.%20Build%20Options%20And%20Presets/) - conditions the *user* controls.
