# 21. Shared Build Variables

Defining product-wide values once and reusing them from every build file.

## Try it

```
riftbuild
Build/Family.exe
```

## How it works

`shared/common.buildvars` holds ordinary key lines - here a version number
and a define set. Any build file can pull them in with:

```
include shared/common.buildvars
```

(`import` is an alias for `include`.) After the include, `$ProductVersion`
and `$CommonDefines` expand like locally-defined variables (example 11).

In a real project several modules would include the same file - bump
`ProductVersion` once and every executable, version resource, and
`<NAME>_VERSION_STRING` macro across the product follows. This repository
does exactly that: `RiftBuild.build` includes `Source/Common.buildvars`.

## Things to try

- Add a `CommonFlags -Wall` line to the `.buildvars` and a
  `Compiler.Flags $CommonFlags` here.
- Create a second sibling module that includes the same file and prints the
  same version.
