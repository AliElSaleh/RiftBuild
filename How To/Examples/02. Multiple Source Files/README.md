# 02. Multiple Source Files

A project with several source files across subfolders - and a `.build` file
that is *one line long*.

## Try it

```
riftbuild
Build/Shouter.exe
```

## How it works

When there is no `SourceFiles` key, RiftBuild recursively discovers every
source file under the build file's folder: `main.c`, `greet.c`, and
`text/shout.c` are all found and compiled automatically.

One exception: files whose **name** starts with `__` are skipped.
`__scratch.c` contains an `#error` directive, so the fact that this project
builds at all proves it was never compiled. Use this for scratch files you
want to keep around without building.

## Things to try

- Rename `__scratch.c` to `scratch.c` and build - the build now fails, because
  discovery picks it up. Rename it back.
- Prefer an explicit list? Add `SourceFiles main greet text/shout` - now only
  those files build, discovery becomes a whitelist.
- Want discovery *minus* a few files? Use `SourceFiles.Exclude broken.c`
  instead of listing everything.
- Keep sources in a subfolder (e.g. `src/`)? Point discovery there with
  `SourceDirectory src`.

Next: [03. Defines And Includes](../03.%20Defines%20And%20Includes/) - passing configuration to the compiler.
