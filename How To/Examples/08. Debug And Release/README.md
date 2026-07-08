# 08. Debug And Release

The most common real-world build-file pattern: one option that switches
between a debug and an optimized configuration.

## Try it

```
riftbuild            # debug build  -> Build/Debug/Ship.exe
riftbuild release    # release build -> Build/Release/Ship.exe
```

Run both executables and compare the output.

## The key ideas

- `option.release` plus an `if release { } else { }` block is all it takes.
- Each configuration gets its **own** `BuildDirectory` and
  `IntermediateDirectory`. That means `riftbuild` and `riftbuild release` can
  alternate freely - each configuration stays incremental because their object
  files never overwrite each other.
- `Compiler.Flags` passes flags straight through to the compiler (`-O2`,
  `-g`, warnings, sanitizers, anything).

## Things to try

- Add `-fsanitize=address` to the debug configuration's `Compiler.Flags`.
- Add a third configuration (e.g. `option.profile`) with `-O2 -g`.
- Make release builds quieter or stricter: `Compiler.Flags:release -Werror`.
- Wrap the combination up as presets: `preset:ship release` reads nicer for
  users than remembering individual options.

Next: [09. Generated Code](../09.%20Generated%20Code/) - hooking into build phases.
