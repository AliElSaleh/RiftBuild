# Rift Build
A simpler build tool for C/C++, because fuck CMake

---

### Usage
A simple build call will look like this (in a directory with or without a .build file)
```bash
riftbuild
```

To build with a specific build file
```bash
riftbuild someapp.build
```
That's it

---

# How to write a .build file
Writing a .build file is simple and straightforward. It almost has no syntax

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
IncludeFlags path/to/include-dir thirdparty/dir anotherdir

Libraries somelib opengl32 etc...
LibraryDirectories path/to/lib/dir another/one

Defines MAX_STUFF=5 SOME_DEFINE
```

---

# Advanced Stuff
The above .build file example is the simplest way to write one for a basic project.

However, complex projects require some quality of life features, like referencing variables, the PATH, command line args, control flow, includes, dependencies, pre/post build commands, icons, windows .rc files, platform-specific options and excluding specific files and directories
```
TODO
```
