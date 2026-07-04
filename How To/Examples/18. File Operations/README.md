# 18. File Operations

The build system as a file butler: staging a distributable folder without a
single shell script.

## Try it

```
riftbuild
dist/Packer.exe
```

After the build, `dist/` contains the executable and a `README.txt` - both
placed there by `PostBuild` verbs. The `scratch.tmp`/`notes.tmp` pair from
the Rename/Delete demo is already gone: created, renamed, and deleted within
the same build.

## The verb set

Every phase hook (`PreBuild`, `PostBuild`, ... - example 09) accepts these:

| Verb | Does |
|------|------|
| `NewDir dir` | Create a folder |
| `NewFile file` | Create an empty file |
| `WriteFile file { ... }` | Write the block's contents (overwrites) |
| `AppendFile file { ... }` | Same, but appends |
| `Copy src dst-dir` | Copy `src` *into* the folder `dst-dir` |
| `Rename old new` | Rename / move a file |
| `Delete file` | Delete a file |
| `Cmd command args...` | Run an arbitrary command |

The big gotcha: **`Copy`'s second argument is a directory**, not a target
filename. `Copy Build/Packer.exe dist` produces `dist/Packer.exe`; to change
the name too, `Copy` then `Rename`.

## Things to try

- Add a `PreBuild.Cmd git rev-parse --short HEAD` style step - `Cmd` is the
  escape hatch when no verb fits.
- Stage assets: `PostBuild.Copy assets/config.ini dist`.
