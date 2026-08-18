<h1 align="center">Rift Build (OPEN BETA)</h1>
<p align="center">A simpler build tool for C/C++, because fuck CMake.</p>

<p align="center">
    <a href="https://github.com/AliElSaleh/RiftBuild/actions/"><img src="https://github.com/AliElSaleh/RiftBuild/actions/workflows/main.yml/badge.svg" alt="GitHub Actions"></a>
</p>

### Usage
A build call will look like this (in a directory with or without a .build file)
```bash
riftbuild
```

To build with a specific build file (specifying the .build extension is optional)
```bash
riftbuild someapp.build
```
That's it

---

# How to write a .build file
Writing a .build file is simple. It almost has no syntax.

```make
Compiler clang # or gcc/cl or specify an absolute or relative path to your C compiler

Assembly SomeName
Extension exe # or dll/lib/a/so/dylib/elf or replace this line with -> Type app or lib

# below are optional but you can edit them for your project's needs
# these directories are relative to where you run "riftbuild" from
SourceDirectory       src # default value is nothing
BuildDirectory        bin # default value is Build
IntermediateDirectory int # default value is Intermediate

# fill in/replace the following below for your program/project
Compiler.Flags        -std=c11 -O2 etc.
Linker.Flags          -fsomeflag etc.

# notice how you don't need to prefix with -D, -L or -I!
Compiler.Defines      MAX_STUFF=5 SOME_DEFINE etc.
Compiler.Includes     path/to/include-dir thirdparty/dir etc.
Libraries             somelib opengl32 etc.
Library.Paths         path/to/lib/dir another/dir etc.
```

---

# I'm not sold on this
You are wise to be skeptical of new tools that claim to be "better" than existing ones.

Below are forks of a few open-source projects that I've translated from CMake (and other build systems) to Rift Build. They can be built with just a single `riftbuild` call on the terminal at the project root directory.

They work on Windows, macOS and Linux (where appropriate).

Note: I still need to sync some of these forks to the latest and update the .build files accordingly. So some may be broken in the meantime while in this beta phase.

| Project                                                         |                                    |
|-----------------------------------------------------------------|------------------------------------|
| [Golden Decoder](https://github.com/AliElSaleh/GoldenDecoder)   | Golden Record Image Decoder        |
| [Jolt Physics](https://github.com/AliElSaleh/JoltPhysics)       | Physics Engine                     |
| [Craft](https://github.com/AliElSaleh/Craft)                    | Minecraft Clone                    |
| [RAD Debugger](https://github.com/AliElSaleh/raddebugger)       | Graphical Native Debugger          |
| [Box3D](https://github.com/AliElSaleh/box3d)                    | Physics Engine                     |
| [PhysX 4.1](https://github.com/AliElSaleh/PhysX)                | Physics Library                    |
| [SDL2 TODO](https://google.com)                                 | Framework Library                  |
| [Star Ruler 2](https://github.com/AliElSaleh/StarRuler2-Source) | Video Game                         |
| [fmt](https://github.com/AliElSaleh/fmt)                        | C++ String Formatting Library      |
| [GLFW](https://github.com/AliElSaleh/glfw)                      | Graphics Framework Library         |
| [FreeType](https://github.com/AliElSaleh/freetype)              | Font Renderer                      |
| [libjpeg-turbo](https://github.com/AliElSaleh/libjpeg-turbo)    | JPEG Library                       |
| [dav1d](https://github.com/AliElSaleh/dav1d)                    | AV1 Decoder                        |
| [PCSX2](https://github.com/AliElSaleh/pcsx2)                    | PS2 Emulator                       |
| [Raylib TODO](https://google.com)                               | Game Framework Library             |
| [Kinema](https://github.com/AliElSaleh/kinema)                  | Voxel Tech Demo                    |
| [PhysFS](https://github.com/AliElSaleh/physfs)                  | Multi-platform Virtual File System |
| [Ninja](https://github.com/AliElSaleh/ninja)                    | Build Tool                         |

---

# Building RiftBuild
### Windows
```
build.bat
```
### Linux / Mac OS / BSD
```
./build.sh
```

<sub><sup>if only every open source project were like this...</sup></sub>

---

# Philosophy and Principles

The guiding north star of RiftBuild, followed in design and implementation.

### One-step build
- A single `riftbuild` call is all that is needed to build a program. No need to configure or setup anything.

### Focused
- RiftBuild is solely focused on alleviating the headaches of building C and C++ projects specifically. This is not a general-purpose build tool (like Make, CMake, Ninja, etc.) and it should not try to be.

### Simplicity
- No `.build` file is necessary to build simple C or C++ programs. Source files are all that is needed to build your program.
- **No complicated syntax or language to learn**, anyone can immediately write a `.build` file without prerequisite knowledge or reading a documentation page.

### Convenience
- RiftBuild brings the lost joy of building C and C++ projects. It automatically finds tools on your system to do the job for you without having to remember esoteric command line arguments of various compilers and linkers.
- Write once, build everywhere. RiftBuild is designed to be cross-platform. No need to write build scripts for every operating system that you want to target.
- Runs on all major operating systems: Windows, macOS, Linux (five major distros), FreeBSD, OpenBSD and NetBSD. Supporting x86 and ARM architectures for 32/64-bit systems.

### Performance
- RiftBuild is written in pure C, from scratch, with minimal dynamic memory allocation, almost everything happens on the stack.
- RiftBuild automatically utilizes all cores of the CPU for efficient compilation. This means less time building and more time programming.
- RiftBuild should essentially only be a wrapper over the compiler and linker with a minimal amount of overhead, **always** in the order of milliseconds.

[See why I made this here](#why-did-i-make-this)

---

# Reference Manual

See the [How To](How%20To/README.md): a numbered series of tiny example
projects covering every feature. From a one-line hello world up to building
third-party libraries and a playable video game, plus syntax reference in
[Reference.md](How%20To/Reference.md) and editor syntax highlighting setup
for VS Code, Vim and Emacs.

---

# Supported Platforms & Architectures

#### Architectures
- x86/x64
- arm/arm64
  
#### Platforms
- Windows
  - 7 and above
- Linux
  - All Debian, Red Hat, Fedora, SUSE and Arch based systems
- macOS
  - 10.12 (Mojave) and above
- BSD
  - FreeBSD, OpenBSD and NetBSD

---

# Why did I make this?
It feels like with build systems, [there is no paradise that you can escape to](https://i.redd.it/m23pfqcnone71.jpg). I want to change that. 

I hate CMake with every fiber of my being.

I was trying to build my game engine using CMake (because that's what professionals should be using right? I had previously used a .bat script). I ended up spending a few hours trying to figure out how to program in it correctly by googling and watching YouTube tutorials. I eventually became depressed with how complicated it was and didn't want to proceed any further.

And yes, I've also tried alternative build systems like Meson, Ninja, Bazel, Premake, etc., and guess what, they were all shit, they were all too complicated and bloated for what should be (in my view) a straightforward process, so I spent one weekend developing the first iteration of my own build system. A perk of this system, is that you don't even need to write **_any_** .build script if your program is dead simple. This allows you to be productive much quicker, so you spend **_zero_** time thinking about how you should build something and more time programming.

Writing a build file **should** be so much simpler than whatever the fuck CMake has concocted, and I firmly believe that you shouldn't have to learn **another** language (or a DSL) to build your program. A simple declarative build file is understood by everyone, thus there is no need to learn any complicated syntax to successfully write one. You get to skip the `cmake ..`, `make`, `make install` dance bullshit and go straight to the compiler with just one command, `riftbuild`.

Note: You may encounter situations where CMake may cover more cases than Rift Build, but I don't care. My mental health and happiness is more important.

# Shitting on other build systems

#### CMake
![image](https://github.com/AliElSaleh/Rift-Build/assets/19608222/fa00ddf9-3cb0-4d74-a30d-7e1be1881f0c)

I'll do you one better CMake...

_The most basic rift build project is an executable built from a single source file. For simple projects like this, **NO** build file is required._

---

#### SCons
![image](https://github.com/user-attachments/assets/4d9eb822-527e-4a84-8bd8-2e3e8d212da9)

Admittedly, one of the simpler build tools out there.

But still... you don't need to write a build file with riftbuild

<img src="https://github.com/user-attachments/assets/62516fa8-c65b-4203-b9f4-9d76f4929b3a" width="88%" height="88%"/>

---

#### Bazel
![image](https://github.com/AliElSaleh/Rift-Build/assets/19608222/fe030f44-99dd-4e03-9fb0-2c3f190238fa)

Bazel can't even work with paths that have spaces in them... like what?? Just wrap the path with `""`, am I missing something??

Rift Build can handle them just fine...

![image](https://github.com/AliElSaleh/Rift-Build/assets/19608222/53c106ca-241f-40d2-8322-6262deedfa21)

---

#### Make

![image](https://github.com/AliElSaleh/Rift-Build/assets/19608222/f14b03df-3572-4798-a90c-629a64086ea5)

Trying to gather all .c files in a makefile is horrendous 🤮

---

#### CMake

![image](https://github.com/AliElSaleh/RiftBuild/assets/19608222/04b059ce-3747-4d4f-af22-e37d8b110568)

[Link](https://hansonry.wordpress.com/2010/12/15/windows-application-icon-using-mingw-and-cmake/)

ahhh yes, very intuitive indeed!

riftbuild however, only requires one step...
```make
Icon app.ico
```

---

#### Meson

![image](https://github.com/AliElSaleh/RiftBuild/assets/19608222/cbbd76e0-e4f4-41be-b990-764fd1693064)

[Link](https://mesonbuild.com/Windows-module.html#compile_resources)

meson doesn't have a way to specify icons, so instead they give you this pile of shit

all this bullshit just to say you want to compile a rc file... 🤦

with riftbuild, you don't need to say anything, they're treated as regular source files

if you need to exclude certain rc files, just do this
```make
ExcludedSourceFiles something.rc anotherone.rc
```

---

#### Premake5

![image](https://github.com/AliElSaleh/RiftBuild/assets/19608222/fcb9f39e-f360-4392-b331-c703e4fac1d6)

[Link](https://stackoverflow.com/questions/54508521/adding-a-c-executable-icon-to-premake5-build-script)

ooo yummy syntax celery 😋 look at me! I'm so smart! 🤓

again, it's crazy how no-one can get this right...
```make
Icon app.ico
```
and riftbuild automatically picks up and compiles `.rc` files as they're just source files too...

and depending on the compiler, it'll use the correct rc compiler. rc.exe for msvc, llvm-rc for clang and windres for gcc

---

#### Meson

![image](https://github.com/AliElSaleh/RiftBuild/assets/19608222/86b2260c-f023-4565-a624-7e8280b3548d)

[Link](https://mesonbuild.com/Creating-OSX-packages.html#creating-an-app-bundle)

meson does not support app bundling for macOS for some reason

we do, and it's not as hard as they claim it to be, they're just lazy.

with RiftBuild, all you need to do is specify this in your .build file
```make
Bundle
```
That's literally it

And, there are also some optional settings for fine-grain custom control (if that is needed for your macOS app)
```make
Bundle.InfoPlist    path/to/custom.plist
Bundle.VersionPlist path/to/custom.plist
Bundle.PkgInfo      path/to/custom/PkgInfo

# OR you can inline plist keys/values!
Info.plist {
    CFBundleDisplayName My App Name
    ...
}

Version.plist {
    ...
}
```
