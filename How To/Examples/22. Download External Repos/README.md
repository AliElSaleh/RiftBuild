# 22. Download External Repos

Using a third-party library that is not on your machine yet: the build
clones GLFW from GitHub, builds it from source, builds a local static
library alongside it, links everything into an app, and runs it.

```
MyProject (exe, src/)
├── MyLib          (local static library, libs/)
└── glfw           (cloned into external/ at build time)
```

## Try it

```
riftbuild
```

Note: i have `riftbuild` symlinked to just `b`, so building literally takes two taps. `b` + `Enter`.

The first build takes a while: it clones GLFW into `external/glfw`, compiles
it, and links everything. When the window titled "Hello Worldo" opens, the
whole chain worked - close it to end the run. Build again: no clone, no
recompile, straight to the window.

## How it works

- `if !external/glfw/` - a trailing slash makes the condition "this
  *directory* does not exist". So the clone only happens once.
- `PreDepend.Cmd git clone ...` - PreDepend runs *before dependencies are
  parsed*, which is exactly when a fetched dependency needs to appear.
- `Depend external/glfw` - from here on, the cloned repo is an ordinary
  dependency: its `glfw.build` describes how it compiles, and its exported
  include path is why `src/MyProject.cpp` can `#include <GLFW/glfw3.h>`
  without this project declaring anything.
- `Assembly %_DirectoryName` (in `libs/MyLib/.build`) - built-in variables
  starting with `%_` describe the build context; this one names the library
  after its own folder.

## Things to try

- Delete `external/` and build - watch the clone happen again.
- Add a second external repo the same way: guard-clone it, `Depend` it.
- Pin a version: `git clone --branch <tag>` in the PreDepend command.
- Explore `external/glfw/glfw.build` - a real library's build file, with
  platform blocks and option-driven configuration, using the ideas from
  examples 06-08.
