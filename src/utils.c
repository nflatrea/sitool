#include <string.h>
#include "utils.h"

int hexnib(unsigned char c)
{
    if ((unsigned)(c - '0') < 10u) return c - '0';
    if ((unsigned)(c - 'A') < 6u)  return c - 'A' + 10;
    if ((unsigned)(c - 'a') < 6u)  return c - 'a' + 10;
    return -1;
}

int htob(const char *hex, unsigned char *out, size_t max)
{
    size_t n = 0;
    int hi, lo;
    while (*hex)
    {
        while (*hex == ' ' || *hex == '\t' || *hex == ':') hex++;
        if (*hex == '\0') break;

        hi = hexnib((unsigned char)*hex++);
        if (hi < 0 || *hex == '\0') return -1;
        lo = hexnib((unsigned char)*hex++);
        if (lo < 0 || n >= max) return -1;

        out[n++] = (unsigned char)((hi << 4) | lo);
    }
    return (int)n;
}

int btoh(const unsigned char *in, size_t len, char *out, size_t max,
         char sep, int upper)
{
    if (len == 0) { if (max) *out = '\0'; return 0; }
    size_t need = sep ? len * 3 : len * 2 + 1;
    if (max < need) return -1;
    const char *h = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    size_t pos = 0;
    for (size_t i = 0; i < len; i++)
    {
        if (i && sep) out[pos++] = sep;
        out[pos++] = h[in[i] >> 4];
        out[pos++] = h[in[i] & 0x0F];
    }
    out[pos] = '\0';
    return (int)pos;
}

int atob(const char *ascii, size_t len, unsigned char *out, size_t max)
{
    if (len > max) return -1;
    memcpy(out, ascii, len);
    return (int)len;
}

int btoa(const unsigned char *in, size_t len, char *out, size_t max)
{
    if (max < len + 1) return -1;
    for (size_t i = 0; i < len; i++)
        out[i] = (in[i] >= 0x20 && in[i] <= 0x7E)
               ? (char)in[i] : '.';
    out[len] = '\0';
    return (int)len;
}

int parse_payload(const char *input, unsigned char *out, size_t max)
{
    size_t n = 0;
    const char *p = input;

    while (*p)
    {
        while (*p == ' ' || *p == '\t' || *p == ':') p++;
        if (*p == '\0') break;

        /* quoted ASCII segment */
        if (*p == '"')
        {
            p++;
            while (*p && *p != '"')
            {
                if (n >= max) return -1;
                out[n++] = (unsigned char)*p++;
            }
            if (*p == '"') p++;
            else return -1;
            continue;
        }

        int hi = hexnib((unsigned char)*p);
        if (hi < 0) return -1;
        p++;
        if (*p == '\0') return -1;
        int lo = hexnib((unsigned char)*p);
        if (lo < 0) return -1;
        p++;
        if (n >= max) return -1;
        out[n++] = (unsigned char)((hi << 4) | lo);
    }
    return (int)n;
}

int parse_args(char *str, char **argv, int max_argc)
{
    int argc = 0;

    if (str == NULL || argv == NULL || max_argc <= 0) return 0;

    char *p = str;
    while (*p)
    {
        while (*p == ' ' || *p == '\t' || *p == '\n') p++;
        if (*p == '\0') break;

        if (argc >= max_argc - 1) break;  /* reserve slot for NULL */

        if (*p == '"')
        {
            argv[argc++] = p;
            p++;
            while (*p && *p != '"') p++;
            if (*p == '"') p++;
            if (*p == ' ' || *p == '\t' || *p == '\n')
                *p++ = '\0';
        }
        else
        {
            argv[argc++] = p;
            while (*p && *p != ' ' && *p != '\t' && *p != '\n') p++;
            if (*p) *p++ = '\0';
        }
    }
    argv[argc] = NULL;
    return argc;
}
