#include "cool.h"

void
cool_html_raw(Cool_StrView sv)
{
    fwrite(sv.cstr, sizeof *sv.cstr, sv.len, COOL_OUTPUT);
}

void
cool_html_raw_cstr(const char *str, size_t len)
{
    fwrite(str, sizeof *str, len, COOL_OUTPUT);
}

void
cool_html_txt(const char *str, size_t len)
{
    const char *s = str;
	while (s && *s && len--) {
        int c = *s;
        if (c == '<' || c == '>' || c == '&') {
            cool_html_raw_cstr(str, s - str);
            switch (c) {
                case '>':
                    cool_html_raw(COOL_SV("&gt;"));
                    break;
                case '<':
                    cool_html_raw(COOL_SV("&lt;"));
                    break;
                case '&':
                    cool_html_raw(COOL_SV("&amp;"));
                    break;
            }
            str = s + 1;
        }
        s++;
    }

    if (s != str) cool_html_raw_cstr(str, s - str);
}

void
cool_htmlf_raw(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vfprintf(COOL_OUTPUT, fmt, args);
    va_end(args);
}
