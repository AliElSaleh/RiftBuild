# 14. Generate A License

Never paste license boilerplate again: the build writes the LICENSE file.

## Try it

```
riftbuild
```

A complete `LICENSE` file appears next to the build file: the MIT license
text with your `Copyright` line filled in.

## How it works

- `License(generate) MIT` is the `(parameter)` pattern from example 13: the
  plain `License` key would only record the license name as project metadata;
  the `(generate)` parameter makes the build *write the file*.
- The copyright line inside the license comes from the `Copyright` key - set
  it, or the license ships with a placeholder.

## Things to try

- Delete the generated `LICENSE` and rebuild - it comes back.
- Change the `Copyright` year or holder and rebuild - the LICENSE follows.
- Pair it with [15. Enforce Copyright](../15.%20Enforce%20Copyright/), which
  turns the same `Copyright` notice into a per-source-file requirement.
