namespace
{
    struct Doubler
    {
        int Value;

        int Doubled() const
        {
            return Value * 2;
        }
    };
}

extern "C" int cpp_bridge(void)
{
    Doubler D;
    D.Value = 10;

    return D.Doubled() + 1;
}
