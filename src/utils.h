#ifndef UTILS_H
#define UTILS_H

#include <stddef.h>
#include <string.h>

int hexnib(unsigned char c);
int htob(const char *hex, unsigned char *out, size_t max);
int btoh(const unsigned char *in, size_t len, char *out, size_t max,
         char sep, int upper);
int atob(const char *ascii, size_t len, unsigned char *out, size_t max);
int btoa(const unsigned char *in, size_t len, char *out, size_t max);
int parse_payload(const char *input, unsigned char *out, size_t max);
int parse_args(char *str, char **argv, int max_argc);

#endif /* UTILS_H */
