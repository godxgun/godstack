/* ===========================================================================
 * WIRE - Copyright @ Vasco Alves - See LICENSE at the end of file.
 *
 * - HTTP server. GET, static files, in-memory bodies.
 * - Currently UNIX only.
 *
 * PREFIX: WIRE (macros)  Wire (types)  wire_ (functions)
 *
 * USAGE:
 *     #include "wire.h"
 *     #include "wire.c"
 *
 *     int on_req(int fd, const WireHttpRequest *req, void *user)
 *     {
 *         (void)req; (void)user;
 *         return wire_http_write_status(fd, 200, "text/plain", "ok\n", 3);
 *     }
 *
 *     int main(void)
 *     {
 *         return wire_http_serve(8080, on_req, NULL);
 *     }
 *
 * =========================================================================== */

#ifndef WIRE_H
#define WIRE_H

#include <stddef.h>

#define WIRE_MAJOR 0
#define WIRE_MINOR 1
#define WIRE_PATCH 0

/* CHANGE LOG
 * 0.1.0 - @vasco - HTTP server: listen, accept, serve loop, GET
 */

#define WIRE_HTTP_MAX_PATH  512
#define WIRE_HTTP_MAX_REQ   8192

typedef struct WireHttpRequest {
	char method[16];
	char path[WIRE_HTTP_MAX_PATH];
	int hx;
} WireHttpRequest;

typedef int (*WireHttpHandler)(int fd, const WireHttpRequest *req, void *user);

int wire_http_listen(int port); // Bind 0.0.0.0:port. Returns fd, or -1.
int wire_http_accept(int listen_fd); // Accept a client. Returns fd, or -1.
int wire_http_serve(int port, WireHttpHandler handler, void *user); // Listen, read GET, call handler, close. Does not return on success.
int wire_http_read_req(int fd, WireHttpRequest *req); // Parse a GET request. 0 ok, else status.
int wire_http_write_status(int fd, int status, const char *ctype, const void *body, size_t len); // Write a complete response. body may be NULL if len is 0.
int wire_http_error(int fd, int status); // Small HTML error page.
int wire_http_serve_static(int fd, const char *url_path); // GET /static/* from ./static. Returns status.

#endif /* WIRE_H */

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
