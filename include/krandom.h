#ifndef H_KRANDOM
#define H_KRANDOM
#include <stdint.h>
void krandom_add(uint32_t n); //add some number to the prng
uint32_t krandom_get();

#endif
