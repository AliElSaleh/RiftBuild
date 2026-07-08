# 09. Generated Code

Build phase hooks: generating a header before compilation and logging after
linking. (`config.h` does not exist until you build - IDE warnings about
the missing include are expected and harmless.)

## Try it

```
riftbuild
Build/Stamped.exe
```

Note: i have `riftbuild` symlinked to just `b`, so building literally takes two taps. `b` + `Enter`.

After the build, look at the two files the hooks produced next to this README:
`config.h` (written fresh every build).

## The key ideas

- Six hook points, always in this order:
  `PreBuild`, `PreCompile`, `PostCompile`, `PreLink`, `PostLink`, `PostBuild`.
- Each hook takes a verb. `WriteFile` overwrites, `AppendFile` appends; other
  verbs include `NewDir`, `NewFile`, `Copy src dst-folder`, `Rename`,
  `Delete`, and `Cmd` (run an arbitrary command - see the
  *Download External Repos* example, which uses `PreDepend.Cmd git clone ...`).
- **Gotcha:** inside a `WriteFile`/`AppendFile` block, `#` still starts a
  build-file comment. Escape preprocessor lines as `\#define` / `\#pragma`.

## Things to try

- Move the version stamp the *easy* way instead: the dedicated
  `Version(define) 1.2.3` key generates `STAMPED_VERSION_STRING`,
  `STAMPED_MAJOR_VERSION`, ... macros without any hook.

Next: [10. Run After Build](../10.%20Run%20After%20Build/) - build-and-run in one command.
