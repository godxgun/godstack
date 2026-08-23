PODEF char*
p_find_char(char *buf, char needle)
{
    while (buf && *buf != needle) buf++;
    return buf;
}

PODEF bool
p_strcmp(const char *a, const char *b)
{
    if (p_strlen(a) != p_strlen(b))
        return false;

    while (a && b && *a != '\0') {
        if (*(a++) != *(b++)) return false;
    }

    return true;
}

PODEF int
p_strlen(const char *a)
{
    int i = 0;
    while (*a++ != '\0') i++;
    return i;
}

PODEF int
p_strtoi(const char *str)
{
    int res = 0;

    while (*str) {
        if (*str >= '0' && *str <= '9') {
            res = res * 10 + (*str - '0');
        } else {
            return res;
        }
        str++;
    }

    return res;
}

PODEF char*
p_strtok(const char *str, char c)
{
    while (*str != c && *str++ != '\0');
    return (*str == c) ? (char*) ++str : 0;
}

PODEF P_StringView
p_strview(const char *cstr) 
{
    P_StringView sv = {0};
    sv.cstr = cstr;
    sv.len = p_strlen(cstr);
    return sv;
}

PODEF void
p_strview_chop_left(P_StringView *sv, u32 n) 
{
    if (n > sv->len) n = sv->len;
    sv->cstr += n;
    sv->len  -= n;
}

PODEF void
p_strview_chop_right(P_StringView *sv, u32 n) 
{
    if (n > sv->len) n = sv->len;
    sv->len  -= n;
}

PODEF P_StringView
p_strview_chop_delim(P_StringView *sv, char delim) 
{
    u32 i = 0;
    while (i < sv->len && sv->cstr[i] != delim) {
        i++;
    }

    P_StringView tok;
    if (i < sv->len) {
        tok.cstr = sv->cstr;
        tok.len  = i; 
        p_strview_chop_left(sv, i + 1);
        return tok;
    }

    tok = *sv;
    p_strview_chop_left(sv, sv->len);
    return tok;
}

PODEF P_StringView
p_strview_chop_type(P_StringView *sv, int(*istype)(int))
{
    u32 i = 0;
    while (i < sv->len && istype(sv->cstr[i])) {
        i++;
    }

    P_StringView tok;
    if (i < sv->len) {
        tok.cstr = sv->cstr;
        tok.len  = i; 
        p_strview_chop_left(sv, i + 1);
        return tok;
    }

    tok = *sv;
    p_strview_chop_left(sv, sv->len);
    return tok;
}


PODEF void
p_strview_trim_left(P_StringView *sv) 
{
    while (sv->len > 0 && p_is_space(sv->cstr[0])) {
        p_strview_chop_left(sv, 1);
    }
}

PODEF void
p_strview_trim_right(P_StringView *sv) 
{
    while (sv->len > 0 && p_is_space(sv->cstr[sv->len-1])) {
        p_strview_chop_right(sv, 1);
    }
}

PODEF void
p_strview_trim(P_StringView *sv) 
{
    p_strview_trim_left(sv);
    p_strview_trim_right(sv);
}

PODEF int 
p_is_space(int c) 
{
    /* Checks for: space, form feed (\f), line feed (\n), 
     * carriage return (\r), horizontal tab (\t), vertical tab (\v) */
    return (c == ' ' || (c >= '\t' && c <= '\r'));
}

PODEF int 
p_is_digit(int c) 
{
    return (c >= '0' && c <= '9');
}

PODEF int 
p_is_alpha(int c) 
{
    return ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'));
}

PODEF int 
p_is_alnum(int c) 
{
    return (p_is_alpha(c) || p_is_digit(c));
}

PODEF int 
p_is_hex(int c) 
{
    return (p_is_digit(c) || 
           (c >= 'a' && c <= 'f') || 
           (c >= 'A' && c <= 'F'));
}

PODEF int 
p_is_upper(int c) 
{
    return (c >= 'A' && c <= 'Z');
}

PODEF int 
p_is_lower(int c) 
{
    return (c >= 'a' && c <= 'z');
}

PODEF int 
p_to_lower(int c) 
{
    if (p_is_upper(c)) return (c + ('a' - 'A'));
    return c;
}

PODEF int 
p_to_upper(int c) 
{
    if (p_is_lower(c)) return (c - ('a' - 'A'));
    return c;
}
