#pragma once

/* On Windows, symbols must be explicitly exported from a DLL. The usual
   pattern is a small API macro like this one. */
#if defined(_WIN32)
    #define GREETER_API __declspec(dllexport)
#else
    #define GREETER_API
#endif

GREETER_API void Greeter_Hello(const char* Name);
GREETER_API int  Greeter_Version(void);
