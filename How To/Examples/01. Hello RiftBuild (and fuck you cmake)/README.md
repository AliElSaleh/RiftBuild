# 01. Hello RiftBuild

The smallest possible project: one source file and a one line `.build` file.

## Try it

```
riftbuild
```

That is the whole workflow. The executable lands in `Build/Hello.exe` and the
object files in `Intermediate/`.

## What just happened

The `.build` file says exactly one thing: `Assembly Hello`, the name of the
thing being built.

Everything else is a default: RiftBuild finds a compiler on your machine,
discovers `main.c` on its own, builds an executable (`Type executable` is the
default), and puts outputs in `Build/` and `Intermediate/`. If you prefer to be
explicit, then add `SourceFiles main` to list the sources by hand (extension optional).

## Things to try

- Run `riftbuild` a second time - nothing recompiles, the build is incremental.
- Touch `main.c` (or edit the message) and run again - only what changed rebuilds.
- `riftbuild clean` removes everything the build produced.
- `riftbuild rebuild` forces a full rebuild from scratch.

Next: [02. Multiple Source Files](../02.%20Multiple%20Source%20Files/) - projects with more than one file.
