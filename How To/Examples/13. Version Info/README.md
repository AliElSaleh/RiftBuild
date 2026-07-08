# 13. Version Info

One version number, three destinations: your C code, the Windows version
resource, and (see later examples) generated files.

## Try it

```
riftbuild
Build/Stamp.exe
```

Then right-click `Build/Stamp.exe` -> Properties -> Details: the description,
company, copyright, and version are all filled in.

## The (parameter) concept

This example introduces a syntax you will meet on several keys: a
**parameter in parentheses** that extends what the key does.

| Key | What the parameter adds |
|-----|------------------------|
| `Version(define) 2.4.1` | Also inject the version as macros into every source file |
| `License(generate) MIT` | Also *write* the LICENSE file (example 14) |
| `Copyright(enforce) ...` | Also *check* every source carries the notice (example 15) |
| `Depend(private) path` | Depend, but don't re-export its usage requirements (example 17) |

Here, `Version(define)` gives every translation unit four macros named after
the assembly (uppercased): `STAMP_VERSION_STRING`, `STAMP_MAJOR_VERSION`,
`STAMP_MINOR_VERSION`, `STAMP_PATCH_VERSION`. Because major/minor/patch are
plain integers, they work in `#if` - see the 2.x feature gate in `main.c`.

## Things to try

- Bump the version to 3.0.0 and rebuild - the macros, the exe's Properties
  dialog, and the `#if` gate all follow from the single line.
- Drop `(define)` and rebuild - the version resource remains, but the macros
  disappear and compilation fails. That is the parameter doing its job.

Next: [14. Generate A License](../14.%20Generate%20A%20License/).
