# 25. macOS App Bundle

Shipping a real `.app`: a Cocoa window, an icon, the plists Finder reads, and
the data files the program cannot run without - staged into
`Contents/Resources` by the build.

Sundial is a small app with an appetite. It draws a sundial face and the
hour's motto, and every part of what you see - the colours, the numerals
around the rim, the line of text underneath - comes out of files it loads at
run time. That is the point of the example: a bundle is not just a renamed
executable, it is an executable plus everything it needs, in the layout macOS
expects.

## Try it

```
riftbuild
open Build/Sundial.app
```

A window opens with the shadow pointing at the current time. The same binary
also runs bare, no bundle involved:

```
Build/sundial
```

`Get Info` on `Build/Sundial.app` (or just look at it in Finder) shows the
icon, the version and the copyright - all of it from the build file.

## What the build produces

```
Build/sundial            the plain executable, exactly as any other example
Build/Sundial.app/
└── Contents/
    ├── Info.plist       generated from the metadata keys
    ├── version.plist    generated
    ├── PkgInfo          generated
    ├── MacOS/
    │   └── sundial      the executable, copied in
    └── Resources/
        ├── sundial.icns built from Sundial.png
        ├── theme.conf   ┐
        ├── numerals.txt ├ copied in by the PostBuild hook
        └── mottos.txt   ┘
```

## How it works

**`Apple.Bundle`** is the whole feature: one flag key, and the executable
RiftBuild just linked gets wrapped in `Build/Sundial.app`. The folder is named
after `TitleName` (falling back to `Assembly`), and it is deleted and rebuilt
from scratch on every build.

**A Cocoa app needs exactly one extra line in the build file.**

```make
Apple.Frameworks Cocoa
```

`.m` files sit in `src/` next to the `.c` files and clang compiles both
without being told which is which - `main.m` and `DialView.m` are the window
and the drawing, `Dial.c` and `Bundle.c` are plain C that never mentions
Cocoa. There is no nib and no storyboard: the menu bar and the window are
built in code, because a nib would be one more resource to explain.

**`Apple.Bundle.IsTerminal`** is the key this example does *not* use. It is
for programs that print: it renames the executable to `sundial-bin` and puts a
small shell script in its place, so double-clicking the app in Finder gets you
a Terminal window instead of a process with nowhere to write. A GUI app leaves
the line out, which is why double-clicking this one opens a window.

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
your own with `Apple.Bundle.InfoPlist`, `Apple.Bundle.VersionPlist` and
`Apple.Bundle.PkgInfo`, or paste the dictionary body straight into the build
file with an `Info.plist { ... }` block - that replaces the generated contents
entirely, so you write every key yourself.

**The icon is one line.** `Icon Sundial` finds `Sundial.png` (the extension is
picked per OS - `.ico` on Windows), then `sips` renders it at 16, 32, 64, 128,
256 and 512 pixels into an `.iconset`, `iconutil` compiles that into
`sundial.icns`, and the generated `Info.plist` already points
`CFBundleIconFile` at the result.

**The resources are a `PostBuild` hook.** Bundling runs before `PostBuild`, so
by the time the hook fires `Contents/Resources` exists (the `.icns` is already
in there) and a wildcard copy is all it takes:

```make
PostBuild
{
    Copy assets/* Build/Sundial.app/Contents/Resources
}
```

**The program finds them from its own path, never the working directory.**
Launched from Finder the working directory is not the app - `src/Bundle.c`
asks the OS where the executable is (`_NSGetExecutablePath`), and if that path
ends in `Contents/MacOS` it reads `../Resources`. Otherwise it falls back to
the project's `assets/` folder, which is why `Build/sundial` works too. Every
platform-specific line in the program lives in that one function.

Cocoa has its own answer - `[[NSBundle mainBundle] resourcePath]` - and in a
bundled app it gives you the same directory. The plain C version is here
because it is the one that still works when there is no bundle, and because
it is the same code on Windows and Linux.

## Gotchas

- **Keep `TitleName` free of spaces.** It names the `.app` folder, and the
  file-operation verbs split their arguments on whitespace, so
  `Copy assets/* Build/My App.app/...` will not do what you want. For a
  display name with spaces set `Info.plist.CFBundleDisplayName` instead.
- **The bundle is recreated on every build.** Editing `theme.conf` inside
  `Build/Sundial.app` is a fine way to play with the app, but the next
  `riftbuild` copies `assets/` over it again.
- **`Apple.Bundle` only does anything on macOS**, which is why this example
  opens with `Assert.Platform macos` - a clear failure beats quietly producing
  a bare executable.
- **`Build/sundial` is not a bundle**, so macOS has no `Info.plist` to read
  and would give it no Dock icon and no menu bar. `main.m` calls
  `setActivationPolicy:NSApplicationActivationPolicyRegular` itself, which is
  what makes the bare binary behave like an app anyway.
- **Warnings from the resource parser go to stderr**, which Finder-launched
  apps send to Console.app rather than a terminal. Misspell a key in
  `theme.conf` and that is where the complaint is.

## Things to try

- Edit `Build/Sundial.app/Contents/Resources/theme.conf` - `gnomon = 6FA8DC`,
  `radius = 0.45` - and press **Cmd-R** in the app. No rebuild, no relaunch:
  the app re-reads the files and redraws.
- Delete a file out of `Contents/Resources` and launch the app. It says which
  folder it looked in and quits - a bundle missing its resources is a broken
  program, not a smaller one.
- Put Arabic numerals in `numerals.txt` (`12`, `1`, `2` ...). The face is
  drawn from whatever the first twelve lines say, noon first and then
  clockwise.
- Add `Info.plist.CFBundleDisplayName Artisan Sundial` and check `Get Info`.
- Ad-hoc sign it: `codesign --force --deep --sign - Build/Sundial.app`, then
  `codesign --verify --verbose Build/Sundial.app`.
- Point `Apple.Bundle.InfoPlist` at a hand-written plist and compare it with
  the generated one in `Intermediate/`.
- Add `Apple.Bundle.IsTerminal` and rebuild, then look inside
  `Contents/MacOS`: the executable has moved aside and a shell script is
  wearing its name. That is the shape a printing program ships in.

This is the last example. `Tests/Feature Set/` in the repository has 61 more
mini-projects - one per feature, each commented and self-verifying.
