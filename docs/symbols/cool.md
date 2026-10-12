# Cool symbols

Documentation: https://example.org/symbols/cool/
Description: Types, macros, and functions declared by Cool.h.


# Cool symbols

For setup and API workflows, read the [Cool guide](/docs/cool/). This vault lists individual declarations; it is not a tutorial.

<!-- BEGIN GENERATED API -->

## API reference

Generated from `Cool.h` by `tools/generate_docs.py`. Edit the header comments, not this section.

Declarations from all platform branches are listed; this is not a preprocessed API.

### Types

#### Cool_StrView {#cool_strview}

Source: `Cool.h:100`

```c
typedef struct {
    char *cstr;
    size_t len;
} Cool_StrView;
```

### Macros

#### COOL_SV {#cool_sv}

Source: `Cool.h:105`

```c
#define COOL_SV(literal) ((Cool_StrView){ .cstr = (literal), .len = sizeof literal - 1 })
```

### Functions

#### cool_html_raw {#cool_html_raw}

Source: `Cool.h:106`

```c
void cool_html_raw(Cool_StrView sv);
```

Writes HTML to buffer without escaping sequences. Vulnerable to XSS. Use with caution.

#### cool_html_raw_cstr {#cool_html_raw_cstr}

Source: `Cool.h:107`

```c
void cool_html_raw_cstr(const char *str, size_t len);
```

Writes unsafe HTML using C strings.

#### cool_html_txt {#cool_html_txt}

Source: `Cool.h:108`

```c
void cool_html_txt(const char *str, size_t len);
```

Meant for inner text. Escapes '<', '>' and '&'.

#### cool_htmlf_raw {#cool_htmlf_raw}

Source: `Cool.h:109`

```c
void cool_htmlf_raw(const char *fmt, ...);
```

Writes formatting HTML string to buffer without escaping sequences. Vulnerable to XSS. Use with caution.

#### cool_md {#cool_md}

Source: `Cool.h:110`

```c
void cool_md(const char *str, size_t len);
```

Markdown subset to HTML. Not CommonMark.

<!-- END GENERATED API -->
