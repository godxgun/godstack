#include "cool.h"

void
cool_html_raw(Cool_StrView sv)
{
    cool_html_raw_cstr(sv.cstr, sv.len);
}

void
cool_html_raw_cstr(const char *str, size_t len)
{
    fwrite(str, 1, len, COOL_OUTPUT);
}

void
cool_html_txt(const char *str, size_t len)
{
    const char *s = str;

    for (; s && *s && len; s++, len--) {
        const char *e;
        size_t n;

        if (*s == '<') {
            e = "&lt;";
            n = 4;
        } else if (*s == '>') {
            e = "&gt;";
            n = 4;
        } else if (*s == '&') {
            e = "&amp;";
            n = 5;
        } else {
            continue;
        }
        if (s != str)
            fwrite(str, 1, (size_t)(s - str), COOL_OUTPUT);
        fwrite(e, 1, n, COOL_OUTPUT);
        str = s + 1;
    }
    if (s != str)
        fwrite(str, 1, (size_t)(s - str), COOL_OUTPUT);
}

void
cool_htmlf_raw(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vfprintf(COOL_OUTPUT, fmt, args);
    va_end(args);
}
