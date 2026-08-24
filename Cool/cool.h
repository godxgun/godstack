/* ===========================================================================   
 * Cool - Server-side Components in C - Copyright (c) 2026 Vasco Alves
 *                          
 *                     _.-\                              _.-\
 *               __.-''    \                       __.-''    \
 *             -----------------------------------'           
 *             \ X X           / \ X X           /            
 *              \ X X         /   \ X X         /             
 *               \ X X       /     \ X X       /              
 *                \---------/       \---------/               
 *  
 *               "Hasta la vista, baby."
 *
 * ---------------------------------------------------------------------------   
 *
 * PREFIX: COOL (macros) Cool_ (types)  cool (function) 
 *
 * USAGE:
 * Can be used directly for very simple applications or through a transpiler
 * that converts a separate "higher level" template language directly into 
 * C functions!
 *
 * --- view.cool ---
 * COOL void GenDocs(char *name, char *func, char *desc) {
 *     <div class="api-card">
 *         <h3 class="api-name">{ name }</h3>
 *         <code class="language-c">{ func } </code>
 *         <div class="api-description">
 *             { desc }
 *         </div>
 *     </div>
 * }
 *
 * --- main.c ---
 * #include "view.cool.c"
 * int main(int argc, char **argv) {
 * 
 *     if (argc < 2) {
 *         puts("Usage: doc_generator [FILE]");
 *         return 1;
 *     }
 * 
 *     FILE *header_file = fopen(argv[1], "r");
 * 
 *     char *line_buf = malloc(256);
 *     size_t len = 0;
 *     while (getline(&line_buf, &len, header_file) > 0) {
 *         char func_name[256];
 *         char func_decl[256];
 *         char *comment_text = NULL;
 * 
 *         if (parse_header_line(line_buf, func_name, func_decl, &comment_text)) {
 *             GenDocs(func_name, func_decl, comment_text);
 *         }
 *     }
 * 
 *     free(line_buf);
 * 
 *     return 0;
 * }
 *
 * =========================================================================== */

#ifndef _COOL_H_
#define _COOL_H_

#define COOL_MAJOR 0  // breaking API changes
#define COOL_MINOR 0  // non-breaking features
#define COOL_PATCH 1  // non-breaking patches and bug fixes

/* CHANGE LOG
 * 0.0.0 - @vasco - server-side HTML from C functions
 * 0.0.1 - COOL_OUTPUT override, cool_html_txt scans *s;
 *         drop unused URL table, stub cool_html / cool_htmlf, cool_sv_write
 */

#define COOLDEF static inline
#ifndef COOL_OUTPUT
#define COOL_OUTPUT stdout
#endif

#include <stdarg.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>

typedef struct {
    char *cstr; // original cstring 
    size_t len; // len
} Cool_StrView;

/* Macro to wrap string literal in a string view. Since we will be dealing with a lot of */
#define COOL_SV(literal) ((Cool_StrView){ .cstr = (literal), .len = sizeof(literal) - 1 })
COOLDEF void cool_html_raw(Cool_StrView sv); // Writes HTML to buffer without escaping sequences. Vulnerable to XSS. Use with caution.
COOLDEF void cool_html_raw_cstr(const char *str, size_t len); // Writes unsafe HTML using C strings.
COOLDEF void cool_html_txt(const char *str, size_t len); // Meant for inner text. Escapes '<', '>' and '&'.
COOLDEF void cool_htmlf_raw(const char *fmt, ...); // Writes formatting HTML string to buffer without escaping sequences. Vulnerable to XSS. Use with caution.
                                                                
#endif

#define COOL_IMPLEMENTATION
#ifdef COOL_IMPLEMENTATION
#undef COOL_IMPLEMENTATION

COOLDEF void
cool_html_raw(Cool_StrView sv)
{
    fwrite(sv.cstr, sizeof *sv.cstr, sv.len, COOL_OUTPUT);
}

COOLDEF void
cool_html_raw_cstr(const char *str, size_t len)
{
    fwrite(str, sizeof *str, len, COOL_OUTPUT);
}

COOLDEF void
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

COOLDEF void
cool_htmlf_raw(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vfprintf(COOL_OUTPUT, fmt, args);
    va_end(args);
}
                    
#endif

/*
------------------------------------------------------------------------------
This software is available under 2 licenses -- choose whichever you prefer.
------------------------------------------------------------------------------
ALTERNATIVE A - MIT License
Copyright (c) 2026 Vasco Alves
Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
------------------------------------------------------------------------------
ALTERNATIVE B - Public Domain (www.unlicense.org)
This is free and unencumbered software released into the public domain.
Anyone is free to copy, modify, publish, use, compile, sell, or distribute this
software, either in source code form or as a compiled binary, for any purpose,
commercial or non-commercial, and by any means.
In jurisdictions that recognize copyright laws, the author or authors of this
software dedicate any and all copyright interest in the software to the public
domain. We make this dedication for the benefit of the public at large and to
the detriment of our heirs and successors. We intend this dedication to be an
overt act of relinquishment in perpetuity of all present and future rights to
this software under copyright law.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
------------------------------------------------------------------------------
*/

