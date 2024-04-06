# Rift Build
A simpler build tool for C/C++, because fuck CMake


### Usage
A simple build call will look like this (in a directory with or without a .build file).
```bash
riftbuild
```

To build with a specific build file.
```bash
riftbuild someapp.build
```
That's it

---

# How to write a .build file
Writing a .build file is simple and straight-forward. It almost has no syntax.

```make
Compiler clang # or gcc/cl or you can specify an absolute or relative path to your C compiler

Assembly SomeName
Extension exe # or dll/lib/a/so/dylib or replace this line with Type app or lib

# below are optional but you can edit them for your project's needs
# these directories are relative to where you run "riftbuild" from
SourceDirectory src       # default value is nothing
BuildDirectory bin        # default value is Build
IntermediateDirectory int # default value is Intermediate

# fill in the blanks below for your program
CompilerFlags -std=c11 -O3 etc...
LinkerFlags 
Includes path/to/include-dir thirdparty/dir anotherdir

Libraries somelib opengl32 etc...
LibraryDirectories path/to/lib/dir another/one

Defines MAX_STUFF=5 SOME_DEFINE
```

---

# I'm not sold on this
It is wise to be skeptical of new tools. Below are forks of a few open source projects that I've translated from CMake (and other build systems) to Rift Build. They can be built with just a single `riftbuild` call on the terminal at the project root directory.

- [Craft TODO](https://github.com/fogleman/Craft)
- [RAD Debugger TODO](https://github.com/EpicGamesExt/raddebugger)
- [Jolt Physics TODO](https://github.com/AliElSaleh/JoltPhysics)
- [Refterm TODO](https://google.com)
- [Kinema TODO](https://google.com)
- [PhysX TODO](https://google.com)
- [SDL2 TODO](https://google.com)
- [StarRuler2 TODO](https://google.com)
- [fmt TODO](https://google.com)
- [Hazel TODO](https://google.com)
- [PCSX2 TODO](https://google.com)
- [RPCS3 TODO](https://google.com)
- [Raylib TODO](https://google.com)
- [Playdate SDK TODO](https://google.com)

---

# Advanced Stuff
The above .build file example is the simplest way to write one for a basic project.

However, complex projects require some quality of life features, like referencing variables, the PATH, command line args, control flow, includes, dependencies, pre/post build commands, icons, windows .rc files, platform-specific options and excluding specific files and directories.

Let's go through each aspect.

### Variables
A .build file is made up of key value pairs. Before the first whitespace is the Key, anything after that is the Value. Keys are case insensitive.

Note: keywords like `if`, `switch`, `goto`, etc are not considered variables.
```make
-------------------------------------
|    Key     |        Value         |
-------------------------------------
CompilerFlags -std=c99 -O3 -Wall ...
```

To reference a variable in another variable, place a `$` before the name of the variable. (Case Insensitive)
```make
CommonFlags some flags
CompilerFlags $CommonFlags
```

This will expand to
```make
CommonFlags some flags
CompilerFlags some flags
```

Sometimes you would want to concatenate using a variable. Wrap the variable around with `$()`.
```make
ThirdPartyFolder Source/ThirdParty
LibraryDirectories $(ThirdPartyFolder)/SomeLib/bin
```

---

### Environment Variables
To reference environment variables, place an `@` before the name of the environment variable and wrap around with `()`. This is mandatory when referencing environment variables. (Case sensitive)
```make
LibraryDirectories @(CURL_PATH)/lib
```
This will expand to
```ini
LibraryDirectories "C:/Program Files/curl/lib"
```

---

### Internal Variables/Command Line Arguments
To reference a command line argument passed into `riftbuild` or an internal variable, place a `%` before the variable. (Case Insensitive)
```make
Assembly %_FileName # an internal variable that will expand to whatever the .build is called (without the extension)
```

Sometimes you need to access a value inside the build file from what was given on the command line. Command line arguments can be a singular phrase or a `Key=Value` option.
```make
riftbuild someapp.build thisisacmdvar
riftbuild someapp.build somekey=somevalue

# inside the .build file
CompilerFlags %somekey # this will expand to somevalue
Hello %thisisacmdvar   # this will expand to 1 (or 0 if not mentioned in the cmd line)
```

Command line arguments can come in handy when you want to do some basic control flow. Like enabling address sanitizer for example.
```make
riftbuild someapp.build asan mode=debug

# inside the .build file
if asan AsanFlags -fsantize=address -fsanitize-trap
if mode == debug   CompilerFlags -O0
if mode == release CompilerFlags -O3

CompilerFlags -std=c99 -Wall $AsanFlags
```
Notice how `%` was not present in the `asan` and `mode` if statement. This is because only internal variables/command line arguments can be used with control flow statements, therefore to save on typing and to simplify the syntax, the `%` is optional.

---

# Supported Platforms
- Windows (64-bit only)
- Linux (Debian and Arch based only)
- MacOS

# Dependencies
- No dependencies (other than a working C/C++ compiler)

---

# Why did i make this?
I hate CMake with every fiber of my being.

I was trying to build my game engine using CMake (previously used a .bat file) and ended by spending several hours trying to figure out how to use it correctly by googling and watching youtube tutorials and became really depressed with how complicated it was and didn't want to proceed any further.

I also tried alternative build systems like Meson, Ninja, Bazel, Premake, etc., and they were all too complicated and bloated for what should be a straight-forward thing, so I spent a day developing my own build system. With Rift Build you don't even need to write a .build file if your program is dead simple.

Writing a build file **should** be so much simpler than whatever the fuck CMake has concocted, and I firmly believe that you shouldn't have to learn **another** language (or a DSL) to build your program. A simple declarative build file is understood by everyone, thus there is no need to learn any complicated syntax to successfully write one. You get to skip the `cmake .`, `make`, `make install` dance and go straight to the compiler with just one command, `riftbuild`.

Note: You may encounter situations where CMake may cover more cases than Rift Build, but i don't care. My mental health and happiness is more important.
