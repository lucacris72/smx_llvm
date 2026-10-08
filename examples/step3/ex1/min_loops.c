typedef signed char i8;
typedef unsigned char u8;

i8 min_i8(const i8 *a, unsigned n) {
  i8 min = 127;
  for (unsigned i = 0; i < n; ++i)
    if (min > a[i])
      min = a[i];
  return min;
}

i8 min_volatile(const volatile i8 *a, unsigned n) {
  i8 min = 127;
  for (unsigned i = 0; i < n; ++i)
    if (min > a[i])
      min = a[i];
  return min;
}

u8 min_u8(const u8 *a, unsigned n) {
  u8 min = 255;
  for (unsigned i = 0; i < n; ++i)
    if (min > a[i])
      min = a[i];
  return min;
}

i8 min_with_store(const i8 *a, i8 *out, unsigned n) {
  i8 min = 127;
  for (unsigned i = 0; i < n; ++i) {
    out[i] = a[i];
    if (min > a[i])
      min = a[i];
  }
  return min;
}
