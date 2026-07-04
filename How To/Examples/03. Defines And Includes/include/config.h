#pragma once

/* ENABLE_GREETING comes from the .build file, not from any source file. */
#if defined(ENABLE_GREETING)
    #define GREETING "welcome to the pantry"
#else
    #define GREETING "(greetings disabled)"
#endif

#if !defined(MAX_ITEMS)
    #define MAX_ITEMS 1
#endif
