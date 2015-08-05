#ifndef SM64_H
#define SM64_H

#include <stddef.h>

// handful of useful routines to call from C

// standard libary
void bcopy(const void *src, void *dest, size_t n);
void bzero(void *s, size_t n);

// math
float sinf(float);
float cosf(float);
float sqrtf(float);

// SM64
void PrintInt(unsigned int x, unsigned int y, const char *format, unsigned int value);
void PrintStr(unsigned int x, unsigned int y, const char *str);
void PrintXY(unsigned int x, unsigned int y, const char *str);

#endif // SM64_H
