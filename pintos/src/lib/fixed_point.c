#include "lib/fixed_point.h"
#include "lib/stdint.h"

int convert_to_fixed_point(int n) 
{
    return n * CONST_F;
}

int convert_to_int_tozero(int f)
{
    return f / CONST_F;
}

int convert_to_int_round(int f) 
{
    if (f >= 0) 
        return (f + CONST_F / 2) / CONST_F;
    else
        return (f - CONST_F / 2) / CONST_F;
}

int ff_add(int f1, int f2)
{
    return f1 + f2;
}

int ff_sub(int f1, int f2)
{
    return f1 - f2;
}

int ff_add_with_int(int f, int n)
{
    return f + n * CONST_F;
}

int ff_sub_with_int(int f, int n)
{
    return f - n * CONST_F;
}

int ff_mul(int f1, int f2)
{
    return ((int64_t) f1) * f2 / CONST_F;
} 

int ff_mul_with_int(int f, int n)
{
    return f * n;
}

int ff_div(int f1, int f2)
{
    return ((int64_t) f1) * CONST_F / f2;
}

int ff_div_with_int(int f, int n)
{
    return f / n;
}
