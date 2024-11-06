#ifndef FIXED_POINT_H
#define FIXED_POINT_H

#define F (1 << 14)

int convert_int_to_fixed(int n);
int convert_fixed_to_int_zero(int x);
int convert_fixed_to_int_nearest(int x);
int fixed_add(int x, int y);
int fixed_subtract(int x, int y);
int fixed_add_int(int x, int n);
int fixed_subtract_int(int x, int n);
int fixed_multiply(int x, int y);
int fixed_multiply_int(int x, int n);
int fixed_divide(int x, int y);
int fixed_divide_int(int x, int n);

int convert_int_to_fixed(int n) {
    return n * F;
}

int convert_fixed_to_int_zero(int x) {
    return x / F;
}

int convert_fixed_to_int_nearest(int x) {
    return x >= 0 ? (x + F / 2) / F : (x - F / 2) / F;
}

int fixed_add(int x, int y) {
    return x + y;
}

int fixed_subtract(int x, int y) {
    return x - y;
}

int fixed_add_int(int x, int n) {
    return x + n * F;
}

int fixed_subtract_int(int x, int n) {
    return x - n * F;
}

int fixed_multiply(int x, int y){
    return ((int64_t) x) * y / F;
}

int fixed_multiply_int(int x, int n){
    return x * n;
}

int fixed_divide(int x, int y){
    return ((int64_t) x) * F / y;
}

int fixed_divide_int(int x, int n){
    return x / n;
}

#endif
