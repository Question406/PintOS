#ifndef __FIXED_POINT
#define __FIXED_POINT

#define CONST_Q 16
#define CONST_F (1 << CONST_Q)

int convert_to_fixed_point(int n);

int convert_to_int_tozero(int f);

int convert_to_int_round(int f);

int ff_add(int f1, int f2);

int ff_sub(int f1, int f2);

int ff_add_with_int(int f, int n);

int ff_sub_with_int(int f, int n);

int ff_mul(int f1, int f2);

int ff_mul_with_int(int f, int n);

int ff_div(int f1, int f2);

int ff_div_with_int(int f, int n);


#endif