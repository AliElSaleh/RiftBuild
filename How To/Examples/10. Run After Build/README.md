# 10. Run After Build

Build-and-run in a single command - the tight loop you want for tools, demos,
and test programs.

## Try it

```
riftbuild
```

Note: i have `riftbuild` symlinked to just `b`, so building literally takes two taps. `b` + `Enter`.

The program builds *and runs*: you see its output ("Echo ran with 2
argument(s)...") right after the link step.

## The key ideas

- `.Run hello world | .` runs the fresh executable with the arguments
  `hello world`.
- The part after `|` is the **working directory** for the run. It matters for
  programs that load files by relative path:
  - omit it -> the program runs from its `Build/` folder
  - `| .`   -> runs from the build file's folder
  - `| ..`  -> runs from the parent folder, and so on.

## Things to try

- Note that a no-op build still runs the program - `.Run` fires on every
  successful `riftbuild`, which is exactly what you want for a test runner.
- Combine with options (example 07): `if run_tests .Run --run-all-tests`
  gives you an opt-in `riftbuild run_tests` test pass.

Next: [11. Variables And Expansion](../11.%20Variables%20And%20Expansion/) - the build file's string toolkit.
