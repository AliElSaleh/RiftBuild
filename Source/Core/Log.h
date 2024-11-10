#ifndef LOG_H
#define LOG_H

#ifndef UNITY_BUILD
#include "EngineTypes.h"
#endif

#ifdef NO_ASSERT
    #define ASSERT_MSG(Expression, Text, ...)
    #define ENSURE_MSG(Expression, Text, ...)
    #define ENSURE_NO_REENTRY()
#else
    // TODO: simplify
    #ifdef NO_LOG
        #define ASSERT_MSG(Expression, Text, ...) ASSERT(Expression)
        #define ENSURE_MSG(Expression, Text, ...) ENSURE(Expression)
        #define ENSURE_NO_REENTRY() do { static bool MACRO_VAR(bBeenHere) = false; if (MACRO_VAR(bBeenHere)) { DEBUG_BREAK(); } MACRO_VAR(bBeenHere) = true; } while (0)
    #else
        #define ASSERT_MSG(Expression, Text, ...) do { if (Expression) {} else { LOG_DEBUG_T(LOG_TYPE_FATAL, "Assert Failed: '" STRINGIZE(Expression) "'" "\n" Text, ##__VA_ARGS__); DEBUG_BREAK();  _Crash_; } } while (0)
        #define ENSURE_MSG(Expression, Text, ...) do { if (Expression) {} else { LOG_DEBUG_T(LOG_TYPE_FATAL, "Ensure Failed: '" STRINGIZE(Expression) "'" "\n" Text, ##__VA_ARGS__); DEBUG_BREAK(); } } while (0)
        #define ENSURE_NO_REENTRY() { static bool MACRO_VAR(bBeenHere) = false; if (MACRO_VAR(bBeenHere)) { static StringLocal(MACRO_VAR(ReentryErrorMsg), 512); String_Format(&MACRO_VAR(ReentryErrorMsg), S("Ensure No Re-Entry Failed: %s(%s) was called more than once!\n"), __func__, FILELINE); Platform_ConsoleWrite((char*)MACRO_VAR(ReentryErrorMsg).Data, 4, true); DEBUG_BREAK(); } MACRO_VAR(bBeenHere) = true; }
    #endif
#endif

#ifdef NO_LOG
#define LOG_CAT(LogCategory, Text, ...)
#define LOG_CAT_INFO(LogCategory, Text, ...)
#define LOG_CAT_SUCCESS(LogCategory, Text, ...)
#define LOG_CAT_WARNING(LogCategory, Text, ...)
#define LOG_CAT_ERROR(LogCategory, Text, ...)
#define LOG_CAT_FATAL(LogCategory, Text, ...)

#define LOG(Text, ...)
#define LOG_MUTE(...)
#define LOG_INFO(Text, ...)
#define LOG_SUCCESS(Text, ...)
#define LOG_WARNING(Text, ...)
#define LOG_ERROR(Text, ...)
#define LOG_FATAL(Text, ...)

#define LOG_INLINE(Text, ...)
#define LOG_INLINE_MUTE(Text, ...)
#define LOG_INLINE_INFO(Text, ...)
#define LOG_INLINE_SUCCESS(Text, ...)
#define LOG_INLINE_WARNING(Text, ...)
#define LOG_INLINE_ERROR(Text, ...)

#define LOG_DEBUG_T(LogType, Text, ...)
#define LOG_DEBUG(Text, ...)

#define LOG_LINE_BREAK()

#define LOG_INT(Int)
#define LOG_UINT(Int)
#define LOG_FLOAT(Float)
#define LOG_BOOL(Bool)
#define LOG_STRING(String)

#define UNIMPLEMENTED 
#else
#define MAX_LOG_MSG_LENGTH 32768

#define LOG_TYPE_INFO 0
#define LOG_TYPE_SUCCESS 1
#define LOG_TYPE_WARNING 2
#define LOG_TYPE_ERROR 3
#define LOG_TYPE_FATAL 4
#define LOG_TYPE_NONE 5
#define LOG_TYPE_MUTE 6

#define LOG_CAT(LogCategory, Text, ...)                LogMessage(LOG_TYPE_NONE,    S(LogCategory), S(Text), ##__VA_ARGS__)
#define LOG_CAT_MUTE(LogCategory, Text, ...)           LogMessage(LOG_TYPE_MUTE,    S(LogCategory), S(Text), ##__VA_ARGS__)
#define LOG_CAT_INFO(LogCategory, Text, ...)           LogMessage(LOG_TYPE_INFO,    S(LogCategory), S(Text), ##__VA_ARGS__)
#define LOG_CAT_SUCCESS(LogCategory, Text, ...)        LogMessage(LOG_TYPE_SUCCESS, S(LogCategory), S(Text), ##__VA_ARGS__)
#define LOG_CAT_WARNING(LogCategory, Text, ...)        LogMessage(LOG_TYPE_WARNING, S(LogCategory), S(Text), ##__VA_ARGS__)
#define LOG_CAT_ERROR(LogCategory, Text, ...)          LogMessage(LOG_TYPE_ERROR,   S(LogCategory), S(Text), ##__VA_ARGS__)
#define LOG_CAT_FATAL(LogCategory, Text, ...)          LogMessage(LOG_TYPE_FATAL,   S(LogCategory), S(Text), ##__VA_ARGS__); _Crash_

#define LOG(Text, ...)                                 LogMessage(LOG_TYPE_NONE,    S(__FILE_NAME__), S(Text), ##__VA_ARGS__)
#define LOG_MUTE(Text, ...)                            LogMessage(LOG_TYPE_MUTE,    S(__FILE_NAME__), S(Text), ##__VA_ARGS__)
#define LOG_INFO(Text, ...)                            LogMessage(LOG_TYPE_INFO,    S(__FILE_NAME__), S(Text), ##__VA_ARGS__)
#define LOG_SUCCESS(Text, ...)                         LogMessage(LOG_TYPE_SUCCESS, S(__FILE_NAME__), S(Text), ##__VA_ARGS__)
#define LOG_WARNING(Text, ...)                         LogMessage(LOG_TYPE_WARNING, S(__FILE_NAME__), S(Text), ##__VA_ARGS__)
#define LOG_ERROR(Text, ...)                           LogMessage(LOG_TYPE_ERROR,   S(__FILE_NAME__), S(Text), ##__VA_ARGS__)
#define LOG_FATAL(Text, ...)                           LogMessage(LOG_TYPE_FATAL,   S(__FILE_NAME__), S(Text), ##__VA_ARGS__)

#define LOG_DEBUG_T(LogType, Text, ...)                LogMessage(LogType,          S(FILELINE),      S(Text), ##__VA_ARGS__)
#define LOG_DEBUG(Text, ...)                           LogMessage(LOG_TYPE_INFO,    S(FILELINE),      S(Text), ##__VA_ARGS__)

#define LOG_LINE_BREAK                                 LogLineBreak

#define LOG_INLINE(Text, ...)                          LogDirectMessage(LOG_TYPE_NONE,    S(Text), ##__VA_ARGS__)
#define LOG_INLINE_MUTE(Text, ...)                     LogDirectMessage(LOG_TYPE_MUTE,    S(Text), ##__VA_ARGS__)
#define LOG_INLINE_INFO(Text, ...)                     LogDirectMessage(LOG_TYPE_INFO,    S(Text), ##__VA_ARGS__)
#define LOG_INLINE_SUCCESS(Text, ...)                  LogDirectMessage(LOG_TYPE_SUCCESS, S(Text), ##__VA_ARGS__)
#define LOG_INLINE_WARNING(Text, ...)                  LogDirectMessage(LOG_TYPE_WARNING, S(Text), ##__VA_ARGS__)
#define LOG_INLINE_ERROR(Text, ...)                    LogDirectMessage(LOG_TYPE_ERROR,   S(Text), ##__VA_ARGS__)

#define LOG_INT(Int)                                   LOG(#Int    ": %i", (i32)Int)
#define LOG_UINT(Int)                                  LOG(#Int    ": %u", (u32)Int)
#define LOG_FLOAT(Float)                               LOG(#Float  ": %f", (f64)Float)
#define LOG_BOOL(Bool)                                 LOG(#Bool   ": %S", ((Bool) ? S("true") : S("false")))
#define LOG_STRING(Str)                                LOG(#Str    ": %S", Str)

#define UNIMPLEMENTED                                  Platform_ConsoleWrite(FUNCTION_NAME, 4, true); Platform_ConsoleWrite(" not implemented!\n", 4, true); _Crash_

RIFT_API NO_DISCARD bool Logging_Initialize(void* Memory, bool bOpenFile);
RIFT_API void Logging_Shutdown(void);
RIFT_API NO_DISCARD usize Logging_GetMemoryRequirement(void);

RIFT_API void Logging_Enable(void);
RIFT_API void Logging_Disable(void);

RIFT_API void Logging_ToggleLogTimeStamp(bool bShow);
RIFT_API void Logging_ToggleLogCategory(bool bShow);
RIFT_API void Logging_ToggleLogType(bool bShow);
RIFT_API void Logging_ToggleLogFile(bool bLogToFile);
RIFT_API void Logging_ToggleEnableOnError(bool bEnable);

RIFT_API void Logging_SetCrashOnFatal(bool bShouldCrash);
RIFT_API bool Logging_ShouldCrashOnFatal(void);

RIFT_API void LogMessage(u8 LogType, const struct String LogCat, const struct String Text, ...);
RIFT_API void LogDirectMessage(u8 LogType, const struct String Text, ...);
RIFT_API void LogLineBreak(void);
#endif // NO_LOG

#endif // LOG_H
