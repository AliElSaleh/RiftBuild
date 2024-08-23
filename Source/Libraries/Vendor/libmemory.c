// memset, memcpy, memmove, and memcmp via x86 string instructions
// Execute this source with a shell to build libmemory.a.
// This is free and unencumbered software released into the public domain.

#if !COMPILER_MSVC

#if PLATFORM_WINDOWS

PRAGMA_DISABLE_WARNINGS

#if COMPILER_CLANG
#pragma clang diagnostic ignored "-Wincompatible-library-redeclaration"
#pragma clang diagnostic ignored "-Wsign-conversion"
#pragma clang diagnostic ignored "-Wmissing-prototypes"
#elif COMPILER_GCC
#pragma GCC diagnostic ignored "-Wbuiltin-declaration-mismatch"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wmissing-prototypes"
#endif

RIFT_API void* memset(void *dst, int c, usize len)
{
    void* r = dst;
    __asm__ volatile (
        "rep stosb"
        : "+D"(dst), "+c"(len)
        : "a"(c)
        : "memory"
    );
    return r;
}

RIFT_API void* memcpy(void* restrict dst, void* restrict src, usize len)
{
    void* r = dst;
    __asm__ volatile (
        "rep movsb"
        : "+D"(dst), "+S"(src), "+c"(len)
        :
        : "memory"
    );
    return r;
}

RIFT_API void* memmove(void* dst, void* src, usize len)
{
    // Use uptr to bypass pointer semantics:
    // (1) comparing unrelated pointers
    // (2) pointer arithmetic on null (i.e. gracefully handle null dst/src)
    // (3) pointer overflow ("one-before-the-beginning" in reversed copy)
    uptr d = (uptr)dst;
    uptr s = (uptr)src;
    if (d > s) {
        d += len - 1;
        s += len - 1;
        __asm__ ("std");
    }
    __asm__ volatile (
        "rep movsb; cld"
        : "+D"(d), "+S"(s), "+c"(len)
        :
        : "memory"
    );
    return dst;
}

RIFT_API int memcmp(const void* s1, const void* s2, usize len)
{
    //#if PLATFORM_32_BIT
    const unsigned char* p1 = (const unsigned char*)s1;
    const unsigned char* p2 = (const unsigned char*)s2;

    for (usize i = 0; i < len; i++)
    {
        if (p1[i] < p2[i])
        {
            return -1;
        }
        else if (p1[i] > p2[i]) 
        {
            return 1;
        }
    }
    return 0;
    /*
    #else
    // CCa "after"  == CF=0 && ZF=0
    // CCb "before" == CF=1
    int a, b;
    __asm__ volatile (
        "xor %%eax, %%eax\n"  // CF=0, ZF=1 (i.e. CCa = CCb = 0)
        "repz cmpsb\n"
        : "+D"(s1), "+S"(s2), "+c"(len), "=@cca"(a), "=@ccb"(b)
        :
        : "ax", "memory"
    );
    return b - a;
    #endif
    */
}

PRAGMA_ENABLE_WARNINGS

#endif
#endif
