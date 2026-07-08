# 23. Video Game

Everything from example 22, grown into a real program: a playable Breakout
game. An executable holds the game logic, a local static library does the
rendering, and GLFW (cloned on the first build) provides the window, the
OpenGL context, and input.

```
Breakout (exe, src/)          main loop, input, game logic
├── Engine (libs/Engine/)     rectangles and seven-segment digits on raw OpenGL
└── glfw   (external/)        cloned into external/ at build time
```

## Try it

```
riftbuild
```

The first build clones GLFW and compiles it; after that, builds are instant.
The game launches when the build finishes.

| Key | Action |
| --- | --- |
| Left / Right (or A / D) | move the paddle |
| Space | launch the ball |
| R | restart |
| Esc | quit |

Score is top-left, lives are top-right. Clear all the bricks to win.

## How it works

- The root `.build` is example 22's shape exactly: guard-clone GLFW with
  `if !external/glfw/` + `PreDepend.Cmd`, then `Depend` the local library and
  the cloned one. A real game needs nothing extra from the build system.
- `libs/Engine` talks to raw OpenGL only - it never includes GLFW - so it is
  a dependency-free module. The game exe is the only place the two meet.
- Nobody in this project mentions `opengl32`: GLFW's own build file exports
  the Win32 libraries it needs via `Libraries.Export`, and the exe inherits
  them by depending on it. On macOS the Cocoa/OpenGL frameworks travel the
  same way through the `Apple.Frameworks` key (a static library auto-exports
  it, and `Apple.Frameworks.Export` exists for being explicit).
- The score display has no font files: `Engine/Digits.c` builds
  seven-segment digits out of the same `Render_Rect` everything else uses.

## Things to try

- Make the bricks take two hits: turn `Bricks[Row][Col]` into a hit counter
  and dim the color after the first hit.
- Add a `release` option to the root `.build` (example 07) and pass `-O2` in
  a `Compiler.Flags` line guarded by `if release`.
- Move the paddle with the mouse: `glfwGetCursorPos` in `main.c`, one new
  field in `GameInput`.
- Grow `Engine` into a real module: circles for the ball, `Render_Line`, a
  second source file for input mapping - the `.build` never changes, new
  files are discovered automatically.
