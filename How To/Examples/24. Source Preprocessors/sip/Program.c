// =============================================================================
// String Interpolation Metaprogram
// -----------------------------------------------------------------------------
// A build-pipeline pre-pass that rewrites string-interpolated printf() calls
// into plain C that a normal C compiler can parse and compile.
//
// Syntax recognised (in the *format string only*):
//
//     printf("{x} {y}");
//
// where `x` and `y` are local variables (or function parameters / globals) of a
// primitive type. The tool scans the source for the declarations of those
// variables, infers their type, and rewrites the call into:
//
//     printf("%d %d", x, y);
//
// Usage:
//     Program.exe [flags] <file1.c> <file2.c> ...
//
//     -s, --single-threaded, -1   Force single-threaded processing.
//     -o, --out-of-place          Write the result to <name>.gen.<ext> instead
//                                 of overwriting the input in place (useful for
//                                 diffing / compiling the output separately).
//     -q, --quiet                 Disable all logging output.
//
// By default each file is rewritten in place. Files are processed in parallel
// across the available logical cores, with a single-threaded fallback (always
// used on non-Windows platforms, when only one file is given, or when -s is
// passed).
//
// Supported types -> format specifier:
//     C builtins (in every legal permutation, e.g. "unsigned long long"):
//         char                        -> %c          char* / const char* -> %s
//         signed/unsigned char        -> %c          char[]              -> %s
//         short / unsigned short      -> %hd / %hu   wchar* / wchar_t[]  -> %ls
//         int / unsigned int          -> %d  / %u
//         long / unsigned long        -> %ld / %lu
//         long long / unsigned ...    -> %lld / %llu
//         float / double              -> %f          long double         -> %Lf
//         _Bool / bool                -> %d          other pointers/arr  -> %p
//     Fixed-width aliases (Rift + <stdint.h>/<stddef.h>):
//         i8/u8/int8_t/uint8_t        -> %hhd / %hhu
//         i16/u16/int16_t/uint16_t    -> %hd  / %hu
//         i32/u32/int32_t/uint32_t    -> %d   / %u
//         i64/u64/int64_t/uint64_t    -> %lld / %llu
//         f32/f64                     -> %f
//         usize/size_t/uintptr_t/...  -> %llu (64-bit target)
//         isize/intptr_t/ptrdiff_t/.. -> %lld
//         uchar -> %c (%s as pointer)  wchar/wchar_t -> %lc (%ls as pointer)
//     Rift String type:
//         String                      -> %S          (String* -> %p)
//
// Escaped braces `{{` and `}}` emit literal `{` and `}`. Interpolation tokens
// that cannot be resolved to a known type are left untouched (so the file still
// compiles) and reported as a warning.
//
// Known limitations (documented, by design for a single-pass text rewriter):
//   * Interpolation inside a variable initializer is not expanded.
//   * Variables declared in a brace-less loop/if body (no `{}`) may not resolve.
//   * Only `printf` (not `sprintf`/`fprintf`) is targeted, per spec.
//   * Specifiers for pointer-width / 64-bit types assume a 64-bit target.
// =============================================================================

// TODO: no windows specific code in here. abstract it away
// TODO: remove direct spec bullshit

#include "Core/EntryPoint.h"
#include "Core/Filesystem.h"
#include "Core/StringUtils.h"
#include "Core/Allocators.h"

#if PLATFORM_WINDOWS
#include "Core/Win32Types.h"
#endif

const usize GEngineMemoryAmount  = Mebibytes(16);
const usize GEngineScratchAmount = Kibibytes(64);

// -----------------------------------------------------------------------------
// Growable byte buffer (OS-heap backed so it is safe to use from worker threads)
// -----------------------------------------------------------------------------

STRUCT(ByteBuf)
{
    LinearAllocator* Arena;
    u8*              Data;
    usize            Len;
    usize            Cap;
};

static void BB_InitCap(ByteBuf* b, LinearAllocator* Arena, usize Cap)
{
    if (Cap < 64)
    {
        Cap = 64;
    }

    b->Arena = Arena;
    b->Data  = LinearAllocator_Allocate(Arena, Cap);
    b->Len   = 0;
    b->Cap   = Cap;
}

static void BB_Ensure(ByteBuf* b, usize Extra)
{
    if (b->Len + Extra <= b->Cap)
    {
        return;
    }

    // Grow by bump-allocating a larger block and copying. Output buffers are
    // pre-sized to their worst case, so this is a rarely-taken safety net.
    usize NewCap = b->Cap ? b->Cap * 2 : 256;
    while (NewCap < b->Len + Extra)
    {
        NewCap *= 2;
    }

    u8* NewData = LinearAllocator_Allocate(b->Arena, NewCap);
    Platform_MemCopy(NewData, b->Data, b->Len);
    b->Data = NewData;
    b->Cap  = NewCap;
}

static void BB_PutChar(ByteBuf* b, u8 c)
{
    BB_Ensure(b, 1);
    b->Data[b->Len] = c;
    b->Len += 1;
}

static void BB_PutBytes(ByteBuf* b, const u8* s, usize Len)
{
    if (Len == 0)
    {
        return;
    }

    BB_Ensure(b, Len);
    Platform_MemCopy(b->Data + b->Len, s, Len);
    b->Len += Len;
}

static void BB_PutCStr(ByteBuf* b, const char* s)
{
    usize Len = 0;
    while (s[Len])
    {
        Len += 1;
    }

    BB_PutBytes(b, (const u8*)s, Len);
}

// -----------------------------------------------------------------------------
// Lexer helpers
// -----------------------------------------------------------------------------

static bool IsIdentStart(u8 c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_';
}

static bool IsIdentCont(u8 c)
{
    return IsIdentStart(c) || (c >= '0' && c <= '9');
}

static bool IsAlpha(u8 c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

static u32 IdentEnd(const u8* s, u32 n, u32 j)
{
    u32 k = j + 1;
    while (k < n && IsIdentCont(s[k]))
    {
        k += 1;
    }

    return k;
}

// View a byte span [s, s+Len) as a String (read-only) for comparisons.
static String Span(const u8* s, u32 Len)
{
    return StrSlice((uchar*)s, Len);
}

// `i` points at the opening quote (" or '). Returns the index just past the
// matching closing quote (or n if the literal runs off the end).
static u32 ScanQuoted(const u8* s, u32 n, u32 i)
{
    u8  Quote = s[i];
    u32 k = i + 1;

    while (k < n)
    {
        if (s[k] == '\\')
        {
            k += 2;
            continue;
        }
        if (s[k] == Quote)
        {
            k += 1;
            break;
        }
        k += 1;
    }

    return k;
}

// `i` points at "//". Returns the index of the terminating newline (or n).
static u32 ScanLineComment(const u8* s, u32 n, u32 i)
{
    u32 k = i + 2;
    while (k < n && s[k] != '\n')
    {
        k += 1;
    }

    return k;
}

// `i` points at the opening "/*". Returns the index just past "*/" (or n).
static u32 ScanBlockComment(const u8* s, u32 n, u32 i)
{
    u32 k = i + 2;
    while (k + 1 < n && !(s[k] == '*' && s[k + 1] == '/'))
    {
        k += 1;
    }

    if (k + 1 < n)
    {
        return k + 2;
    }

    return n;
}

// Skip whitespace and comments without emitting anything; returns the new index.
static u32 SkipTrivia(const u8* s, u32 n, u32 i)
{
    while (1)
    {
        if (i >= n)
        {
            return i;
        }

        u8 c = s[i];

        if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
        {
            i += 1;
            continue;
        }
        if (c == '/' && i + 1 < n && s[i + 1] == '/')
        {
            i = ScanLineComment(s, n, i);
            continue;
        }
        if (c == '/' && i + 1 < n && s[i + 1] == '*')
        {
            i = ScanBlockComment(s, n, i);
            continue;
        }

        return i;
    }
}

// -----------------------------------------------------------------------------
// Type inference
// -----------------------------------------------------------------------------

// Whether a (possibly pointer/array) type prints as a string.
ENUM(ECharClass)
{
    CharClass_None,    // not a character type
    CharClass_Narrow,  // char-like:  scalar %c, pointer/array %s
    CharClass_Wide     // wchar-like: scalar %lc, pointer/array %ls
};

STRUCT(TypeFlags)
{
    // C builtin type components (these combine: "unsigned long long" etc.)
    bool Void;
    bool Char;
    bool Int;
    bool Float;
    bool Double;
    bool Bool;
    bool Short;
    bool Signed;
    bool Unsigned;
    i32  LongCount;

    // A single-token fixed-width / typedef alias fully resolves the type.
    bool        HasDirect;
    const char* DirectSpec;
    ECharClass  CharClass;
};

// A fixed-width / typedef alias that maps directly to a format specifier.
// Specifiers for 64-bit / pointer-width types assume a 64-bit target, where
// they share a canonical type with `long long` and so compile warning-free.
STRUCT(TypeAlias)
{
    const char* Name;
    const char* Spec;
    ECharClass  CharClass;
};

static const TypeAlias g_TypeAliases[] =
{
    // --- Rift fixed-width aliases (EngineTypes.h) ---
    { "i8",    "%hhd", CharClass_None   },
    { "i16",   "%hd",  CharClass_None   },
    { "i32",   "%d",   CharClass_None   },
    { "i64",   "%lld", CharClass_None   },
    { "u8",    "%hhu", CharClass_None   },
    { "u16",   "%hu",  CharClass_None   },
    { "u32",   "%u",   CharClass_None   },
    { "u64",   "%llu", CharClass_None   },
    { "f32",   "%f",   CharClass_None   },
    { "f64",   "%f",   CharClass_None   },
    { "b8",    "%hhu", CharClass_None   },
    { "b16",   "%hu",  CharClass_None   },
    { "b32",   "%u",   CharClass_None   },
    { "b64",   "%llu", CharClass_None   },
    { "usize", "%llu", CharClass_None   },
    { "isize", "%lld", CharClass_None   },
    { "uptr",  "%llu", CharClass_None   },
    { "ulong", "%lu",  CharClass_None   },
    { "ilong", "%ld",  CharClass_None   },
    { "uchar", "%c",   CharClass_Narrow },
    { "wchar", "%lc",  CharClass_Wide   },

    // --- C <stdint.h> / <stddef.h> ---
    { "int8_t",    "%hhd", CharClass_None },
    { "int16_t",   "%hd",  CharClass_None },
    { "int32_t",   "%d",   CharClass_None },
    { "int64_t",   "%lld", CharClass_None },
    { "uint8_t",   "%hhu", CharClass_None },
    { "uint16_t",  "%hu",  CharClass_None },
    { "uint32_t",  "%u",   CharClass_None },
    { "uint64_t",  "%llu", CharClass_None },
    { "intptr_t",  "%lld", CharClass_None },
    { "uintptr_t", "%llu", CharClass_None },
    { "intmax_t",  "%lld", CharClass_None },
    { "uintmax_t", "%llu", CharClass_None },
    { "ptrdiff_t", "%lld", CharClass_None },
    { "size_t",    "%llu", CharClass_None },
    { "ssize_t",   "%lld", CharClass_None },
    { "rsize_t",   "%llu", CharClass_None },
    { "wchar_t",   "%lc",  CharClass_Wide },
    { "char16_t",  "%hu",  CharClass_None },
    { "char32_t",  "%u",   CharClass_None },

    // --- Rift String type (the one user type we resolve; %S is the Rift/LOG
    //     length-terminated String specifier) ---
    { "String",      "%S",   CharClass_None },
    { "StringLocal", "%S",   CharClass_None },
};

// Returns 1 if `Tok` is a type-component or qualifier keyword (updating the
// type flags accordingly), 0 otherwise.
static int ClassifyKeyword(String Tok, TypeFlags* f)
{
    // C builtin type components (combinable).
    if (String_IsEqual(Tok, S("void"), true))
    {
        f->Void = true;
        return 1;
    }
    if (String_IsEqual(Tok, S("char"), true))
    {
        f->Char = true;
        return 1;
    }
    if (String_IsEqual(Tok, S("int"), true))
    {
        f->Int = true;
        return 1;
    }
    if (String_IsEqual(Tok, S("float"), true))
    {
        f->Float = true;
        return 1;
    }
    if (String_IsEqual(Tok, S("double"), true))
    {
        f->Double = true;
        return 1;
    }
    if (String_IsEqual(Tok, S("short"), true))
    {
        f->Short = true;
        return 1;
    }
    if (String_IsEqual(Tok, S("long"), true))
    {
        f->LongCount += 1;
        return 1;
    }
    if (String_IsEqual(Tok, S("signed"), true))
    {
        f->Signed = true;
        return 1;
    }
    if (String_IsEqual(Tok, S("unsigned"), true))
    {
        f->Unsigned = true;
        return 1;
    }
    if (String_IsEqual(Tok, S("_Bool"), true) || String_IsEqual(Tok, S("bool"), true))
    {
        f->Bool = true;
        return 1;
    }

    // Storage-class / qualifier keywords: allowed in a declaration, no type info.
    if (String_IsEqual(Tok, S("const"), true)    ||
        String_IsEqual(Tok, S("volatile"), true) ||
        String_IsEqual(Tok, S("static"), true)   ||
        String_IsEqual(Tok, S("register"), true) ||
        String_IsEqual(Tok, S("auto"), true)     ||
        String_IsEqual(Tok, S("extern"), true)   ||
        String_IsEqual(Tok, S("restrict"), true) ||
        String_IsEqual(Tok, S("inline"), true)   ||
        String_IsEqual(Tok, S("_Atomic"), true))
    {
        return 1;
    }

    // Fixed-width / typedef aliases (single token, fully resolved).
    for (u32 a = 0; a < SArray_Capacity(g_TypeAliases); a++)
    {
        if (String_IsEqual(Tok, CStr(g_TypeAliases[a].Name), true))
        {
            f->HasDirect  = true;
            f->DirectSpec = g_TypeAliases[a].Spec;
            f->CharClass  = g_TypeAliases[a].CharClass;
            return 1;
        }
    }

    return 0;
}

static bool SpanIsPtrQualifier(String Tok)
{
    return String_IsEqual(Tok, S("const"), true)    ||
           String_IsEqual(Tok, S("volatile"), true) ||
           String_IsEqual(Tok, S("restrict"), true) ||
           String_IsEqual(Tok, S("_Atomic"), true);
}

// Resolve the gathered type plus pointer/array decoration to a printf specifier,
// or NULL if the type is not a printable value (e.g. non-pointer void).
static const char* ComputeSpec(const TypeFlags* f, i32 Pointer, bool IsArray)
{
    ECharClass  CharCls = f->CharClass;
    const char* Scalar  = NULL;

    if (f->HasDirect)
    {
        Scalar = f->DirectSpec;
    }
    else
    {
        bool CharLike = f->Char && !f->Int && !f->Short && f->LongCount == 0 &&
                        !f->Float && !f->Double && !f->Bool && !f->Void;
        if (CharLike)
        {
            CharCls = CharClass_Narrow;
        }

        if (f->Void)
        {
            Scalar = NULL;
        }
        else if (f->Double)
        {
            Scalar = (f->LongCount >= 1) ? "%Lf" : "%f";
        }
        else if (f->Float)
        {
            Scalar = "%f";
        }
        else if (f->Char)
        {
            Scalar = "%c";
        }
        else if (f->Bool)
        {
            Scalar = "%d";
        }
        else if (f->Unsigned)
        {
            if (f->LongCount >= 2)
            {
                Scalar = "%llu";
            }
            else if (f->LongCount == 1)
            {
                Scalar = "%lu";
            }
            else if (f->Short)
            {
                Scalar = "%hu";
            }
            else
            {
                Scalar = "%u";
            }
        }
        else
        {
            if (f->LongCount >= 2)
            {
                Scalar = "%lld";
            }
            else if (f->LongCount == 1)
            {
                Scalar = "%ld";
            }
            else if (f->Short)
            {
                Scalar = "%hd";
            }
            else
            {
                Scalar = "%d";
            }
        }
    }

    if (Pointer >= 1 || IsArray)
    {
        // char* / char[] print as %s, but char** / char* x[] are just pointers.
        bool SingleIndirection = (Pointer <= 1) && !(Pointer == 1 && IsArray);
        if (CharCls == CharClass_Narrow && SingleIndirection)
        {
            return "%s";
        }
        if (CharCls == CharClass_Wide && SingleIndirection)
        {
            return "%ls";
        }
        return "%p";
    }

    return Scalar;
}

static bool TypeFlags_SawCoreType(const TypeFlags* f)
{
    return f->HasDirect || f->Void || f->Char || f->Int || f->Float ||
           f->Double || f->Bool || f->Short || (f->LongCount > 0) ||
           f->Signed || f->Unsigned;
}

// -----------------------------------------------------------------------------
// Declaration table (scoped by brace depth)
// -----------------------------------------------------------------------------

STRUCT(Decl)
{
    const u8*   Name;
    u32         NameLen;
    const char* Spec;
    i32         Depth;
};

STRUCT(DeclList)
{
    LinearAllocator* Arena;
    Decl*            Items;
    u32              Count;
    u32              Cap;
};

static void DeclList_InitCap(DeclList* d, LinearAllocator* Arena, u32 Cap)
{
    if (Cap < 16)
    {
        Cap = 16;
    }

    d->Arena = Arena;
    d->Items = LinearAllocator_Allocate(Arena, Cap * sizeof(Decl));
    d->Count = 0;
    d->Cap   = Cap;
}

static void DeclList_Push(DeclList* d, const u8* Name, u32 NameLen, const char* Spec, i32 Depth)
{
    if (d->Count >= d->Cap)
    {
        // Grow by bump-allocating a larger array and copying. The list is
        // pre-sized to the worst-case declaration count, so this is a safety net.
        u32   NewCap  = d->Cap ? d->Cap * 2 : 64;
        Decl* NewItems = LinearAllocator_Allocate(d->Arena, NewCap * sizeof(Decl));
        Platform_MemCopy(NewItems, d->Items, d->Count * sizeof(Decl));
        d->Items = NewItems;
        d->Cap   = NewCap;
    }

    d->Items[d->Count].Name    = Name;
    d->Items[d->Count].NameLen = NameLen;
    d->Items[d->Count].Spec    = Spec;
    d->Items[d->Count].Depth   = Depth;
    d->Count += 1;
}

// Most-recently-declared (innermost) match wins, so shadowing works.
static const char* FindDecl(const DeclList* d, const u8* Name, u32 NameLen, i32 Depth)
{
    for (i32 x = (i32)d->Count - 1; x >= 0; x--)
    {
        if (d->Items[x].Depth <= Depth &&
            d->Items[x].NameLen == NameLen &&
            Platform_MemEqual(d->Items[x].Name, Name, NameLen))
        {
            return d->Items[x].Spec;
        }
    }

    return NULL;
}

// Skip a declarator initializer (after '='). Stops *at* a top-level ',', ';',
// ')' or '}' while respecting nested brackets, strings, chars and comments.
static u32 SkipInitializer(const u8* s, u32 n, u32 i)
{
    i32 Depth = 0;

    while (i < n)
    {
        u8 c = s[i];

        if (c == '/' && i + 1 < n && s[i + 1] == '/')
        {
            i = ScanLineComment(s, n, i);
            continue;
        }
        if (c == '/' && i + 1 < n && s[i + 1] == '*')
        {
            i = ScanBlockComment(s, n, i);
            continue;
        }
        if (c == '"' || c == '\'')
        {
            i = ScanQuoted(s, n, i);
            continue;
        }

        if (Depth == 0 && (c == ',' || c == ';' || c == ')' || c == '}'))
        {
            return i;
        }

        if (c == '(' || c == '[' || c == '{')
        {
            Depth += 1;
        }
        else if (c == ')' || c == ']' || c == '}')
        {
            if (Depth > 0)
            {
                Depth -= 1;
            }
        }

        i += 1;
    }

    return i;
}

// Scan a declaration starting at `Start` (which begins with a type keyword),
// recording every declared variable into `Decls`. Does not emit anything; the
// caller copies the consumed range [Start, return) verbatim. On no recognised
// declarator, returns Start and sets *OutRecognized = false.
static u32 ScanDeclaration(const u8* s, u32 n, u32 Start, i32 Depth, DeclList* Decls, bool* OutRecognized)
{
    *OutRecognized = false;

    TypeFlags f = {0};
    u32 i = Start;

    // Gather the run of leading type / qualifier keywords.
    while (1)
    {
        u32 j = SkipTrivia(s, n, i);
        if (j >= n)
        {
            i = j;
            break;
        }

        if (IsIdentStart(s[j]))
        {
            u32 e = IdentEnd(s, n, j);
            if (ClassifyKeyword(Span(s + j, e - j), &f))
            {
                i = e;
                continue;
            }

            i = j; // a non-keyword identifier: start of the declarator
            break;
        }

        i = j; // '*', '(' etc: start of the declarator
        break;
    }

    if (!TypeFlags_SawCoreType(&f))
    {
        return Start;
    }

    bool RecordedAny = false;
    u32  LastEnd = i;

    while (1)
    {
        u32 p = SkipTrivia(s, n, i);

        i32 Pointer = 0;
        while (p < n && s[p] == '*')
        {
            Pointer += 1;
            p = SkipTrivia(s, n, p + 1);
        }

        // Qualifiers between '*' and the name (e.g. `char * const p`).
        while (p < n && IsIdentStart(s[p]))
        {
            u32 e = IdentEnd(s, n, p);
            if (!SpanIsPtrQualifier(Span(s + p, e - p)))
            {
                break;
            }
            p = SkipTrivia(s, n, e);
        }

        if (!(p < n && IsIdentStart(s[p])))
        {
            break; // no declarator name (function pointer / prototype)
        }

        u32 NameStart = p;
        u32 NameEnd   = IdentEnd(s, n, p);
        u32 After     = SkipTrivia(s, n, NameEnd);

        if (After < n && s[After] == '(')
        {
            break; // function declarator, not a variable
        }

        bool IsArray = false;
        u32  q = After;
        while (q < n && s[q] == '[')
        {
            IsArray = true;

            i32 Brackets = 0;
            while (q < n)
            {
                u8 c = s[q];
                q += 1;
                if (c == '[')
                {
                    Brackets += 1;
                }
                else if (c == ']')
                {
                    Brackets -= 1;
                    if (Brackets == 0)
                    {
                        break;
                    }
                }
            }

            q = SkipTrivia(s, n, q);
        }

        if (q < n && s[q] == '=')
        {
            q = SkipInitializer(s, n, q + 1);
        }

        const char* Spec = ComputeSpec(&f, Pointer, IsArray);
        if (Spec)
        {
            DeclList_Push(Decls, s + NameStart, NameEnd - NameStart, Spec, Depth);
            RecordedAny = true;
        }
        LastEnd = q;

        u32 r = SkipTrivia(s, n, q);
        if (!(r < n && s[r] == ','))
        {
            break;
        }

        // Peek the token after the comma: a type keyword means a new declaration
        // (e.g. the next function parameter) rather than another declarator
        // sharing this base type.
        u32 t = SkipTrivia(s, n, r + 1);
        if (t < n && IsIdentStart(s[t]))
        {
            u32       te  = IdentEnd(s, n, t);
            TypeFlags Tmp = {0};
            if (ClassifyKeyword(Span(s + t, te - t), &Tmp))
            {
                break;
            }
        }

        i = r + 1; // consume comma, parse the next declarator
    }

    if (RecordedAny)
    {
        *OutRecognized = true;
        return LastEnd;
    }

    return Start;
}

// -----------------------------------------------------------------------------
// printf-like call rewriting
// -----------------------------------------------------------------------------

// A printf-like function whose calls we rewrite. `FormatArgIndex` is the
// zero-based argument position that holds the format string -- printf and the
// LOG_* family put it first, while String_Format/String_AppendF/LOG_DEBUG_T put
// it second (after a destination / log-type argument). Extend support to a new
// printf-like function by adding a row here.
STRUCT(PrintfLike)
{
    const char* Name;
    u32         FormatArgIndex;
};

static const PrintfLike g_PrintfLike[] =
{
    { "printf",             0 },
    { "LOG",                0 },
    { "LOG_INFO",           0 },
    { "LOG_WARNING",        0 },
    { "LOG_SUCCESS",        0 },
    { "LOG_ERROR",          0 },
    { "LOG_FATAL",          0 },
    { "LOG_MUTE",           0 },
    { "LOG_DEBUG",          0 },
    { "LOG_INLINE_INFO",    0 },
    { "LOG_INLINE_WARNING", 0 },
    { "LOG_INLINE_SUCCESS", 0 },
    { "LOG_INLINE_ERROR",   0 },
    { "LOG_INLINE_FATAL",   0 },
    { "LOG_DEBUG_T",        1 },
    { "String_Format",      1 },
    { "String_AppendF",     1 },
};

static bool LookupPrintfLike(String Tok, u32* OutFormatArgIndex)
{
    for (u32 i = 0; i < SArray_Capacity(g_PrintfLike); i++)
    {
        if (String_IsEqual(Tok, CStr(g_PrintfLike[i].Name), true))
        {
            *OutFormatArgIndex = g_PrintfLike[i].FormatArgIndex;
            return true;
        }
    }

    return false;
}

// Copy the remainder of a call (existing extra args + the closing ')') verbatim.
// Paren depth starts at 1 because the opening '(' has already been emitted.
static u32 CopyCallRemainder(const u8* s, u32 n, u32 i, ByteBuf* Out)
{
    i32 Depth = 1;

    while (i < n)
    {
        u8 c = s[i];

        if (c == '/' && i + 1 < n && s[i + 1] == '/')
        {
            u32 k = ScanLineComment(s, n, i);
            BB_PutBytes(Out, s + i, k - i);
            i = k;
            continue;
        }
        if (c == '/' && i + 1 < n && s[i + 1] == '*')
        {
            u32 k = ScanBlockComment(s, n, i);
            BB_PutBytes(Out, s + i, k - i);
            i = k;
            continue;
        }
        if (c == '"' || c == '\'')
        {
            u32 k = ScanQuoted(s, n, i);
            BB_PutBytes(Out, s + i, k - i);
            i = k;
            continue;
        }

        if (c == '(')
        {
            Depth += 1;
        }
        else if (c == ')')
        {
            Depth -= 1;
            BB_PutChar(Out, c);
            i += 1;
            if (Depth == 0)
            {
                return i;
            }
            continue;
        }

        BB_PutChar(Out, c);
        i += 1;
    }

    return i;
}

// Within a call argument, find the first string literal (the format string).
// Returns its index, or n if the argument ends (top-level ',' or the call's
// closing ')') before any literal is seen. *OutLevel receives the bracket
// nesting at the literal (e.g. 1 for the literal inside `S("...")`).
static u32 FindFormatLiteral(const u8* s, u32 n, u32 i, i32* OutLevel)
{
    i32 Level = 0;

    while (i < n)
    {
        u8 c = s[i];

        if (c == '/' && i + 1 < n && s[i + 1] == '/')
        {
            i = ScanLineComment(s, n, i);
            continue;
        }
        if (c == '/' && i + 1 < n && s[i + 1] == '*')
        {
            i = ScanBlockComment(s, n, i);
            continue;
        }
        if (c == '"')
        {
            *OutLevel = Level;
            return i;
        }
        if (c == '\'')
        {
            i = ScanQuoted(s, n, i);
            continue;
        }
        if (c == '(' || c == '[' || c == '{')
        {
            Level += 1;
            i += 1;
            continue;
        }
        if (c == ')' || c == ']' || c == '}')
        {
            if (Level == 0)
            {
                return n; // call/argument closed before a literal
            }
            Level -= 1;
            i += 1;
            continue;
        }
        if (c == ',' && Level == 0)
        {
            return n; // argument boundary before a literal
        }

        i += 1;
    }

    return n;
}

// Copy verbatim from `i` to the end of the current argument: a ',' at the given
// bracket level, or the call's closing ')'. `Level` is the bracket nesting at
// `i`. Returns the boundary index (not consumed).
static u32 CopyToArgEnd(const u8* s, u32 n, u32 i, i32 Level, ByteBuf* Out)
{
    while (i < n)
    {
        u8 c = s[i];

        if (c == '/' && i + 1 < n && s[i + 1] == '/')
        {
            u32 k = ScanLineComment(s, n, i);
            BB_PutBytes(Out, s + i, k - i);
            i = k;
            continue;
        }
        if (c == '/' && i + 1 < n && s[i + 1] == '*')
        {
            u32 k = ScanBlockComment(s, n, i);
            BB_PutBytes(Out, s + i, k - i);
            i = k;
            continue;
        }
        if (c == '"' || c == '\'')
        {
            u32 k = ScanQuoted(s, n, i);
            BB_PutBytes(Out, s + i, k - i);
            i = k;
            continue;
        }
        if (Level == 0 && c == ',')
        {
            return i;
        }
        if (c == '(' || c == '[' || c == '{')
        {
            Level += 1;
            BB_PutChar(Out, c);
            i += 1;
            continue;
        }
        if (c == ')' || c == ']' || c == '}')
        {
            if (Level == 0)
            {
                return i; // call-closing ')'
            }
            Level -= 1;
            BB_PutChar(Out, c);
            i += 1;
            continue;
        }

        BB_PutChar(Out, c);
        i += 1;
    }

    return i;
}

// One argument position implied by the format string, in left-to-right order.
// Either a `{name}` interpolation (its arg is the inlined variable) or a
// pre-existing `%` conversion spec (its arg(s) come from the call's existing
// trailing arguments). The final argument list is rebuilt from these in order,
// which is what keeps a mixed format like "%S {blah}" correctly ordered.
STRUCT(ArgSlot)
{
    bool      IsInterp;    // true: interpolation; false: pre-existing % spec
    const u8* Name;        // interpolation variable name (IsInterp)
    u32       NameLen;
    u32       ManualArgs;  // existing args this % spec consumes (1 + '*' count)
};

// Maximum number of conversion directives (interpolations + % specs) handled in
// a single call. A format with more than this is left partially untransformed.
#define MAX_CALL_DIRECTIVES 256

// From `i` (start of a call argument), return the index of the top-level ',' or
// ')' that ends it (nested brackets, strings and comments are skipped). Does not
// emit anything.
static u32 ScanArgEnd(const u8* s, u32 n, u32 i)
{
    i32 Level = 0;

    while (i < n)
    {
        u8 c = s[i];

        if (c == '/' && i + 1 < n && s[i + 1] == '/')
        {
            i = ScanLineComment(s, n, i);
            continue;
        }
        if (c == '/' && i + 1 < n && s[i + 1] == '*')
        {
            i = ScanBlockComment(s, n, i);
            continue;
        }
        if (c == '"' || c == '\'')
        {
            i = ScanQuoted(s, n, i);
            continue;
        }
        if (Level == 0 && c == ',')
        {
            return i;
        }
        if (c == '(' || c == '[' || c == '{')
        {
            Level += 1;
            i += 1;
            continue;
        }
        if (c == ')' || c == ']' || c == '}')
        {
            if (Level == 0)
            {
                return i;
            }
            Level -= 1;
            i += 1;
            continue;
        }

        i += 1;
    }

    return i;
}

// Rewrite a format string literal (s[i] == '"'), emitting the new literal
// directly to `Out`. Records one `ArgSlot` per conversion directive (resolved
// `{name}` interpolation or pre-existing `%` spec) in left-to-right order.
// Returns the index just past the closing quote.
static u32 TransformLiteral(const u8* s, u32 n, u32 i, i32 Depth, const DeclList* Decls,
                            ByteBuf* Out, ArgSlot* Slots, u32* SlotCount, u32 SlotCap,
                            bool* OutFoundInterp, u32* OutWarnings)
{
    u32 ContentStart = i + 1;
    u32 p = ContentStart;
    while (p < n)
    {
        if (s[p] == '\\')
        {
            p += 2;
            continue;
        }
        if (s[p] == '"')
        {
            break;
        }
        p += 1;
    }
    u32 ContentEnd = (p < n) ? p : n; // index of the closing quote (or EOF)

    bool FoundInterp = false;
    u32  Warnings    = 0;

    BB_PutChar(Out, '"');

    u32 k = ContentStart;
    while (k < ContentEnd)
    {
        u8 c = s[k];

        if (c == '\\' && k + 1 < ContentEnd)
        {
            BB_PutChar(Out, c);
            BB_PutChar(Out, s[k + 1]);
            k += 2;
            continue;
        }
        if (c == '{' && k + 1 < ContentEnd && s[k + 1] == '{')
        {
            BB_PutChar(Out, '{');
            k += 2;
            continue;
        }
        if (c == '}' && k + 1 < ContentEnd && s[k + 1] == '}')
        {
            BB_PutChar(Out, '}');
            k += 2;
            continue;
        }

        // A pre-existing conversion spec (e.g. "%S" left by a previous run, or a
        // hand-written one). It consumes one existing trailing arg, plus one per
        // '*' (dynamic width/precision). Record a Manual slot to keep ordering.
        if (c == '%')
        {
            if (k + 1 < ContentEnd && s[k + 1] == '%')
            {
                BB_PutChar(Out, '%');
                BB_PutChar(Out, '%');
                k += 2;
                continue;
            }

            BB_PutChar(Out, '%');
            k += 1;

            u32 Stars = 0;
            while (k < ContentEnd && !IsAlpha(s[k]))
            {
                if (s[k] == '*')
                {
                    Stars += 1;
                }
                BB_PutChar(Out, s[k]);
                k += 1;
            }
            if (k < ContentEnd)
            {
                BB_PutChar(Out, s[k]); // the conversion (or length) character
                k += 1;
            }

            if (*SlotCount < SlotCap)
            {
                Slots[*SlotCount].IsInterp   = false;
                Slots[*SlotCount].Name       = NULL;
                Slots[*SlotCount].NameLen    = 0;
                Slots[*SlotCount].ManualArgs = 1 + Stars;
                *SlotCount += 1;
            }
            continue;
        }

        if (c != '{')
        {
            BB_PutChar(Out, c);
            k += 1;
            continue;
        }

        // Interpolation token: find the closing brace.
        u32 Close = k + 1;
        while (Close < ContentEnd && s[Close] != '}')
        {
            Close += 1;
        }
        if (Close >= ContentEnd)
        {
            BB_PutChar(Out, '{'); // unterminated: keep literally
            k += 1;
            continue;
        }

        // Trim whitespace inside the braces.
        u32 a = k + 1;
        u32 b = Close;
        while (a < b && (s[a] == ' ' || s[a] == '\t'))
        {
            a += 1;
        }
        while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t'))
        {
            b -= 1;
        }

        bool Valid = (b > a) && IsIdentStart(s[a]);
        if (Valid)
        {
            for (u32 x = a + 1; x < b; x++)
            {
                if (!IsIdentCont(s[x]))
                {
                    Valid = false;
                    break;
                }
            }
        }

        const char* Spec = Valid ? FindDecl(Decls, s + a, b - a, Depth) : NULL;
        if (Spec && *SlotCount < SlotCap)
        {
            FoundInterp = true;
            BB_PutCStr(Out, Spec);
            Slots[*SlotCount].IsInterp   = true;
            Slots[*SlotCount].Name       = s + a;
            Slots[*SlotCount].NameLen    = b - a;
            Slots[*SlotCount].ManualArgs = 0;
            *SlotCount += 1;
        }
        else
        {
            // Unresolved type, not an identifier, or no room for another slot:
            // keep the token verbatim so the file still compiles.
            if (Valid)
            {
                Warnings += 1;
            }
            BB_PutBytes(Out, s + k, Close - k + 1);
        }

        k = Close + 1;
    }

    BB_PutChar(Out, '"');

    *OutFoundInterp = FoundInterp;
    *OutWarnings    = Warnings;
    return (ContentEnd < n) ? ContentEnd + 1 : n;
}

// Handle a printf-like call beginning at `NameStart` (name length `NameLen`),
// whose format string is the `FormatArgIndex`-th argument. Emits the (possibly
// transformed) call to `Out` and returns the index just past it.
static u32 HandleInterpolatedCall(const u8* s, u32 n, u32 NameStart, u32 NameLen, u32 FormatArgIndex,
                                  i32 Depth, const DeclList* Decls, ByteBuf* Out,
                                  u32* OutTransformed, u32* OutWarnings)
{
    u32 i = NameStart;
    BB_PutBytes(Out, s + i, NameLen);
    i += NameLen;

    // Copy trivia up to the '('.
    u32 k = SkipTrivia(s, n, i);
    BB_PutBytes(Out, s + i, k - i);
    i = k;

    if (!(i < n && s[i] == '('))
    {
        return i;
    }
    BB_PutChar(Out, '(');
    i += 1;

    // Copy the arguments preceding the format string, comma-separated. These are
    // fixed leading parameters (e.g. a destination or log type), not format args.
    for (u32 ArgIdx = 0; ArgIdx < FormatArgIndex; ArgIdx++)
    {
        u32 b = CopyToArgEnd(s, n, i, 0, Out);
        i = b;
        if (i >= n || s[i] == ')')
        {
            return CopyCallRemainder(s, n, i, Out); // too few args: nothing to interpolate
        }
        BB_PutChar(Out, ','); // s[i] == ','
        i += 1;
    }

    // Copy the format argument's leading trivia.
    k = SkipTrivia(s, n, i);
    BB_PutBytes(Out, s + i, k - i);
    i = k;

    // Locate the format string literal (possibly wrapped, e.g. S("...")).
    i32 LevelAtLit = 0;
    u32 L = FindFormatLiteral(s, n, i, &LevelAtLit);
    if (L >= n)
    {
        return CopyCallRemainder(s, n, i, Out); // no literal: nothing to interpolate
    }

    BB_PutBytes(Out, s + i, L - i); // copy any prefix before the literal (e.g. "S(")
    i = L;

    ArgSlot Slots[MAX_CALL_DIRECTIVES];
    u32     SlotCount = 0;

    bool FoundInterp = false;
    u32  Warnings    = 0;
    i = TransformLiteral(s, n, i, Depth, Decls, Out, Slots, &SlotCount, MAX_CALL_DIRECTIVES, &FoundInterp, &Warnings);

    // Copy any suffix to the end of the format argument (e.g. the ')' of S(...)).
    u32 B = CopyToArgEnd(s, n, i, LevelAtLit, Out);

    *OutWarnings += Warnings;

    if (!FoundInterp)
    {
        // No interpolation took place: leave the existing argument list untouched.
        return CopyCallRemainder(s, n, B, Out);
    }
    *OutTransformed += 1;

    // Collect the call's existing trailing arguments (those after the format
    // string), in order. `B` is at the ',' before them, or at the closing ')'.
    const u8* ExistingPtr[MAX_CALL_DIRECTIVES];
    u32       ExistingLen[MAX_CALL_DIRECTIVES];
    u32       ExistingCount = 0;

    u32 j = B;
    while (j < n && s[j] == ',')
    {
        j += 1; // skip the comma
        u32 ArgStart = SkipTrivia(s, n, j);
        u32 ArgEnd   = ScanArgEnd(s, n, ArgStart);

        // Trim trailing whitespace from the argument slice.
        u32 Trimmed = ArgEnd;
        while (Trimmed > ArgStart &&
               (s[Trimmed - 1] == ' '  || s[Trimmed - 1] == '\t' ||
                s[Trimmed - 1] == '\r' || s[Trimmed - 1] == '\n'))
        {
            Trimmed -= 1;
        }

        if (ExistingCount < MAX_CALL_DIRECTIVES)
        {
            ExistingPtr[ExistingCount] = s + ArgStart;
            ExistingLen[ExistingCount] = Trimmed - ArgStart;
            ExistingCount += 1;
        }
        j = ArgEnd;
    }
    u32 CloseParen = j; // s[j] is the call's ')' (or EOF if malformed)

    // Rebuild the argument list in conversion-directive order. A pre-existing %
    // spec pulls the next unused existing arg; an interpolation emits its name,
    // but if that variable is already supplied as an existing arg it consumes
    // that one instead of adding a duplicate (the in-place edit case where a
    // hand-written %d's arg is later rewritten as {name}).
    bool ExistingUsed[MAX_CALL_DIRECTIVES] = {0};

    for (u32 sI = 0; sI < SlotCount; sI++)
    {
        if (Slots[sI].IsInterp)
        {
            for (u32 e = 0; e < ExistingCount; e++)
            {
                if (!ExistingUsed[e] &&
                    ExistingLen[e] == Slots[sI].NameLen &&
                    Platform_MemEqual(ExistingPtr[e], Slots[sI].Name, Slots[sI].NameLen))
                {
                    ExistingUsed[e] = true; // already passed; don't duplicate it
                    break;
                }
            }

            BB_PutCStr(Out, ", ");
            BB_PutBytes(Out, Slots[sI].Name, Slots[sI].NameLen);
        }
        else
        {
            for (u32 c = 0; c < Slots[sI].ManualArgs; c++)
            {
                u32 e = 0;
                while (e < ExistingCount && ExistingUsed[e])
                {
                    e += 1;
                }
                if (e >= ExistingCount)
                {
                    break;
                }
                ExistingUsed[e] = true;
                BB_PutCStr(Out, ", ");
                BB_PutBytes(Out, ExistingPtr[e], ExistingLen[e]);
            }
        }
    }

    // Preserve any existing args not matched by a spec or interpolation.
    for (u32 e = 0; e < ExistingCount; e++)
    {
        if (!ExistingUsed[e])
        {
            BB_PutCStr(Out, ", ");
            BB_PutBytes(Out, ExistingPtr[e], ExistingLen[e]);
        }
    }

    if (CloseParen < n && s[CloseParen] == ')')
    {
        BB_PutChar(Out, ')');
        return CloseParen + 1;
    }
    return CloseParen;
}

// -----------------------------------------------------------------------------
// Whole-file transform: single forward pass that tracks scope and rewrites.
// -----------------------------------------------------------------------------

static void TransformBuffer(const u8* s, u32 n, ByteBuf* Out, DeclList* Decls, u32* OutTransformed, u32* OutWarnings)
{
    i32  Brace   = 0;
    i32  Paren   = 0;
    bool DeclPos = true; // are we at a position where a declaration may begin?
    u32  i = 0;

    while (i < n)
    {
        u8 c = s[i];

        if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
        {
            BB_PutChar(Out, c);
            i += 1;
            continue;
        }

        if (c == '/' && i + 1 < n && s[i + 1] == '/')
        {
            u32 k = ScanLineComment(s, n, i);
            BB_PutBytes(Out, s + i, k - i);
            i = k;
            continue;
        }
        if (c == '/' && i + 1 < n && s[i + 1] == '*')
        {
            u32 k = ScanBlockComment(s, n, i);
            BB_PutBytes(Out, s + i, k - i);
            i = k;
            continue;
        }
        if (c == '"' || c == '\'')
        {
            u32 k = ScanQuoted(s, n, i);
            BB_PutBytes(Out, s + i, k - i);
            i = k;
            DeclPos = false;
            continue;
        }

        // Preprocessor directive: consume the whole logical line (honouring
        // backslash line-continuations) verbatim. A directive ends a statement,
        // so a declaration may follow it (e.g. a global right after #include).
        if (c == '#')
        {
            u32 k = i;
            while (1)
            {
                while (k < n && s[k] != '\n')
                {
                    k += 1;
                }

                bool Continued = false;
                if (k > i)
                {
                    u32 q = k - 1;
                    if (q > i && s[q] == '\r')
                    {
                        q -= 1;
                    }
                    if (s[q] == '\\')
                    {
                        Continued = true;
                    }
                }

                if (Continued && k < n)
                {
                    k += 1; // skip the '\n' and keep consuming the next line
                    continue;
                }
                break;
            }

            BB_PutBytes(Out, s + i, k - i);
            i = k;
            DeclPos = true;
            continue;
        }

        if (c == '{')
        {
            BB_PutChar(Out, c);
            i += 1;
            Brace += 1;
            DeclPos = true;
            continue;
        }
        if (c == '}')
        {
            BB_PutChar(Out, c);
            i += 1;
            Brace -= 1;
            while (Decls->Count > 0 && Decls->Items[Decls->Count - 1].Depth > Brace)
            {
                Decls->Count -= 1;
            }
            DeclPos = true;
            continue;
        }
        if (c == ';')
        {
            BB_PutChar(Out, c);
            i += 1;
            DeclPos = true;
            continue;
        }
        if (c == '(')
        {
            BB_PutChar(Out, c);
            i += 1;
            Paren += 1;
            DeclPos = true;
            continue;
        }
        if (c == ')')
        {
            BB_PutChar(Out, c);
            i += 1;
            if (Paren > 0)
            {
                Paren -= 1;
            }
            DeclPos = false;
            continue;
        }
        if (c == ',')
        {
            BB_PutChar(Out, c);
            i += 1;
            DeclPos = true;
            continue;
        }

        if (IsIdentStart(c))
        {
            u32 e = IdentEnd(s, n, i);

            u32 FormatArgIndex = 0;
            if (LookupPrintfLike(Span(s + i, e - i), &FormatArgIndex))
            {
                u32 t = SkipTrivia(s, n, e);
                if (t < n && s[t] == '(')
                {
                    i = HandleInterpolatedCall(s, n, i, e - i, FormatArgIndex, Brace, Decls, Out, OutTransformed, OutWarnings);
                    DeclPos = false;
                    continue;
                }

                BB_PutBytes(Out, s + i, e - i);
                i = e;
                DeclPos = false;
                continue;
            }

            if (DeclPos)
            {
                TypeFlags Probe = {0};
                if (ClassifyKeyword(Span(s + i, e - i), &Probe))
                {
                    i32  RecDepth   = Brace + (Paren > 0 ? 1 : 0);
                    bool Recognized = false;
                    u32  End = ScanDeclaration(s, n, i, RecDepth, Decls, &Recognized);
                    if (Recognized && End > i)
                    {
                        BB_PutBytes(Out, s + i, End - i);
                        i = End;
                        DeclPos = false;
                        continue;
                    }
                }
            }

            BB_PutBytes(Out, s + i, e - i);
            i = e;
            DeclPos = false;
            continue;
        }

        BB_PutChar(Out, c);
        i += 1;
        DeclPos = false;
    }
}

// -----------------------------------------------------------------------------
// File IO
// -----------------------------------------------------------------------------

static bool WriteWholeFile(String Path, const u8* Data, usize Size)
{
    FileHandle Handle;
    if (!Filesystem_Open(Path, FileMode_Write, &Handle)) // CREATE_ALWAYS truncates
    {
        return false;
    }

    bool ok = true;
    if (Size > 0)
    {
        usize Written = 0;
        ok = Filesystem_Write(Handle, Size, Data, &Written) && (Written == Size);
    }

    Filesystem_Close(&Handle);
    return ok;
}

// -----------------------------------------------------------------------------
// Per-file task + worker dispatch
// -----------------------------------------------------------------------------

ENUM(EFileResult)
{
    FileResult_Pending = 0,
    FileResult_Modified,
    FileResult_Unchanged,
    FileResult_ReadError,
    FileResult_WriteError
};

STRUCT(FileTask)
{
    String      Path;     // input path
    String      OutPath;  // path actually written (== Path when in-place)
    EFileResult Result;
    u32         NumTransformed;
    u32         NumWarnings;
};

// Build a sibling output path by inserting `Suffix` before the file extension,
// e.g. "Tests/sample1.c" + ".gen" -> "Tests/sample1.gen.c". The returned String
// is allocated (NUL-terminated) from `Arena`.
static String BuildOutputPath(LinearAllocator* Arena, String Path, const char* Suffix)
{
    u32  Sep    = 0;
    bool HasSep = false;
    for (u32 i = 0; i < Path.Length; i++)
    {
        if (Path.Data[i] == '/' || Path.Data[i] == '\\')
        {
            Sep = i;
            HasSep = true;
        }
    }

    u32  Dot    = Path.Length;
    bool HasDot = false;
    for (u32 i = (HasSep ? Sep + 1 : 0); i < Path.Length; i++)
    {
        if (Path.Data[i] == '.')
        {
            Dot = i;
            HasDot = true;
        }
    }

    usize SuffixLen = 0;
    while (Suffix[SuffixLen])
    {
        SuffixLen += 1;
    }

    u8*   Buffer = LinearAllocator_Allocate(Arena, Path.Length + SuffixLen + 1);
    usize w = 0;

    Platform_MemCopy(Buffer + w, Path.Data, Dot);
    w += Dot;
    Platform_MemCopy(Buffer + w, Suffix, SuffixLen);
    w += SuffixLen;
    if (HasDot)
    {
        Platform_MemCopy(Buffer + w, Path.Data + Dot, Path.Length - Dot);
        w += Path.Length - Dot;
    }
    Buffer[w] = 0;

    String Result;
    Result.Data     = Buffer;
    Result.Length   = (u32)w;
    Result.Capacity = (u32)w;
    return Result;
}

// Worst-case bytes a single file of `FileSize` needs from its arena: a copy of
// the input, the rewritten output (which expands by < 3x), the declaration
// table (at most one declaration per two source bytes), plus slack.
static usize ComputeArenaSize(usize FileSize)
{
    usize Input  = FileSize + 1;
    usize Output = FileSize * 3 + 256;
    usize Decls  = (FileSize / 2 + 64) * sizeof(Decl);
    return Input + Output + Decls + 1024;
}

// Ensure the linear allocator has at least `Needed` bytes of backing and reset
// its bump offset for reuse. The backing (the allocator's own memory) is the
// only OS allocation, grown only when a larger file requires it. Returns false
// on OOM. A zero-initialised allocator starts with no backing.
static bool Arena_EnsureAndReset(LinearAllocator* Arena, usize Needed)
{
    if (Needed > Arena->TotalSize)
    {
        if (Arena->Memory)
        {
            Platform_MemFree(Arena->Memory);
        }

        usize NewSize = Needed + Needed / 2; // headroom to avoid frequent regrows
        void* Backing = Platform_MemAlloc(NewSize);
        if (!Backing)
        {
            Arena->Memory    = NULL;
            Arena->TotalSize = 0;
            Arena->Allocated = 0;
            return false;
        }

        // Memory != NULL: the allocator borrows the backing and won't free it.
        LinearAllocator_Create(NewSize, Backing, Arena);
    }
    else
    {
        LinearAllocator_Reset(Arena, 0);
    }

    return true;
}

static void Arena_Free(LinearAllocator* Arena)
{
    if (Arena->Memory)
    {
        Platform_MemFree(Arena->Memory);
    }

    Arena->Memory    = NULL;
    Arena->TotalSize = 0;
    Arena->Allocated = 0;
}

static void ProcessFile(FileTask* Task, bool InPlace, LinearAllocator* Arena)
{
    FileHandle Handle;
    if (!Filesystem_Open(Task->Path, FileMode_Read, &Handle))
    {
        Task->Result = FileResult_ReadError;
        return;
    }

    usize Size = 0;
    if (!Filesystem_GetFileSize(Handle, &Size) || Size > UINT32_MAX)
    {
        Filesystem_Close(&Handle);
        Task->Result = FileResult_ReadError;
        return;
    }

    if (!Arena_EnsureAndReset(Arena, ComputeArenaSize(Size)))
    {
        Filesystem_Close(&Handle);
        Task->Result = FileResult_ReadError;
        return;
    }

    u8*   Data      = LinearAllocator_Allocate(Arena, Size + 1);
    usize BytesRead = 0;
    bool  ok        = Filesystem_ReadEntireFile(Handle, Data, &BytesRead);
    Filesystem_Close(&Handle);

    if (!ok)
    {
        Task->Result = FileResult_ReadError;
        return;
    }

    Data[BytesRead] = 0;
    u32 n = (u32)BytesRead;

    ByteBuf Out;
    BB_InitCap(&Out, Arena, (usize)n * 3 + 256);

    DeclList Decls;
    DeclList_InitCap(&Decls, Arena, n / 2 + 64);

    u32 Transformed = 0;
    u32 Warnings    = 0;
    TransformBuffer(Data, n, &Out, &Decls, &Transformed, &Warnings);

    Task->NumTransformed = Transformed;
    Task->NumWarnings    = Warnings;

    bool Changed = (Out.Len != n) || (n > 0 && !Platform_MemEqual(Out.Data, Data, n));

    if (InPlace && !Changed)
    {
        Task->Result = FileResult_Unchanged; // nothing to do; leave the file alone
    }
    else
    {
        // In-place writes back to Path; out-of-place always emits OutPath (even
        // when unchanged) so it can be diffed and compiled independently.
        bool Wrote = WriteWholeFile(Task->OutPath, Out.Data, Out.Len);
        Task->Result = !Wrote ? FileResult_WriteError : (Changed ? FileResult_Modified : FileResult_Unchanged);
    }
}

static void ProcessRange(FileTask* Tasks, u32 Start, u32 End, bool InPlace)
{
    LinearAllocator WorkerArena = {0}; // one backing allocation per worker, reused across its files

    for (u32 i = Start; i < End; i++)
    {
        ProcessFile(&Tasks[i], InPlace, &WorkerArena);
    }

    Arena_Free(&WorkerArena);
}

STRUCT(WorkerCtx)
{
    FileTask* Tasks;
    u32       Start;
    u32       End;
    bool      InPlace;
};

#if PLATFORM_WINDOWS
static DWORD WINAPI ThreadProc(LPVOID Param)
{
    WorkerCtx* w = (WorkerCtx*)Param;
    ProcessRange(w->Tasks, w->Start, w->End, w->InPlace);
    return 0;
}
#endif

#define MAX_WORKER_THREADS 64

// Processes all tasks, multithreaded when possible. Returns the number of worker
// threads actually used (1 == single-threaded).
static u32 DispatchTasks(FileTask* Tasks, u32 NumFiles, bool ForceSingle, bool InPlace)
{
    u32 Want = 1;

#if PLATFORM_WINDOWS
    if (!ForceSingle)
    {
        u32 Procs = Platform_GetNumLogicalProcessors();
        if (Procs == 0)
        {
            Procs = 1;
        }

        Want = (NumFiles < Procs) ? NumFiles : Procs;
        if (Want > MAX_WORKER_THREADS)
        {
            Want = MAX_WORKER_THREADS;
        }
        if (Want < 1)
        {
            Want = 1;
        }
    }
#else
    UNUSED_PARAM(ForceSingle); // no thread backend here: always single-threaded fallback
#endif

    if (Want <= 1)
    {
        ProcessRange(Tasks, 0, NumFiles, InPlace);
        return 1;
    }

#if PLATFORM_WINDOWS
    WorkerCtx Ctxs[MAX_WORKER_THREADS] = {0};
    HANDLE    Handles[MAX_WORKER_THREADS];
    u32       NumHandles = 0;

    u32 Base = NumFiles / Want;
    u32 Rem  = NumFiles % Want;
    u32 Idx  = 0;

    for (u32 t = 0; t < Want; t++)
    {
        u32 Count = Base + (t < Rem ? 1 : 0);
        Ctxs[t].Tasks   = Tasks;
        Ctxs[t].Start   = Idx;
        Ctxs[t].End     = Idx + Count;
        Ctxs[t].InPlace = InPlace;
        Idx += Count;

        if (Count == 0)
        {
            continue;
        }

        HANDLE h = CreateThread(NULL, 0, ThreadProc, &Ctxs[t], 0, NULL);
        if (h)
        {
            Handles[NumHandles] = h;
            NumHandles += 1;
        }
        else
        {
            ProcessRange(Tasks, Ctxs[t].Start, Ctxs[t].End, InPlace); // fallback inline on failure
        }
    }

    if (NumHandles > 0)
    {
        DWORD wr = WaitForMultipleObjects(NumHandles, Handles, TRUE, INFINITE);
        (void)wr;
        for (u32 h = 0; h < NumHandles; h++)
        {
            CloseHandle(Handles[h]);
        }
    }

    return Want;
#else
    return 1;
#endif
}

// Only C source files are processed; anything else is silently ignored.
static bool IsCSourceFile(String Path)
{
    return String_EndsWith(Path, S(".c"), false);
}

// -----------------------------------------------------------------------------
// Entry point
// -----------------------------------------------------------------------------

u32 RunApplication(const StringArray Arguments)
{
    Logging_ToggleLogCategory(false);
    Logging_ToggleLogTimeStamp(false);
    Logging_ToggleLogType(false);
    Logging_ToggleLogFile(false);

    // Honour quiet mode before emitting anything (so nothing leaks out first).
    for (u32 i = 0; i < Arguments.Num; i++)
    {
        if (String_IsEqual(Arguments.List[i], S("-q"), true) ||
            String_IsEqual(Arguments.List[i], S("--quiet"), true))
        {
            Logging_Disable();
            break;
        }
    }

    bool ForceSingle = false;
    bool InPlace     = true;

    // First pass: classify flags and measure the file arguments.
    u32   NumFiles       = 0;
    usize TotalPathBytes = 0;
    for (u32 i = 0; i < Arguments.Num; i++)
    {
        String Arg = Arguments.List[i];
        if (Arg.Length == 0)
        {
            continue;
        }

        if (Arg.Data[0] == '-')
        {
            if (String_IsEqual(Arg, S("-s"), true) ||
                String_IsEqual(Arg, S("-1"), true) ||
                String_IsEqual(Arg, S("--single-threaded"), true))
            {
                ForceSingle = true;
            }
            else if (String_IsEqual(Arg, S("-o"), true) ||
                     String_IsEqual(Arg, S("--out-of-place"), true) ||
                     String_IsEqual(Arg, S("--no-inplace"), true))
            {
                InPlace = false;
            }
            else if (String_IsEqual(Arg, S("-q"), true) ||
                     String_IsEqual(Arg, S("--quiet"), true))
            {
                // already handled by the pre-scan above (logging disabled)
            }
            else
            {
                LOG_WARNING("Ignoring unknown flag: %S", Arg);
            }
            continue;
        }

        if (!IsCSourceFile(Arg))
        {
            continue; // silently ignore non-.c files
        }

        NumFiles       += 1;
        TotalPathBytes += Arg.Length;
    }

    if (NumFiles == 0)
    {
        LOG("String interpolation metaprogram");
        LOG("Usage: %s [flags] <file1.c> <file2.c> ...", "Program.exe");
        LOG("  -s, --single-threaded, -1     Force single-threaded processing.");
        LOG("  -o, --out-of-place            Write <name>.gen.<ext> instead of overwriting in place.");
        LOG("  -q, --quiet                   Disable all logging output.");
        return 0;
    }

    // One OS allocation backs a linear allocator for the long-lived task list
    // and output paths; everything else is bump-allocated from it.
    usize PersistentSize = NumFiles * sizeof(FileTask) + TotalPathBytes + NumFiles * 32 + 256;
    u8*   PersistentMem  = (u8*)Platform_MemAlloc(PersistentSize);
    if (!PersistentMem)
    {
        LOG_ERROR("Out of memory allocating the task arena.");
        return 1;
    }

    LinearAllocator Persistent;
    LinearAllocator_Create(PersistentSize, PersistentMem, &Persistent);

    FileTask* Tasks = LinearAllocator_Allocate(&Persistent, NumFiles * sizeof(FileTask));
    MemZero(Tasks, NumFiles * sizeof(FileTask));

    // Second pass: record each file's input path and its output path.
    u32 FileIdx = 0;
    for (u32 i = 0; i < Arguments.Num; i++)
    {
        String Arg = Arguments.List[i];
        if (Arg.Length == 0 || Arg.Data[0] == '-' || !IsCSourceFile(Arg))
        {
            continue;
        }

        Tasks[FileIdx].Path    = Arg;
        Tasks[FileIdx].OutPath = InPlace ? Arg : BuildOutputPath(&Persistent, Arg, ".gen");
        Tasks[FileIdx].Result  = FileResult_Pending;
        FileIdx += 1;
    }

    LOG_INFO("Processing %u file(s) (%s)...", NumFiles, InPlace ? "in place" : "out of place");

    f64 StartTime   = Platform_GetAbsoluteTime();
    u32 ThreadsUsed = DispatchTasks(Tasks, NumFiles, ForceSingle, InPlace);
    f64 Elapsed     = Platform_GetAbsoluteTime() - StartTime;

    // Report (main thread only).
    u32 Modified         = 0;
    u32 Unchanged        = 0;
    u32 Errors           = 0;
    u32 TotalTransformed = 0;
    u32 TotalWarnings    = 0;

    for (u32 i = 0; i < NumFiles; i++)
    {
        FileTask* t = &Tasks[i];
        TotalTransformed += t->NumTransformed;
        TotalWarnings    += t->NumWarnings;

        switch (t->Result)
        {
            case FileResult_Modified:
            {
                Modified += 1;
                if (InPlace)
                {
                    LOG_SUCCESS("rewrote %S (%u call(s) transformed)", t->Path, t->NumTransformed);
                }
                else
                {
                    LOG_SUCCESS("rewrote %S -> %S (%u call(s) transformed)", t->Path, t->OutPath, t->NumTransformed);
                }
                if (t->NumWarnings)
                {
                    LOG_WARNING("  %u unresolved interpolation(s) in %S left untouched", t->NumWarnings, t->Path);
                }
                break;
            }
            case FileResult_Unchanged:
            {
                Unchanged += 1;
                if (InPlace)
                {
                    LOG_MUTE("unchanged %S", t->Path);
                }
                else
                {
                    LOG_MUTE("copied %S -> %S (no changes)", t->Path, t->OutPath);
                }
                if (t->NumWarnings)
                {
                    LOG_WARNING("  %u unresolved interpolation(s) in %S left untouched", t->NumWarnings, t->Path);
                }
                break;
            }
            case FileResult_ReadError:
            {
                Errors += 1;
                LOG_ERROR("could not read %S", t->Path);
                break;
            }
            case FileResult_WriteError:
            {
                Errors += 1;
                LOG_ERROR("could not write %S", t->Path);
                break;
            }
            default:
            {
                Errors += 1;
                LOG_ERROR("file not processed: %S", t->Path);
                break;
            }
        }
    }

    LOG_INFO("Done in %.3f ms using %u thread(s):", Elapsed * 1000.0, ThreadsUsed);
    LOG_INFO("  Modified:    %u", Modified);
    LOG_INFO("  Unchanged:   %u", Unchanged);
    LOG_INFO("  Errors:      %u", Errors);
    LOG_INFO("  Transformed: %u call(s)", TotalTransformed);
    LOG_INFO("  Warnings:    %u", TotalWarnings);

    Platform_MemFree(PersistentMem); // frees the task list and all output paths at once
    return Errors > 0 ? 1 : 0;
}
