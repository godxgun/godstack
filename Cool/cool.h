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
 */
#if 0
#include "view.cool.c"
int
main(int argc, char **argv)
{
    FILE *header_file;
    char *line_buf;
    size_t len;

    if (argc < 2) {
        puts("Usage: doc_generator [FILE]");
        return 1;
    }

    header_file = fopen(argv[1], "r");

    line_buf = malloc(256);
    len = 0;
    while (getline(&line_buf, &len, header_file) > 0) {
        char func_name[256];
        char func_decl[256];
        char *comment_text = NULL;

        if (parse_header_line(line_buf, func_name, func_decl, &comment_text)) {
            GenDocs(func_name, func_decl, comment_text);
        }
    }

    free(line_buf);

    return 0;
}
#endif
/* =========================================================================== */

#ifndef _COOL_H_
#define _COOL_H_

#define COOL_MAJOR 0  // breaking API changes
#define COOL_MINOR 1  // non-breaking features
#define COOL_PATCH 0  // non-breaking patches and bug fixes

/* CHANGE LOG
 * 0.0.0 - @vasco - server-side HTML from C functions
 * 0.0.1 - @vasco - COOL_OUTPUT override, cool_html_txt scans *s;
 *                  drop unused URL table, stub cool_html / cool_htmlf, cool_sv_write
 * 0.0.2 - @vasco - include cool.c
 * 0.0.3 - @vasco - html_txt escape without nested raw
 * 0.1.0 - @vasco - cool_md subset to HTML
 */

#ifndef COOL_OUTPUT
#define COOL_OUTPUT stdout
#endif

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    char *cstr;
    size_t len;
} Cool_StrView;

#define COOL_SV(literal) ((Cool_StrView){ .cstr = (literal), .len = sizeof literal - 1 })
void cool_html_raw(Cool_StrView sv); // Writes HTML to buffer without escaping sequences. Vulnerable to XSS. Use with caution.
void cool_html_raw_cstr(const char *str, size_t len); // Writes unsafe HTML using C strings.
void cool_html_txt(const char *str, size_t len); // Meant for inner text. Escapes '<', '>' and '&'.
void cool_htmlf_raw(const char *fmt, ...); // Writes formatting HTML string to buffer without escaping sequences. Vulnerable to XSS. Use with caution.
void cool_md(const char *str, size_t len); // Markdown subset to HTML. Not CommonMark.
                                                                
#endif


/*
------------------------------------------------------------------------------
MIT License
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
*/

