#include <stdio.h>

/* The block comment hid this define... */
#if defined(SHOULD_NOT_EXIST)
    #error "## block comments should swallow key-looking lines"
#endif

/* ...and the backtick reset replaced this one... */
#if defined(FIRST_TRY)
    #error "the backtick reset should have discarded FIRST_TRY"
#endif

/* ...leaving only the final value. */
#if !defined(FINAL_ONLY)
    #error "FINAL_ONLY should have survived the reset"
#endif

extern const char* Helper_Word(void);

int main(void)
{
    printf("syntax %s\n", Helper_Word());
    return 0;
}
