#include "cool.h"

#include <string.h>

static void cool_internal_md_tag(const char *s);
static void cool_internal_md_txt(const char *s, size_t n);
static void cool_internal_md_href(const char *s, size_t n);
static size_t cool_internal_md_ws(const char *s, size_t n);
static int cool_internal_md_blank(const char *s, size_t n);
static size_t cool_internal_md_next_line(const char *s, size_t n, size_t off, size_t *out_n);
static void cool_internal_md_inline(const char *s, size_t n);
static int cool_internal_md_atx(const char *s, size_t n, int *level, size_t *body, size_t *body_n);
static int cool_internal_md_ul(const char *s, size_t n, size_t *body);
static int cool_internal_md_ol(const char *s, size_t n, size_t *body);
static int cool_internal_md_hr(const char *s, size_t n);
static int cool_internal_md_fence_line(const char *s, size_t n, size_t *ticks, size_t *lang, size_t *lang_n);
static int cool_internal_md_lang_ok(const char *s, size_t n);
static int cool_internal_md_has_pipe(const char *s, size_t n);
static int cool_internal_md_table_sep(const char *s, size_t n);
static void cool_internal_md_table_row(const char *s, size_t n, const char *cell);
static void cool_internal_md_close_p(int *in_p);
static void cool_internal_md_close_list(int *list);

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

void
cool_internal_md_tag(const char *s)
{
    cool_html_raw_cstr(s, strlen(s));
}

void
cool_internal_md_txt(const char *s, size_t n)
{
    if (n)
        cool_html_txt(s, n);
}

void
cool_internal_md_href(const char *s, size_t n)
{
    size_t i;

    for (i = 0; i < n; i++) {
        const char *e;
        size_t el;

        if (s[i] == '&') {
            e = "&amp;";
            el = 5;
        } else if (s[i] == '"') {
            e = "&quot;";
            el = 6;
        } else if (s[i] == '<') {
            e = "&lt;";
            el = 4;
        } else {
            continue;
        }
        cool_internal_md_txt(s, i);
        cool_html_raw_cstr(e, el);
        s += i + 1;
        n -= i + 1;
        i = (size_t)-1;
    }
    cool_internal_md_txt(s, n);
}

size_t
cool_internal_md_ws(const char *s, size_t n)
{
    size_t i;

    i = 0;
    while (i < n && (s[i] == ' ' || s[i] == '\t'))
        i++;
    return i;
}

int
cool_internal_md_blank(const char *s, size_t n)
{
    return cool_internal_md_ws(s, n) == n;
}

size_t
cool_internal_md_next_line(const char *s, size_t n, size_t off, size_t *out_n)
{
    size_t i;

    i = off;
    while (i < n && s[i] != '\n')
        i++;
    *out_n = i - off;
    if (*out_n && s[off + *out_n - 1] == '\r')
        (*out_n)--;
    if (i < n)
        i++;
    return i;
}

void
cool_internal_md_inline(const char *s, size_t n)
{
    size_t i, run;

    i = 0;
    run = 0;
    while (i < n) {
        if (s[i] == '`') {
            size_t j;

            j = i + 1;
            while (j < n && s[j] != '`')
                j++;
            if (j < n) {
                cool_internal_md_txt(s + run, i - run);
                cool_internal_md_tag("<code>");
                cool_internal_md_txt(s + i + 1, j - (i + 1));
                cool_internal_md_tag("</code>");
                i = j + 1;
                run = i;
                continue;
            }
            i++;
            continue;
        }
        if (i + 1 < n && s[i] == '*' && s[i + 1] == '*') {
            size_t j;

            j = i + 2;
            while (j + 1 < n && !(s[j] == '*' && s[j + 1] == '*'))
                j++;
            if (j + 1 < n) {
                cool_internal_md_txt(s + run, i - run);
                cool_internal_md_tag("<strong>");
                cool_internal_md_inline(s + i + 2, j - (i + 2));
                cool_internal_md_tag("</strong>");
                i = j + 2;
                run = i;
                continue;
            }
            i++;
            continue;
        }
        if (s[i] == '*') {
            size_t j;

            j = i + 1;
            while (j < n && s[j] != '*')
                j++;
            if (j < n && j > i + 1) {
                cool_internal_md_txt(s + run, i - run);
                cool_internal_md_tag("<em>");
                cool_internal_md_inline(s + i + 1, j - (i + 1));
                cool_internal_md_tag("</em>");
                i = j + 1;
                run = i;
                continue;
            }
            i++;
            continue;
        }
        if (s[i] == '[') {
            size_t rb, rp;

            rb = i + 1;
            while (rb < n && s[rb] != ']')
                rb++;
            if (rb + 1 < n && s[rb] == ']' && s[rb + 1] == '(') {
                rp = rb + 2;
                while (rp < n && s[rp] != ')')
                    rp++;
                if (rp < n) {
                    cool_internal_md_txt(s + run, i - run);
                    cool_internal_md_tag("<a href=\"");
                    cool_internal_md_href(s + rb + 2, rp - (rb + 2));
                    cool_internal_md_tag("\">");
                    cool_internal_md_inline(s + i + 1, rb - (i + 1));
                    cool_internal_md_tag("</a>");
                    i = rp + 1;
                    run = i;
                    continue;
                }
            }
            i++;
            continue;
        }
        i++;
    }
    cool_internal_md_txt(s + run, n - run);
}

int
cool_internal_md_atx(const char *s, size_t n, int *level, size_t *body, size_t *body_n)
{
    size_t i, end;
    int lv;

    i = cool_internal_md_ws(s, n);
    if (i > 3)
        return 0;
    lv = 0;
    while (i < n && s[i] == '#' && lv < 6) {
        lv++;
        i++;
    }
    if (lv == 0)
        return 0;
    if (i < n && s[i] != ' ' && s[i] != '\t')
        return 0;
    while (i < n && (s[i] == ' ' || s[i] == '\t'))
        i++;
    end = n;
    while (end > i && (s[end - 1] == ' ' || s[end - 1] == '\t'))
        end--;
    while (end > i && s[end - 1] == '#')
        end--;
    while (end > i && (s[end - 1] == ' ' || s[end - 1] == '\t'))
        end--;
    *level = lv;
    *body = i;
    *body_n = end - i;
    return 1;
}

int
cool_internal_md_ul(const char *s, size_t n, size_t *body)
{
    size_t i;

    i = cool_internal_md_ws(s, n);
    if (i > 3 || i >= n)
        return 0;
    if (s[i] != '-' && s[i] != '*' && s[i] != '+')
        return 0;
    if (i + 1 >= n || (s[i + 1] != ' ' && s[i + 1] != '\t'))
        return 0;
    *body = i + 2;
    return 1;
}

int
cool_internal_md_ol(const char *s, size_t n, size_t *body)
{
    size_t i;

    i = cool_internal_md_ws(s, n);
    if (i > 3 || i >= n || s[i] < '0' || s[i] > '9')
        return 0;
    while (i < n && s[i] >= '0' && s[i] <= '9')
        i++;
    if (i >= n || s[i] != '.')
        return 0;
    if (i + 1 >= n || (s[i + 1] != ' ' && s[i + 1] != '\t'))
        return 0;
    *body = i + 2;
    return 1;
}

int
cool_internal_md_hr(const char *s, size_t n)
{
    size_t i, dashes;

    i = cool_internal_md_ws(s, n);
    dashes = 0;
    while (i < n && s[i] == '-') {
        dashes++;
        i++;
    }
    while (i < n && (s[i] == ' ' || s[i] == '\t'))
        i++;
    return dashes >= 3 && i == n;
}

int
cool_internal_md_fence_line(const char *s, size_t n, size_t *ticks, size_t *lang, size_t *lang_n)
{
    size_t i, t;

    i = cool_internal_md_ws(s, n);
    if (i > 3)
        return 0;
    t = 0;
    while (i < n && s[i] == '`') {
        t++;
        i++;
    }
    if (t < 3)
        return 0;
    while (i < n && (s[i] == ' ' || s[i] == '\t'))
        i++;
    *ticks = t;
    *lang = i;
    while (i < n && s[i] != ' ' && s[i] != '\t' && s[i] != '`')
        i++;
    *lang_n = i - *lang;
    while (i < n && (s[i] == ' ' || s[i] == '\t'))
        i++;
    return i == n;
}

int
cool_internal_md_lang_ok(const char *s, size_t n)
{
    size_t i;
    char c;

    if (!n)
        return 0;
    for (i = 0; i < n; i++) {
        c = s[i];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '+')
            continue;
        return 0;
    }
    return 1;
}

int
cool_internal_md_has_pipe(const char *s, size_t n)
{
    size_t i;

    for (i = 0; i < n; i++) {
        if (s[i] == '|')
            return 1;
    }
    return 0;
}

int
cool_internal_md_table_sep(const char *s, size_t n)
{
    size_t i, dashes;

    i = cool_internal_md_ws(s, n);
    if (i < n && s[i] == '|')
        i++;
    dashes = 0;
    while (i < n) {
        while (i < n && (s[i] == ' ' || s[i] == '\t'))
            i++;
        if (i < n && s[i] == ':')
            i++;
        if (i >= n || s[i] != '-')
            return 0;
        while (i < n && s[i] == '-') {
            dashes++;
            i++;
        }
        if (i < n && s[i] == ':')
            i++;
        while (i < n && (s[i] == ' ' || s[i] == '\t'))
            i++;
        if (i < n && s[i] == '|') {
            i++;
            continue;
        }
        break;
    }
    while (i < n && (s[i] == ' ' || s[i] == '\t'))
        i++;
    return i == n && dashes > 0;
}

void
cool_internal_md_table_row(const char *s, size_t n, const char *cell)
{
    size_t i, a, b;
    char open[8], close[8];

    i = cool_internal_md_ws(s, n);
    if (i < n && s[i] == '|')
        i++;
    snprintf(open, sizeof open, "<%s>", cell);
    snprintf(close, sizeof close, "</%s>", cell);
    cool_internal_md_tag("<tr>");
    while (i < n) {
        a = i;
        while (i < n && s[i] != '|')
            i++;
        b = i;
        while (a < b && (s[a] == ' ' || s[a] == '\t'))
            a++;
        while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t'))
            b--;
        if (a == b && i >= n)
            break;
        cool_internal_md_tag(open);
        cool_internal_md_inline(s + a, b - a);
        cool_internal_md_tag(close);
        if (i < n && s[i] == '|')
            i++;
    }
    cool_internal_md_tag("</tr>\n");
}

void
cool_internal_md_close_p(int *in_p)
{
    if (*in_p) {
        cool_internal_md_tag("</p>\n");
        *in_p = 0;
    }
}

void
cool_internal_md_close_list(int *list)
{
    if (*list == 1)
        cool_internal_md_tag("</ul>\n");
    else if (*list == 2)
        cool_internal_md_tag("</ol>\n");
    *list = 0;
}

void
cool_md(const char *str, size_t len)
{
    size_t off;
    int in_p;
    int list;
    int fence;
    int in_table;
    size_t fence_ticks;

    if (!str)
        return;
    off = 0;
    in_p = 0;
    list = 0;
    fence = 0;
    in_table = 0;
    fence_ticks = 0;

    while (off < len) {
        size_t n, next;
        const char *line;
        size_t body, ticks, lang, lang_n, n2, next2;
        int lv;
        char open[8], close[8];

        next = cool_internal_md_next_line(str, len, off, &n);
        line = str + off;

        if (fence) {
            if (cool_internal_md_fence_line(line, n, &ticks, &lang, &lang_n) &&
                lang_n == 0 && ticks >= fence_ticks) {
                cool_internal_md_tag("</code></pre>\n");
                fence = 0;
            } else {
                cool_internal_md_txt(line, n);
                cool_internal_md_tag("\n");
            }
            off = next;
            continue;
        }

        if (cool_internal_md_blank(line, n)) {
            cool_internal_md_close_p(&in_p);
            cool_internal_md_close_list(&list);
            if (in_table) {
                cool_internal_md_tag("</tbody></table>\n");
                in_table = 0;
            }
            off = next;
            continue;
        }

        if (in_table) {
            if (cool_internal_md_has_pipe(line, n) && !cool_internal_md_table_sep(line, n)) {
                cool_internal_md_table_row(line, n, "td");
                off = next;
                continue;
            }
            cool_internal_md_tag("</tbody></table>\n");
            in_table = 0;
        }

        next2 = cool_internal_md_next_line(str, len, next, &n2);
        if (cool_internal_md_has_pipe(line, n) && next < len && cool_internal_md_table_sep(str + next, n2)) {
            cool_internal_md_close_p(&in_p);
            cool_internal_md_close_list(&list);
            cool_internal_md_tag("<table>\n<thead>\n");
            cool_internal_md_table_row(line, n, "th");
            cool_internal_md_tag("</thead>\n<tbody>\n");
            in_table = 1;
            off = next2;
            continue;
        }

        if (cool_internal_md_atx(line, n, &lv, &body, &lang_n)) {
            cool_internal_md_close_p(&in_p);
            cool_internal_md_close_list(&list);
            snprintf(open, sizeof open, "<h%d>", lv);
            snprintf(close, sizeof close, "</h%d>\n", lv);
            cool_internal_md_tag(open);
            cool_internal_md_inline(line + body, lang_n);
            cool_internal_md_tag(close);
            off = next;
            continue;
        }

        if (cool_internal_md_fence_line(line, n, &ticks, &lang, &lang_n)) {
            cool_internal_md_close_p(&in_p);
            cool_internal_md_close_list(&list);
            cool_internal_md_tag("<pre><code");
            if (cool_internal_md_lang_ok(str + off + lang, lang_n)) {
                cool_internal_md_tag(" class=\"language-");
                cool_internal_md_txt(str + off + lang, lang_n);
                cool_internal_md_tag("\"");
            }
            cool_internal_md_tag(">");
            fence = 1;
            fence_ticks = ticks;
            off = next;
            continue;
        }

        if (cool_internal_md_hr(line, n)) {
            cool_internal_md_close_p(&in_p);
            cool_internal_md_close_list(&list);
            cool_internal_md_tag("<hr/>\n");
            off = next;
            continue;
        }

        if (cool_internal_md_ul(line, n, &body)) {
            cool_internal_md_close_p(&in_p);
            if (list == 2)
                cool_internal_md_close_list(&list);
            if (list == 0) {
                cool_internal_md_tag("<ul>\n");
                list = 1;
            }
            cool_internal_md_tag("<li>");
            cool_internal_md_inline(line + body, n - body);
            cool_internal_md_tag("</li>\n");
            off = next;
            continue;
        }

        if (cool_internal_md_ol(line, n, &body)) {
            cool_internal_md_close_p(&in_p);
            if (list == 1)
                cool_internal_md_close_list(&list);
            if (list == 0) {
                cool_internal_md_tag("<ol>\n");
                list = 2;
            }
            cool_internal_md_tag("<li>");
            cool_internal_md_inline(line + body, n - body);
            cool_internal_md_tag("</li>\n");
            off = next;
            continue;
        }

        cool_internal_md_close_list(&list);
        if (!in_p) {
            cool_internal_md_tag("<p>");
            in_p = 1;
        } else {
            cool_internal_md_tag(" ");
        }
        cool_internal_md_inline(line, n);
        off = next;
    }
    cool_internal_md_close_p(&in_p);
    cool_internal_md_close_list(&list);
    if (in_table)
        cool_internal_md_tag("</tbody></table>\n");
    if (fence)
        cool_internal_md_tag("</code></pre>\n");
}
