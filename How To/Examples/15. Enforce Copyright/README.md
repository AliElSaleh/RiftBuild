# 15. Enforce Copyright

Turning a convention into a build rule: every source file must carry the
copyright notice, or the build fails.

## Try it

```
riftbuild
Build/Guarded.exe
```

It builds, because both `main.c` and `helper.c` start with the exact notice
named in the build file.

## Now break it

Delete the `// Copyright ...` line from `helper.c` and run `riftbuild` again.
The build stops and tells you which file is missing the notice. Put the line
back (or let your editor's file template add it) and you are green again.

## How it works

`Copyright(enforce) <text>` is the `(parameter)` pattern again (example 13):
the plain `Copyright` key just records the notice as metadata (the version
resource and `License(generate)` both use it); adding `(enforce)` makes the
build *check* that every compiled source contains the text.

This is the build-system version of a lint rule: instead of a wiki page
saying "please add the header", new files without it simply do not build -
in anyone's checkout, with zero CI configuration.
