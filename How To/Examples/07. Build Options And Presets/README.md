# 07. Build Options And Presets

Letting the *user* of your build file flip switches from the command line.

## Try it

```
riftbuild                 # level 0, turbo off
riftbuild turbo           # turbo on
riftbuild level=7         # level 7
riftbuild turbo level=7   # both
riftbuild preset:max      # same as: riftbuild turbo level=9
riftbuild options         # lists the options with their help text
```

Run `Build/Rocket.exe` after each build and watch the output change. Changing
options changes the define set, which automatically triggers a recompile - no
`rebuild` needed.

## The key ideas

- `option.turbo <help text>` declares an option. A **binary** option is used
  as a condition (`Compiler.Defines:turbo`), just like `windows` or `x64`.
- A **value** option is pasted with `%level` wherever a value is needed. It
  expands to `0` when the user did not pass it.
- `preset:max turbo level=9` names a bundle of options. Users invoke it with
  `riftbuild preset:max` and can still stack extra options on top
  (`riftbuild preset:max some_other_option`).

## Things to try

- Add a `quiet` option that defines away the second printf.
- Use an option to swap source files: `SourceFiles:turbo` vs
  `SourceFiles:!turbo` lists.
- Forward an option to a dependency: `Depend ../Lib | turbo=%turbo`.

Next: [08. Debug And Release](../08.%20Debug%20And%20Release/) - the most common use of options.
