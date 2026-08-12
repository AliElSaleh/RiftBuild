# 25. macOS App Bundle

Shipping a real `.app`: an icon, the plists Finder reads, and the data files
the program cannot run without - staged into `Contents/Resources` by the
build.

Sundial is a small program with an appetite: it prints an ASCII sundial whose
banner, theme and hourly mottos all come out of files it loads at startup.
That is the point of the example - a bundle is not just a renamed executable,
it is an executable plus everything it needs, in the layout macOS expects.

## Try it

```
riftbuild
open Build/Sundial.app
```

Finder opens Terminal and the app draws the current time. The same binary
also runs bare:

```
Build/Sundial
```

`Get Info` on `Build/Sundial.app` (or just look at it in Finder) shows the
icon, the version and the copyright - all of it from the build file.

## What the build produces

```
Build/Sundial            the plain executable, exactly as any other example
Build/Sundial.app/
└── Contents/
    ├── Info.plist       generated from the metadata keys
    ├── version.plist    generated
    ├── PkgInfo          generated
    ├── MacOS/
    │   ├── Sundial      shell script (because of Bundle.IsTerminal)
    │   └── Sundial-bin  the real executable
    └── Resources/
        ├── Sundial.icns built from Sundial.png
        ├── dial.txt     ┐
        ├── mottos.txt   ├ copied in by the PostBuild hook
        └── theme.conf   ┘
```

## How it works

**`Bundle`** is the whole feature: one flag key, and the executable RiftBuild
just linked gets wrapped in `Build/Sundial.app`. The folder is named after
`TitleName` (falling back to `Assembly`), and it is deleted and rebuilt from
scratch on every build.

**`Bundle.IsTerminal`** is for programs that print. It renames the executable
to `Sundial-bin` and puts a two-line shell script in its place, so
double-clicking the app in Finder gets you a Terminal window instead of a
process that starts and exits invisibly. A GUI app leaves this line out.

**The plists are generated from the metadata keys** you would set anyway.
`Version`, `TitleName`, `Description`, `CompanyName` and `Copyright` become
`CFBundleShortVersionString`, `CFBundleName`, `CFBundleGetInfoString`,
`CFBundleIdentifier` (`com.<CompanyName>.<Assembly>`) and friends. Anything
they do not cover is a line of its own:

```make
Info.plist.LSMinimumSystemVersion   11.0
Info.plist.NSHumanReadableCopyright $Copyright
```

Values become `<string>`, or `<integer>` when they are whole numbers, or an
`<array>` when written `(one two three)`. The same works for
`Version.plist.<key>`. If you would rather own the files outright, point at
your own with `Bundle.InfoPlist`, `Bundle.VersionPlist` and `Bundle.PkgInfo`,
or paste the dictionary body straight into the build file with an
`Info.plist { ... }` block - that replaces the generated contents entirely,
so you write every key yourself.

**The icon is one line.** `Icon Sundial` finds `Sundial.png` (the extension
is picked per OS - `.ico` on Windows), then `sips` renders it at 16, 32, 64,
128, 256 and 512 pixels into an `.iconset`, `iconutil` compiles that into
`Sundial.icns`, and the generated `Info.plist` already points
`CFBundleIconFile` at the result.

**The resources are a `PostBuild` hook.** Bundling runs before `PostBuild`,
so by the time the hook fires `Contents/Resources` exists (the `.icns` is
already in there) and a wildcard copy is all it takes:

```make
PostBuild
{
    Copy assets/* Build/Sundial.app/Contents/Resources
}
```

**The program finds them from its own path, never the working directory.**
Launched from Finder the working directory is not the app - `src/Bundle.c`
asks the OS where the executable is (`_NSGetExecutablePath`), and if that
path ends in `Contents/MacOS` it reads `../Resources`. Otherwise it falls
back to the project's `assets/` folder, which is why `Build/Sundial` works
too. Every platform-specific line in the program lives in that one function.

## Gotchas

- **Keep `TitleName` free of spaces.** It names the `.app` folder, and the
  file-operation verbs split their arguments on whitespace, so
  `Copy assets/* Build/My App.app/...` will not do what you want. For a
  display name with spaces set `Info.plist.CFBundleDisplayName` instead.
- **The bundle is recreated on every build.** Editing `theme.conf` inside
  `Build/Sundial.app` is a fine way to play with the app, but the next
  `riftbuild` copies `assets/` over it again.
- **`Bundle` only does anything on macOS**, which is why this example opens
  with `Assert.Platform macos` - a clear failure beats quietly producing a
  bare executable.

## Things to try

- Edit `Build/Sundial.app/Contents/Resources/theme.conf` - `radius = 14`,
  `shadow = *` - and run the app again. No rebuild: the program reads the
  file every time it starts.
- Delete a file out of `Contents/Resources` and run the app. It says which
  folder it looked in and stops - a bundle missing its resources is a broken
  program, not a smaller one.
- Add `Info.plist.CFBundleDisplayName Artisan Sundial` and check `Get Info`.
- Ad-hoc sign it: `codesign --force --deep --sign - Build/Sundial.app`, then
  `codesign --verify --verbose Build/Sundial.app`.
- Point `Bundle.InfoPlist` at a hand-written plist and compare it with the
  generated one in `Intermediate/`.
- Drop `Bundle.IsTerminal` and rebuild: double-clicking the app now starts a
  process with nowhere to print. That is the moment a GUI app would open a
  window instead.

This is the last example. `Tests/Feature Set/` in the repository has 61 more
mini-projects - one per feature, each commented and self-verifying.
