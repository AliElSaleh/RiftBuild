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

However, complex projects require some quality of life features, like referencing variables, the PATH, command line args, control flow, includes, dependencies, pre/post build commands, icons, windows .rc files, platform-specific options and excluding specific files and directories.

Let's go through each aspect.

### Variables
A .build file is made up of key value pairs. Before the first whitespace is the Key, anything after that is the Value. Keys are case insensitive

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

Sometimes you would want to concatenate using a variable. Wrap the variable around with `$()`
```make
ThirdPartyFolder Source/ThirdParty
LibraryDirectories $(ThirdPartyFolder)/SomeLib/bin
```

---

### Environment Variables
To reference environment variables, place an `@` before the name of the environment variable and wrap around with `()`. This is mandatory when referencing environment variables
```make
LibraryDirectories @(CURL_PATH)/lib
```
This will expand to
```ini
LibraryDirectories "C:\Program Files\curl"/lib # riftbuild will take care of fixing up the paths, so don't worry too much
```

---

### Internal/Command Line Variables
```make

```
