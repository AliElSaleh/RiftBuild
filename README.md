# Rift Build
A simpler build tool for C/C++, because fuck cmake

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

```
# This is a comment!

Compiler clang # or gcc/cl or you can specify an absolute or relative path to your C compiler

Assembly SomeName
Extension exe # or dll/lib/a/so/dylib or replace this line with Type app or lib

# below are optional but you can edit for your project's needs
# these directories are relative to where you run "riftbuild" from

SourceDirectory src       # default value is nothing
BuildDirectory bin        # default value is Build
IntermediateDirectory int # default value is Intermediate

# fill in the blanks below for your program
CompilerFlags -std=c11 -O3 etc...
LinkerFlags 
IncludeFlags path/to/include-dir another/one thirdparty/dir

Libraries somelib opengl32 etc...
LibraryDirectories path/to/lib/dir another/one

Defines MAX_STUFF=5 SOME_DEFINE
```
