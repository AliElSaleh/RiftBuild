#pragma once

#include <stddef.h>

/* Finding the program's own data files at run time.

   Inside an .app bundle the executable lives in Contents/MacOS and its data in
   Contents/Resources; run straight out of Build/ there is no bundle at all and
   the same data is still sitting in the project's assets/ folder. Both cases
   are answered from the executable's own path, so nothing here depends on the
   working directory - which matters, because Finder does not set it to
   anything useful. */

#define BUNDLE_PATH_MAX 1024

/* Writes the resource directory into Out. Returns 0 if the OS would not say
   where this executable is. */
int Bundle_FindResourceDirectory(char* Out, size_t OutSize);

/* Reads Directory/FileName into a NUL-terminated buffer the caller frees.
   Returns NULL when the file is missing or unreadable. */
char* Bundle_ReadTextFile(const char* Directory, const char* FileName);
