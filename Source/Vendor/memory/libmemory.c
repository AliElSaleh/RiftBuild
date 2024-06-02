// memset, memcpy, memmove, and memcmp via x86 string instructions
// Execute this source with a shell to build libmemory.a.
// This is free and unencumbered software released into the public domain.

#if PLATFORM_WINDOWS
typedef __SIZE_TYPE__    size_t;
typedef __UINTPTR_TYPE__ uintptr_t;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wincompatible-library-redeclaration"
#pragma clang diagnostic ignored "-Wsign-conversion"

RIFT_API void* memset(void *dst, int c, size_t len)
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

RIFT_API void* memcpy(void* restrict dst, void* restrict src, size_t len)
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

RIFT_API void* memmove(void* dst, void* src, size_t len)
{
    // Use uintptr_t to bypass pointer semantics:
    // (1) comparing unrelated pointers
    // (2) pointer arithmetic on null (i.e. gracefully handle null dst/src)
    // (3) pointer overflow ("one-before-the-beginning" in reversed copy)
    uintptr_t d = (uintptr_t)dst;
    uintptr_t s = (uintptr_t)src;
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

RIFT_API int memcmp(void* s1, void* s2, size_t len)
{
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
}

#pragma clang diagnostic pop

#endif
