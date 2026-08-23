typedef struct {
    const char *cstr;
    u32 len;
} P_StringView;

typedef struct {
    P_StringView sv;
} P_StringBuilder;

/* Classic C string functions */
PODEF char* p_find_char(char *buf, char needle);
PODEF bool  p_strcmp(const char *a, const char *b);
PODEF int   p_strlen(const char *a);
PODEF int   p_strtoi(const char *str);
PODEF char* p_strtok(const char *str, char c);

/* String views */
PODEF P_StringView  p_strview(const char *cstr);
PODEF void          p_strview_chop_left(P_StringView *sv, u32 n);
PODEF void          p_strview_chop_right(P_StringView *sv, u32 n);
PODEF P_StringView  p_strview_chop_delim(P_StringView *sv, char delim);
PODEF P_StringView  p_strview_chop_type(P_StringView *sv, int(*istype)(int));
PODEF void          p_strview_trim_left(P_StringView *sv);
PODEF void          p_strview_trim_right(P_StringView *sv);
PODEF void          p_strview_trim(P_StringView *sv);

/* Classification */
PODEF int p_is_space(int c);
PODEF int p_is_digit(int c);
PODEF int p_is_alpha(int c);
PODEF int p_is_alnum(int c);
PODEF int p_is_upper(int c);
PODEF int p_is_lower(int c);
PODEF int p_is_hex(int c);

/* Transformation */
PODEF int p_to_lower(int c);
PODEF int p_to_upper(int c);
