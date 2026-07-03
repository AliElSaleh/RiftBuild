#ifdef _WIN32
    #define SHARED_EXPORT __declspec(dllexport)
#else
    #define SHARED_EXPORT
#endif

SHARED_EXPORT int shared_answer(void);

SHARED_EXPORT int shared_answer(void)
{
    return 42;
}
