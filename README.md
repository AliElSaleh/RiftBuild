[![Build Status](https://github.com/AliElSaleh/RiftBuild/actions/workflows/main.yml/badge.svg)](https://github.com/AliElSaleh/RiftBuild/actions/)
[![Codacy Badge](https://app.codacy.com/project/badge/Grade/ff3c18bcf56545d2b1b08fa100281bfd)](https://app.codacy.com?utm_source=gh&utm_medium=referral&utm_content=&utm_campaign=Badge_grade)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](https://opensource.org/licenses/MIT)


# Rift Build (ALPHA)
A simpler build tool for C/C++, because fuck CMake


### Usage
A simple build call will look like this (in a directory with or without a .build file).
```bash
riftbuild
```

To build with a specific build file. (specifying the .build extension is optional)
```bash
riftbuild someapp.build
```
That's it

---

# How to write a .build file
Writing a .build file is simple. It almost has no syntax.

```make
Compiler clang # or gcc/cl or you can specify an absolute or relative path to your C compiler

Assembly SomeName
Extension exe # or dll/lib/a/so/dylib/elf or replace this line with Type app or lib

# below are optional but you can edit them for your project's needs
# these directories are relative to where you run "riftbuild" from
SourceDirectory src       # default value is nothing
BuildDirectory bin        # default value is Build
IntermediateDirectory int # default value is Intermediate

# fill in/replace the following below for your program/project
CompilerFlags -std=c11 -O3 etc...
LinkerFlags 
Includes path/to/include-dir thirdparty/dir anotherdir

Libraries somelib opengl32 etc...
LibraryDirectories path/to/lib/dir another/one

Defines MAX_STUFF=5 SOME_DEFINE
```

---

# I'm not sold on this
You are wise to be skeptical of new tools.

Below are forks of a few open-source projects that I've translated from CMake (and other build systems) to Rift Build. They can be built with just a single `riftbuild` call on the terminal at the project root directory on Windows, Mac and Linux (where appropriate).

- [Jolt Physics](https://github.com/AliElSaleh/JoltPhysics)
- [Refterm TODO](https://google.com)
- [Craft](https://github.com/AliElSaleh/Craft)
- [RAD Debugger](https://github.com/AliElSaleh/raddebugger)
- [PhysX TODO](https://google.com)
- [SDL2 TODO](https://google.com)
- [Star Ruler 2](https://github.com/AliElSaleh/StarRuler2-Source)
- [fmt](https://github.com/AliElSaleh/fmt)
- [GLFW](https://github.com/AliElSaleh/glfw)
- [FreeType](https://github.com/AliElSaleh/freetype)
- [libjpeg-turbo](https://github.com/AliElSaleh/libjpeg-turbo)
- [libpng](https://github.com/AliElSaleh/libpng)
- [zlib](https://github.com/AliElSaleh/zlib)
- [Hazel TODO](https://google.com)
- [PCSX2 TODO](https://google.com)
- [RPCS3 TODO](https://google.com)
- [Raylib TODO](https://google.com)
- [Playdate SDK TODO](https://google.com)
- [Kinema TODO](https://google.com)
- [PhysFS](https://github.com/AliElSaleh/physfs)
- [Zydis](https://github.com/AliElSaleh/zydis)

---

# Building RiftBuild
RiftBuild can of course build itself with just a single call like this
```
riftbuild
```

However, if you'd like to compile from source without `riftbuild`, then there are OS specific build scripts that you can use.
From the project root directory, run them like so
### Windows
```
build.bat
```
### Unix (Linux / Mac OS / BSD)
```
./build.sh
```
RiftBuild compiles with Clang so make sure that is installed before building

---

# Advanced Stuff (Work In Progress!)
The above .build file example is the simplest way to write one for a basic project. In fact, if your program is too simple, you don't even need to write a build file.

However, complex projects require some quality-of-life features, like referencing variables, the PATH, command line args, control flow, includes, dependencies, pre/post build commands, icons, windows .rc files, platform-specific options and excluding specific files and directories.

Let's go through each aspect.

### Variables
A .build file is made up of key-value pairs. Before the first whitespace is the Key, anything after that is the Value. Keys are case insensitive.

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
To reference environment variables, place an `@` before the name of the environment variable. (Case sensitive)
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
# on the cmd line
> riftbuild someapp.build thisisacmdarg
> riftbuild someapp.build somekey=somevalue

# inside the .build file
CompilerFlags %somekey # this will expand to somevalue
Hello %thisisacmdvar   # this will expand to 1 (or 0 if not mentioned in the cmd line)
```

Command line arguments can come in handy when you want to do some basic control flow. Like enabling address sanitizer for example.
```make
# on the cmd line
> riftbuild someapp.build asan mode=debug

# inside the .build file
if asan AsanFlags -fsantize=address -fsanitize-trap
if mode == debug   CompilerFlags -O0
if mode == release CompilerFlags -O3

CompilerFlags -std=c99 -Wall $AsanFlags
```
Notice how `%` was not present in the `asan` and `mode` if statement. This is because we search all variables whether it be a user made build file variable or a command line argument, therefore to save on typing and to simplify the syntax, the `%` or `$` is optional.

---

# Icon
To set an icon for an executable, either specify the .ico name or give an absolute or relative path to the .ico file.
```make
Icon someicon.ico # or path/to/icon/file.ico
```
The fact that other build systems are unable to do this is fucking pathetic

---

# Supported Platforms
- Windows (64-bit only)
- Linux (Debian and Arch-based only)
- MacOS
- FreeBSD/OpenBSD (TODO)

# Dependencies
- No dependencies (other than a working C/C++ compiler)

---

# Why did I make this?
I hate CMake with every fiber of my being.

I was trying to build my game engine using CMake (previously used a .bat file) and ended up spending several hours trying to figure out how to use it correctly by googling and watching youtube tutorials and became depressed with how complicated it was and didn't want to proceed any further.

I also tried alternative build systems like Meson, Ninja, Bazel, Premake, etc., and they were all too complicated and bloated for what should be a straightforward thing, so I spent a day developing my own build system. With Rift Build you don't even need to write a .build file if your program is dead simple.

Writing a build file **should** be so much simpler than whatever the fuck CMake has concocted, and I firmly believe that you shouldn't have to learn **another** language (or a DSL) to build your program. A simple declarative build file is understood by everyone, thus there is no need to learn any complicated syntax to successfully write one. You get to skip the `cmake .`, `make`, `make install` dance bullshit and go straight to the compiler with just one command, `riftbuild`.

Note: You may encounter situations where CMake may cover more cases than Rift Build, but I don't care. My mental health and happiness is more important.

# Shitting on other build systems

#### CMake
![image](https://github.com/AliElSaleh/Rift-Build/assets/19608222/fa00ddf9-3cb0-4d74-a30d-7e1be1881f0c)

I'll do you one better CMake...

_The most basic rift build project is an executable built from a single source file. For simple projects like this, **NO** build file is required._

---

#### Bazel
![image](https://github.com/AliElSaleh/Rift-Build/assets/19608222/fe030f44-99dd-4e03-9fb0-2c3f190238fa)

Bazel can't even work with paths that have spaces in them... like what?? Just wrap the path with `""`, am i missing something??

Rift Build can handle them just fine...

![image](https://github.com/AliElSaleh/Rift-Build/assets/19608222/53c106ca-241f-40d2-8322-6262deedfa21)

---

#### Make

![image](https://github.com/AliElSaleh/Rift-Build/assets/19608222/f14b03df-3572-4798-a90c-629a64086ea5)

Trying to gather all .c files in a makefile is horrendous 🤮

