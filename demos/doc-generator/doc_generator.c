#define _POSIX_C_SOURCE 200809L

/*
 * This simple demo illustrates how I use this library to generate my documentation.
 * Could be used either statically or live because it's as simple as executing doc_generator
 * with the given header file and reading from stdout.
 */

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "cool.h"
#include "cool.c"

#include "view.cool.c"

int parse_header_line(const char *line, char *func_name, char *func_decl, char **comment_out);

int main(int argc, char **argv) {

    if (argc < 2) {
        puts("Usage: doc_generator [FILE]");
        return 1;
    }

    FILE *header_file = fopen(argv[1], "r");

    char *line_buf = malloc(256);
    size_t len = 0;
    while (getline(&line_buf, &len, header_file) > 0) {
        char func_name[256];
        char func_decl[256];
        char *comment_text = NULL;

        if (parse_header_line(line_buf, func_name, func_decl, &comment_text)) {
            Api(func_name, func_decl, comment_text);
        }
    }

    free(line_buf);

    return 0;
}

int parse_header_line(const char *line, char *func_name, char *func_decl, char **comment_out) {
    char *comment_ptr = strstr(line, "//");
    if (!comment_ptr) return 0;

    *comment_out = comment_ptr + 2;
    while (**comment_out == ' ' || **comment_out == '\t') {
        (*comment_out)++;
    }

    size_t decl_len = comment_ptr - line;
    if (decl_len >= 256) decl_len = 255;
    strncpy(func_decl, line, decl_len);
    func_decl[decl_len] = '\0';

    while (decl_len > 0 && (isspace((unsigned char)func_decl[decl_len - 1]) || func_decl[decl_len - 1] == ';')) {
        func_decl[--decl_len] = '\0';
    }

    char *open_paren = strchr(func_decl, '(');
    if (!open_paren) return 0;

    char *end_name = open_paren - 1;
    while (end_name > func_decl && isspace((unsigned char)*end_name)) {
        end_name--;
    }

    char *start_name = end_name;
    while (start_name > func_decl && (isalnum((unsigned char)start_name[-1]) || start_name[-1] == '_')) {
        start_name--;
    }

    size_t name_len = (end_name - start_name) + 1;
    if (name_len >= 256) name_len = 255;
    strncpy(func_name, start_name, name_len);
    func_name[name_len] = '\0';

    return 1;
}
