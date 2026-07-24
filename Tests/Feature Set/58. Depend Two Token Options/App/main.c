#if !defined FIRST_PICKED || DEPTH != 3
    #error two-token Depend with | options must build First.build and forward depth=3
#endif

#if !defined SECOND_PICKED || !defined SECOND_TOK
    #error path-form Depend with | options must build Second.build and forward tok
#endif

int FirstValue(void);
int SecondValue(void);

int main(void)
{
    int Result = 1;

    if (FirstValue() == 1 && SecondValue() == 2)
    {
        Result = 0;
    }

    return Result;
}
