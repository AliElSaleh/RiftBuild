#pragma once

#include "EngineTypes.h"

#define _Crash_ do { int* volatile _nptr_ = (int*)1; *_nptr_ = 69; } while (0)

#ifdef NO_ASSERT
	#define ASSERT(Expression)
	#define ASSERT_MSG(Expression, Text, ...)
	#define DEBUG_BREAK
#else
	#if PLATFORM_WINDOWS
		#define DEBUG_BREAK __debugbreak
	#else
		#define DEBUG_BREAK __builtin_trap
	#endif

    #ifdef NO_LOG
        #define ASSERT(Expression) do { if (Expression) {} else { DEBUG_BREAK(); _Crash_; } } while (0)
    #else
        #ifdef FAST_LOG
        #define ASSERT(Expression) do { if (Expression) {} else { if (Logging_ShouldCrashOnFatal()) { Logging_Flush(); DEBUG_BREAK(); _Crash_; } } } while (0)
        #else
        #define ASSERT(Expression) do { if (Expression) {} else { if (Logging_ShouldCrashOnFatal()) { DEBUG_BREAK(); _Crash_; } } } while (0)
        #endif
    #endif

    #ifdef NO_LOG
        #define ASSERT_MSG(Expression, Text, ...) do { if (Expression) {} else { DEBUG_BREAK(); _Crash_; } } while (0)
    #else
        #ifdef FAST_LOG
        #define ASSERT_MSG(Expression, Text, ...) do { if (Expression) {} else { LOG_DEBUG_T(LOG_TYPE_FATAL, "Assert Failed: '" STRINGIZE(Expression) "'" ". " Text, ##__VA_ARGS__); if (Logging_ShouldCrashOnFatal()) { Logging_Flush(); DEBUG_BREAK(); _Crash_; } } } while (0)
        #else
        #define ASSERT_MSG(Expression, Text, ...) do { if (Expression) {} else { LOG_DEBUG_T(LOG_TYPE_FATAL, "Assert Failed: '" STRINGIZE(Expression) "'" "\n" Text, ##__VA_ARGS__); if (Logging_ShouldCrashOnFatal()) { DEBUG_BREAK(); _Crash_; } } } while (0)
        #endif
    #endif
#endif // NO_ASSERT

#ifdef NO_LOG
#define LOG_INTERNAL(LogCategory, LogType, Text, ...)

#define LOG_CAT(LogCategory, Text, ...)
#define LOG_CAT_INFO(LogCategory, Text, ...)
#define LOG_CAT_SUCCESS(LogCategory, Text, ...)
#define LOG_CAT_WARNING(LogCategory, Text, ...)
#define LOG_CAT_ERROR(LogCategory, Text, ...)
#define LOG_CAT_FATAL(LogCategory, Text, ...)

#define LOG(Text, ...)
#define LOG_INFO(Text, ...)
#define LOG_SUCCESS(Text, ...)
#define LOG_WARNING(Text, ...)
#define LOG_ERROR(Text, ...)
#define LOG_FATAL(Text, ...)

#define LOG_INLINE(Text, ...)
#define LOG_INLINE_INFO(Text, ...)
#define LOG_INLINE_SUCCESS(Text, ...)
#define LOG_INLINE_WARNING(Text, ...)
#define LOG_INLINE_ERROR(Text, ...)

#define LOG_DEBUG_T(LogType, Text, ...)
#define LOG_DEBUG(Text, ...)

#define LOG_LINE_BREAK()

#define UNIMPLEMENTED 
#else
#include "String/BaseString.h"

#define MAX_LOG_MSG_LENGTH 32768

#define LOG_TYPE_INFO 0
#define LOG_TYPE_SUCCESS 1
#define LOG_TYPE_WARNING 2
#define LOG_TYPE_ERROR 3
#define LOG_TYPE_FATAL 4
#define LOG_TYPE_NONE 5

#define FILELINE __FILE__ " | Line: " STRINGIZE(__LINE__)

#define LOG_INTERNAL(LogCategory, LogType, Text, ...)           LogMessage(LogType, StrLit(LogCategory), StrLit(Text), ##__VA_ARGS__)
//#define LOG_INTERNAL(LogCategory, LogType, Text, ...)         LogMessage(LogType, LogCategory, Text, ##__VA_ARGS__)

#define LOG_CAT(LogCategory, Text, ...) 			            LOG_INTERNAL(LogCategory, LOG_TYPE_NONE, Text, ##__VA_ARGS__)
#define LOG_CAT_INFO(LogCategory, Text, ...) 		            LOG_INTERNAL(LogCategory, LOG_TYPE_INFO, Text, ##__VA_ARGS__)
#define LOG_CAT_SUCCESS(LogCategory, Text, ...) 	            LOG_INTERNAL(LogCategory, LOG_TYPE_SUCCESS, Text, ##__VA_ARGS__)
#define LOG_CAT_WARNING(LogCategory, Text, ...) 	            LOG_INTERNAL(LogCategory, LOG_TYPE_WARNING, Text, ##__VA_ARGS__)
#define LOG_CAT_ERROR(LogCategory, Text, ...) 		            LOG_INTERNAL(LogCategory, LOG_TYPE_ERROR, Text, ##__VA_ARGS__)

#ifdef FAST_LOG
#define LOG_CAT_FATAL(LogCategory, Text, ...) 		            LOG_INTERNAL(LogCategory, LOG_TYPE_FATAL, Text, ##__VA_ARGS__); Logging_Flush(); _Crash_
#else
#define LOG_CAT_FATAL(LogCategory, Text, ...) 		            LOG_INTERNAL(LogCategory, LOG_TYPE_FATAL, Text, ##__VA_ARGS__); _Crash_
#endif

#define LOG(Text, ...) 								            LOG_CAT(__FILE_NAME__, Text, ##__VA_ARGS__)
#define LOG_INFO(Text, ...) 						            LOG_CAT_INFO(__FILE_NAME__, Text, ##__VA_ARGS__)
#define LOG_SUCCESS(Text, ...) 						            LOG_CAT_SUCCESS(__FILE_NAME__, Text, ##__VA_ARGS__)
#define LOG_WARNING(Text, ...) 						            LOG_CAT_WARNING(__FILE_NAME__, Text, ##__VA_ARGS__)
#define LOG_ERROR(Text, ...) 						            LOG_CAT_ERROR(__FILE_NAME__, Text, ##__VA_ARGS__)

#define LOG_FATAL(Text, ...) 						            LOG_CAT_FATAL(__FILE_NAME__, Text, ##__VA_ARGS__)

#define LOG_DEBUG_T(LogType, Text, ...) 			            LOG_INTERNAL(FILELINE, LogType, Text, ##__VA_ARGS__)
#define LOG_DEBUG(Text, ...) 						            LOG_DEBUG_T(LOG_TYPE_INFO, Text, ##__VA_ARGS__)

#define LOG_LINE_BREAK LogLineBreak

#define LOG_INLINE(Text, ...) 						            LogDirectMessage(LOG_TYPE_NONE, StrLit(Text), ##__VA_ARGS__)
#define LOG_INLINE_INFO(Text, ...) 					            LogDirectMessage(LOG_TYPE_INFO, StrLit(Text), ##__VA_ARGS__)
#define LOG_INLINE_SUCCESS(Text, ...) 				            LogDirectMessage(LOG_TYPE_SUCCESS, StrLit(Text), ##__VA_ARGS__)
#define LOG_INLINE_WARNING(Text, ...) 				            LogDirectMessage(LOG_TYPE_WARNING, StrLit(Text), ##__VA_ARGS__)
#define LOG_INLINE_ERROR(Text, ...) 				            LogDirectMessage(LOG_TYPE_ERROR, StrLit(Text), ##__VA_ARGS__)

#define LOG_INT(Int)                                            LOG(#Int ": %i", Int)
#define LOG_UINT(Int)                                           LOG(#Int ": %u", Int)
#define LOG_FLOAT(Float)                                        LOG(#Float ": %f", (f64)Float)
#define LOG_BOOL(Bool)                                          LOG(#Bool ": %s", ((Bool) ? "true" : "false"))
#define LOG_STRING(String)                                      LOG(#String ": %s", (String).Data)

#define UNIMPLEMENTED                                           Platform_ConsoleWrite(__FUNCTION__, 4, true); Platform_ConsoleWrite(" not implemented!\n", 4, true); _Crash_

RIFT_API bool Logging_Initialize(void* Memory, bool bOpenFile);
RIFT_API void Logging_Shutdown(void);
RIFT_API u64  Logging_GetMemoryRequirement(void);

RIFT_API void Logging_Enable(void);
RIFT_API void Logging_Disable(void);

RIFT_API void Logging_ToggleLogTimeStamp(bool bShow);
RIFT_API void Logging_ToggleLogCategory(bool bShow);
RIFT_API void Logging_ToggleLogType(bool bShow);
RIFT_API void Logging_ToggleLogFile(bool bLogToFile);

RIFT_API void Logging_SetCrashOnFatal(bool bShouldCrash);
RIFT_API bool Logging_ShouldCrashOnFatal(void);

RIFT_API void Logging_PrintStackTrace(void);

#ifdef FAST_LOG
RIFT_API void Logging_Flush(void);
#endif

RIFT_API void LogMessage(u8 LogType, const String LogCat, const String Text, ...);
RIFT_API void LogDirectMessage(u8 LogType, const String Text, ...);
RIFT_API void LogLineBreak(void);
#endif // NO_LOG
