#pragma once

double Ops_Add(double A, double B);
double Ops_Subtract(double A, double B);
double Ops_Multiply(double A, double B);
double Ops_Negate(double A);

/* Returns 0 and leaves OutResult untouched when B is zero. */
int Ops_Divide(double A, double B, double* OutResult);
