#if FROM_GLOBAL_FLAG != 3
    #error "Compiler.Flags -D did not reach other.c"
#endif

#ifdef FROM_FILE_FLAG
    #error "main.c.Compiler.Flags leaked into other.c"
#endif

int other_value(void)
{
    return FROM_GLOBAL_FLAG;
}
