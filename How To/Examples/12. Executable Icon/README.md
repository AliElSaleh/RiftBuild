# 12. Executable Icon

Giving your program a face: embedding an `.ico` file into the executable.

## Try it

```
riftbuild
```

Open `Build/` in Explorer - `Iconic.exe` shows the icon instead of the generic
executable glyph.

## How it works

`Icon app` names an icon file relative to the build file's folder (the `.ico`
extension is optional). Behind the scenes RiftBuild writes a small resource
script, compiles it with the resource compiler, and links the result into the
executable - you never touch an `.rc` file yourself.

One gotcha: the value must be a *relative* path. Absolute Windows paths do
not work because backslashes are escape characters in build files.

## Things to try

- Swap in your own `.ico` (a multi-resolution icon looks best - Explorer
  picks different sizes for list view, tiles, and the taskbar).
- Combine with the version metadata from
  [13. Version Info](../13.%20Version%20Info/) - icon plus version resource is
  what makes an exe look "shipped" in the file properties dialog.
