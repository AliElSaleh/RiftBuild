# 20. Syntax Toolbox

Four bits of build-file syntax that keep bigger files readable. The source
is full of `#error` traps, so a successful build *is* the test that each one
behaves as described.

## The four

1. **Block comments** - `##` ... `##` swallows everything in between, even
   lines that look like keys. (A single `#` comments to end of line.)

2. **Value blocks** - a long value can be split across lines:
   ```
   SourceFiles
   {
       main
       helper
   }
   ```

3. **Namespace blocks** - group related keys instead of repeating a prefix:
   ```
   Linker
   {
       Subsystem Console
       Stack     4194304
   }
   ```
   is exactly `Linker.Subsystem Console` + `Linker.Stack 4194304`. The same
   works for `Compiler { ... }` and friends.

4. **The backtick reset** - values normally *accumulate* across repeated
   keys (that is what makes conditional `Compiler.Defines:windows` lines
   compose). When you want to discard what accumulated and start over,
   put a backtick after the key: ``Compiler.Defines` FINAL_ONLY=1``.

## Try it

```
riftbuild
Build/Toolbox.exe
```

Then try to make it fail: move the fake define out of the block comment, or
remove the backtick - the corresponding trap in `main.c` fires.
