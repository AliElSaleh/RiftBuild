#include "ops.h"

double Ops_Add(double A, double B)
{
    return A + B;
}

double Ops_Subtract(double A, double B)
{
    return A - B;
}

double Ops_Multiply(double A, double B)
{
    return A * B;
}

double Ops_Negate(double A)
{
    return -A;
}

int Ops_Divide(double A, double B, double* OutResult)
{
    int bOk = 0;

    if (B != 0.0)
    {
        *OutResult = A / B;
        bOk = 1;
    }

    return bOk;
}
