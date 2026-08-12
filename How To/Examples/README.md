# Examples

Learn RiftBuild by doing. Each folder is a tiny, self-contained project:
`cd` in, run `riftbuild`, then open its `README.md` and `.build` file side by
side. The examples build on each other, so going in order works best - but
each one also stands alone.

| # | Example | What you learn |
|---|---------|----------------|
| 01 | [Hello World](01.%20Hello%20World/) | The one-line build file; build, clean, rebuild |
| 02 | [Multiple Source Files](02.%20Multiple%20Source%20Files/) | Auto source discovery, subfolders, `__` skip files |
| 03 | [Defines And Includes](03.%20Defines%20And%20Includes/) | Include paths and preprocessor macros |
| 04 | [Static Library](04.%20Static%20Library/) | Multi-module projects: `Depend`, `Type static_lib`, `.Export` usage requirements |
| 05 | [Shared Library](05.%20Shared%20Library/) | Building a DLL / shared object |
| 06 | [Platform Conditionals](06.%20Platform%20Conditionals/) | `Key:cond` suffixes, `if/else` blocks, single-line `if` |
| 07 | [Build Options And Presets](07.%20Build%20Options%20And%20Presets/) | User-facing options, `%value` paste, `preset:` bundles |
| 08 | [Debug And Release](08.%20Debug%20And%20Release/) | The classic two-configuration setup |
| 09 | [Generated Code](09.%20Generated%20Code/) | Phase hooks: `PreBuild.WriteFile`, `PostBuild.AppendFile` |
| 10 | [Run After Build](10.%20Run%20After%20Build/) | `.Run` - build-and-run in one command |
| 11 | [Variables And Expansion](11.%20Variables%20And%20Expansion/) | `$Var`, `@ENV`, `!command` expansion |
| 12 | [Executable Icon](12.%20Executable%20Icon/) | Embedding an `.ico` with the `Icon` key |
| 13 | [Version Info](13.%20Version%20Info/) | `Version(define)`, the `Key(parameter)` concept, version resources |
| 14 | [Generate A License](14.%20Generate%20A%20License/) | `License(generate) MIT` writes the LICENSE file |
| 15 | [Enforce Copyright](15.%20Enforce%20Copyright/) | `Copyright(enforce)` as a build-time lint rule |
| 16 | [Build Asserts](16.%20Build%20Asserts/) | `Assert.Platform/Program/EnvVarExists/...` fail-fast checks |
| 17 | [Exports And Private Dependencies](17.%20Exports%20And%20Private%20Dependencies/) | `.Export` usage requirements in depth, `Depend(private)` |
| 18 | [File Operations](18.%20File%20Operations/) | `NewDir`/`Copy`/`Rename`/`Delete` verbs; staging a dist folder |
| 19 | [Detecting Programs](19.%20Detecting%20Programs/) | `if program_exists(...)`, choosing a compiler |
| 20 | [Syntax Toolbox](20.%20Syntax%20Toolbox/) | `##` comments, value blocks, namespace blocks, the backtick reset |
| 21 | [Shared Build Variables](21.%20Shared%20Build%20Variables/) | `include x.buildvars` - one version number for many modules |
| 22 | [Download External Repos](22.%20Download%20External%20Repos/) | `PreDepend.Cmd git clone`, building a real third-party library (GLFW) |
| 23 | [Video Game](23.%20Video%20Game/) | Everything combined into a playable Breakout game: exe + static library + GLFW |
| 24 | [Source Preprocessors](24.%20Source%20Preprocessors/) | `PreCompileFile`/`PreCompileAllFiles`: build a metaprogram, then run it over sources before compiling |
| 25 | [macOS App Bundle](25.%20macOS%20App%20Bundle/) | `Bundle`: a `.app` with an icon, generated plists, and staged `Contents/Resources` |

Two housekeeping notes:

- The example sources only compile with the defines their `.build` file
  provides, so IDE/clangd squiggles inside these folders are expected noise.
- Everything a build produces (`Build/`, `Intermediate/`, generated files) can
  be removed with `riftbuild clean` in that example's folder.

Want more? `Tests/Feature Set/` in this repository contains 50 more
mini-projects - one per feature, each commented and self-verifying.
