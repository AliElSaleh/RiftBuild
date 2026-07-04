# 09. Generated Code

Build phase hooks: generating a header before compilation and logging after
linking. (`generated.h` does not exist until you build - IDE warnings about
the missing include are expected and harmless.)

## Try it

```
riftbuild
Build/Stamped.exe
```

After the build, look at the two files the hooks produced next to this README:
`generated.h` (written fresh every build) and `build_log.txt` (one line
appended per build).

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

- Paste build-time information into the generated header - variables expand
  inside the block, so `\#define BUILT_ON_HOST "$HostName"` works together
  with example 11's `HostName !hostname`.
- Move the version stamp the *easy* way instead: the dedicated
  `Version(define) 1.2.3` key generates `STAMPED_VERSION_STRING`,
  `STAMPED_MAJOR_VERSION`, ... macros without any hook.
- Use `PostBuild.Copy` to stage the finished executable into a `dist/` folder.

Next: [10. Run After Build](../10.%20Run%20After%20Build/) - build-and-run in one command.
